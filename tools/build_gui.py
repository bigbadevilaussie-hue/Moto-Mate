#!/usr/bin/env python3
"""Optional macOS app bundle build. Does not compile or upload ESP32 firmware."""
import subprocess
import sys
from pathlib import Path
import platform
from verify_gui_icon import main as verify_icon

ROOT=Path(__file__).resolve().parents[1]

def main():
    if platform.system()!="Darwin":
        raise SystemExit("Build the macOS bundle on macOS")
    verify_icon()
    subprocess.run([sys.executable,"-m","PyInstaller","--noconfirm","--windowed","--onedir",
                    "--name","Moto-Mate","--osx-bundle-identifier","local.motomate.gui",
                    "--icon","motomate/assets/Moto-Mate.icns",
                    "--add-data","motomate/assets/motomate-icon.png:motomate/assets",
                    "--workpath","gui-build","--distpath","dist","motomate.py"],
                   cwd=ROOT,check=True)
    print("Bundle created:",ROOT/"dist"/"Moto-Mate.app")

if __name__=="__main__":
    main()
