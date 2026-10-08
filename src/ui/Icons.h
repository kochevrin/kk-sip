#pragma once

#include "sip/SipEngine.h"

#include <QColor>
#include <QIcon>

namespace Icons {

QIcon app();
// White handsets for the green/red call buttons.
QIcon callWhite();
QIcon hangupWhite();
QIcon status(RegState state);
QIcon dot(const QColor &color);
// Theme icon (Breeze etc.) with a bundled fallback from :/icons.
QIcon get(const QString &themeName, const QString &fallback = {});

} // namespace Icons
