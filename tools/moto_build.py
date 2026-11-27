#!/usr/bin/env python3
"""Compile one application to the fixed build path with a version/hash receipt."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
FQBN="esp32:esp32:esp32s3:PartitionScheme=min_spiffs,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled"


def version():
    s=(ROOT/"Config.h").read_text()
    m=re.search(r'^#define MOTO_MATE_VERSION "(\d+\.\d+(?:\.\d+)?)"$',s,re.M)
    if not m:
        raise ValueError("firmware version not found")
    return m.group(1)


def receipt(data,revision):
    if not 24<=len(data)<=1966080 or data[0]!=0xe9 or int.from_bytes(data[12:14],"little")!=9:
        raise ValueError("invalid S3 application binary")
    return {"schema":1,"image":"motomate.ino.bin","version":revision,"bytes":len(data),
            "md5":hashlib.md5(data).hexdigest(),"sha256":hashlib.sha256(data).hexdigest()}


def publish(target,data):
    with tempfile.NamedTemporaryFile(dir=target.parent,prefix=".build-",delete=False) as f:
        tmp=Path(f.name)
        try:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        except BaseException:
            tmp.unlink(missing_ok=True)
            raise
    try:
        os.replace(tmp,target)
    finally:
        tmp.unlink(missing_ok=True)


def main():
    out=ROOT/"build"
    out.mkdir(exist_ok=True)
    manifest=out/"motomate-build.json"
    manifest.unlink(missing_ok=True)
    revision=version()
    with tempfile.TemporaryDirectory(prefix="motomate-compile-") as d:
        subprocess.run(["arduino-cli","compile","--fqbn",FQBN,"--output-dir",d,"."],cwd=ROOT,check=True)
        if version()!=revision:
            raise ValueError("source revision changed during compilation")
        data=(Path(d)/"motomate.ino.bin").read_bytes()
        meta=receipt(data,revision)
        publish(out/"motomate.ino.bin",data)
        publish(manifest,(json.dumps(meta,sort_keys=True)+"\n").encode())
    print("Build ready for OSA:",json.dumps(meta,indent=2))


if __name__=="__main__":
    main()
