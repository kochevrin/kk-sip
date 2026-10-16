# Central configuration

An administrator can set up kk-sip for everyone at once: the PBX account, codecs,
start at logon, update checks. Users then only type their password once (or nothing
at all, if the administrator provides it).

The rule is the same everywhere:

- **A value the administrator sets wins.** kk-sip uses it instead of the user's own
  and greys out its control in Settings, with the tooltip "Set by your administrator".
- **A value the administrator leaves out stays with the user.** An empty string counts
  as left out.
- When the administrator removes a value, the user's own setting comes back.

## Where kk-sip reads it

| Platform | Source | Managed with |
|---|---|---|
| Windows | `HKLM\SOFTWARE\Policies\kk-sip` (computer) and `HKCU\SOFTWARE\Policies\kk-sip` (user); a computer value wins over the user value of the same name | Group Policy with the [ADMX templates](#group-policy), Group Policy Preferences, Intune (ADMX ingestion), Ansible `win_regedit`, `reg add` |
| Linux | `/etc/kk-sip/policy.ini` | Ansible, Puppet, Salt, a package of your own |
| macOS | `/Library/Application Support/kk-sip/policy.ini` | Jamf/Munki scripts, Ansible |
| Android (planned) | Managed configurations (app restrictions) from the MDM | Intune, Google Admin, any EMM |

kk-sip reads the policy at start, so changes apply after kk-sip restarts (on Windows,
after the next logon or `gpupdate` and a restart of kk-sip).

On Linux and macOS the file is an INI file. Keys outside any section are general
settings, each `[Account…]` section is an account, and comments start with `;` (not `#`).
See [`packaging/policy.ini.example`](../packaging/policy.ini.example). In the registry the
general settings are values of the `kk-sip` key and each account is a subkey.

## Schema

Names are case-insensitive. Types are deliberately flat: string, integer, boolean.
Booleans accept `1`/`0` and `true`/`false` (a REG_DWORD 1/0 in the registry).

### General

| Key | Type | Meaning |
|---|---|---|
| `Autostart` | bool | Start kk-sip at logon, straight into the tray |
| `CloseToTray` | bool | Closing the window keeps kk-sip running in the tray |
| `CheckUpdates` | bool | Ask GitHub once a day whether a newer release exists. Turn it off when updates come from the administrator |
| `Language` | string | `system`, `en`, `uk` or `ru` |
| `Codecs` | string | Codecs to offer, in order of preference, separated by commas: `opus, G722, PCMA, PCMU`. A name covers every variant (`speex` = speex/8000, /16000 and /32000; `speex/16000` only that one). Codecs not listed are switched off |
| `SipPort` | int | Local SIP port, `0` = random. A fixed port breaks a second user on the same machine (terminal servers) |

### Accounts

Every group whose name starts with `Account` is an account: `Account`, `Account2`,
`AccountQueue`... They appear in that order above the user's own accounts. An account
needs at least `Server`.

| Key | Type | Meaning |
|---|---|---|
| `Server` | string | Registrar, `host` or `host:port`. **Required** |
| `Transport` | string | `udp`, `tcp` or `tls` (default `udp`) |
| `User` | string | Extension / SIP user name |
| `Password` | string | SIP password. See [Passwords](#passwords) before you set it here |
| `AuthUser` | string | Login for authentication when it differs from `User` |
| `DisplayName` | string | Caller name sent with calls |
| `Label` | string | Name in the account switcher; default `User@Domain` |
| `Domain` | string | SIP domain for addresses when it differs from the server |
| `Proxy` | string | Outbound proxy |
| `SRTP` | bool | Offer encrypted media |
| `NatRewrite` | bool | Rewrite Contact/Via/SDP from what the server sees (off for a LAN PBX) |
| `RegExpiry` | int | Registration interval in seconds, 60–3600 (default 300) |

Fields the administrator leaves out (typically `User`, `Password`, `DisplayName`) are
filled in by the user and kept in the user's own settings. Each user can still turn a
managed account off and on and add accounts of their own, but cannot remove a managed
one or change its locked fields.

On the desktop, string values may contain environment variables written as `%NAME%`:
`User=%USERNAME%` on Windows, `User=%USER%` on Linux and macOS. A variable that is not
set stays as it is.

On Windows, `%AD:attribute%` takes an attribute of the signed-in user's own Active
Directory account: `User=%AD:ipPhone%`, `DisplayName=%AD:displayName%`. kk-sip asks a
domain controller of `%USERDNSDOMAIN%` over LDAP as that user at start (3 seconds at most
to reach one) and keeps the answer in the user's settings for starts away from the
office. When the account has no such attribute, the whole field counts as left out, so
the user fills it in.

### Android (planned)

The Android app reads the same keys as managed configurations, with these differences:

- Restriction keys are case-sensitive there: spell them exactly as in the tables.
- The accounts are a `bundle_array` named `Accounts`. A bundle has no group name, so it
  may carry an optional string `Group` (`Account`, `Account2`, `AccountQueue`); without it
  the first bundle is `Account`, the second `Account2` and so on. The group gives the
  account the same internal id (`policy-` + group in lower case) on every platform.
- `Autostart`, `CloseToTray`, `SipPort` and `RegExpiry` are ignored: there is no tray,
  the port is always random, and a phone renews its registration on its own schedule
  (every 10 minutes, waking up for it) so it keeps receiving calls while it sleeps.
- No `%NAME%` expansion. Use the MDM's own per-user placeholders instead, for example
  Intune's `{{username}}`.
- Phones without an MDM import a file in the INI format above, from a file or a QR code
  (this part is done). Imported values become the user's own settings, not locked ones,
  and an account may carry `Enabled=false` to arrive switched off.
  `android/scripts/export-accounts.py` writes both from a desktop's own settings.

## Passwords

From best to worst:

1. **The user types it once.** Leave `Password` out. On first start kk-sip shows the
   account with the locked fields filled in and asks for the password (and the
   extension, if `User` is left out too). It is stored in the user's own settings file.
2. **In the per-machine file on Linux or macOS** (single-user machines only):
   `/etc/kk-sip/policy.ini` must be readable by the user, so every local user can read it.
3. **In Group Policy**: avoid. Everything in a GPO is stored in SYSVOL, readable by every
   domain user, and the `Policies` registry keys are readable by everyone on the PC. The
   ADMX template therefore has no password field. If a shared phone really needs it, set
   the `Password` value with a Group Policy Preferences registry item.

## One extension per user

Each user needs their own `User`. Ways to get it there, simplest first:

- **Let users type it.** Leave `User` out: the first-start dialog asks for the extension
  and the password together. Nothing to maintain.
- **From Active Directory.** Fill in each user's extension in Active Directory Users and
  Computers → user → Telephones → *IP phone* (attribute `ipPhone`; any other attribute
  works too, such as `telephoneNumber`), and set `User=%AD:ipPhone%` once for everyone.
  Users without a number there are asked for it.
- **Extension = login.** When AD logins match extensions (or FreePBX extensions were made
  from the logins), set `User=%USERNAME%`.
- **Per user in Group Policy.** Set the shared fields in the computer part of a GPO, and
  `User` (and `DisplayName`) under User Configuration → Preferences → Windows Settings →
  Registry, key `HKCU\SOFTWARE\Policies\kk-sip\Account`, one item per user with
  item-level targeting "User = DOMAIN\name". Computer and user values merge per field.
- **One GPO per department OU** when a whole OU shares one line (reception, a queue).

## Group Policy

Release assets include `kk-sip-<version>-policy-templates.zip` with `kk-sip.admx` and
its English, Ukrainian and Russian `.adml` files (they also live in
[`packaging/windows/admx`](../packaging/windows/admx)).

1. **Templates.** Copy `kk-sip.admx` into the central store
   `\\<domain>\SYSVOL\<domain>\Policies\PolicyDefinitions\` and each language folder
   (`en-US`, `uk-UA`, `ru-RU`) next to it. Without a central store, use
   `C:\Windows\PolicyDefinitions` on the machine you edit policies on.
2. **Installation.** Put the MSI on a share readable by "Domain Computers". In a GPO
   linked to the computers' OU: Computer Configuration → Policies → Software Settings →
   Software installation → New → Package → `\\server\share\kk-sip-<version>-windows-x64.msi`
   → Assigned. It installs at the next boot. For a newer version, add its MSI to the same
   GPO; the MSI upgrades the old one in place. (SCCM, Intune and PDQ work with the same
   MSI: `msiexec /i kk-sip-<version>-windows-x64.msi /qn`.)
3. **Settings.** In the same or another GPO: Computer Configuration (or User
   Configuration) → Policies → Administrative Templates → kk-sip:
   - *SIP account*: server, transport, and whatever else is common.
   - *Start kk-sip at user logon*: Enabled. This also registers kk-sip in Windows' own
     "run at logon" policy, so it starts at the very first logon, before anyone opens it.
   - *Check GitHub for new versions*: Disabled, since updates come through the GPO.
   - *Audio codecs*, *Interface language*, *Local SIP port* as needed.
4. **Check** on a client: `gpupdate /force`, sign out and in, then
   `reg query HKLM\SOFTWARE\Policies\kk-sip /s`. In kk-sip, ☰ → Settings shows the
   managed values greyed out, and the account list marks managed accounts.

Without the templates, any tool that writes the registry works the same way:

```bat
reg add HKLM\SOFTWARE\Policies\kk-sip /v CheckUpdates /t REG_DWORD /d 0 /f
reg add HKLM\SOFTWARE\Policies\kk-sip\Account /v Server /d pbx.office.lan /f
reg add HKLM\SOFTWARE\Policies\kk-sip\Account /v User /d %%USERNAME%% /f
```

## Ansible

Windows:

```yaml
- name: kk-sip
  hosts: windows
  tasks:
    - win_package:
        path: \\fileserver\soft\kk-sip-0.3.0-windows-x64.msi
        state: present
    - win_regedit:
        path: HKLM:\SOFTWARE\Policies\kk-sip\Account
        name: "{{ item.name }}"
        data: "{{ item.data }}"
      loop:
        - { name: Server, data: pbx.office.lan }
        - { name: Transport, data: udp }
    - win_regedit:
        path: HKLM:\SOFTWARE\Policies\kk-sip
        name: CheckUpdates
        data: 0
        type: dword
```

Linux (Debian/Ubuntu; on Arch use the `.pkg.tar.zst` with `community.general.pacman`):

```yaml
- name: kk-sip
  hosts: linux_desktops
  become: true
  tasks:
    - apt:
        deb: https://github.com/kochevrin/kk-sip/releases/download/v0.3.0/kk-sip_0.3.0_amd64.deb
    - file: { path: /etc/kk-sip, state: directory, mode: "0755" }
    - copy:
        dest: /etc/kk-sip/policy.ini
        mode: "0644"
        content: |
          Autostart=true
          CheckUpdates=false

          [Account]
          Server=pbx.office.lan
          User=%USER%
    # First logon: start kk-sip even before the user has opened it. kk-sip's own entry
    # in ~/.config/autostart has the same name and takes over from the second logon.
    # Deploy it only together with Autostart=true.
    - copy:
        dest: /etc/xdg/autostart/kk-sip.desktop
        mode: "0644"
        content: |
          [Desktop Entry]
          Type=Application
          Name=kk-sip
          Exec=kk-sip --minimized
          Icon=kk-sip
          X-KDE-autostart-after=panel
```
