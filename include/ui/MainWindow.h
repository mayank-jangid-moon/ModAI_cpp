#pragma once

#include <QMainWindow>
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
#include <memory>

namespace ModAI {

class MainWindow : public QMainWindow {
    Q_OBJECT

private:
    QTableView* tableView_;
    DashboardModel* model_;
    DetailPanel* detailPanel_;
    RailguardOverlay* railguardOverlay_;
    
    QPushButton* startButton_;
    QPushButton* stopButton_;
    QLineEdit* subredditInput_;
    QLabel* statusLabel_;
    
    std::unique_ptr<ModerationEngine> moderationEngine_;
    std::unique_ptr<RedditScraper> scraper_;
    
    QThread* workerThread_;
    
    void setupUI();
    void setupConnections();
    void loadExistingData();

private slots:
    void onStartScraping();
    void onStopScraping();
    void onItemProcessed(const ContentItem& item);
    void onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onReviewRequested(const std::string& itemId);
    void onOverrideAction(const std::string& itemId, const std::string& newStatus);

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
};

} // namespace ModAI

