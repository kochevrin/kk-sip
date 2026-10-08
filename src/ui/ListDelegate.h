#pragma once

#include <QStyledItemDelegate>

// Two-line list row: title, muted subtitle, optional icon and BLF lamp. Sizes itself
// from the fonts, so rows never overlap whatever the style sheet padding is.
class ListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum Role {
        SubtitleRole = Qt::UserRole + 100,
        LampRole, // QColor of the BLF lamp; invalid = no lamp, but keep its slot
        HasLampSlotRole, // bool: reserve space for a lamp in this list
        BadgeRole, // short text in a pill at the right of the title, e.g. a call count
    };

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
