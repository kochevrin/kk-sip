#pragma once

#include "sip/SipEngine.h"

#include <QColor>
#include <QIcon>

class QAbstractButton;

namespace Icons {

QIcon app();
// White handsets for the green/red call buttons.
QIcon callWhite();
QIcon hangupWhite();
QIcon status(RegState state);
QIcon dot(const QColor &color);
// History view modes: calls grouped under each number / one flat list.
QIcon grouped();
QIcon flatList();
// Theme icon (Breeze etc.) with a bundled fallback from :/icons.
QIcon get(const QString &themeName, const QString &fallback = {});
// One-colour bundled icon (:/icons/<name>.svg) painted in the given colour, and in
// `hover` while the mouse is over an auto-raise tool button.
QIcon tinted(const QString &name, const QColor &color, const QColor &hover = {});
// Bundled icon in the text colour, repainted when the theme switches. The same look
// on every desktop and on Windows, where there is no icon theme.
void setThemed(QAbstractButton *button, const QString &name);

} // namespace Icons
