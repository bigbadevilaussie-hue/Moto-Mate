# Moto-Mate GUI commissioning configuration. No donor credentials or device address.
import os
import threading
import time
from pathlib import Path
from collections import deque
from datetime import datetime
from tkinter import messagebox
import tkinter as tk

GUI_VERSION = "1.00"
OSA_URL = os.environ.get("MOTO_OSA_URL", "http://127.0.0.1:9999").rstrip("/")
MOTO_IP = None
HIST_LEN = 60
BLE_STALE_SEC = 60
PULL_TIMEOUT_SEC = 35
ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = str(ROOT / "build")
BUILD_BIN = str(ROOT / "build" / "motomate.ino.bin")
CONFIG_H = str(ROOT / "Config.h")
THEME = {
    "bg": "#eef1f7", "card": "#ffffff", "card_border": "#d8dde8",
    "grid": "#e6e9f0", "fg": "#1e1e2e", "muted": "#7a8194",
    "accent": "#1e66f5", "blue": "#1e66f5", "green": "#2f9e44",
    "yellow": "#d99a00", "orange": "#e8590c", "red": "#d20f39",
    "fill": "#cfe0ff",
}
# V4.49: day-only. No night mode, no auto-switch, no toggle.


# Legacy aliases
BG, CARD, GRID = THEME["bg"], THEME["card"], THEME["grid"]
FG, BLUE, GREEN, YELLOW, ORANGE, MUTED = (
    THEME["fg"], THEME["blue"], THEME["green"],
    THEME["yellow"], THEME["orange"], THEME["muted"])
RED = THEME["red"]
FILL_GREEN = THEME["fill"]
FILL_BLUE  = THEME["fill"]

