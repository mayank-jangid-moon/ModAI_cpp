#ifndef MODAI_DASHBOARD_ITEM_DELEGATE_H
#define MODAI_DASHBOARD_ITEM_DELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>

namespace ModAI {

class DashboardItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DashboardItemDelegate(QObject* parent = nullptr);
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

} // namespace ModAI

#endif // MODAI_DASHBOARD_ITEM_DELEGATE_H
