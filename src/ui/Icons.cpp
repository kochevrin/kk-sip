#include "ui/Icons.h"

#include "ui/Theme.h"

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

// Drawn in the text colour so they follow the light/dark theme.
static QIcon lines(bool indented)
{
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Theme::colors().foreground, 3, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 3; ++i) {
        const int y = 8 + i * 8;
        const int x = indented && i > 0 ? 12 : 5;
        p.drawLine(x, y, 27, y);
    }
    return QIcon(pm);
}

QIcon grouped()
{
    return lines(true);
}

QIcon flatList()
{
    return lines(false);
}

QIcon status(RegState state)
{
    switch (state) {
    case RegState::Online:
        return dot(Theme::colors().ok);
    case RegState::Registering:
        return dot(Theme::colors().warn);
    case RegState::Failed:
        return dot(Theme::colors().danger);
    case RegState::Disabled:
        break;
    }
    return dot(Theme::colors().muted);
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
