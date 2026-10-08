<p align="center">
  <img src="docs/logo.png" alt="kk-sip logo" width="112">
</p>

# kk-sip

[![Build](https://github.com/kochevrin/kk-sip/actions/workflows/build.yml/badge.svg)](https://github.com/kochevrin/kk-sip/actions/workflows/build.yml)
[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2%2B-blue.svg)](LICENSE)

A minimal SIP softphone for Linux in the spirit of [MicroSIP](https://www.microsip.org):
a small window, simple settings, many accounts, call history and a phone book.
Nothing else. Built on [PJSIP](https://www.pjsip.org) (the SIP stack MicroSIP
uses) and Qt 6.

<p align="center">
  <img src="docs/screenshots/overview.png" alt="Dial pad, active call, history and contacts" width="900">
</p>

## Features

- **Compact window**, about 300×500 px, that lives in the system tray
- **Many accounts registered at the same time.** All of them receive calls; a
  drop-down at the top picks the one for outgoing calls, with a status dot for each
- **Call history** with incoming, outgoing, missed and declined calls. Double-click
  to call back. Missed calls show a badge and a tray notification
- **Phone book** with search, autocomplete in the dial field, and CSV import/export
- **Easy migration from MicroSIP**: import `Contacts.xml` and the accounts from
  `microsip.ini`
- **Calls**: answer/reject popup, hold, mute, blind transfer, DTMF from the keypad,
  several calls at once (others go on hold automatically)
- UDP / TCP / TLS, optional SRTP, outbound proxy, separate auth ID and domain
- Codecs: Opus, G.722, G.711 (PCMA/PCMU), GSM, Speex, iLBC; order and on/off
  are set in Settings
- Do-not-disturb mode, custom WAV ringtone
- Starts with the system straight into the tray (optional)
- Light and dark theme in the kk family style, follows the system or set by hand
- Opens `sip:`, `tel:` and `callto:` links (`kk-sip tel:+380...`); a second launch
  hands the number to the running instance
- English and Russian UI

<p align="center">
  <img src="docs/screenshots/incoming.png" alt="Incoming call" width="280">
  &nbsp;
  <img src="docs/screenshots/settings.png" alt="Settings" width="440">
</p>

## Install

Packages are attached to every [release](https://github.com/kochevrin/kk-sip/releases):

| System | File | Install |
|---|---|---|
| Arch, EndeavourOS, Manjaro | `kk-sip-<ver>-1-x86_64.pkg.tar.zst` | `sudo pacman -U kk-sip-*.pkg.tar.zst` |
| Ubuntu 24.04+, Debian 13+ | `kk-sip_<ver>_amd64.deb` | `sudo apt install ./kk-sip_*_amd64.deb` |
| Any other distribution | `kk-sip-<ver>-x86_64.AppImage` | `chmod +x kk-sip-*.AppImage` and run it |

After installing, kk-sip shows up in the application menu. To start it with the
system, use ☰ → Settings → General → *Start with the system*.

Moving from MicroSIP? Copy `microsip.ini` and `Contacts.xml` from the Windows
machine (`%APPDATA%\MicroSIP`, or the MicroSIP folder for the portable version), then run

```sh
kk-sip --import-microsip /path/to/that/folder
```

MicroSIP encrypts passwords with a Windows key, so accounts arrive switched off.
Pick one and press *Call*, and kk-sip asks for its password.

## Build

Arch / EndeavourOS:

```sh
sudo pacman -S --needed base-devel cmake qt6-base qt6-svg qt6-tools opus openssl alsa-lib
git clone https://github.com/kochevrin/kk-sip.git && cd kk-sip
scripts/build-pjsip.sh            # static PJSIP 2.17 into .deps/ (~2 min, once)
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/kk-sip
```

Debian / Ubuntu: `packaging/linux/build-ubuntu.sh --install-deps` installs the
dependencies and produces the `.deb` and the AppImage in `dist/`.

Arch package of the latest tagged release: `cd packaging/arch && makepkg -si`.

If you would rather use a system PJSIP (for example the AUR `pjproject`
package), skip `build-pjsip.sh`. CMake picks up `libpjproject` through
pkg-config.

Install system-wide (binary, `.desktop` file with the `sip:`/`tel:` handlers, icon):

```sh
sudo cmake --install build
```

## Where data lives

| What | Path |
|---|---|
| Autostart entry (when enabled) | `~/.config/autostart/kk-sip.desktop` |
| Settings and accounts | `~/.config/kk-sip/kk-sip.ini` (mode 600; passwords are stored in plain text) |
| History and contacts | `~/.local/share/kk-sip/kk-sip.db` (SQLite) |
| SIP debug log (when enabled) | `~/.local/share/kk-sip/pjsip.log` |

## Audio

kk-sip talks to ALSA. On a PipeWire or PulseAudio desktop, "System default" uses
the sound server's ALSA plugin, so calls follow the default device. You can move
the kk-sip stream to a headset in the KDE/GNOME volume applet, and it will be
remembered.

## Architecture

```
Qt Widgets UI (src/ui)                     SipEngine (src/sip)
 MainWindow ─ account switcher, tray       Qt facade over PJSUA2
  ├─ CallPanel   (active calls)      ◄──►   ├─ KkAccount : pj::Account (one per account)
  ├─ DialerTab / HistoryTab / ContactsTab   ├─ KkCall    : pj::Call
  └─ IncomingDialog, SettingsDialog         └─ ring / ringback tones
              │                                     │ polled from a QTimer on the GUI thread
 core (src/core): Settings (INI), Database          │ (threadCnt = 0, mainThreadOnly)
 (SQLite), SipUri, SingleInstance, MicroSIP import  ▼
                                            PJSIP 2.17 (static) → ALSA → PipeWire
```

All PJSIP callbacks run on the GUI thread, so there is no locking between the SIP
stack and the UI.

## Tests

`scripts/smoke-test.sh` runs an end-to-end test against Asterisk in Docker:
registration, outgoing, busy, incoming, missed calls, hold, mute and DTMF.
See [tests/README.md](tests/README.md).

## License

GPL-2.0-or-later, see [LICENSE](LICENSE). Third-party components are listed in
[THIRD-PARTY.md](THIRD-PARTY.md).

---

**По-русски.** kk-sip — минималистичный SIP-телефон для Linux по образцу MicroSIP:
маленькое окно, несколько аккаунтов одновременно с быстрым переключением,
история с обратным звонком и телефонная книга. Интерфейс на русском включается
автоматически по языку системы. Контакты и аккаунты можно перенести из MicroSIP:
«Контакты → … → Импорт» и «Настройки → Аккаунты → Из MicroSIP…».
