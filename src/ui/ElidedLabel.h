#pragma once

#include <QLabel>
#include <QPainter>

// One line of text cut with "…" when it does not fit; never widens its layout.
class ElidedLabel : public QLabel {
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override { return {16, QLabel::minimumSizeHint().height()}; }
    QSize sizeHint() const override { return {16, QLabel::sizeHint().height()}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(palette().color(foregroundRole()));
        p.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter,
                   fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};
