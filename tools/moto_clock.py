#!/usr/bin/env python3
"""Authenticated BENCH clock sync/read. Never adjusts the host clock."""
import argparse
import json
from pathlib import Path
import re
import time
import urllib.parse
import urllib.request

def main():
    p = argparse.ArgumentParser()
    p.add_argument("ip")
    p.add_argument("--sync", action="store_true")
    a = p.parse_args()
    path = Path(__file__).resolve().parents[1] / "Ota.local.h"
    match = re.search(r'^#define MOTO_OTA_KEY "([a-f0-9]{64})"$', path.read_text(), re.M)
    if not match:
        raise SystemExit("Invalid local OTA key")
    def request(sync=False):
        data = urllib.parse.urlencode({"epoch_us": time.time_ns() // 1000}).encode("ascii") if sync else None
        req = urllib.request.Request("http://" + a.ip + "/clock", data=data, headers={"X-Moto-Key": match.group(1)})
        start_wall = time.time_ns()
        start_mono = time.monotonic_ns()
        with urllib.request.urlopen(req, timeout=5) as response:
            result = json.load(response)
        elapsed = time.monotonic_ns() - start_mono
        end_wall = time.time_ns()
        result["host_rtt_us"] = elapsed // 1000
        if abs(end_wall - start_wall - elapsed) > 20_000_000:
            result["host_offset_us"] = None
            result["host_clock_step_detected"] = True
        elif result["utc_us"] is not None:
            result["host_offset_us"] = result["utc_us"] - (start_wall + elapsed // 2) // 1000
            result["network_uncertainty_us"] = (elapsed + 1999) // 2000
        else:
            result["host_offset_us"] = None
        return result
    before = request()
    print("BEFORE:", json.dumps(before))
    if a.sync:
        applied = request(True)
        after = request()
        print("APPLIED:", json.dumps(applied))
        print("AFTER:", json.dumps(after))
        if after["quality"] != "host_synced" or after["utc_us"] is None:
            raise SystemExit("Clock sync readback FAILED")
        print("PASS: host sync/readback; offset and RTT reported separately")
if __name__ == "__main__":
    main()
