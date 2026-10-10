#!/usr/bin/env bash
# Builds a self-contained Urbs.app (release) with data and assets in Contents/Resources,
# ad-hoc signs it and zips it into dist/. Usage: tools/package/package_macos.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT=$(pwd)
VERSION=$(sed -n 's/^project(urbs VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

cmake --preset release >/dev/null
cmake --build --preset release

DIST="$ROOT/dist"
APP="$DIST/Urbs.app"
rm -rf "$APP" "$DIST/urbs-$VERSION-macos.zip"
mkdir -p "$DIST"
cp -R build/release/urbs.app "$APP"
RES="$APP/Contents/Resources"
mkdir -p "$RES"
# Game content: definitions, campaign, scenarios, sprite atlas. Generators and sources stay out.
rsync -a --delete data/ "$RES/data/"
[ -d mods ] && rsync -a --delete mods/ "$RES/mods/"
mkdir -p "$RES/assets"
rsync -a --delete assets/atlas/ "$RES/assets/atlas/"
[ -d assets/sprites ] && rsync -a --delete assets/sprites/ "$RES/assets/sprites/"
for f in LICENSE.md LICENSE-ASSETS.txt NOTICE README.md RELEASE_NOTES.md; do
  [ -f "$f" ] && cp "$f" "$RES/"
done
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $VERSION" "$APP/Contents/Info.plist" 2>/dev/null ||
  /usr/libexec/PlistBuddy -c "Add :CFBundleShortVersionString string $VERSION" "$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $VERSION" "$APP/Contents/Info.plist" 2>/dev/null ||
  /usr/libexec/PlistBuddy -c "Add :CFBundleVersion string $VERSION" "$APP/Contents/Info.plist"
codesign --force --deep --sign - "$APP"
(cd "$DIST" && ditto -c -k --keepParent Urbs.app "urbs-$VERSION-macos.zip")
echo "Packaged $DIST/urbs-$VERSION-macos.zip"
