# Third-party components

kk-sip itself is licensed under the [GNU GPL v2 or later](LICENSE). It is built
on the following components.

## Linked into the binary

| Component | Role | License |
|---|---|---|
| [PJSIP](https://www.pjsip.org) 2.17 (static, built by `scripts/build-pjsip.sh`) | SIP, RTP, audio devices, codecs | GPL-2.0-or-later (commercial license available from Teluu) |
| ↳ bundled in PJSIP: libsrtp, resample, GSM, Speex, iLBC, G.722.1 | media encryption and codecs | BSD-style / see `third_party/` in the PJSIP tree |
| [Qt 6](https://www.qt.io) Widgets, Sql, Network (system libraries) | UI, SQLite storage, single-instance socket | LGPL-3.0 |
| [Opus](https://opus-codec.org) (system library) | wideband codec | BSD-3-Clause |
| [OpenSSL](https://www.openssl.org) (system library) | SIP over TLS, SRTP crypto | Apache-2.0 |
| [alsa-lib](https://www.alsa-project.org) (system library, Linux only) | sound I/O (through the PipeWire/Pulse ALSA plugin) | LGPL-2.1 |

The Windows and macOS packages carry Qt, Opus and OpenSSL inside (next to
`kk-sip.exe`, or in `kk-sip.app/Contents/Frameworks`) as unmodified shared libraries.

## Used only by tests

| Component | Role | License |
|---|---|---|
| [Asterisk](https://www.asterisk.org) via the `andrius/asterisk` Docker image | PBX for the smoke test | GPL-2.0 |
| `pjsua` from PJSIP | remote party in the smoke test | GPL-2.0-or-later |

## Inspiration

The feature set and layout follow [MicroSIP](https://www.microsip.org) (GPL-2.0),
a Windows softphone. kk-sip contains no MicroSIP code; it reads MicroSIP's
`Contacts.xml` and `microsip.ini` formats to make migration easy.

## License note

OpenSSL 3 is Apache-2.0, which is compatible with GPL version 3 but not with
version 2. Since kk-sip and PJSIP are "GPL v2 or later", a binary linked
against OpenSSL 3 is distributed under the terms of GPL v3.
