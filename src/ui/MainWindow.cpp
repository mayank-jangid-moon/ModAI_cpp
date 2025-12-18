#include "ui/MainWindow.h"
#include "core/ModerationEngine.h"
#include "core/RuleEngine.h"
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
#include "ui/DashboardItemDelegate.h"
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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>

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
    
    // Create moderation engine
    moderationEngine_ = std::make_unique<ModerationEngine>(
        std::move(textDetector),
        std::move(imageModerator),
        std::move(textModerator),
        std::move(ruleEngine),
        std::move(storage)
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
    auto imageModeratorForPanel = std::make_shared<HiveImageModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    aiImageDetectorPanel_->initialize(imageModeratorForPanel);
    
    // Also set image moderator for chatbot (for generated image moderation)
    chatbotPanel_->setImageModerator(imageModeratorForPanel);
    
    // Set image moderator for Reddit scraper (for Reddit image moderation)
    scraper_->setImageModerator(std::make_unique<HiveImageModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey));
    
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
    
    // Status bar with modern styling
    statusLabel_ = new QLabel("Ready");
    statusLabel_->setStyleSheet(
        "QLabel { "
        "  color: #6c757d; "
        "  font-size: 13px; "
        "  padding: 4px 8px; "
        "  background-color: transparent; "
        "  border: none; "
        "}"
    );
    statusBar()->addWidget(statusLabel_);
    
    // Theme toggle button (hidden but exists for theme code compatibility)
    themeToggleButton_ = new QPushButton("🌙 Dark");
    themeToggleButton_->setFixedSize(100, 32);
    themeToggleButton_->hide(); // Hidden from UI
    statusBar()->addPermanentWidget(themeToggleButton_);
    connect(themeToggleButton_, &QPushButton::clicked, this, [this]() {
        isDarkTheme_ = !isDarkTheme_;
        applyTheme();
    });

    // Railguard overlay
    railguardOverlay_ = new RailguardOverlay(this);
    railguardOverlay_->hide();
    
    // Apply initial theme (always light)
    isDarkTheme_ = false;
    applyTheme();
}

void MainWindow::setupRedditScraperTab() {
    redditScraperTab_ = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(redditScraperTab_);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);
    
    // Create horizontal splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    
    // Left: Table view
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(16);
    
    // Header with title and description
    QWidget* headerWidget = new QWidget;
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(8, 8, 8, 16);
    headerLayout->setSpacing(12);
    
    QLabel* headerLabel = new QLabel("Reddit Analysis");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setStyleSheet("color: #2c3e50;");
    headerLayout->addWidget(headerLabel);
    
    QLabel* descLabel = new QLabel("Scrape and moderate Reddit content with automated analysis");
    QFont descFont = descLabel->font();
    descFont.setPointSize(12);
    descLabel->setFont(descFont);
    descLabel->setStyleSheet("color: #6c757d;");
    descLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    headerLayout->addWidget(descLabel);
    
    headerLayout->addStretch();
    leftLayout->addWidget(headerWidget);
    
    // Control Frame - Contains scraper controls
    QFrame* controlCard = new QFrame;
    controlCard->setObjectName("controlCard");
    controlCard->setStyleSheet(
        "QFrame#controlCard { "
        "  background-color: #f8f9fa; "
        "  border-radius: 8px; "
        "  border: 1px solid #dee2e6; "
        "  padding: 12px; "
        "}"
    );
    QVBoxLayout* controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setSpacing(12);
    
    // Subreddit Input Row
    QHBoxLayout* subredditLayout = new QHBoxLayout;
    subredditLayout->setSpacing(12);
    
    QLabel* subredditLabel = new QLabel("Subreddit:");
    subredditLabel->setStyleSheet(
        "QLabel { "
        "  font-size: 16pt; "
        "  font-weight: bold; "
        "  color: #2c3e50; "
        "  border: none; "
        "  background: transparent; "
        "  min-width: 100px; "
        "}"
    );
    
    subredditInput_ = new QLineEdit;
    subredditInput_->setPlaceholderText("e.g., technology, programming");
    subredditInput_->setStyleSheet(
        "QLineEdit { "
        "  padding: 10px 14px; "
        "  border: 2px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  font-size: 14px; "
        "  background-color: #fafafa; "
        "}"
        "QLineEdit:focus { "
        "  border-color: #4a90e2; "
        "  background-color: white; "
        "}"
    );
    subredditInput_->setMinimumHeight(40);
    
    subredditLayout->addWidget(subredditLabel);
    subredditLayout->addWidget(subredditInput_, 1);
    controlLayout->addLayout(subredditLayout);
    
    // Action Row
    QHBoxLayout* actionLayout = new QHBoxLayout;
    actionLayout->setSpacing(12);
    
    toggleScrapingButton_ = new QPushButton("▶ Start Scraping");
    toggleScrapingButton_->setStyleSheet(
        "QPushButton { "
        "  padding: 10px 24px; "
        "  background-color: #4a90e2; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-size: 16px; "
        "  font-weight: 600; "
        "  min-width: 150px; "
        "}"
        "QPushButton:hover { "
        "  background-color: #357abd; "
        "}"
        "QPushButton:pressed { "
        "  background-color: #2868a8; "
        "}"
    );
    toggleScrapingButton_->setMinimumHeight(40);
    toggleScrapingButton_->setCursor(Qt::PointingHandCursor);
    
    scrapingStatusLabel_ = new QLabel("");
    scrapingStatusLabel_->setStyleSheet(
        "QLabel { "
        "  color: #666; "
        "  font-style: italic; "
        "  font-size: 13px; "
        "}"
    );
    
    actionLayout->addWidget(toggleScrapingButton_);
    actionLayout->addWidget(scrapingStatusLabel_);
    actionLayout->addStretch();
    controlLayout->addLayout(actionLayout);
    
    leftLayout->addWidget(controlCard);
    
    // Search and Filter Card
    QFrame* filterCard = new QFrame;
    filterCard->setObjectName("filterCard");
    filterCard->setStyleSheet(
        "QFrame#filterCard { "
        "  background-color: white; "
        "  border-radius: 12px; "
        "  border: 1px solid #e0e0e0; "
        "  padding: 16px; "
        "}"
    );
    QHBoxLayout* filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setSpacing(12);
    
    searchInput_ = new QLineEdit;
    searchInput_->setPlaceholderText("🔍 Search posts...");
    searchInput_->setStyleSheet(
        "QLineEdit { "
        "  padding: 8px 14px; "
        "  border: 2px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  font-size: 13px; "
        "  background-color: #fafafa; "
        "}"
        "QLineEdit:focus { "
        "  border-color: #4a90e2; "
        "  background-color: white; "
        "}"
    );
    searchInput_->setMinimumHeight(36);
    
    filterCombo_ = new QComboBox;
    filterCombo_->addItem("All Statuses");
    filterCombo_->addItem("Blocked");
    filterCombo_->addItem("Review");
    filterCombo_->addItem("Allowed");
    filterCombo_->setStyleSheet(
        "QComboBox { "
        "  padding: 8px 32px 8px 12px; "
        "  border: 2px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  font-size: 13px; "
        "  background-color: #fafafa; "
        "  min-width: 150px; "
        "}"
        "QComboBox:hover { "
        "  border-color: #4a90e2; "
        "}"
        "QComboBox::drop-down { "
        "  subcontrol-origin: padding; "
        "  subcontrol-position: top right; "
        "  width: 30px; "
        "  border-left: 1px solid #e0e0e0; "
        "  border-top-right-radius: 8px; "
        "  border-bottom-right-radius: 8px; "
        "}"
        "QComboBox::down-arrow { "
        "  image: none; "
        "  border-left: 5px solid transparent; "
        "  border-right: 5px solid transparent; "
        "  border-top: 6px solid #666; "
        "  width: 0; "
        "  height: 0; "
        "  margin-right: 8px; "
        "}"
        "QComboBox:hover::drop-down { "
        "  background-color: #f0f0f0; "
        "}"
        "QComboBox QAbstractItemView { "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  background-color: white; "
        "  selection-background-color: #4a90e2; "
        "  selection-color: white; "
        "  padding: 4px; "
        "}"
    );
    filterCombo_->setMinimumHeight(36);
    
    // Export/Import buttons
    QPushButton* exportCsvButton = new QPushButton("Export CSV");
    exportCsvButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #28a745; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 6px; "
        "  font-size: 13px; "
        "  font-weight: 500; "
        "}"
        "QPushButton:hover { "
        "  background-color: #218838; "
        "}"
        "QPushButton:pressed { "
        "  background-color: #1e7e34; "
        "}"
    );
    
    QPushButton* exportJsonButton = new QPushButton("Export JSON");
    exportJsonButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #17a2b8; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 6px; "
        "  font-size: 13px; "
        "  font-weight: 500; "
        "}"
        "QPushButton:hover { "
        "  background-color: #138496; "
        "}"
        "QPushButton:pressed { "
        "  background-color: #117a8b; "
        "}"
    );
    
    QPushButton* importButton = new QPushButton("Import Data");
    importButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #6c757d; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 6px; "
        "  font-size: 13px; "
        "  font-weight: 500; "
        "}"
        "QPushButton:hover { "
        "  background-color: #5a6268; "
        "}"
        "QPushButton:pressed { "
        "  background-color: #545b62; "
        "}"
    );
    
    connect(exportCsvButton, &QPushButton::clicked, this, &MainWindow::onExportCsv);
    connect(exportJsonButton, &QPushButton::clicked, this, &MainWindow::onExportJson);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportData);
    
    filterLayout->addWidget(searchInput_, 1);
    filterLayout->addWidget(filterCombo_);
    filterLayout->addWidget(exportCsvButton);
    filterLayout->addWidget(exportJsonButton);
    filterLayout->addWidget(importButton);
    
    leftLayout->addWidget(filterCard);
    
    // Table Container Card
    QFrame* tableCard = new QFrame;
    tableCard->setObjectName("tableCard");
    tableCard->setStyleSheet(
        "QFrame#tableCard { "
        "  background-color: white; "
        "  border-radius: 12px; "
        "  border: 1px solid #e0e0e0; "
        "}"
    );
    QVBoxLayout* tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(0, 0, 0, 0);
    tableCardLayout->setSpacing(0);
    
    // Table
    tableView_ = new QTableView;
    tableView_->setStyleSheet(
        "QTableView { "
        "  border: none; "
        "  border-radius: 12px; "
        "  gridline-color: #f0f0f0; "
        "  font-size: 13px; "
        "}"
        "QTableView::item { "
        "  padding: 8px; "
        "  border-bottom: 1px solid #f0f0f0; "
        "}"
        "QHeaderView::section { "
        "  background-color: #fafafa; "
        "  padding: 10px; "
        "  border: none; "
        "  border-bottom: 2px solid #e0e0e0; "
        "  font-weight: 600; "
        "  font-size: 13px; "
        "  color: #555; "
        "}"
        "QScrollBar:horizontal { "
        "  background: transparent; "
        "  height: 8px; "
        "  margin: 0px 2px 4px 2px; "
        "}"
        "QScrollBar::handle:horizontal { "
        "  background: rgba(0, 0, 0, 0.2); "
        "  border-radius: 4px; "
        "  min-width: 30px; "
        "}"
        "QScrollBar::handle:horizontal:hover { "
        "  background: rgba(0, 0, 0, 0.35); "
        "}"
        "QScrollBar:vertical { "
        "  background: transparent; "
        "  width: 8px; "
        "  margin: 4px 2px 4px 0px; "
        "}"
        "QScrollBar::handle:vertical { "
        "  background: rgba(0, 0, 0, 0.2); "
        "  border-radius: 4px; "
        "  min-height: 30px; "
        "}"
        "QScrollBar::handle:vertical:hover { "
        "  background: rgba(0, 0, 0, 0.35); "
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal, "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "  height: 0px; "
        "  width: 0px; "
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal, "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "  background: none; "
        "}"
    );
    
    model_ = new DashboardModel(this);
    
    // Proxy Model
    proxyModel_ = new DashboardProxyModel(this);
    proxyModel_->setSourceModel(model_);
    
    tableView_->setModel(proxyModel_);
    tableView_->setItemDelegate(new DashboardItemDelegate(this));  // Custom delegate for row coloring
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    
    // Set default column widths
    tableView_->setColumnWidth(0, 180);  // Timestamp
    tableView_->setColumnWidth(1, 150);  // Author
    tableView_->setColumnWidth(2, 120);  // Subreddit
    tableView_->setColumnWidth(3, 350);  // Snippet (large)
    tableView_->setColumnWidth(4, 70);   // Type
    tableView_->setColumnWidth(5, 80);   // AI Score
    tableView_->setColumnWidth(6, 120);  // Labels
    tableView_->setColumnWidth(7, 80);   // Status
    tableView_->setColumnWidth(8, 280);  // Link (stretches to fill)
    
    tableView_->setAlternatingRowColors(false);
    tableView_->setShowGrid(false);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tableView_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    tableCardLayout->addWidget(tableView_);
    leftLayout->addWidget(tableCard, 1);
    splitter->addWidget(leftWidget);
    
    // Right: Detail panel with card styling
    QWidget* rightWidget = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    
    QFrame* detailCard = new QFrame;
    detailCard->setObjectName("detailCard");
    detailCard->setStyleSheet(
        "QFrame#detailCard { "
        "  background-color: white; "
        "  border-radius: 12px; "
        "  border: 1px solid #e0e0e0; "
        "}"
    );
    QVBoxLayout* detailCardLayout = new QVBoxLayout(detailCard);
    detailCardLayout->setContentsMargins(0, 0, 0, 0);
    
    detailPanel_ = new DetailPanel(this);
    detailCardLayout->addWidget(detailPanel_);
    rightLayout->addWidget(detailCard);
    
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setHandleWidth(8);
    splitter->setStyleSheet(
        "QSplitter::handle { "
        "  background-color: transparent; "
        "  margin: 0 4px; "
        "}"
    );
    
    mainLayout->addWidget(splitter);
    
    // Add tab
    tabWidget_->addTab(redditScraperTab_, "Reddit Analysis");
}

void MainWindow::setupChatbotTab() {
    chatbotPanel_ = new ChatbotPanel(this);
    tabWidget_->addTab(chatbotPanel_, "AI Chatbot (Railguard)");
}

void MainWindow::setupAIDetectorTabs() {
    // AI Text Detector
    aiTextDetectorPanel_ = new AITextDetectorPanel(this);
    tabWidget_->addTab(aiTextDetectorPanel_, "Text Fingerprinting");
    
    // AI Image Detector  
    aiImageDetectorPanel_ = new AIImageDetectorPanel(this);
    tabWidget_->addTab(aiImageDetectorPanel_, "Image Fingerprinting");
}

void MainWindow::setupConnections() {
    connect(toggleScrapingButton_, &QPushButton::clicked, this, &MainWindow::onToggleScraping);
    connect(tableView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onTableSelectionChanged);
    
    // Handle clicks on the Link column to open Reddit posts
    connect(tableView_, &QTableView::clicked, this, [this](const QModelIndex& index) {
        // Column 8 is the Link column
        if (index.column() == 8) {
            QString url = index.data().toString();
            if (!url.isEmpty() && url.startsWith("http")) {
                QDesktopServices::openUrl(QUrl(url));
            }
        }
    });
    
    connect(railguardOverlay_, &RailguardOverlay::reviewRequested,
            this, &MainWindow::onReviewRequested);
    connect(railguardOverlay_, &RailguardOverlay::overrideAction,
            this, &MainWindow::onOverrideAction);
    connect(detailPanel_, &DetailPanel::actionRequested,
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

void MainWindow::onToggleScraping() {
    if (scraper_->isScraping()) {
        // Stop scraping
        scraper_->stop();
        
        // Clear the processing queue
        {
            QMutexLocker locker(&queueMutex_);
            processingQueue_.clear();
            processingActive_ = false;
        }
        
        toggleScrapingButton_->setText("▶ Start Scraping");
        toggleScrapingButton_->setStyleSheet(
            "QPushButton { "
            "  padding: 10px 24px; "
            "  background-color: #4a90e2; "
            "  color: white; "
            "  border: none; "
            "  border-radius: 8px; "
            "  font-size: 14px; "
            "  font-weight: 600; "
            "  min-width: 150px; "
            "}"
            "QPushButton:hover { "
            "  background-color: #357abd; "
            "}"
            "QPushButton:pressed { "
            "  background-color: #2868a8; "
            "}"
        );
        scrapingStatusLabel_->setText("");
        statusLabel_->setText("Stopped");
        Logger::info("Scraping stopped and processing queue cleared");
    } else {
        // Start scraping
        QString subreddit = subredditInput_->text().trimmed();
        if (subreddit.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Please enter a subreddit name");
            return;
        }
        
        std::vector<std::string> subreddits = {subreddit.toStdString()};
        scraper_->setSubreddits(subreddits);
        scraper_->start(60);  // Scrape every 60 seconds
        
        toggleScrapingButton_->setText("⏸ Stop Scraping");
        toggleScrapingButton_->setStyleSheet(
            "QPushButton { "
            "  padding: 10px 24px; "
            "  background-color: #dc3545; "
            "  color: white; "
            "  border: none; "
            "  border-radius: 8px; "
            "  font-size: 14px; "
            "  font-weight: 600; "
            "  min-width: 150px; "
            "}"
            "QPushButton:hover { "
            "  background-color: #c82333; "
            "}"
            "QPushButton:pressed { "
            "  background-color: #bd2130; "
            "}"
        );
        scrapingStatusLabel_->setText("Scraping...");
        statusLabel_->setText("Scraping: " + subreddit);
    }
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
        
        themeToggleButton_->setText("Light");
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
        "  border: none; "
        "  border-top: 1px solid #e0e0e0; "
        "  background-color: %1; "
        "}"
        "QTabBar::tab { "
        "  background-color: transparent; "
        "  color: #7f8c8d; "
        "  border: none; "
        "  border-bottom: 3px solid transparent; "
        "  padding: 14px 28px; "
        "  margin-right: 8px; "
        "  margin-bottom: 0px; "
        "  font-size: 15px; "
        "  font-weight: 600; "
        "}"
        "QTabBar::tab:hover { "
        "  color: #4a90e2; "
        "  background-color: rgba(74, 144, 226, 0.08); "
        "  border-bottom: 3px solid rgba(74, 144, 226, 0.4); "
        "}"
        "QTabBar::tab:selected { "
        "  background-color: transparent; "
        "  color: #2c3e50; "
        "  border-bottom: 3px solid #4a90e2; "
        "  font-weight: 700; "
        "}"
        "QTableView { "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  gridline-color: %3; "
        "}"
        "QTableView::item:selected { "
        "  background-color: %5; "
        "  color: white; "
        "}"
        "QScrollBar:horizontal { "
        "  background: transparent; "
        "  height: 8px; "
        "  margin: 0px 2px 4px 2px; "
        "}"
        "QScrollBar::handle:horizontal { "
        "  background: rgba(0, 0, 0, 0.2); "
        "  border-radius: 4px; "
        "  min-width: 30px; "
        "}"
        "QScrollBar::handle:horizontal:hover { "
        "  background: rgba(0, 0, 0, 0.35); "
        "}"
        "QScrollBar:vertical { "
        "  background: transparent; "
        "  width: 8px; "
        "  margin: 4px 2px 4px 0px; "
        "}"
        "QScrollBar::handle:vertical { "
        "  background: rgba(0, 0, 0, 0.2); "
        "  border-radius: 4px; "
        "  min-height: 30px; "
        "}"
        "QScrollBar::handle:vertical:hover { "
        "  background: rgba(0, 0, 0, 0.35); "
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal, "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "  height: 0px; "
        "  width: 0px; "
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal, "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "  background: none; "
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
        "  background-color: #f8f9fa; "
        "  border-top: 1px solid #e0e0e0; "
        "  color: #6c757d; "
        "  padding: 4px; "
        "}"
        "QMenuBar { "
        "  background-color: transparent; "
        "  border: none; "
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

void MainWindow::onExportCsv() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export to CSV",
        QDir::homePath() + "/modai_export.csv",
        "CSV Files (*.csv);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Could not open file for writing: " + fileName);
        return;
    }
    
    QTextStream out(&file);
    
    // Write CSV header
    out << "ID,Timestamp,Author,Subreddit,Content Type,Text,Image Path,";
    out << "AI Score,AI Label,Moderation Sexual,Moderation Violence,Moderation Hate,Moderation Drugs,";
    out << "Additional Labels,Status,Rule ID\n";
    
    // Write data rows
    for (int i = 0; i < model_->rowCount(); ++i) {
        ContentItem item = model_->getItem(i);
        
        // Escape CSV fields (wrap in quotes if they contain commas, quotes, or newlines)
        auto escapeField = [](const std::string& field) -> QString {
            QString qfield = QString::fromStdString(field);
            if (qfield.contains(',') || qfield.contains('"') || qfield.contains('\n')) {
                qfield.replace('"', "\"\"");
                return "\"" + qfield + "\"";
            }
            return qfield;
        };
        
        out << escapeField(item.id) << ",";
        out << escapeField(item.timestamp) << ",";
        out << escapeField(item.author.value_or("N/A")) << ",";
        out << escapeField(item.subreddit) << ",";
        out << escapeField(item.content_type) << ",";
        out << escapeField(item.text.value_or("")) << ",";
        out << escapeField(item.image_path.value_or("")) << ",";
        out << QString::number(item.ai_detection.ai_score, 'f', 4) << ",";
        out << escapeField(item.ai_detection.label) << ",";
        out << QString::number(item.moderation.labels.sexual, 'f', 4) << ",";
        out << QString::number(item.moderation.labels.violence, 'f', 4) << ",";
        out << QString::number(item.moderation.labels.hate, 'f', 4) << ",";
        out << QString::number(item.moderation.labels.drugs, 'f', 4) << ",";
        
        // Additional labels as JSON-like string
        QString additionalLabels = "{";
        bool first = true;
        for (const auto& [label, conf] : item.moderation.labels.additional_labels) {
            if (!first) additionalLabels += ", ";
            additionalLabels += QString::fromStdString(label) + ": " + QString::number(conf, 'f', 4);
            first = false;
        }
        additionalLabels += "}";
        out << escapeField(additionalLabels.toStdString()) << ",";
        
        out << escapeField(item.decision.auto_action) << ",";
        out << escapeField(item.decision.rule_id) << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, "Export Complete", 
        QString("Successfully exported %1 items to:\n%2").arg(model_->rowCount()).arg(fileName));
}

void MainWindow::onExportJson() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export to JSON",
        QDir::homePath() + "/modai_export.json",
        "JSON Files (*.json);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Could not open file for writing: " + fileName);
        return;
    }
    
    QTextStream out(&file);
    
    // Write JSON array
    out << "[\n";
    for (int i = 0; i < model_->rowCount(); ++i) {
        ContentItem item = model_->getItem(i);
        
        if (i > 0) {
            out << ",\n";
        }
        
        // Use ContentItem's toJson method
        out << QString::fromStdString(item.toJson());
    }
    out << "\n]\n";
    
    file.close();
    
    QMessageBox::information(this, "Export Complete", 
        QString("Successfully exported %1 items to:\n%2").arg(model_->rowCount()).arg(fileName));
}

void MainWindow::onImportData() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Import Data",
        QDir::homePath(),
        "Data Files (*.csv *.json);;CSV Files (*.csv);;JSON Files (*.json);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Import Error", "Could not open file for reading: " + fileName);
        return;
    }
    
    // Determine file type by extension
    bool isJson = fileName.endsWith(".json", Qt::CaseInsensitive);
    bool isCsv = fileName.endsWith(".csv", Qt::CaseInsensitive);
    
    if (!isJson && !isCsv) {
        QMessageBox::warning(this, "Import Error", "Unknown file format. Please use .csv or .json files.");
        file.close();
        return;
    }
    
    int importedCount = 0;
    QTextStream in(&file);
    
    try {
        if (isJson) {
            // Read entire JSON file
            QString jsonContent = in.readAll();
            
            // Parse as JSON array
            QJsonDocument doc = QJsonDocument::fromJson(jsonContent.toUtf8());
            if (!doc.isArray()) {
                throw std::runtime_error("JSON file must contain an array of items");
            }
            
            QJsonArray itemsArray = doc.array();
            
            // Ask user if they want to clear existing data
            if (model_->rowCount() > 0) {
                auto reply = QMessageBox::question(
                    this,
                    "Import Data",
                    QString("Current table has %1 items. Do you want to clear them before importing?").arg(model_->rowCount()),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                );
                
                if (reply == QMessageBox::Cancel) {
                    file.close();
                    return;
                }
                
                if (reply == QMessageBox::Yes) {
                    model_->clear();
                }
            }
            
            // Import each item
            for (const QJsonValue& value : itemsArray) {
                if (value.isObject()) {
                    QJsonDocument itemDoc(value.toObject());
                    QString itemJson = itemDoc.toJson(QJsonDocument::Compact);
                    
                    try {
                        ContentItem item = ContentItem::fromJson(itemJson.toStdString());
                        model_->addItem(item);
                        importedCount++;
                    } catch (const std::exception& e) {
                        Logger::warn("Failed to parse item: " + std::string(e.what()));
                    }
                }
            }
            
        } else if (isCsv) {
            // Read entire file content for proper multi-line CSV parsing
            QString content = in.readAll();
            
            // Function to read a complete CSV record (handles multi-line quoted fields)
            auto readCSVRecord = [](const QString& content, int& pos) -> QString {
                QString record;
                bool inQuotes = false;
                
                while (pos < content.length()) {
                    QChar c = content[pos];
                    
                    if (c == '"') {
                        inQuotes = !inQuotes;
                        record += c;
                        pos++;
                    } else if ((c == '\n' || c == '\r') && !inQuotes) {
                        // End of record (skip \r\n or \n)
                        if (c == '\r' && pos + 1 < content.length() && content[pos + 1] == '\n') {
                            pos += 2;
                        } else {
                            pos++;
                        }
                        break;
                    } else {
                        record += c;
                        pos++;
                    }
                }
                return record;
            };
            
            int contentPos = 0;
            
            // Read header line
            QString headerLine = readCSVRecord(content, contentPos);
            
            // Verify header
            if (!headerLine.startsWith("ID,Timestamp")) {
                throw std::runtime_error("Invalid CSV format. Expected header starting with 'ID,Timestamp'");
            }
            
            // Ask user if they want to clear existing data
            if (model_->rowCount() > 0) {
                auto reply = QMessageBox::question(
                    this,
                    "Import Data",
                    QString("Current table has %1 items. Do you want to clear them before importing?").arg(model_->rowCount()),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                );
                
                if (reply == QMessageBox::Cancel) {
                    file.close();
                    return;
                }
                
                if (reply == QMessageBox::Yes) {
                    model_->clear();
                }
            }
            
            auto parseCSVField = [](const QString& record, int& pos) -> QString {
                QString field;
                if (pos >= record.length()) return field;
                
                if (record[pos] == '"') {
                    // Quoted field
                    pos++; // Skip opening quote
                    while (pos < record.length()) {
                        if (record[pos] == '"') {
                            if (pos + 1 < record.length() && record[pos + 1] == '"') {
                                // Escaped quote
                                field += '"';
                                pos += 2;
                            } else {
                                // End of quoted field
                                pos++; // Skip closing quote
                                break;
                            }
                        } else {
                            field += record[pos];
                            pos++;
                        }
                    }
                    // Skip comma after quoted field
                    if (pos < record.length() && record[pos] == ',') {
                        pos++;
                    }
                } else {
                    // Unquoted field
                    while (pos < record.length() && record[pos] != ',') {
                        field += record[pos];
                        pos++;
                    }
                    if (pos < record.length()) {
                        pos++; // Skip comma
                    }
                }
                return field;
            };
            
            // Parse data records
            while (contentPos < content.length()) {
                QString record = readCSVRecord(content, contentPos);
                if (record.trimmed().isEmpty()) continue;
                
                int pos = 0;
                ContentItem item;
                
                try {
                    // Parse CSV fields in order
                    item.id = parseCSVField(record, pos).toStdString();
                    item.timestamp = parseCSVField(record, pos).toStdString();
                    
                    QString author = parseCSVField(record, pos);
                    if (author != "N/A" && !author.isEmpty()) {
                        item.author = author.toStdString();
                    }
                    
                    item.subreddit = parseCSVField(record, pos).toStdString();
                    item.content_type = parseCSVField(record, pos).toStdString();
                    
                    QString text = parseCSVField(record, pos);
                    if (!text.isEmpty()) {
                        item.text = text.toStdString();
                    }
                    
                    QString imagePath = parseCSVField(record, pos);
                    if (!imagePath.isEmpty()) {
                        item.image_path = imagePath.toStdString();
                    }
                    
                    item.ai_detection.ai_score = parseCSVField(record, pos).toDouble();
                    item.ai_detection.label = parseCSVField(record, pos).toStdString();
                    item.moderation.labels.sexual = parseCSVField(record, pos).toDouble();
                    item.moderation.labels.violence = parseCSVField(record, pos).toDouble();
                    item.moderation.labels.hate = parseCSVField(record, pos).toDouble();
                    item.moderation.labels.drugs = parseCSVField(record, pos).toDouble();
                    
                    // Parse additional labels field: format is "{label1: value1, label2: value2}"
                    QString additionalLabelsStr = parseCSVField(record, pos);
                    if (!additionalLabelsStr.isEmpty() && additionalLabelsStr != "{}") {
                        // Remove braces
                        QString inner = additionalLabelsStr.mid(1, additionalLabelsStr.length() - 2);
                        if (!inner.isEmpty()) {
                            // Split by ", " to get individual label:value pairs
                            QStringList pairs = inner.split(", ");
                            for (const QString& pair : pairs) {
                                int colonPos = pair.lastIndexOf(": ");
                                if (colonPos > 0) {
                                    QString label = pair.left(colonPos).trimmed();
                                    QString valueStr = pair.mid(colonPos + 2).trimmed();
                                    double value = valueStr.toDouble();
                                    if (!label.isEmpty() && value > 0) {
                                        item.moderation.labels.additional_labels[label.toStdString()] = value;
                                    }
                                }
                            }
                        }
                    }
                    
                    item.decision.auto_action = parseCSVField(record, pos).toStdString();
                    item.decision.rule_id = parseCSVField(record, pos).toStdString();
                    
                    model_->addItem(item);
                    importedCount++;
                    
                } catch (const std::exception& e) {
                    Logger::warn("Failed to parse CSV record: " + std::string(e.what()));
                }
            }
        }
        
        file.close();
        
        QMessageBox::information(this, "Import Complete", 
            QString("Successfully imported %1 items from:\n%2").arg(importedCount).arg(fileName));
            
    } catch (const std::exception& e) {
        file.close();
        QMessageBox::critical(this, "Import Error", 
            QString("Failed to import data: %1").arg(e.what()));
    }
}

} // namespace ModAI

