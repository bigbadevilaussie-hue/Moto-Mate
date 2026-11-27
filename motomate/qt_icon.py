"""Locked desktop artwork. Never changes the ESP32 splash."""
from pathlib import Path
import hashlib
from PySide6.QtGui import QIcon

ASSETS = Path(__file__).resolve().parent / "assets"
PNG_SHA256 = "3ff405971385a4acb56dceb752f9b6f09577a29c45a70020f237ce5508c90d09"
ICNS_SHA256 = "fed71437feddc42008d183a4cd1a45aa4b7b0691c399971d52754472d331244e"
MASTER_SHA256 = "79fb5cff8ce4a9be2d1e5ad33a7f2f2110f6d8ade524404e8785cdf4f62e2a95"


def app_icon():
    path = ASSETS / "motomate-icon.png"
    try:
        if hashlib.sha256(path.read_bytes()).hexdigest() == PNG_SHA256:
            return QIcon(str(path))
    except OSError:
        pass
    # Source launch remains usable until the approved asset pack is installed.
    return QIcon()
