#!/usr/bin/env python3
"""Read or update persistent Moto-Mate settings over the authenticated LAN API."""
import argparse
import json
from pathlib import Path
import re
import urllib.parse
import urllib.request
import urllib.error

def main():
    p = argparse.ArgumentParser()
    p.add_argument("ip")
    p.add_argument("--profile", choices=("BENCH", "FIELD"))
    p.add_argument("--bench-sleep", choices=("0", "1"))
    p.add_argument("--verbose", choices=("0", "1"))
    a = p.parse_args()
    path = Path(__file__).resolve().parents[1] / "Ota.local.h"
    match = re.search(r'^#define MOTO_OTA_KEY "([a-f0-9]{64})"$', path.read_text(), re.M)
    if not match:
        raise SystemExit("Invalid local OTA key")
    url = "http://" + a.ip + "/settings"
    def request(data=None):
        req = urllib.request.Request(url, data=data, headers={"X-Moto-Key": match.group(1)})
        with urllib.request.urlopen(req, timeout=5) as r:
            return json.load(r)
    try:
        before = request()
        if not before.get("nvs_ok"):
            print(json.dumps(before, indent=2))
            raise SystemExit("NVS unavailable; no settings written")
        if any(x is not None for x in (a.profile, a.bench_sleep, a.verbose)):
            desired = {
                "profile": a.profile if a.profile is not None else before["profile"],
                "bench_sleep": a.bench_sleep if a.bench_sleep is not None else str(int(before["bench_sleep"])),
                "verbose": a.verbose if a.verbose is not None else str(int(before["verbose"])),
            }
            result = request(urllib.parse.urlencode(desired).encode("ascii"))
            after = request()
            for key, val in desired.items():
                actual = after[key] if key == "profile" else str(int(after[key]))
                if actual != val or after[key] != result[key] or not after["nvs_ok"]:
                    raise SystemExit("Settings readback FAILED")
            print("PASS: persisted settings readback")
        else:
            after = before
        print(json.dumps(after, indent=2))
        if after["effective_profile"] == "RECOVERY" or (after["bench_sleep"] and not after["bench_sleep_active"]):
            print("Requested settings saved but runtime capability is not enabled.")
    except urllib.error.HTTPError as e:
        raise SystemExit(f"Settings request failed: HTTP {e.code}: {e.read().decode(errors='replace')}")
if __name__ == "__main__":
    main()
