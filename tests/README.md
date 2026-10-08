# Smoke test

`smoke_test.cpp` drives the real application code (SIP engine, main window,
dialogs) against a real PBX:

- registration of two accounts at once (UDP and TCP)
- outgoing call to an echo service, mute, hold/unhold, DTMF, hang-up
- busy target (486) recorded as a failed call
- incoming call on the second account: popup, answer, caller name taken from contacts
- missed incoming call and the unread badge on the History tab
- SIP URI helpers and import of MicroSIP's UTF-16 `microsip.ini`

Run it with Docker:

```sh
scripts/smoke-test.sh
```

The script starts Asterisk 22 (`andrius/asterisk`) on `127.0.0.1:5070` with the
config in [`asterisk/`](asterisk/): extensions 101–103 (password `secret`),
600 = echo, 601 = busy. The remote party is the `pjsua` binary that
`scripts/build-pjsip.sh` builds. Audio uses PJSIP's null device, so the test
is silent and works without a sound card.

Set `KKSIP_SCREENSHOTS=<dir>` to save PNG screenshots of each screen and
`KKSIP_LANG=ru` to render them in Russian. `KKSIP_DEBUG=1` writes a full SIP
log to the test data directory.
