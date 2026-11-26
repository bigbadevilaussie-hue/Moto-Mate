#!/usr/bin/env python3
"""Authenticated LAN app-only OTA; confirm actual boot, not just HTTP 200."""
import argparse
import hashlib
import http.client
import json
from pathlib import Path
import re
import secrets
import time
import urllib.request

def main():
    p=argparse.ArgumentParser()
    p.add_argument("ip")
    p.add_argument("image",type=Path)
    p.add_argument("version")
    a=p.parse_args()
    token_file=Path(__file__).resolve().parents[1]/"Ota.local.h"
    m=re.search(r'^#define MOTO_OTA_KEY "([a-f0-9]{64})"$',token_file.read_text(),re.M)
    if not m: raise SystemExit("Invalid local OTA key")
    key=m.group(1)
    data=a.image.read_bytes()
    if len(data)<24 or data[0]!=0xe9 or int.from_bytes(data[12:14],"little")!=9:
        raise SystemExit("Not ESP32-S3 application image")
    def status():
        with urllib.request.urlopen("http://"+a.ip+"/status",timeout=3) as r:
            s=json.load(r)
        if s.get("device")!="Moto-Mate" or s.get("mode")!="ota_bench":
            raise ValueError("unexpected device or mode")
        return s
    before=status()
    if before["ota_busy"] or not 0<len(data)<=before["update_capacity"]:
        raise SystemExit("Device busy or image does not fit inactive slot")
    print("Before:",json.dumps(before),flush=True)
    boundary="Moto"+secrets.token_hex(16)
    body=(("--"+boundary+'\r\nContent-Disposition: form-data; name="firmware"; filename="app.bin"\r\nContent-Type: application/octet-stream\r\n\r\n').encode()+data+("\r\n--"+boundary+"--\r\n").encode())
    c=http.client.HTTPConnection(a.ip,80,timeout=120)
    try:
        c.request("POST","/ota",body,{"Content-Type":"multipart/form-data; boundary="+boundary,
                  "X-Moto-Key":key,"X-Moto-Size":str(len(data)),
                  "X-Moto-MD5":hashlib.md5(data).hexdigest()})
        r=c.getresponse()
        result=r.read()
        if r.status!=200 or not json.loads(result).get("accepted"):
            raise SystemExit("OTA rejected: "+str(r.status)+" "+result.decode(errors="replace"))
        print("Image accepted; checking reboot",flush=True)
    finally:
        c.close()
    end=time.monotonic()+60
    while time.monotonic()<end:
        time.sleep(1)
        try: after=status()
        except (OSError,ValueError): continue
        if after["boot_id"]==before["boot_id"]: continue
        if after["slot"]==before["slot"] or after["version"]!=a.version:
            raise SystemExit("Reboot observed but slot/version verification FAILED: "+json.dumps(after))
        print("PASS: new boot, alternate slot, expected version:",json.dumps(after))
        return
    raise SystemExit("OTA accepted but reboot NOT VERIFIED; check device before retrying")
if __name__=="__main__": main()
