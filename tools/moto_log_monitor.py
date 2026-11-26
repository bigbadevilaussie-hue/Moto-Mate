#!/usr/bin/env python3
"""Explicit bounded BENCH monitor session. No automatic wake or deletion."""
import argparse
import ipaddress
import json
from pathlib import Path
import re
import urllib.parse
import urllib.request
import urllib.error

def main():
    p=argparse.ArgumentParser()
    p.add_argument("ip")
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument("--start",type=int,choices=range(1,5))
    g.add_argument("--status",action="store_true")
    g.add_argument("--seal",action="store_true")
    a=p.parse_args()
    keyfile=Path(__file__).resolve().parents[1]/"Ota.local.h"
    m=re.search(r'^#define MOTO_OTA_KEY "([a-f0-9]{64})"$',keyfile.read_text(),re.M)
    if not m: raise SystemExit("Invalid local key")
    ip=str(ipaddress.IPv4Address(a.ip))
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self,*args,**kwargs):
            raise ValueError("redirect refused")
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect())
    def call(path,fields=None):
        data=None if fields is None else urllib.parse.urlencode(fields).encode("ascii")
        req=urllib.request.Request("http://"+ip+path,data=data,headers={"X-Moto-Key":m.group(1)})
        with opener.open(req,timeout=10) as res:
            return json.loads(res.read(2048))
    if a.start is not None:
        ident=call("/log/identity")
        history=json.loads((Path.home()/"Documents"/"MotoMate_log_identity.json").read_text())
        gen=ident.get("generation")
        if ident.get("state")!="valid" or gen not in history.get("devices",{}).get(ident.get("device_mac"),[]):
            raise SystemExit("Unverified identity")
        result=call("/log/monitor/start",{"generation":gen,"count":a.start})
    elif a.seal: result=call("/log/monitor/seal",{})
    else: result=call("/log/monitor")
    print(json.dumps(result,indent=2))
    if result.get("sequence"): print("Sequence decimal:",int(result["sequence"],16))

if __name__=="__main__":
    try: main()
    except (OSError,ValueError,KeyError,TypeError) as e:
        raise SystemExit(f"Monitor request stopped: {e}; inspect before retry")
