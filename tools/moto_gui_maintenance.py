#!/usr/bin/env python3
"""Explicit GUI maintenance bridge to current BENCH firmware. No automatic activity."""
import argparse
import json
from pathlib import Path
import re
import sys
import urllib.request
import urllib.parse
import urllib.error
import ota_upload

IP = "192.168.8.136"
BASE = "http://" + IP
ROOT = Path(__file__).resolve().parents[1]


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())


def get_json(path, key=None, fields=None):
    headers = {"X-Moto-Key": key} if key else {}
    data = urllib.parse.urlencode(fields).encode("ascii") if fields is not None else None
    req = urllib.request.Request(BASE + path, headers=headers, data=data)
    with OPENER.open(req, timeout=5) as response:
        raw = response.read(65537)
    if len(raw) > 65536:
        raise ValueError("oversized response")
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError("invalid response")
    return value


def verify_status(value):
    if (value.get("device") != "Moto-Mate" or value.get("mode") != "ota_bench"
            or value.get("ota_busy") is not False
            or value.get("settings", {}).get("profile") != "BENCH"):
        raise ValueError("idle BENCH maintenance required")


def check_settings(value):
    if (value.get("nvs_ok") is not True or value.get("profile") != "BENCH"
            or any(type(value.get(k)) is not bool for k in ("bench_sleep","verbose","bench_sleep_log"))):
        raise ValueError("healthy BENCH settings required")


def desired_settings(before, sleep, verbose, old_sleep, old_verbose):
    check_settings(before)
    if before["bench_sleep_log"] or (int(before["bench_sleep"]),int(before["verbose"])) != (old_sleep,old_verbose):
        raise ValueError("settings changed or sleep logging armed; reread before writing")
    return {"profile":"BENCH","bench_sleep":str(sleep),"verbose":str(verbose)}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("action",choices=("status","read","write","ota"))
    p.add_argument("args",nargs="*")
    a = p.parse_args()
    expected = {"status":0,"read":0,"write":4,"ota":2}[a.action]
    if len(a.args) != expected:
        p.error("incorrect arguments")
    status = get_json("/status")
    verify_status(status)
    if a.action == "status":
        print(json.dumps(status,indent=2))
        return
    if a.action == "ota":
        image, version = a.args
        if not re.fullmatch(r"\d+\.\d+(?:\.\d+)?",version):
            raise ValueError("invalid expected version")
        if not 24 <= Path(image).stat().st_size <= status["update_capacity"]:
            raise ValueError("image size exceeds slot capacity")
        # Reuse the qualified CLI's image/header/hash and reboot/slot checks.
        # Its urllib status calls also inherit this no-proxy/no-redirect opener.
        urllib.request.install_opener(OPENER)
        sys.argv = ["ota_upload.py",IP,image,version]
        ota_upload.main()
        return
    match = re.search(r'^#define MOTO_OTA_KEY "([a-f0-9]{64})"$',
                      (ROOT/"Ota.local.h").read_text(),re.M)
    if not match:
        raise ValueError("invalid local OTA key")
    key = match.group(1)
    before = get_json("/settings",key)
    check_settings(before)
    if a.action == "write":
        if any(v not in ("0","1") for v in a.args):
            raise ValueError("invalid settings flags")
        desired = desired_settings(before,*(int(v) for v in a.args))
        result = get_json("/settings",key,desired)
        after = get_json("/settings",key)
        for value in (result,after):
            check_settings(value)
            if (value["bench_sleep_log"] or str(int(value["bench_sleep"])) != desired["bench_sleep"]
                    or str(int(value["verbose"])) != desired["verbose"]):
                raise ValueError("settings readback not verified")
        before = after
    print(json.dumps(before,indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError,ValueError,KeyError,TypeError) as exc:
        raise SystemExit("Maintenance operation not verified: " + str(exc))
