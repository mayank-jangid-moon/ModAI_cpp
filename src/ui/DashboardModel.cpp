#include "ui/DashboardModel.h"
#include <QColor>
#include <QBrush>
#include <QString>

namespace ModAI {

DashboardModel::DashboardModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

int DashboardModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(items_.size());
}

int DashboardModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return 9;  // timestamp, author, subreddit, snippet, type, ai_score, labels, status, actions
}

QVariant DashboardModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(items_.size())) {
        return QVariant();
    }
    
    const ContentItem& item = items_[index.row()];
    
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case 0: return QString::fromStdString(item.timestamp);
            case 1: return item.author.has_value() ? QString::fromStdString(item.author.value()) : QString("N/A");
            case 2: return QString::fromStdString(item.subreddit);
            case 3: {
                if (item.text.has_value()) {
                    QString text = QString::fromStdString(item.text.value());
                    return text.left(50) + (text.length() > 50 ? "..." : "");
                }
                return QString("N/A");
            }
            case 4: return QString::fromStdString(item.content_type);
            case 5: return QString::number(item.ai_detection.ai_score, 'f', 2);
            case 6: {
                QString labels;
                // Only show labels with value > 0.3 (filters out very mild detections)
                if (item.moderation.labels.sexual > 0.3) labels += "sexual ";
                if (item.moderation.labels.violence > 0.3) labels += "violence ";
                if (item.moderation.labels.hate > 0.3) labels += "hate ";
                if (item.moderation.labels.drugs > 0.3) labels += "drugs ";
                
                // Add additional labels (only if confidence > 0.3)
                for (const auto& [label, conf] : item.moderation.labels.additional_labels) {
                    if (conf > 0.3) {
                        labels += QString::fromStdString(label) + " ";
                    }
                }
                
                return labels.isEmpty() ? QString("none") : labels.trimmed();
            }
            case 7: return QString::fromStdString(item.decision.auto_action);
            case 8: return QString("View");
            default: return QVariant();
        }
    }
    
    if (role == Qt::BackgroundRole) {
        // Priority 1: Moderation-based flagging (darker red for blocked content)
        if (item.decision.auto_action == "block") {
            // Check if blocked due to moderation labels (not AI)
            bool moderationBlocked = (item.moderation.labels.sexual > 0.9 || 
                                     item.moderation.labels.violence > 0.9 || 
                                     item.moderation.labels.hate > 0.7 ||
                                     item.moderation.labels.drugs > 0.9);
            
            if (moderationBlocked) {
                return QBrush(QColor(220, 53, 69));  // Strong red for moderation blocking
            }
            // AI-based blocking - make it much more visible
            return QBrush(QColor(255, 150, 150));  // More visible light red for AI blocking
        }
        
        // Priority 2: Review status (yellow for moderation review)
        if (item.decision.auto_action == "review") {
            return QBrush(QColor(255, 235, 120));  // More visible yellow
        }
        
        // Priority 3: Check for moderation labels > 0.3 (even if allowed)
        // This catches content with significant labels that didn't trigger blocking threshold
        bool hasSignificantLabels = (item.moderation.labels.sexual > 0.3 ||
                                    item.moderation.labels.violence > 0.3 ||
                                    item.moderation.labels.hate > 0.3 ||
                                    item.moderation.labels.drugs > 0.3);
        
        // Also check additional labels
        if (!hasSignificantLabels) {
            for (const auto& [label, conf] : item.moderation.labels.additional_labels) {
                if (conf > 0.3) {
                    hasSignificantLabels = true;
                    break;
                }
            }
        }
        
        if (hasSignificantLabels) {
            return QBrush(QColor(255, 220, 180));  // Light orange for content with significant labels
        }
        
        // Priority 4: AI detection coloring for allowed content
        if (item.decision.auto_action == "allow") {
            float aiScore = item.ai_detection.ai_score;
            
            if (aiScore > 0.6 && aiScore <= 0.8) {
                // Moderate AI confidence - light yellow
                return QBrush(QColor(255, 245, 180));  // More visible very light yellow
            } else if (aiScore > 0.8) {
                // High AI confidence - light red (but passed other checks)
                return QBrush(QColor(255, 200, 200));  // More visible very light red
            }
        }
        
        // Return white background for clean content
        return QBrush(Qt::white);
    }
    
    if (role == Qt::ForegroundRole) {
        // Ensure text is always readable (black text)
        return QColor(Qt::black);
    }
    
    return QVariant();
}

QVariant DashboardModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "Timestamp";
            case 1: return "Author";
            case 2: return "Subreddit";
            case 3: return "Snippet";
            case 4: return "Type";
            case 5: return "AI Score";
            case 6: return "Labels";
            case 7: return "Status";
            case 8: return "Actions";
            default: return QVariant();
        }
    }
    return QVariant();
}

void DashboardModel::addItem(const ContentItem& item) {
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    items_.push_back(item);
    endInsertRows();
}

void DashboardModel::clear() {
    beginResetModel();
    items_.clear();
    endResetModel();
}

ContentItem DashboardModel::getItem(int row) const {
    if (row >= 0 && row < static_cast<int>(items_.size())) {
        return items_[row];
    }
    return ContentItem();
}

void DashboardModel::updateItem(int row, const ContentItem& item) {
    if (row >= 0 && row < static_cast<int>(items_.size())) {
        items_[row] = item;
        emit dataChanged(index(row, 0), index(row, columnCount() - 1));
    }
}

int DashboardModel::findRowById(const std::string& id) const {
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1; // Not found
}

Qt::ItemFlags DashboardModel::flags(const QModelIndex& index) const {
    return QAbstractTableModel::flags(index);
}

} // namespace ModAI

