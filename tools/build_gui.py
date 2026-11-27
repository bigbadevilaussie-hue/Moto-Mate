#!/usr/bin/env python3
"""Compatibility launcher for the supported macOS standalone build."""
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

if __name__=="__main__":
    raise SystemExit(subprocess.call(["/bin/bash",str(ROOT/"build_mac_app.sh")],
                                    cwd=ROOT,env={**os.environ,"PYTHON":sys.executable}))
