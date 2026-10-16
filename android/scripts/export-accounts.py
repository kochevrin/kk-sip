#!/usr/bin/env python3
"""Exports the desktop kk-sip accounts for the Android app.

Reads the desktop settings (~/.config/kk-sip/kk-sip.ini) and writes, into OUTDIR:
  kk-sip-accounts.ini   the accounts in the policy.ini format (docs/provisioning.md);
                        open it in the app: Settings -> Import accounts from a file
  kk-sip-accounts.png   a QR code with kksip://import?d=...; scan it with the phone's
                        camera and kk-sip opens with the accounts (needs qrencode)
  kk-sip-contacts.csv   the phone book, when there is one: Contacts -> More -> Import

Both hold the SIP passwords: delete them once the phone has the accounts.

  export-accounts.py [OUTDIR] [--ini PATH] [--db PATH] [--enabled-only]
"""
import argparse
import base64
import configparser
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import zlib

FIELDS = [  # desktop key -> policy key
    ("label", "Label"), ("server", "Server"), ("domain", "Domain"), ("proxy", "Proxy"),
    ("user", "User"), ("authUser", "AuthUser"), ("password", "Password"),
    ("displayName", "DisplayName"), ("transport", "Transport"),
]
BOOLS = [("srtp", "SRTP"), ("natRewrite", "NatRewrite"), ("enabled", "Enabled")]


def qt_unquote(v):
    """QSettings puts strings with special characters in quotes and escapes inside."""
    v = v.strip()
    if len(v) < 2 or v[0] != '"' or v[-1] != '"':
        return v
    s, out, i = v[1:-1], [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            if n == "x":
                m = re.match(r"[0-9a-fA-F]+", s[i + 2:])
                if m:
                    out.append(chr(int(m.group(0), 16)))
                    i += 2 + len(m.group(0))
                    continue
            out.append({"n": "\n", "t": "\t", "r": "\r"}.get(n, n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def ini_value(v):
    """Quotes a value only when a plain one would be misread."""
    if v != v.strip() or any(c in v for c in ';"#=\\') or "\n" in v:
        return '"' + v.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'
    return v


def read_desktop(path):
    cp = configparser.RawConfigParser(strict=False, interpolation=None)
    cp.optionxform = str
    cp.read(path, encoding="utf-8")
    if not cp.has_section("accounts"):
        return []
    sec = cp["accounts"]
    size = int(sec.get("size", "0"))
    accounts = []
    for i in range(1, size + 1):
        def get(k, default=""):
            return qt_unquote(sec.get(f"{i}\\{k}", default))
        if not get("server"):
            continue
        accounts.append({k: get(k) for k, _ in FIELDS} | {k: get(k, "false" if k != "enabled" else "true") for k, _ in BOOLS})
    return accounts


def to_ini(accounts):
    lines = ["; kk-sip accounts for the phone. Holds passwords: delete after importing.", ""]
    for n, a in enumerate(accounts, 1):
        lines.append("[Account]" if n == 1 else f"[Account{n}]")
        for k, key in FIELDS:
            if a[k] and not (k == "transport" and a[k] == "udp"):
                lines.append(f"{key}={ini_value(a[k])}")
        for k, key in BOOLS:
            on = a[k].lower() in ("true", "1")
            if on != (k == "enabled"):  # write only what differs from the default
                lines.append(f"{key}={'true' if on else 'false'}")
        lines.append("")
    return "\n".join(lines)


def export_contacts(db_path, out_path):
    """The desktop phone book as the CSV both apps read: name,number."""
    if not os.path.exists(db_path):
        return 0
    con = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    rows = con.execute("SELECT name, number FROM contacts ORDER BY name").fetchall()
    con.close()
    if not rows:
        return 0
    q = lambda s: '"' + (s or "").replace('"', '""') + '"'
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("name,number\n")
        for name, number in rows:
            f.write(f"{q(name)},{q(number)}\n")
    return len(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir", nargs="?", default=".")
    ap.add_argument("--ini", default=os.path.expanduser("~/.config/kk-sip/kk-sip.ini"))
    ap.add_argument("--db", default=os.path.expanduser("~/.local/share/kk-sip/kk-sip.db"))
    ap.add_argument("--enabled-only", action="store_true")
    args = ap.parse_args()

    accounts = read_desktop(args.ini)
    if args.enabled_only:
        accounts = [a for a in accounts if a["enabled"].lower() in ("true", "1")]
    if not accounts:
        sys.exit(f"no accounts in {args.ini}")

    os.makedirs(args.outdir, exist_ok=True)
    text = to_ini(accounts)
    ini_path = os.path.join(args.outdir, "kk-sip-accounts.ini")
    with open(os.open(ini_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w", encoding="utf-8") as f:
        f.write(text)

    z = zlib.compressobj(9, zlib.DEFLATED, -15)
    packed = z.compress(text.encode("utf-8")) + z.flush()
    link = "kksip://import?d=" + base64.urlsafe_b64encode(packed).decode().rstrip("=")
    enabled = sum(a["enabled"].lower() in ("true", "1") for a in accounts)
    print(f"{len(accounts)} accounts ({enabled} on) -> {ini_path}")

    n = export_contacts(args.db, os.path.join(args.outdir, "kk-sip-contacts.csv"))
    if n:
        print(f"{n} contacts -> {os.path.join(args.outdir, 'kk-sip-contacts.csv')}")

    if len(link) > 2900:  # a QR code holds at most ~2950 bytes
        print(f"QR skipped: {len(link)} bytes is too much for one code; use the file or --enabled-only")
    elif shutil.which("qrencode"):
        png = os.path.join(args.outdir, "kk-sip-accounts.png")
        # The link goes in on stdin so the passwords never show in a process list.
        subprocess.run(["qrencode", "-o", png, "-s", "6", "-m", "4", "-l", "L"], input=link.encode(), check=True)
        os.chmod(png, 0o600)
        print(f"QR ({len(link)} bytes) -> {png}")
    else:
        print("QR skipped: qrencode is not installed")


if __name__ == "__main__":
    main()
