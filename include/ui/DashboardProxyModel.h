#pragma once

#include <QSortFilterProxyModel>

namespace ModAI {

class DashboardProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit DashboardProxyModel(QObject* parent = nullptr);

    void setStatusFilter(const QString& status);
    void setSearchFilter(const QString& text);
    
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    QString statusFilter_;
    QString searchFilter_;
};

} // namespace ModAI
