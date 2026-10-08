#pragma once

#include <QColor>

class QFormLayout;
#include <QObject>
#include <QString>

// kk visual identity, shared with whispr-open (kochevrin/voice-to-text) and taken from
// the logo: navy ground, one signal-orange accent (#F57C00), state colours kept apart
// from the brand accent. Every text/background pair meets WCAG AA (4.5:1, UI parts 3:1).
namespace Theme {

struct Colors {
    QColor background;
    QColor card;
    QColor foreground;
    QColor muted;       // secondary text
    QColor border;
    QColor input;       // border of fields and check boxes
    QColor secondary;   // button / tab surface
    QColor accent;      // hover surface
    QColor selection;   // selected list row: a light tint of the brand orange
    QColor primary;     // brand orange: fills, underlines, focus
    QColor primaryText; // text on orange
    QColor link;        // orange text on card/background (darker in the light theme)
    QColor ok;          // online lamp, incoming arrow, free BLF
    QColor okFill;      // Call / Answer buttons (white text on it)
    QColor warn;        // registering, ringing BLF
    QColor danger;      // missed calls text, failed lamp, busy BLF
    QColor dangerFill;  // Hang up / Reject buttons (white text on it)
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

// Even spacing in a form, labels centred on their 32px fields (not glued to the top).
void tidyForm(QFormLayout *form);

Mode modeFromString(const QString &s);
QString modeToString(Mode mode);

} // namespace Theme
