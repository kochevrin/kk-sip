#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace Theme {

namespace {

QColor hsl(int h, int s, int l)
{
    return QColor::fromHslF(h / 360.0f, s / 100.0f, l / 100.0f);
}

// Same HSL tokens as whispr-open's globals.css.
Colors lightColors()
{
    Colors c;
    c.background = hsl(220, 30, 97);
    c.card = QColor(Qt::white);
    c.foreground = hsl(220, 30, 12);
    c.muted = hsl(220, 12, 42);
    c.border = hsl(220, 18, 86);
    c.input = hsl(220, 18, 82);
    c.secondary = hsl(220, 20, 92);
    c.accent = hsl(220, 20, 88);
    c.primary = hsl(28, 89, 50);
    c.primaryText = hsl(220, 40, 8);
    c.ok = hsl(152, 60, 40);
    c.warn = hsl(38, 92, 50);
    c.danger = hsl(0, 72, 50);
    return c;
}

Colors darkColors()
{
    Colors c;
    c.background = hsl(220, 30, 7);
    c.card = hsl(220, 26, 10);
    c.foreground = hsl(214, 20, 92);
    c.muted = hsl(216, 12, 60);
    c.border = hsl(220, 20, 18);
    c.input = hsl(220, 20, 22);
    c.secondary = hsl(220, 22, 14);
    c.accent = hsl(220, 22, 19);
    c.primary = hsl(28, 92, 54);
    c.primaryText = hsl(220, 40, 8);
    c.ok = hsl(152, 60, 42);
    c.warn = hsl(38, 92, 58);
    c.danger = hsl(2, 72, 54);
    return c;
}

Colors g_colors = darkColors();
bool g_dark = true;
Mode g_mode = Mode::System;

QString css(const QColor &c)
{
    return c.name(QColor::HexArgb);
}

QString styleSheet(const Colors &c)
{
    QString s = QStringLiteral(R"(
QWidget { color: @fg; }
QToolTip { color: @fg; background: @card; border: 1px solid @border; padding: 4px; }

QPushButton, QToolButton {
    background: @secondary; border: 1px solid @border; border-radius: 8px; padding: 4px 10px;
}
QPushButton:hover, QToolButton:hover { background: @accent; }
QPushButton:pressed, QToolButton:pressed { border-color: @primary; }
QPushButton:checked, QToolButton:checked { background: @accent; border-color: @primary; }
QPushButton:disabled { color: @muted; }
QToolButton { padding: 4px 6px; }
QToolButton::menu-indicator { image: none; width: 0; }
QToolButton[popupMode="2"] { padding-right: 6px; }

QLineEdit, QSpinBox, QComboBox {
    background: @card; border: 1px solid @input; border-radius: 8px; padding: 4px 8px;
    selection-background-color: @primary; selection-color: @primaryText;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QComboBox:on { border-color: @primary; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: @card; border: 1px solid @border; selection-background-color: @accent;
    selection-color: @fg; outline: none;
}

QTabWidget::pane { border: none; }
QTabBar::tab {
    background: transparent; color: @muted; border: none; border-bottom: 2px solid transparent;
    padding: 6px 10px; margin-right: 2px;
}
QTabBar::tab:selected { color: @fg; border-bottom-color: @primary; }
QTabBar::tab:hover:!selected { color: @fg; }

QListWidget, QListView {
    background: @card; border: 1px solid @border; border-radius: 8px; outline: none;
    alternate-background-color: @secondary;
}
QListWidget::item { padding: 6px 6px; border-radius: 6px; }
QListWidget::item:selected { background: @accent; color: @fg; }
QListWidget::item:hover:!selected { background: @secondary; }

QCheckBox::indicator {
    width: 16px; height: 16px; border: 1px solid @input; border-radius: 4px; background: @card;
}
QCheckBox::indicator:checked { background: @primary; border-color: @primary; }

QMenu { background: @card; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 5px 18px 5px 10px; border-radius: 6px; }
QMenu::item:selected { background: @accent; }
QMenu::separator { height: 1px; background: @border; margin: 4px 6px; }

QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; border-radius: 3px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }

/* kk-sip specific */
QFrame#callPanel { background: @card; border: 1px solid @border; border-radius: 10px; }
QPushButton#keypadKey {
    background: @card; border: 1px solid @border; border-radius: 12px;
}
QPushButton#keypadKey:hover { background: @accent; }
QPushButton#keypadKey:pressed { border-color: @primary; background: @accent; }
QPushButton#callButton, QPushButton#answerButton {
    background: @ok; color: white; border: none; border-radius: 10px; font-weight: 600;
}
QPushButton#callButton:hover, QPushButton#answerButton:hover { background: @okHover; }
QPushButton#hangupButton {
    background: @danger; color: white; border: none; border-radius: 10px; font-weight: 600;
}
QPushButton#hangupButton:hover { background: @dangerHover; }
QPushButton#suggestion {
    background: transparent; border: none; color: @primary; text-align: left; padding: 0 4px;
}
QPushButton#suggestion:hover { text-decoration: underline; }
QLabel#statusLabel { color: @muted; }
QLabel#muted { color: @muted; }
)");
    const QList<QPair<QString, QColor>> vars = {
        {QStringLiteral("@primaryText"), c.primaryText},
        {QStringLiteral("@primary"), c.primary},
        {QStringLiteral("@fg"), c.foreground},
        {QStringLiteral("@card"), c.card},
        {QStringLiteral("@border"), c.border},
        {QStringLiteral("@input"), c.input},
        {QStringLiteral("@secondary"), c.secondary},
        {QStringLiteral("@accent"), c.accent},
        {QStringLiteral("@muted"), c.muted},
        {QStringLiteral("@okHover"), c.ok.lighter(112)},
        {QStringLiteral("@ok"), c.ok},
        {QStringLiteral("@dangerHover"), c.danger.lighter(112)},
        {QStringLiteral("@danger"), c.danger},
    };
    for (const auto &v : vars)
        s.replace(v.first, css(v.second));
    return s;
}

QPalette palette(const Colors &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.background);
    p.setColor(QPalette::WindowText, c.foreground);
    p.setColor(QPalette::Base, c.card);
    p.setColor(QPalette::AlternateBase, c.secondary);
    p.setColor(QPalette::Text, c.foreground);
    p.setColor(QPalette::PlaceholderText, c.muted);
    p.setColor(QPalette::Button, c.secondary);
    p.setColor(QPalette::ButtonText, c.foreground);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, c.primary);
    p.setColor(QPalette::HighlightedText, c.primaryText);
    p.setColor(QPalette::ToolTipBase, c.card);
    p.setColor(QPalette::ToolTipText, c.foreground);
    p.setColor(QPalette::Link, c.primary);
    p.setColor(QPalette::Light, c.accent);
    p.setColor(QPalette::Midlight, c.secondary);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Dark, c.input);
    p.setColor(QPalette::Shadow, Qt::black);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, c.muted);
    return p;
}

bool systemIsDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown)
        return scheme == Qt::ColorScheme::Dark;
#endif
    // Fall back to the desktop's own palette before we replaced it.
    const QColor window = QGuiApplication::palette().color(QPalette::Window);
    return window.lightness() < 128;
}

} // namespace

Notifier *Notifier::instance()
{
    static Notifier n;
    return &n;
}

const Colors &colors()
{
    return g_colors;
}

bool isDark()
{
    return g_dark;
}

void apply(Mode mode)
{
    static bool hooked = false;
    if (!hooked) {
        hooked = true;
        // Fusion renders the same on KDE, GNOME and anything else, so the brand looks identical.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, qApp, [] {
            if (g_mode == Mode::System)
                apply(Mode::System);
        });
#endif
    }
    g_mode = mode;
    g_dark = mode == Mode::Dark || (mode == Mode::System && systemIsDark());
    g_colors = g_dark ? darkColors() : lightColors();
    QApplication::setPalette(palette(g_colors));
    qApp->setStyleSheet(styleSheet(g_colors));
    emit Notifier::instance()->changed();
}

Mode modeFromString(const QString &s)
{
    if (s == QLatin1String("light"))
        return Mode::Light;
    if (s == QLatin1String("dark"))
        return Mode::Dark;
    return Mode::System;
}

QString modeToString(Mode mode)
{
    switch (mode) {
    case Mode::Light:
        return QStringLiteral("light");
    case Mode::Dark:
        return QStringLiteral("dark");
    case Mode::System:
        break;
    }
    return QStringLiteral("system");
}

} // namespace Theme
