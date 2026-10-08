#pragma once

#include <QSize>

// Remembering the window position.
//
// X11 and most desktops: QWidget::saveGeometry()/restoreGeometry() is enough.
// Wayland: clients cannot place themselves, the compositor decides. On KDE Plasma
// we add a KWin window rule "remember position" for our app id instead; other
// rules in ~/.config/kwinrulesrc are left byte-for-byte untouched.
namespace WindowPlacement {

// True when position is managed through a KWin rule (KDE Wayland session).
bool usesKWinRule();

// Adds (on) or removes (off) the kk-sip rule. initialSize is used to put a new
// window into the bottom-right corner, next to the tray, on the very first start.
void setKWinRule(bool on, const QSize &initialSize);

} // namespace WindowPlacement
