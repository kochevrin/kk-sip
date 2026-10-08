#pragma once

#include <QColor>
#include <QObject>
#include <QString>

// kk visual identity, shared with whispr-open (kochevrin/voice-to-text): deep navy
// ground, one signal-orange accent, state colours kept apart from the brand accent.
namespace Theme {

struct Colors {
    QColor background;
    QColor card;
    QColor foreground;
    QColor muted;       // secondary text
    QColor border;
    QColor input;
    QColor secondary;   // button / tab surface
    QColor accent;      // hover surface
    QColor primary;     // brand orange
    QColor primaryText; // text on orange
    QColor ok;          // call / online
    QColor warn;        // registering
    QColor danger;      // hang up / missed / failed
};

enum class Mode { System, Light, Dark };

void apply(Mode mode);
bool isDark();
const Colors &colors();

// Emits changed() after the palette switches (system scheme change or setting).
class Notifier : public QObject {
    Q_OBJECT
public:
    static Notifier *instance();
signals:
    void changed();
};

Mode modeFromString(const QString &s);
QString modeToString(Mode mode);

} // namespace Theme
