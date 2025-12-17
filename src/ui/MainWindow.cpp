#include "ui/MainWindow.h"
#include "core/ModerationEngine.h"
#include "core/RuleEngine.h"
#include "core/ResultCache.h"
#include "detectors/LocalAIDetector.h"
#include "detectors/HiveImageModerator.h"
#include "detectors/HiveTextModerator.h"
#include "network/QtHttpClient.h"
#include "storage/JsonlStorage.h"
#include "storage/Storage.h"
#include "utils/Logger.h"
#include "utils/Crypto.h"
#include "ui/ChatbotPanel.h"
#include "ui/AITextDetectorPanel.h"
#include "ui/AIImageDetectorPanel.h"
#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QItemSelectionModel>
#include <QDateTime>
#include <QUuid>
#include <QMenuBar>
#include <QTabWidget>

namespace ModAI {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , tableView_(nullptr)
    , model_(nullptr)
    , detailPanel_(nullptr)
    , railguardOverlay_(nullptr)
    , workerThread_(nullptr)
    , processingActive_(false) {
    
    // Register ContentItem as Qt meta-type for cross-thread signals
    qRegisterMetaType<ContentItem>("ContentItem");
    
    setupUI();
    setupConnections();
    
    // Initialize components
    dataPath_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString() + "/data";
    QDir().mkpath(QString::fromStdString(dataPath_));
    
    Logger::init(dataPath_ + "/logs/system.log");
    Logger::info("Application started");
    
    // Load API keys from secure storage
    std::string hiveKey = Crypto::getApiKey("hive_api_key");
    std::string redditClientId = Crypto::getApiKey("reddit_client_id");
    std::string redditClientSecret = Crypto::getApiKey("reddit_client_secret");
    
    if (hiveKey.empty()) {
        Logger::warn("No Hive API key - moderation disabled");
        statusBar()->showMessage("⚠ Hive API key missing - moderation disabled", 0);
    }
    
    // Create HTTP client
    auto httpClient = std::make_unique<QtHttpClient>(this);
    
    // Initialize local AI model for text detection
    std::string modelPath = dataPath_ + "/models/ai_detector.onnx";
    std::string tokenizerPath = dataPath_ + "/models";
    
    std::unique_ptr<TextDetector> textDetector;
    
    // Use local ONNX model only
    auto localDetector = std::make_unique<LocalAIDetector>(modelPath, tokenizerPath, 768, 0.5f);
    if (localDetector->isAvailable()) {
        Logger::info("Local ONNX AI detector initialized (desklib/ai-text-detector-v1.01)");
        statusBar()->showMessage("✓ Local AI detection enabled", 3000);
        textDetector = std::move(localDetector);
    } else {
        Logger::error("Local AI model not available. Please export the model first:");
        Logger::error("  python3 scripts/export_model_to_onnx.py --output " + dataPath_ + "/models");
        statusBar()->showMessage("❌ AI model not found - please export model (see logs)", 0);
        
        QMessageBox::warning(this, "AI Model Not Found",
            "Local AI detection model not found.\n\n"
            "Please export the model by running:\n"
            "  python3 scripts/export_model_to_onnx.py --output " +
            QString::fromStdString(dataPath_) + "/models\n\n"
            "The application will continue without AI detection.");
        
        // Create a disabled detector
        textDetector = std::make_unique<LocalAIDetector>(modelPath, tokenizerPath, 768, 0.5f);
    }
    
    auto imageModerator = std::make_unique<HiveImageModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    auto textModerator = std::make_unique<HiveTextModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    
    // Create rule engine
    auto ruleEngine = std::make_unique<RuleEngine>();
    std::string rulesPath = dataPath_ + "/rules.json";
    
    // Copy default rules if not exists
    if (!QFile::exists(QString::fromStdString(rulesPath))) {
        QFile defaultRules("../config/rules.json");
        if (!defaultRules.exists()) {
            // Try alternative path
            defaultRules.setFileName("config/rules.json");
        }
        if (defaultRules.exists()) {
            defaultRules.copy(QString::fromStdString(rulesPath));
        }
    }
    
    ruleEngine->loadRulesFromJson(rulesPath);
    
    // Create storage
    auto storage = std::make_unique<JsonlStorage>(dataPath_);
    storagePtr_ = storage.get();
    
    // Create cache
    std::string cachePath = dataPath_ + "/cache/results.jsonl";
    QDir().mkpath(QString::fromStdString(dataPath_ + "/cache"));
    auto cache = std::make_unique<ResultCache>(cachePath);
    
    // Create moderation engine
    moderationEngine_ = std::make_unique<ModerationEngine>(
        std::move(textDetector),
        std::move(imageModerator),
        std::move(textModerator),
        std::move(ruleEngine),
        std::move(storage),
        std::move(cache)
    );
    
    moderationEngine_->setOnItemProcessed([this](const ContentItem& item) {
        // Use QTimer::singleShot to ensure we're in the right thread
        QTimer::singleShot(0, this, [this, item]() {
            onItemProcessed(item);
        });
    });
    
    // Create scraper
    scraper_ = std::make_unique<RedditScraper>(
        std::make_unique<QtHttpClient>(this),
        redditClientId,
        redditClientSecret,
        "ModAI/1.0 by /u/yourusername",
        dataPath_
    );
    
    scraper_->setOnItemScraped([this](const ContentItem& item) {
        // Add to queue for sequential processing
        QMetaObject::invokeMethod(this, "onItemScraped", Qt::QueuedConnection,
                                  Q_ARG(ContentItem, item));
    });
    
    // Initialize new mode panels with shared components
    
    // Chatbot panel - needs HTTP client and text moderator
    chatbotPanel_->initialize(
        std::make_unique<QtHttpClient>(this),
        std::make_unique<HiveTextModerator>(
            std::make_unique<QtHttpClient>(this), hiveKey),
        "", // LLM API key (optional - can use Ollama locally)
        "http://localhost:11434/api/chat" // Default Ollama endpoint
    );
    
    // AI Text Detector - needs text detector (share a reference)
    // Create another instance for the detector panel
    auto textDetectorForPanel = std::make_unique<LocalAIDetector>(modelPath, tokenizerPath, 768, 0.5f);
    aiTextDetectorPanel_->initialize(std::move(textDetectorForPanel));
    
    // AI Image Detector - needs image moderator
    auto imageModeratorForPanel = std::make_unique<HiveImageModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    aiImageDetectorPanel_->initialize(std::move(imageModeratorForPanel));
    
    // Do not auto-load old records on startup; user can load via menu.
}

MainWindow::~MainWindow() {
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait();
    }
    cleanupOnExit();
}

void MainWindow::setupUI() {
    setWindowTitle("ModAI - Multi-Mode Content Analysis");
    setMinimumSize(1400, 900);
    
    // Create tab widget as central widget
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setTabPosition(QTabWidget::North);
    setCentralWidget(tabWidget_);
    
    // Setup all mode tabs
    setupRedditScraperTab();
    setupChatbotTab();
    setupAIDetectorTabs();
    
    // Status bar
    statusLabel_ = new QLabel("Ready");
    statusBar()->addWidget(statusLabel_);
    
    // Theme toggle button in top right corner
    themeToggleButton_ = new QPushButton("🌙 Dark");
    themeToggleButton_->setFixedSize(100, 32);
    themeToggleButton_->setStyleSheet(
        "QPushButton { "
        "  background-color: #f0f0f0; "
        "  border: 1px solid #ccc; "
        "  border-radius: 16px; "
        "  font-weight: bold; "
        "  padding: 5px 15px; "
        "}"
        "QPushButton:hover { background-color: #e0e0e0; }"
    );
    statusBar()->addPermanentWidget(themeToggleButton_);
    connect(themeToggleButton_, &QPushButton::clicked, this, [this]() {
        isDarkTheme_ = !isDarkTheme_;
        applyTheme();
    });
    
    // Menu: History -> Load Previous
    QMenu* historyMenu = menuBar()->addMenu("History");
    QAction* loadHistory = new QAction("Load Previous Reddit Session", this);
    historyMenu->addAction(loadHistory);
    connect(loadHistory, &QAction::triggered, this, &MainWindow::onLoadHistory);

    // Railguard overlay
    railguardOverlay_ = new RailguardOverlay(this);
    railguardOverlay_->hide();
    
    // Apply initial theme (light)
    applyTheme();
}

void MainWindow::setupRedditScraperTab() {
    redditScraperTab_ = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(redditScraperTab_);
    
    // Create horizontal splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    
    // Left: Table view
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    
    // Toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout;
    startButton_ = new QPushButton("Start Scraping");
    stopButton_ = new QPushButton("Stop Scraping");
    stopButton_->setEnabled(false);
    subredditInput_ = new QLineEdit;
    subredditInput_->setPlaceholderText("Enter subreddit (e.g., technology)");
    
    toolbarLayout->addWidget(new QLabel("Subreddit:"));
    toolbarLayout->addWidget(subredditInput_);
    toolbarLayout->addWidget(startButton_);
    toolbarLayout->addWidget(stopButton_);
    toolbarLayout->addStretch();
    
    leftLayout->addLayout(toolbarLayout);
    
    // Search and Filter toolbar
    QHBoxLayout* filterLayout = new QHBoxLayout;
    searchInput_ = new QLineEdit;
    searchInput_->setPlaceholderText("Search...");
    filterCombo_ = new QComboBox;
    filterCombo_->addItem("All Statuses");
    filterCombo_->addItem("Blocked");
    filterCombo_->addItem("Review");
    filterCombo_->addItem("Allowed");
    
    filterLayout->addWidget(new QLabel("Search:"));
    filterLayout->addWidget(searchInput_);
    filterLayout->addWidget(new QLabel("Filter:"));
    filterLayout->addWidget(filterCombo_);
    filterLayout->addStretch();
    
    leftLayout->addLayout(filterLayout);
    
    // Table
    tableView_ = new QTableView;
    model_ = new DashboardModel(this);
    
    // Proxy Model
    proxyModel_ = new DashboardProxyModel(this);
    proxyModel_->setSourceModel(model_);
    
    tableView_->setModel(proxyModel_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->setAlternatingRowColors(true);
    
    leftLayout->addWidget(tableView_);
    splitter->addWidget(leftWidget);
    
    // Right: Detail panel
    detailPanel_ = new DetailPanel(this);
    splitter->addWidget(detailPanel_);
    
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter);
    
    // Add tab
    tabWidget_->addTab(redditScraperTab_, "📱 Reddit Scraper & Moderator");
}

void MainWindow::setupChatbotTab() {
    chatbotPanel_ = new ChatbotPanel(this);
    tabWidget_->addTab(chatbotPanel_, "💬 AI Chatbot (Railguard)");
}

void MainWindow::setupAIDetectorTabs() {
    // AI Text Detector
    aiTextDetectorPanel_ = new AITextDetectorPanel(this);
    tabWidget_->addTab(aiTextDetectorPanel_, "📝 AI Text Detector");
    
    // AI Image Detector  
    aiImageDetectorPanel_ = new AIImageDetectorPanel(this);
    tabWidget_->addTab(aiImageDetectorPanel_, "🖼️ AI Image Detector");
}

void MainWindow::setupConnections() {
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::onStartScraping);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopScraping);
    connect(tableView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onTableSelectionChanged);
    connect(railguardOverlay_, &RailguardOverlay::reviewRequested,
            this, &MainWindow::onReviewRequested);
    connect(railguardOverlay_, &RailguardOverlay::overrideAction,
            this, &MainWindow::onOverrideAction);
    connect(detailPanel_, &DetailPanel::processCommentsRequested,
            this, &MainWindow::onProcessCommentsRequested);
            
    connect(searchInput_, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(filterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFilterChanged);
}

void MainWindow::loadExistingData() {
    if (!storagePtr_) {
        return;
    }
    auto items = storagePtr_->loadAllContent();
    model_->clear();
    for (const auto& item : items) {
        model_->addItem(item);
    }
    historyLoaded_ = true;
    statusBar()->showMessage("Loaded previous session items", 3000);
}

void MainWindow::onStartScraping() {
    QString subreddit = subredditInput_->text().trimmed();
    if (subreddit.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a subreddit name");
        return;
    }
    
    std::vector<std::string> subreddits = {subreddit.toStdString()};
    scraper_->setSubreddits(subreddits);
    scraper_->start(60);  // Scrape every 60 seconds
    
    startButton_->setEnabled(false);
    stopButton_->setEnabled(true);
    statusLabel_->setText("Scraping: " + subreddit);
}

void MainWindow::onStopScraping() {
    scraper_->stop();
    
    // Clear the processing queue
    {
        QMutexLocker locker(&queueMutex_);
        processingQueue_.clear();
        processingActive_ = false;
    }
    
    startButton_->setEnabled(true);
    stopButton_->setEnabled(false);
    statusLabel_->setText("Stopped");
    Logger::info("Scraping stopped and processing queue cleared");
}

void MainWindow::onItemScraped(const ContentItem& item) {
    // Add item to processing queue (don't show in UI until processed)
    {
        QMutexLocker locker(&queueMutex_);
        processingQueue_.enqueue(item);
    }
    
    // Start processing if not already active
    if (!processingActive_) {
        processingActive_ = true;
        processNextItem();
    }
}

void MainWindow::processNextItem() {
    ContentItem item;
    
    {
        QMutexLocker locker(&queueMutex_);
        if (processingQueue_.isEmpty()) {
            processingActive_ = false;
            return;
        }
        item = processingQueue_.dequeue();
    }
    
    // Process in background thread
    QtConcurrent::run([this, item]() {
        moderationEngine_->processItem(item);
        
        // Process next item after this one completes
        QMetaObject::invokeMethod(this, "processNextItem", Qt::QueuedConnection);
    });
}

void MainWindow::onItemProcessed(const ContentItem& item) {
    // Add item to UI after processing is complete
    model_->addItem(item);
    
    // Show railguard if blocked
    if (item.decision.auto_action == "block") {
        railguardOverlay_->showBlockedItem(item);
    }
}

void MainWindow::onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    Q_UNUSED(deselected);
    
    if (!selected.indexes().isEmpty()) {
        int row = selected.indexes().first().row();
        ContentItem item = model_->getItem(row);
        detailPanel_->setContentItem(item);
    }
}

void MainWindow::onReviewRequested(const std::string& itemId) {
    // Find item and show in detail panel
    for (int i = 0; i < model_->rowCount(); ++i) {
        ContentItem item = model_->getItem(i);
        if (item.id == itemId) {
            tableView_->selectRow(i);
            detailPanel_->setContentItem(item);
            break;
        }
    }
}

void MainWindow::onOverrideAction(const std::string& itemId, const std::string& newStatus) {
    if (!storagePtr_) {
        Logger::warn("Storage unavailable for override action");
        return;
    }

    for (int i = 0; i < model_->rowCount(); ++i) {
        ContentItem item = model_->getItem(i);
        if (item.id == itemId) {
            std::string prev = item.decision.auto_action;
            item.decision.auto_action = newStatus;
            model_->updateItem(i, item);

            HumanAction action;
            action.action_id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            action.content_id = itemId;
            action.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
            action.reviewer = "local_user";
            action.previous_status = prev;
            action.new_status = newStatus;
            action.reason = "manual override";
            action.schema_version = 1;

            try {
                storagePtr_->saveAction(action);
            } catch (const std::exception& e) {
                Logger::error("Failed to save override action: " + std::string(e.what()));
            }
            break;
        }
    }
}

void MainWindow::onSearchTextChanged(const QString& text) {
    proxyModel_->setSearchFilter(text);
}

void MainWindow::onFilterChanged(int index) {
    QString text = filterCombo_->itemText(index);
    QString status;
    if (text == "Blocked") status = "block";
    else if (text == "Review") status = "review";
    else if (text == "Allowed") status = "allow";
    else status = ""; // All Statuses
    
    proxyModel_->setStatusFilter(status);
}

void MainWindow::onLoadHistory() {
    if (historyLoaded_) {
        statusBar()->showMessage("History already loaded", 2000);
        return;
    }
    loadExistingData();
}

void MainWindow::onProcessCommentsRequested(const std::string& subreddit, const std::string& postId) {
    Logger::info("Fetching comments for post " + postId + " in r/" + subreddit);
    
    if (!scraper_) {
        Logger::error("Scraper not initialized");
        return;
    }
    
    // Fetch comments in background thread
    QtConcurrent::run([this, subreddit, postId]() {
        try {
            auto comments = scraper_->fetchPostComments(subreddit, postId);
            Logger::info("Processing " + std::to_string(comments.size()) + " comments");
            
            // Queue each comment for processing
            for (const auto& comment : comments) {
                QMetaObject::invokeMethod(this, "onItemScraped", Qt::QueuedConnection,
                                          Q_ARG(ContentItem, comment));
            }
            
            // Re-enable button on UI thread
            QMetaObject::invokeMethod(this, [this]() {
                statusBar()->showMessage("Comments queued for processing", 3000);
            }, Qt::QueuedConnection);
            
        } catch (const std::exception& e) {
            Logger::error("Error fetching comments: " + std::string(e.what()));
        }
    });
}

void MainWindow::cleanupOnExit() {
    // Delete scraped content when closing the app
    QString contentFile = QString::fromStdString(dataPath_ + "/content.jsonl");
    if (QFile::exists(contentFile)) {
        QFile::remove(contentFile);
    }
    // Optionally clear cached images directory if it exists
    QDir imagesDir(QString::fromStdString(dataPath_ + "/images"));
    if (imagesDir.exists()) {
        imagesDir.removeRecursively();
    }
    // Clear cache results
    QString cacheFile = QString::fromStdString(dataPath_ + "/cache/results.jsonl");
    if (QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }
}

void MainWindow::applyTheme() {
    QString bgColor, fgColor, borderColor, tabBg, tabSelectedBg, buttonBg, buttonHover;
    
    if (isDarkTheme_) {
        // Dark theme colors
        bgColor = "#1e1e1e";
        fgColor = "#e0e0e0";
        borderColor = "#333";
        tabBg = "#2d2d2d";
        tabSelectedBg = "#0066cc";
        buttonBg = "#2d2d2d";
        buttonHover = "#3d3d3d";
        
        themeToggleButton_->setText("☀️ Light");
        themeToggleButton_->setStyleSheet(
            "QPushButton { "
            "  background-color: #2d2d2d; "
            "  color: #e0e0e0; "
            "  border: 1px solid #444; "
            "  border-radius: 16px; "
            "  font-weight: bold; "
            "  padding: 5px 15px; "
            "}"
            "QPushButton:hover { background-color: #3d3d3d; }"
        );
    } else {
        // Light theme colors
        bgColor = "#ffffff";
        fgColor = "#333333";
        borderColor = "#ddd";
        tabBg = "#f0f0f0";
        tabSelectedBg = "#0066cc";
        buttonBg = "#f0f0f0";
        buttonHover = "#e0e0e0";
        
        themeToggleButton_->setText("🌙 Dark");
        themeToggleButton_->setStyleSheet(
            "QPushButton { "
            "  background-color: #f0f0f0; "
            "  color: #333; "
            "  border: 1px solid #ccc; "
            "  border-radius: 16px; "
            "  font-weight: bold; "
            "  padding: 5px 15px; "
            "}"
            "QPushButton:hover { background-color: #e0e0e0; }"
        );
    }
    
    // Apply to main window
    QString mainStyle = QString(
        "QMainWindow, QWidget { "
        "  background-color: %1; "
        "  color: %2; "
        "}"
        "QTabWidget::pane { "
        "  border: 1px solid %3; "
        "  background-color: %1; "
        "}"
        "QTabBar::tab { "
        "  background-color: %4; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  padding: 8px 20px; "
        "  margin-right: 2px; "
        "  border-top-left-radius: 4px; "
        "  border-top-right-radius: 4px; "
        "}"
        "QTabBar::tab:selected { "
        "  background-color: %5; "
        "  color: white; "
        "}"
        "QTableView { "
        "  background-color: %1; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  gridline-color: %3; "
        "}"
        "QTableView::item:selected { "
        "  background-color: %5; "
        "  color: white; "
        "}"
        "QHeaderView::section { "
        "  background-color: %4; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  padding: 4px; "
        "}"
        "QLineEdit, QTextEdit { "
        "  background-color: %1; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  border-radius: 4px; "
        "  padding: 5px; "
        "}"
        "QPushButton { "
        "  background-color: %6; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  border-radius: 4px; "
        "  padding: 6px 12px; "
        "}"
        "QPushButton:hover { "
        "  background-color: %7; "
        "}"
        "QLabel { color: %2; }"
        "QComboBox { "
        "  background-color: %1; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  border-radius: 4px; "
        "  padding: 5px; "
        "}"
        "QComboBox QAbstractItemView { "
        "  background-color: %1; "
        "  color: %2; "
        "  selection-background-color: %5; "
        "}"
        "QProgressBar { "
        "  border: 1px solid %3; "
        "  border-radius: 4px; "
        "  text-align: center; "
        "  background-color: %4; "
        "  color: %2; "
        "}"
        "QProgressBar::chunk { "
        "  background-color: %5; "
        "}"
        "QMenuBar { "
        "  background-color: %4; "
        "  color: %2; "
        "}"
        "QMenuBar::item:selected { "
        "  background-color: %5; "
        "}"
        "QMenu { "
        "  background-color: %1; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "}"
        "QMenu::item:selected { "
        "  background-color: %5; "
        "}"
        "QStatusBar { "
        "  background-color: %4; "
        "  color: %2; "
        "}"
    ).arg(bgColor, fgColor, borderColor, tabBg, tabSelectedBg, buttonBg, buttonHover);
    
    setStyleSheet(mainStyle);
    
    // Apply theme to all panels
    if (chatbotPanel_) {
        chatbotPanel_->setTheme(isDarkTheme_);
    }
    if (aiTextDetectorPanel_) {
        aiTextDetectorPanel_->setTheme(isDarkTheme_);
    }
    if (aiImageDetectorPanel_) {
        aiImageDetectorPanel_->setTheme(isDarkTheme_);
    }
}

} // namespace ModAI

