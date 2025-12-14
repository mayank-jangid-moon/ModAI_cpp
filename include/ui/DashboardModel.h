#pragma once

#include "core/ContentItem.h"
#include <QAbstractTableModel>
#include <vector>

namespace ModAI {

class DashboardModel : public QAbstractTableModel {
    Q_OBJECT

private:
    std::vector<ContentItem> items_;

public:
    explicit DashboardModel(QObject* parent = nullptr);
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void addItem(const ContentItem& item);
    void clear();
    ContentItem getItem(int row) const;
    void updateItem(int row, const ContentItem& item);
    
    Qt::ItemFlags flags(const QModelIndex& index) const override;
};

} // namespace ModAI

