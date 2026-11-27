#!/usr/bin/env python3
"""Check the approved icon pack, without importing Qt."""
import hashlib
import json
from pathlib import Path

ASSETS=Path(__file__).resolve().parents[1]/"motomate"/"assets"
LOCKED={
    "motomate-icon-master.png":"79fb5cff8ce4a9be2d1e5ad33a7f2f2110f6d8ade524404e8785cdf4f62e2a95",
    "motomate-icon.png":"3ff405971385a4acb56dceb752f9b6f09577a29c45a70020f237ce5508c90d09",
    "Moto-Mate.icns":"fed71437feddc42008d183a4cd1a45aa4b7b0691c399971d52754472d331244e",
}

def main():
    hashes=json.loads((ASSETS/"icon-lock.json").read_text())
    if any(hashes.get(k)!=v for k,v in LOCKED.items()):
        raise SystemExit("Approved icon lock mismatch")
    names={f"Moto-Mate.iconset/icon_{size}x{size}{scale}.png"
           for size in (16,32,128,256,512) for scale in ("","@2x")}
    if set(hashes)!=set(LOCKED)|names:
        raise SystemExit("Icon pack inventory mismatch")
    for name,expected in hashes.items():
        if hashlib.sha256((ASSETS/name).read_bytes()).hexdigest()!=expected:
            raise SystemExit("Icon checksum failed: "+name)
    print("PASS: locked Moto-Mate icon and all 10 iconset sizes")

if __name__=="__main__":
    main()
