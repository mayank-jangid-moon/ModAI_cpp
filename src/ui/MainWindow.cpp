#include "ui/MainWindow.h"
#include "core/ModerationEngine.h"
#include "core/RuleEngine.h"
#include "core/ResultCache.h"
#include "detectors/HFTextDetector.h"
#include "detectors/HiveImageModerator.h"
#include "detectors/HiveTextModerator.h"
#include "network/QtHttpClient.h"
#include "storage/JsonlStorage.h"
#include "utils/Logger.h"
#include "utils/Crypto.h"
#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QItemSelectionModel>

namespace ModAI {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , tableView_(nullptr)
    , model_(nullptr)
    , detailPanel_(nullptr)
    , railguardOverlay_(nullptr)
    , workerThread_(nullptr) {
    
    setupUI();
    setupConnections();
    
    // Initialize components
    std::string dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString() + "/data";
    QDir().mkpath(QString::fromStdString(dataPath));
    
    Logger::init(dataPath + "/logs/system.log");
    Logger::info("Application started");
    
    // Load API keys from secure storage
    std::string hfToken = Crypto::getApiKey("huggingface_token");
    std::string hiveKey = Crypto::getApiKey("hive_api_key");
    std::string redditClientId = Crypto::getApiKey("reddit_client_id");
    std::string redditClientSecret = Crypto::getApiKey("reddit_client_secret");
    
    if (hfToken.empty() && hiveKey.empty()) {
        Logger::warn("No API keys configured - using public Reddit scraping only");
        statusBar()->showMessage("⚠ No API keys - detection features disabled. Scraping only.", 0);
    } else if (hfToken.empty()) {
        Logger::warn("No HuggingFace token - AI text detection disabled");
        statusBar()->showMessage("⚠ HuggingFace token missing - AI detection disabled", 0);
    } else if (hiveKey.empty()) {
        Logger::warn("No Hive API key - moderation disabled");
        statusBar()->showMessage("⚠ Hive API key missing - moderation disabled", 0);
    }
    
    // Create HTTP client
    auto httpClient = std::make_unique<QtHttpClient>(this);
    
    // Create detectors
    auto textDetector = std::make_unique<HFTextDetector>(
        std::make_unique<QtHttpClient>(this), hfToken);
    auto imageModerator = std::make_unique<HiveImageModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    auto textModerator = std::make_unique<HiveTextModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    
    // Create rule engine
    auto ruleEngine = std::make_unique<RuleEngine>();
    std::string rulesPath = dataPath + "/rules.json";
    
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
    auto storage = std::make_unique<JsonlStorage>(dataPath);
    
    // Create cache
    std::string cachePath = dataPath + "/cache/results.jsonl";
    QDir().mkpath(QString::fromStdString(dataPath + "/cache"));
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
        dataPath
    );
    
    scraper_->setOnItemScraped([this](const ContentItem& item) {
        // Process item through moderation engine
        moderationEngine_->processItem(item);
    });
    
    loadExistingData();
}

MainWindow::~MainWindow() {
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait();
    }
}

void MainWindow::setupUI() {
    setWindowTitle("Trust & Safety Dashboard");
    setMinimumSize(1200, 800);
    
    // Central widget with splitter
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(mainSplitter);
    
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
    
    mainSplitter->addWidget(leftWidget);
    
    // Right: Detail panel
    detailPanel_ = new DetailPanel(this);
    mainSplitter->addWidget(detailPanel_);
    
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);
    
    // Status bar
    statusLabel_ = new QLabel("Ready");
    statusBar()->addWidget(statusLabel_);
    
    // Railguard overlay
    railguardOverlay_ = new RailguardOverlay(this);
    railguardOverlay_->hide();
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
            
    connect(searchInput_, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(filterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFilterChanged);
}

void MainWindow::loadExistingData() {
    // Load existing content from storage
    // This would be done asynchronously in production
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
    startButton_->setEnabled(true);
    stopButton_->setEnabled(false);
    statusLabel_->setText("Stopped");
}

void MainWindow::onItemProcessed(const ContentItem& item) {
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
    // Update item status and save action
    // Implementation would update storage and model
    Logger::info("Override action: " + itemId + " -> " + newStatus);
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

} // namespace ModAI

