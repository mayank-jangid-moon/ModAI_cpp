#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QTableView>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QStatusBar>
#include <QThread>
#include <QThreadPool>
#include "core/ModerationEngine.h"
#include "scraper/RedditScraper.h"
#include "ui/DashboardModel.h"
#include "ui/DetailPanel.h"
#include "ui/RailguardOverlay.h"
#include "ui/ChatbotPanel.h"
#include "ui/AITextDetectorPanel.h"
#include "ui/AIImageDetectorPanel.h"
#include "storage/Storage.h"
#include <QSortFilterProxyModel>
#include <QComboBox>
#include "ui/DashboardProxyModel.h"
#include <QtConcurrent>
#include <QAction>
#include <QQueue>
#include <QMutex>

namespace ModAI {

class MainWindow : public QMainWindow {
    Q_OBJECT

private:
    // Tab widget for different modes
    QTabWidget* tabWidget_;
    
    // Reddit Scraper Mode (existing functionality)
    QWidget* redditScraperTab_;
    QTableView* tableView_;
    DashboardModel* model_;
    DashboardProxyModel* proxyModel_;
    DetailPanel* detailPanel_;
    QPushButton* startButton_;
    QPushButton* stopButton_;
    QLineEdit* subredditInput_;
    QLineEdit* searchInput_;
    QComboBox* filterCombo_;
    
    // New mode panels
    ChatbotPanel* chatbotPanel_;
    AITextDetectorPanel* aiTextDetectorPanel_;
    AIImageDetectorPanel* aiImageDetectorPanel_;
    
    RailguardOverlay* railguardOverlay_;
    QLabel* statusLabel_;
    
    std::unique_ptr<ModerationEngine> moderationEngine_;
    std::unique_ptr<RedditScraper> scraper_;
    Storage* storagePtr_{nullptr};
    bool historyLoaded_{false};
    std::string dataPath_;
    
    QThread* workerThread_;
    QQueue<ContentItem> processingQueue_;
    QMutex queueMutex_;
    bool processingActive_;
    
    // Theme support
    bool isDarkTheme_{false};
    QPushButton* themeToggleButton_;
    void applyTheme();
    
    void setupUI();
    void setupRedditScraperTab();
    void setupChatbotTab();
    void setupAIDetectorTabs();
    void setupConnections();
    void loadExistingData();
    void cleanupOnExit();

private slots:
    void onStartScraping();
    void onStopScraping();
    void onItemProcessed(const ContentItem& item);
    void onItemScraped(const ContentItem& item);
    void processNextItem();
    void onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onReviewRequested(const std::string& itemId);
    void onOverrideAction(const std::string& itemId, const std::string& newStatus);
    void onSearchTextChanged(const QString& text);
    void onFilterChanged(int index);
    void onLoadHistory();
    void onProcessCommentsRequested(const std::string& subreddit, const std::string& postId);

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
};

} // namespace ModAI

