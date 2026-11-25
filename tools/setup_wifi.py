#!/usr/bin/env python3
"""Create local Wi-Fi settings without echoing or committing credentials."""
import getpass
import os
from pathlib import Path
import subprocess
import sys
import warnings


def c_string(value):
    # Encode UTF-8 bytes, with fixed three-digit octal escapes for nonprintables.
    return '"' + ''.join(
        chr(b) if 32 <= b <= 126 and b not in (34, 92, 63)
        else '\\%03o' % b
        for b in value.encode("utf-8")
    ) + '"'


def main():
    root = Path(__file__).resolve().parents[1]
    target = root / "Config.local.h"
    if subprocess.run(
        ["git", "check-ignore", "-q", "--", target.name], cwd=root
    ).returncode != 0:
        sys.exit("STOP: Config.local.h is not safely ignored by Git.")
    if target.exists() or target.is_symlink():
        sys.exit("STOP: Config.local.h already exists; left unchanged.")
    if not sys.stdin.isatty():
        sys.exit("STOP: run interactively in your local terminal.")
    # Do not permit getpass to fall back to echoed input.
    warnings.simplefilter("error", getpass.GetPassWarning)
    try:
        ssid = getpass.getpass("Home Wi-Fi SSID (hidden): ")
        password = getpass.getpass("Home Wi-Fi password (hidden): ")
        confirm = getpass.getpass("Confirm password (hidden): ")
    except (EOFError, KeyboardInterrupt, getpass.GetPassWarning):
        sys.exit("\nCancelled; no credentials saved.")
    if password != confirm:
        sys.exit("STOP: passwords do not match; nothing saved.")
    if not 1 <= len(ssid.encode("utf-8")) <= 32 or "\0" in ssid:
        sys.exit("STOP: invalid SSID length or NUL character.")
    if not password or "\0" in password:
        sys.exit("STOP: password is empty or contains a NUL character.")
    content = (
        "#pragma once\n\n#define WIFI_SSID " + c_string(ssid)
        + "\n#define WIFI_PASSWORD " + c_string(password) + "\n"
    )
    try:
        fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        sys.exit("STOP: Config.local.h already exists; left unchanged.")
    try:
        with os.fdopen(fd, "w", encoding="ascii") as stream:
            stream.write(content)
    except BaseException:
        target.unlink()
        raise
    print("PASS: Config.local.h saved locally (mode 0600); Git ignored.")
    print("No credentials displayed. Wi-Fi firmware integration is pending.")


if __name__ == "__main__":
    main()
