"""Optional legacy Tk bundle build is not compatible with the Qt6 launcher.
Use the Qt6 source launcher until packaging is qualified.
"""
if __name__ == "__main__":
    raise SystemExit("Qt6 bundle packaging pending; run python3 motomate.py")
