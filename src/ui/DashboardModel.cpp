#include "ui/DashboardModel.h"
#include <QColor>
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
                if (item.moderation.labels.sexual > 0.5) labels += "sexual ";
                if (item.moderation.labels.violence > 0.5) labels += "violence ";
                if (item.moderation.labels.hate > 0.5) labels += "hate ";
                if (item.moderation.labels.drugs > 0.5) labels += "drugs ";
                return labels.isEmpty() ? QString("none") : labels;
            }
            case 7: return QString::fromStdString(item.decision.auto_action);
            case 8: return QString("View");
            default: return QVariant();
        }
    }
    
    if (role == Qt::BackgroundRole) {
        // Color based on severity
        if (item.decision.auto_action == "block") {
            return QColor(255, 200, 200);  // Light red
        } else if (item.decision.auto_action == "review") {
            return QColor(255, 255, 200);  // Light yellow
        }
        return QVariant();
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

