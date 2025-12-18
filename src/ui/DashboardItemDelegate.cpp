#include "ui/DashboardItemDelegate.h"
#include <QApplication>

namespace ModAI {

DashboardItemDelegate::DashboardItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {
}

void DashboardItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
    painter->save();
    
    // Get the background color from the model
    QVariant bgData = index.data(Qt::BackgroundRole);
    
    // Fill background with the model's color
    if (bgData.isValid()) {
        QBrush brush = bgData.value<QBrush>();
        painter->fillRect(option.rect, brush);
    } else {
        // Default white background
        painter->fillRect(option.rect, Qt::white);
    }
    
    // Draw selection highlight on top if selected
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, QColor(74, 144, 226, 100)); // Semi-transparent blue
    }
    
    // Draw hover highlight
    if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, QColor(0, 0, 0, 20)); // Very subtle hover
    }
    
    // Now draw the text and other content
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    
    // Clear the background so parent doesn't repaint it
    opt.backgroundBrush = Qt::NoBrush;
    
    // Draw the text
    painter->setPen(Qt::black);
    QRect textRect = option.rect.adjusted(8, 0, -8, 0); // Add padding
    QString text = index.data(Qt::DisplayRole).toString();
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
    
    // Draw bottom border
    painter->setPen(QColor(240, 240, 240));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    
    painter->restore();
}

} // namespace ModAI
