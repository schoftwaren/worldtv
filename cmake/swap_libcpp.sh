#!/bin/bash
# Post-build APK fixup (viddeck-proven): androiddeployqt ships Qt's
# libc++_shared, but the bundled libmpv needs the mpv-android prebuilt
# copy (NDK 27 libc++ dropped __ndk1 symbols). Swap + zipalign + sign
# with the debug keystore (fine for sideload/GitHub; never Play-updatable).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
APK="$REPO/build-android-arm64/android-build/worldtv.apk"
MPVLIB="${MPV_LIBRARY:-$HOME/Android/libmpv-android/arm64-v8a/libmpv.so}"
BT="${ANDROID_BUILD_TOOLS:-$HOME/Android/android-sdk/build-tools/35.0.0}"
DIR="$(dirname "$MPVLIB")"
TMP=$(mktemp -d)
cp "$APK" "$TMP/base.apk"
cd "$TMP"
zip -q -d base.apk 'lib/arm64-v8a/libc++_shared.so'
mkdir -p lib/arm64-v8a
cp "$DIR/libc++_shared.so" lib/arm64-v8a/
zip -q -0 base.apk lib/arm64-v8a/libc++_shared.so
"$BT/zipalign" -f 4 base.apk aligned.apk
"$BT/apksigner" sign --ks "$HOME/.android/debug.keystore" --ks-pass pass:android --key-pass pass:android --out "$(dirname "$APK")/worldtv-signed.apk" aligned.apk
cd /
rm -rf "$TMP"
echo "signed: $(dirname "$APK")/worldtv-signed.apk"
