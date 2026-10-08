#include "ui/Icons.h"

#include <QPainter>
#include <QPixmap>

namespace Icons {

QIcon app()
{
    return QIcon(QStringLiteral(":/icons/kk-sip.svg"));
}

QIcon callWhite()
{
    return QIcon(QStringLiteral(":/icons/call-white.svg"));
}

QIcon hangupWhite()
{
    return QIcon(QStringLiteral(":/icons/hangup-white.svg"));
}

QIcon dot(const QColor &color)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color.darker(130), 1));
    p.setBrush(color);
    p.drawEllipse(QRectF(3.5, 3.5, 9, 9));
    return QIcon(pm);
}

QIcon status(RegState state)
{
    switch (state) {
    case RegState::Online:
        return dot(QColor(0x2e, 0xb8, 0x4b));
    case RegState::Registering:
        return dot(QColor(0xf0, 0xb4, 0x29));
    case RegState::Failed:
        return dot(QColor(0xd9, 0x3f, 0x3f));
    case RegState::Disabled:
        break;
    }
    return dot(QColor(0x9a, 0x9a, 0x9a));
}

QIcon get(const QString &themeName, const QString &fallback)
{
    if (QIcon::hasThemeIcon(themeName))
        return QIcon::fromTheme(themeName);
    if (!fallback.isEmpty())
        return QIcon(QStringLiteral(":/icons/") + fallback);
    return {};
}

} // namespace Icons
