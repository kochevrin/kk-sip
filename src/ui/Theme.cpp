#include "ui/Theme.h"

#include <QApplication>
#include <QFormLayout>
#include <QLabel>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace Theme {

namespace {

QColor hsl(int h, int s, int l)
{
    return QColor::fromHslF(h / 360.0f, s / 100.0f, l / 100.0f);
}

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

// The logo's navy (#12243E..#1E3A5F) and orange (#F57C00) are the anchors: the dark
// theme sits on deep navy, the light theme writes in it. Contrast ratios are noted
// against the card colour.
Colors lightColors()
{
    Colors c;
    c.background = hsl(214, 32, 96);
    c.card = QColor(Qt::white);
    c.foreground = hsl(215, 52, 15);  // 15.8:1
    c.muted = hsl(215, 16, 38);       // 6.6:1
    c.border = hsl(214, 24, 85);
    c.input = hsl(214, 18, 68);       // 2.3:1, fields stay visible on white
    c.secondary = hsl(214, 30, 93);
    c.accent = hsl(214, 30, 88);
    c.primary = hsl(27, 100, 45);     // 3.3:1 for underlines and focus rings
    c.primaryText = QColor(0x12, 0x24, 0x3E);
    c.link = hsl(24, 95, 36);         // 5.4:1
    c.ok = hsl(150, 72, 30);          // 4.8:1
    c.okFill = hsl(150, 72, 28);      // white text 5.3:1
    c.warn = hsl(32, 95, 42);         // 3.4:1
    c.danger = hsl(0, 72, 42);        // 6.5:1
    c.dangerFill = hsl(0, 70, 45);    // white text 5.9:1
    c.selection = mix(c.card, c.primary, 0.13);
    return c;
}

Colors darkColors()
{
    Colors c;
    c.background = hsl(215, 42, 8);
    c.card = hsl(215, 38, 11);
    c.foreground = hsl(213, 30, 94);  // 15.1:1
    c.muted = hsl(214, 16, 66);       // 7.1:1
    c.border = hsl(215, 28, 20);
    c.input = hsl(215, 22, 32);       // 2.1:1
    c.secondary = hsl(215, 32, 15);
    c.accent = hsl(215, 30, 20);
    c.primary = QColor(0xF5, 0x7C, 0x00); // the logo's orange, 6.5:1
    c.primaryText = QColor(0x12, 0x24, 0x3E);
    c.link = hsl(30, 100, 60);        // 8.2:1
    c.ok = hsl(148, 58, 50);          // 8.2:1
    c.okFill = hsl(150, 70, 29);      // white text 5.1:1
    c.warn = hsl(38, 92, 58);         // 9.2:1
    c.danger = hsl(2, 90, 70);        // 6.3:1
    c.dangerFill = hsl(0, 68, 47);    // white text 5.5:1
    c.selection = hsl(214, 40, 22);   // navy highlight; an orange tint turns brown here
    return c;
}

Colors g_colors = darkColors();
bool g_dark = true;
Mode g_mode = Mode::System;

QString css(const QColor &c)
{
    return c.name(QColor::HexArgb);
}

// Sizes: controls are 32px tall (20 + 2 * 5 padding + 2 * 1 border), radii 8 for
// controls, 10 for cards and lists, spacing on a 4px grid.
QString styleSheet(const Colors &c, bool dark)
{
    QString s = QStringLiteral(R"(
QWidget { color: @fg; }
QToolTip { color: @fg; background: @card; border: 1px solid @border; border-radius: 6px; padding: 4px 6px; }

QPushButton, QToolButton {
    background: @secondary; border: 1px solid @border; border-radius: 8px; padding: 5px 12px; min-height: 20px;
}
QPushButton:hover, QToolButton:hover { background: @accent; }
QPushButton:pressed, QToolButton:pressed { background: @accent; border-color: @input; }
QPushButton:checked, QToolButton:checked { background: @selection; border-color: @primary; }
QPushButton:disabled, QToolButton:disabled { color: @muted; background: @secondary; }
QPushButton:focus, QToolButton:focus { border-color: @input; }
QToolButton { padding: 5px; min-width: 20px; }
QToolButton::menu-indicator { image: none; width: 0; }
QToolButton[popupMode="2"] { padding-right: 5px; }
QDialog QPushButton { min-width: 64px; }
QDialog QPushButton:default {
    background: @primary; color: @primaryText; border-color: @primary; font-weight: 600;
}
QDialog QPushButton:default:hover { background: @primaryHover; }
QDialog QPushButton:default:disabled { background: @secondary; color: @muted; border-color: @border; }

QLineEdit, QSpinBox, QComboBox {
    background: @card; border: 1px solid @input; border-radius: 8px; padding: 5px 8px; min-height: 20px;
    selection-background-color: @primary; selection-color: @primaryText;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QComboBox:on { border-color: @primary; }
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled { color: @muted; background: @secondary; border-color: @border; }
QComboBox { padding-right: 28px; }
QComboBox::drop-down {
    subcontrol-origin: padding; subcontrol-position: center right; width: 24px; border: none;
}
QComboBox::down-arrow { image: url(@chevronDown); width: 12px; height: 12px; }
QComboBox QAbstractItemView {
    background: @card; border: 1px solid @border; border-radius: 8px; padding: 4px; outline: none;
    selection-background-color: @selection; selection-color: @fg;
}
QSpinBox { padding-right: 24px; }
QSpinBox::up-button, QSpinBox::down-button {
    subcontrol-origin: border; width: 22px; border: none; background: transparent;
}
QSpinBox::up-button { subcontrol-position: top right; margin: 3px 3px 0 0; }
QSpinBox::down-button { subcontrol-position: bottom right; margin: 0 3px 3px 0; }
QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: @accent; border-radius: 4px; }
QSpinBox::up-arrow { image: url(@chevronUp); width: 10px; height: 10px; }
QSpinBox::down-arrow { image: url(@chevronDown); width: 10px; height: 10px; }

QTabWidget::pane { border: none; border-top: 1px solid @border; }
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
    background: transparent; color: @muted; border: none; border-bottom: 2px solid transparent;
    padding: 6px 10px; margin-right: 4px; min-width: 24px;
}
QTabBar::tab:selected { color: @fg; border-bottom-color: @primary; }
QTabBar::tab:hover:!selected { color: @fg; border-bottom-color: @border; }
QTabBar::tab:disabled { color: @muted; }

QListWidget, QListView, QTreeView {
    background: @card; border: 1px solid @border; border-radius: 10px; padding: 4px; outline: none;
}
QListWidget::item, QTreeView::item { padding: 4px 6px; border-radius: 6px; }
QListWidget::item:selected, QTreeView::item:selected { background: @selection; color: @fg; }
QListWidget::item:hover:!selected, QTreeView::item:hover:!selected { background: @secondary; }
QTreeView::branch { background: transparent; }
QListView::indicator, QCheckBox::indicator {
    width: 16px; height: 16px; border: 1px solid @input; border-radius: 4px; background: @card;
}
QListView::indicator:checked, QCheckBox::indicator:checked {
    background: @primary; border-color: @primary; image: url(:/icons/check.svg);
}
QCheckBox::indicator:hover { border-color: @primary; }
QCheckBox::indicator:disabled { background: @secondary; border-color: @border; }
QCheckBox::indicator:checked:disabled { background: @input; border-color: @input; }
QCheckBox { spacing: 8px; }
QCheckBox:disabled { color: @muted; }

QMenu { background: @card; border: 1px solid @border; border-radius: 8px; padding: 4px; }
QMenu::item { padding: 6px 20px 6px 10px; border-radius: 6px; }
QMenu::item:selected { background: @accent; }
QMenu::item:disabled { color: @muted; }
QMenu::icon { padding-left: 6px; }
QMenu::indicator { width: 14px; height: 14px; border: 1px solid @input; border-radius: 4px; margin-left: 6px; }
QMenu::indicator:checked { background: @primary; border-color: @primary; image: url(:/icons/check.svg); }
QMenu::separator { height: 1px; background: @border; margin: 4px 6px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @input; border-radius: 3px; min-height: 24px; margin: 0 2px; }
QScrollBar::handle:vertical:hover { background: @muted; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @input; border-radius: 3px; min-width: 24px; margin: 2px 0; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* kk-sip specific */
QFrame#callPanel { background: @card; border: 1px solid @border; border-radius: 10px; }
QFrame#sidePanel { background: transparent; }
QFrame#columnDivider { background: @border; border: none; }
QPushButton#keypadKey {
    background: @card; border: 1px solid @border; border-radius: 10px; padding: 0; min-height: 0;
}
QPushButton#keypadKey:hover { background: @accent; }
QPushButton#keypadKey:pressed { border-color: @primary; background: @selection; }
QPushButton#callButton, QPushButton#answerButton {
    background: @okFill; color: white; border: none; border-radius: 10px; font-weight: 600;
}
QPushButton#callButton:hover, QPushButton#answerButton:hover { background: @okHover; }
QPushButton#callButton:pressed, QPushButton#answerButton:pressed { background: @okPressed; }
QPushButton#hangupButton {
    background: @dangerFill; color: white; border: none; border-radius: 10px; font-weight: 600;
}
QPushButton#hangupButton:hover { background: @dangerHover; }
QPushButton#hangupButton:pressed { background: @dangerPressed; }
QPushButton#callAction { padding: 5px; min-width: 20px; }
QFrame#callPanel QPushButton#hangupButton { padding: 5px 10px; }
QPushButton#suggestion {
    background: transparent; border: none; color: @link; text-align: left; padding: 0 4px; min-height: 0;
}
QPushButton#suggestion:hover { text-decoration: underline; }
QPushButton#updateLink {
    background: transparent; border: none; color: @link; padding: 0 2px; min-height: 0; font-weight: 600;
}
QPushButton#updateLink:hover { text-decoration: underline; }
QLabel#statusLabel { color: @muted; padding: 0 2px; }
QLabel#callStatus { color: @muted; }
QWidget#titleBar { background: transparent; }
QPushButton#accountSwitcher { text-align: left; padding: 5px 10px; background: @card; border-color: @input; }
QPushButton#accountSwitcher:hover { background: @secondary; }
QPushButton#accountSwitcher:focus { border-color: @input; }
QPushButton#accountSwitcher::menu-indicator {
    image: url(@chevronDown); width: 12px; height: 12px; subcontrol-position: right center; right: 10px;
}
QWidget#accountRow { border-radius: 6px; }
QWidget#accountRow:hover { background: @accent; }

QToolButton#titleButton, QToolButton#titleCloseButton {
    background: transparent; border: none; border-radius: 6px; padding: 0; min-height: 0; min-width: 0;
}
QToolButton#titleButton:hover { background: @accent; }
QToolButton#titleButton:checked { background: @selection; }
QToolButton#titleCloseButton:hover { background: @dangerFill; }
QLabel#muted { color: @muted; }
QLabel#link { color: @link; }
)");
    const QString chevron = dark ? QStringLiteral("dark") : QStringLiteral("light");
    // Longer names first: "@primaryText" must not be eaten by "@primary".
    const QList<QPair<QString, QString>> vars = {
        {QStringLiteral("@chevronDown"), QStringLiteral(":/icons/chevron-down-%1.svg").arg(chevron)},
        {QStringLiteral("@chevronUp"), QStringLiteral(":/icons/chevron-up-%1.svg").arg(chevron)},
        {QStringLiteral("@primaryText"), css(c.primaryText)},
        {QStringLiteral("@primaryHover"), css(c.primary.lighter(108))},
        {QStringLiteral("@primary"), css(c.primary)},
        {QStringLiteral("@selection"), css(c.selection)},
        {QStringLiteral("@link"), css(c.link)},
        {QStringLiteral("@fg"), css(c.foreground)},
        {QStringLiteral("@card"), css(c.card)},
        {QStringLiteral("@border"), css(c.border)},
        {QStringLiteral("@input"), css(c.input)},
        {QStringLiteral("@secondary"), css(c.secondary)},
        {QStringLiteral("@accent"), css(c.accent)},
        {QStringLiteral("@muted"), css(c.muted)},
        {QStringLiteral("@okHover"), css(c.okFill.lighter(112))},
        {QStringLiteral("@okPressed"), css(c.okFill.darker(110))},
        {QStringLiteral("@okFill"), css(c.okFill)},
        {QStringLiteral("@dangerHover"), css(c.dangerFill.lighter(112))},
        {QStringLiteral("@dangerPressed"), css(c.dangerFill.darker(110))},
        {QStringLiteral("@dangerFill"), css(c.dangerFill)},
    };
    for (const auto &v : vars)
        s.replace(v.first, v.second);
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
    p.setColor(QPalette::Link, c.link);
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
    qApp->setStyleSheet(styleSheet(g_colors, g_dark));
    emit Notifier::instance()->changed();
}

void tidyForm(QFormLayout *form)
{
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    for (int i = 0; i < form->rowCount(); ++i) {
        QLayoutItem *labelItem = form->itemAt(i, QFormLayout::LabelRole);
        QLayoutItem *fieldItem = form->itemAt(i, QFormLayout::FieldRole);
        auto *label = labelItem ? qobject_cast<QLabel *>(labelItem->widget()) : nullptr;
        if (label && fieldItem)
            label->setMinimumHeight(fieldItem->sizeHint().height());
    }
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
