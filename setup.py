"""Optional macOS GUI bundle; independent of Arduino firmware."""
from setuptools import setup

setup(
    name="Moto-Mate",
    version="1.00",
    app=["motomate.py"],
    options={"py2app": {"plist": {"CFBundleName": "Moto-Mate",
                                  "CFBundleDisplayName": "Moto-Mate",
                                  "CFBundleIdentifier": "local.motomate.gui"},
                        "packages": ["motomate", "customtkinter"]}},
    setup_requires=["py2app"],
)
