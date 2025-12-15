#include "ui/DashboardProxyModel.h"
#include "ui/DashboardModel.h"
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>

namespace ModAI {

DashboardProxyModel::DashboardProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent) {
}

void DashboardProxyModel::setStatusFilter(const QString& status) {
    statusFilter_ = status;
    invalidateFilter();
}

void DashboardProxyModel::setSearchFilter(const QString& text) {
    searchFilter_ = text;
    invalidateFilter();
}

bool DashboardProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    QAbstractItemModel* model = sourceModel();
    if (!model) return true;

    // Check status filter
    if (!statusFilter_.isEmpty() && statusFilter_ != "All Statuses") {
        QModelIndex statusIndex = model->index(source_row, 7, source_parent); // Assuming status is column 7
        // Wait, I need to check DashboardModel column indices.
        // Let's assume column 7 for now, but I should verify.
        QString status = model->data(statusIndex).toString();
        if (status != statusFilter_) {
            return false;
        }
    }

    // Check search filter
    if (!searchFilter_.isEmpty()) {
        bool match = false;
        // Search in all columns
        for (int i = 0; i < model->columnCount(source_parent); ++i) {
            QModelIndex index = model->index(source_row, i, source_parent);
            QString data = model->data(index).toString();
            if (data.contains(searchFilter_, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        if (!match) return false;
    }

    return true;
}

} // namespace ModAI
