#include "core/WindowPlacement.h"

#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QStringList>

namespace WindowPlacement {

static const QString RuleId = QStringLiteral("kk-sip-position");

static QString rulesPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kwinrulesrc");
}

bool usesKWinRule()
{
    return QGuiApplication::platformName() == QLatin1String("wayland")
        && qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("KDE"), Qt::CaseInsensitive);
}

// Splits the file into "[section]" blocks, keeping every line as is.
struct Section {
    QString name; // empty for lines before the first header
    QStringList lines;
};

static QList<Section> parse(const QString &text)
{
    QList<Section> out{Section{}};
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        if (i == lines.size() - 1 && line.isEmpty())
            break; // trailing newline
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')))
            out.append(Section{line.mid(1, line.size() - 2), {}});
        else
            out.last().lines.append(line);
    }
    return out;
}

static QString serialize(const QList<Section> &sections)
{
    QString out;
    for (const Section &s : sections) {
        if (!s.name.isEmpty())
            out += QLatin1Char('[') + s.name + QStringLiteral("]\n");
        for (const QString &l : s.lines)
            out += l + QLatin1Char('\n');
    }
    return out;
}

static void reloadKWin()
{
    if (QStandardPaths::isTestModeEnabled())
        return; // tests write to ~/.qttest, don't poke the real KWin
    QProcess::startDetached(QStringLiteral("dbus-send"),
                            {QStringLiteral("--session"), QStringLiteral("--type=method_call"),
                             QStringLiteral("--dest=org.kde.KWin"), QStringLiteral("/KWin"),
                             QStringLiteral("org.kde.KWin.reconfigure")});
}

void setKWinRule(bool on, const QSize &initialSize)
{
    QFile in(rulesPath());
    QString text;
    QFileDevice::Permissions perms = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    if (in.open(QIODevice::ReadOnly)) {
        text = QString::fromUtf8(in.readAll());
        perms = in.permissions();
        in.close();
    }
    QList<Section> sections = parse(text);

    const bool present = std::any_of(sections.cbegin(), sections.cend(),
                                     [](const Section &s) { return s.name == RuleId; });
    if (present == on)
        return;

    // [General] count= / rules= bookkeeping.
    int generalIdx = -1;
    for (int i = 0; i < sections.size(); ++i)
        if (sections.at(i).name == QLatin1String("General"))
            generalIdx = i;
    if (generalIdx < 0) {
        sections.append(Section{QStringLiteral("General"), {}});
        generalIdx = int(sections.size()) - 1;
    }
    QStringList &general = sections[generalIdx].lines;
    QStringList ids;
    int rulesLine = -1;
    int countLine = -1;
    for (int i = 0; i < general.size(); ++i) {
        if (general.at(i).startsWith(QLatin1String("rules="))) {
            rulesLine = i;
            ids = general.at(i).mid(6).split(QLatin1Char(','), Qt::SkipEmptyParts);
        } else if (general.at(i).startsWith(QLatin1String("count="))) {
            countLine = i;
        }
    }
    if (on) {
        ids.append(RuleId);
        // First start: bottom-right of the screen, next to the tray.
        QPoint pos(100, 100);
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            const QRect area = screen->availableGeometry();
            pos = QPoint(area.right() - initialSize.width() - 24, area.bottom() - initialSize.height() - 48);
        }
        sections.append(Section{RuleId,
                                {QStringLiteral("Description=kk-sip -> remember position"),
                                 QStringLiteral("position=%1,%2").arg(pos.x()).arg(pos.y()),
                                 QStringLiteral("positionrule=4"), // Remember
                                 QStringLiteral("wmclass=kk-sip"), QStringLiteral("wmclasscomplete=false"),
                                 QStringLiteral("wmclassmatch=1")}});
    } else {
        ids.removeAll(RuleId);
        sections.erase(std::remove_if(sections.begin(), sections.end(),
                                      [](const Section &s) { return s.name == RuleId; }),
                       sections.end());
        // Indices may have moved; find [General] again.
        for (int i = 0; i < sections.size(); ++i)
            if (sections.at(i).name == QLatin1String("General"))
                generalIdx = i;
    }
    QStringList &gen = sections[generalIdx].lines;
    const QString rulesValue = QStringLiteral("rules=") + ids.join(QLatin1Char(','));
    const QString countValue = QStringLiteral("count=%1").arg(ids.size());
    if (rulesLine >= 0 && rulesLine < gen.size() && gen.at(rulesLine).startsWith(QLatin1String("rules=")))
        gen[rulesLine] = rulesValue;
    else
        gen.append(rulesValue);
    if (countLine >= 0 && countLine < gen.size() && gen.at(countLine).startsWith(QLatin1String("count=")))
        gen[countLine] = countValue;
    else
        gen.prepend(countValue);

    QSaveFile out(rulesPath());
    if (!out.open(QIODevice::WriteOnly))
        return;
    out.write(serialize(sections).toUtf8());
    if (out.commit()) {
        QFile::setPermissions(rulesPath(), perms);
        reloadKWin();
    }
}

} // namespace WindowPlacement
