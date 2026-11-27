#!/bin/bash
# Adapted from StripSolve's native macOS packaging workflow; GUI only.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$PWD"
NAME=Moto-Mate
DEST="$HOME/Desktop/$NAME.app"
PYTHON="${PYTHON:-$HOME/.venvs/motomate-qt/bin/python}"
[[ "$(uname -s)" == Darwin ]] || { echo "Build on the target Mac."; exit 1; }
[[ ! -e "$DEST" && ! -L "$DEST" ]] || { echo "Move the existing $DEST aside first; nothing overwritten."; exit 1; }
for tool in git iconutil ditto codesign; do
    command -v "$tool" >/dev/null || { echo "Missing $tool"; exit 1; }
done
"$PYTHON" -m PyInstaller --version || { echo "Install: $PYTHON -m pip install pyinstaller"; exit 1; }
"$PYTHON" -c 'import PySide6'
"$PYTHON" tools/verify_gui_icon.py
BUILD="$(mktemp -d /tmp/motomate-gui-build.XXXXXXXX)"
echo "Build and diagnostic files: $BUILD"
VERSION="$("$PYTHON" -c 'from motomate.qt_model import GUI_VERSION; print(GUI_VERSION)')"
git rev-parse --short=12 HEAD > "$BUILD/BUILD_REVISION"
echo "Building $NAME GUI $VERSION, revision $(cat "$BUILD/BUILD_REVISION")"
# Generate Apple's native ICNS container from the locked Retina artwork.
iconutil -c icns "$ROOT/motomate/assets/Moto-Mate.iconset" -o "$BUILD/Moto-Mate.icns"
[[ -s "$BUILD/Moto-Mate.icns" ]]
QT_QPA_PLATFORM=offscreen "$PYTHON" -B -m unittest discover -s tests -p 'test_gui*.py' -v
QT_QPA_PLATFORM=offscreen "$PYTHON" -B motomate.py --smoke-test "$BUILD/source-smoke.json"
cat "$BUILD/source-smoke.json"

# PyInstaller includes Python/Qt; no firmware, token headers or OSA source.
"$PYTHON" -m PyInstaller --clean --noconfirm --windowed --onedir \
    --name "$NAME" --icon "$BUILD/Moto-Mate.icns" \
    --osx-bundle-identifier local.motomate.gui \
    --paths "$ROOT" \
    --distpath "$BUILD/dist" --workpath "$BUILD/work" --specpath "$BUILD" \
    --add-data "$ROOT/motomate/assets/motomate-icon.png:motomate/assets" \
    --add-data "$BUILD/BUILD_REVISION:." \
    motomate.py
APP="$BUILD/dist/$NAME.app"
[[ -x "$APP/Contents/MacOS/$NAME" && -d "$APP/Contents/Frameworks" ]]
# Verify actual bundled resources independently of development cwd/venv/PATH.
SMOKE="$BUILD/smoke"
mkdir -p "$SMOKE"
(
    cd "$SMOKE"
    export PATH="/usr/bin:/bin"
    unset PYTHONPATH PYTHONHOME VIRTUAL_ENV QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH
    QT_QPA_PLATFORM=offscreen "$APP/Contents/MacOS/$NAME" --smoke-test "$SMOKE/bundle.json"
    [[ -s "$SMOKE/bundle.json" ]]
    cat "$SMOKE/bundle.json"
)
codesign --verify --deep --strict "$APP"
[[ ! -e "$DEST" && ! -L "$DEST" ]] || { echo "Destination now exists: $DEST"; exit 1; }
ditto "$APP" "$DEST"
codesign --verify --deep --strict "$DEST"
echo "BUILT: $DEST"
echo "Double-click Moto-Mate.app on your Desktop. OSA must be running separately for live data."
echo "Build files and smoke logs: $BUILD"
