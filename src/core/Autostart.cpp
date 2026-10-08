#include "core/Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace Autostart {

static QString entryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/kk-sip.desktop");
}

bool isEnabled()
{
    return QFile::exists(entryPath());
}

QString command()
{
    // Inside an AppImage the binary lives in a temporary mount; the image itself is stable.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty())
        return appImage;
    const QString self = QCoreApplication::applicationFilePath();
    // Installed in PATH: keep the entry short so it survives package updates.
    if (QStandardPaths::findExecutable(QStringLiteral("kk-sip")) == self)
        return QStringLiteral("kk-sip");
    return self;
}

static QString quoted(const QString &path)
{
    if (!path.contains(QLatin1Char(' ')))
        return path;
    QString escaped = path;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

void setEnabled(bool on, bool minimized)
{
    const QString path = entryPath();
    if (!on) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QString exec = quoted(command());
    if (minimized)
        exec += QStringLiteral(" --minimized");
    f.write(QStringLiteral("[Desktop Entry]\n"
                           "Type=Application\n"
                           "Name=kk-sip\n"
                           "Comment=SIP softphone\n"
                           "Icon=kk-sip\n"
                           "Exec=%1\n"
                           "Terminal=false\n"
                           "X-GNOME-Autostart-enabled=true\n"
                           "X-KDE-autostart-after=panel\n")
                .arg(exec)
                .toUtf8());
    f.commit();
}

void refresh(bool minimized)
{
    if (isEnabled())
        setEnabled(true, minimized);
}

} // namespace Autostart
