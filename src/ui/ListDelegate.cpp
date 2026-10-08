#include "ui/ListDelegate.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QPainter>

static constexpr int PadX = 8;
static constexpr int PadY = 6;
static constexpr int LineGap = 1;
static constexpr int Lamp = 10;

static QFont subtitleFont(const QFont &base)
{
    QFont f = base;
    f.setPointSizeF(base.pointSizeF() * 0.88);
    return f;
}

QSize ListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QFontMetrics title(option.font);
    const QFontMetrics sub(subtitleFont(option.font));
    int h = PadY * 2 + title.height();
    if (!index.data(SubtitleRole).toString().isEmpty())
        h += LineGap + sub.height();
    return {option.rect.width() > 0 ? option.rect.width() : 200, qMax(h, 28)};
}

void ListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Background (selection, hover, alternate rows) from the style / style sheet, no text.
    QStyleOptionViewItem bg = option;
    initStyleOption(&bg, index);
    bg.text.clear();
    bg.icon = QIcon();
    const QWidget *w = option.widget;
    QStyle *style = w ? w->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &bg, painter, w);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QRect r = option.rect.adjusted(PadX, PadY, -PadX, -PadY);

    if (index.data(HasLampSlotRole).toBool()) {
        const QColor lamp = index.data(LampRole).value<QColor>();
        if (lamp.isValid()) {
            const QRectF dot(r.left(), r.top() + (QFontMetrics(option.font).height() - Lamp) / 2.0, Lamp, Lamp);
            // Soft halo so a lit lamp reads at a glance.
            QColor halo = lamp;
            halo.setAlpha(70);
            painter->setPen(Qt::NoPen);
            painter->setBrush(halo);
            painter->drawEllipse(dot.adjusted(-3, -3, 3, 3));
            painter->setBrush(lamp);
            painter->drawEllipse(dot);
        }
        r.setLeft(r.left() + Lamp + 8);
    }

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!icon.isNull()) {
        const int s = 16;
        icon.paint(painter, QRect(r.left(), r.top() + (QFontMetrics(option.font).height() - s) / 2, s, s));
        r.setLeft(r.left() + s + 8);
    }

    const QVariant fg = index.data(Qt::ForegroundRole);
    const QColor titleColor = fg.isValid() ? fg.value<QBrush>().color() : option.palette.color(QPalette::Text);
    const QFontMetrics tm(option.font);
    painter->setFont(option.font);
    painter->setPen(titleColor);
    painter->drawText(QRect(r.left(), r.top(), r.width(), tm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      tm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, r.width()));

    const QString subtitle = index.data(SubtitleRole).toString();
    if (!subtitle.isEmpty()) {
        const QFont sf = subtitleFont(option.font);
        const QFontMetrics sm(sf);
        painter->setFont(sf);
        painter->setPen(fg.isValid() ? titleColor : Theme::colors().muted);
        painter->drawText(QRect(r.left(), r.top() + tm.height() + LineGap, r.width(), sm.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, sm.elidedText(subtitle, Qt::ElideRight, r.width()));
    }
    painter->restore();
}
