#!/usr/bin/env python3
"""Explicit initial schema-1 identity commissioning; never replaces an identity.
The history file is the seed of OSA's future generation registry. Keep it.
"""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import urllib.parse
import urllib.request
import urllib.error

def main():
    p = argparse.ArgumentParser()
    p.add_argument("ip")
    g = p.add_mutually_exclusive_group()
    g.add_argument("--initial", action="store_true",
                   help="Assert no prior schema-1 OSA history for this device; create new registry")
    g.add_argument("--test-sequence", action="store_true", help="Consume one real ID; no log file")
    p.add_argument("--history", type=Path, default=Path.home() / "Documents" / "MotoMate_log_identity.json")
    a = p.parse_args()
    key = Path(__file__).resolve().parents[1] / "Ota.local.h"
    match = re.search(r'^#define MOTO_OTA_KEY "([a-f0-9]{64})"$', key.read_text(), re.M)
    if not match:
        raise SystemExit("Invalid local OTA key")
    def request(path, fields=None):
        data = None if fields is None else urllib.parse.urlencode(fields).encode("ascii")
        req = urllib.request.Request("http://" + a.ip + path, data=data,
                                     headers={"X-Moto-Key": match.group(1)})
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.load(r)
    before = request("/log/identity")
    mac = before.get("device_mac")
    if not isinstance(mac, str) or not re.fullmatch(r"[0-9a-f]{12}", mac):
        raise SystemExit("Device MAC unavailable")
    if a.initial:
        if before.get("state") != "unprovisioned" or not before.get("filesystem_mounted"):
            raise SystemExit("Device is not virgin/mounted; refusing provisioning")
        # Exclusive creation is intentional. Never discard an earlier candidate,
        # including when the HTTP response was lost. No automatic reprovisioning.
        generation = f"{secrets.randbits(64):016x}"
        while generation == "0000000000000000":
            generation = f"{secrets.randbits(64):016x}"
        history = {"schema": 1, "devices": {mac: [generation]}}
        with a.history.open("x", encoding="utf-8") as f:
            json.dump(history, f, indent=2)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        fd = os.open(a.history.parent, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
        print(f"Generation reserved in {a.history}; preserve even if request fails.")
        request("/log/identity", {"generation": generation})
        after = request("/log/identity")
        if (after.get("device_mac") != mac or after.get("state") != "valid" or
            after.get("generation") != generation or after.get("reserved_through") != "0000000000000000"):
            raise SystemExit("Provisioning readback FAILED; do not retry with new identity")
        print("PASS: initial identity persisted and read back")
    elif a.test_sequence:
        if before.get("state") != "valid":
            raise SystemExit("No valid identity")
        history = json.loads(a.history.read_text())
        generation = before["generation"]
        if history.get("schema") != 1 or generation not in history.get("devices", {}).get(mac, []):
            raise SystemExit("Generation absent from host registry; refusing allocation")
        result = request("/log/test-sequence", {"generation": generation})
        after = request("/log/identity")
        allocated = int(result["allocated"], 16)
        if (after.get("device_mac") != mac or after.get("generation") != generation or
            after.get("state") != "valid" or not 0 < allocated <= int(after["reserved_through"], 16)
            or int(after["next_reserved"], 16) != allocated + 1):
            raise SystemExit("Allocation readback FAILED; do not assume ID reusable")
        print(f"PASS: consumed test sequence {allocated}")
    else:
        after = before
    print(json.dumps(after, indent=2))

if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, urllib.error.URLError) as e:
        raise SystemExit(f"Commissioning failed: {e}; preserve registry and inspect device before retry")
