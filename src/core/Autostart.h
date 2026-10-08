#pragma once

#include <QString>

// XDG autostart entry (~/.config/autostart/kk-sip.desktop), honoured by KDE, GNOME,
// XFCE and others.
namespace Autostart {

bool isEnabled();
// minimized: start straight into the tray instead of showing the window.
void setEnabled(bool on, bool minimized);
// Rewrites an existing entry so it points at the current binary (AppImage moved,
// package updated); no-op when autostart is off.
void refresh(bool minimized);
// What the entry runs: the AppImage, the installed binary, or the build tree binary.
QString command();

} // namespace Autostart
