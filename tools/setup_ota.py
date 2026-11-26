#!/usr/bin/env python3
"""Generate an independent local OTA token, never printed or committed."""
import os
from pathlib import Path
import secrets
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
target = root / "Ota.local.h"
if subprocess.run(["git","check-ignore","-q","--",target.name],cwd=root).returncode:
    sys.exit("STOP: Ota.local.h is not ignored")
try:
    fd = os.open(target, os.O_CREAT|os.O_EXCL|os.O_WRONLY, 0o600)
except FileExistsError:
    sys.exit("Ota.local.h already exists; preserved.")
with os.fdopen(fd,"w",encoding="ascii") as f:
    f.write('#pragma once\n#define MOTO_OTA_KEY "'+secrets.token_hex(32)+'"\n')
print("PASS: local OTA key created (0600), not displayed.")
