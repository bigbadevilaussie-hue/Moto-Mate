#!/usr/bin/env python3
"""Authenticated BENCH spool commissioning. Creates real boot evidence only."""
import argparse
import json
from pathlib import Path
import re
import urllib.parse
import urllib.request

def main():
    p = argparse.ArgumentParser()
    p.add_argument("ip")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--seal", action="store_true")
    g.add_argument("--open", action="store_true")
    g.add_argument("--recover", type=lambda v: int(v, 0))
    p.add_argument("--history", type=Path, default=Path.home() / "Documents" / "MotoMate_log_identity.json")
    a = p.parse_args()
    key = Path(__file__).resolve().parents[1] / "Ota.local.h"
    m = re.search(r'^#define MOTO_OTA_KEY "([a-f0-9]{64})"$', key.read_text(), re.M)
    if not m:
        raise SystemExit("Invalid OTA key")
    def request(path, fields=None):
        data = None if fields is None else urllib.parse.urlencode(fields).encode("ascii")
        req = urllib.request.Request("http://" + a.ip + path, data=data, headers={"X-Moto-Key": m.group(1)})
        with urllib.request.urlopen(req, timeout=15) as r:
            return json.load(r)
    ident = request("/log/identity")
    history = json.loads(a.history.read_text())
    generation = ident.get("generation")
    if ident.get("state") != "valid" or history.get("schema") != 1 or generation not in history.get("devices", {}).get(ident.get("device_mac"), []):
        raise SystemExit("Unverified identity; no write")
    if a.recover is not None:
        if not 0 < a.recover <= (1 << 64) - 1:
            raise SystemExit("Invalid sequence")
        result = request("/log/test-recover", {"generation": generation, "sequence": f"{a.recover:016x}"})
    else:
        result = request("/log/test-write", {"generation": generation, "mode": "open" if a.open else "seal"})
    print(json.dumps(result, indent=2))
    if not result.get("ok"):
        raise SystemExit("Operation failed; preserve evidence")
    if a.recover is None and (result["bytes"] != 128 or result["records"] != 1 or result["state"] != ("open" if a.open else "complete")):
        raise SystemExit("Unexpected write result")
    print("PASS: explicit BENCH spool operation")

if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as e:
        raise SystemExit(f"HTTP {e.code}: {e.read().decode(errors='replace')}; inspect before retry")
    except (OSError, ValueError, KeyError, TypeError) as e:
        raise SystemExit(f"{e}; inspect before retry")
