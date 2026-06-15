#!/usr/bin/env bash
set -e

APK_EXTRACT_DIR=/tmp/hachimi-apk-decode
TMP_BASE_APK=/tmp/hachimi-base-repacked.apk
APK_ARM64_LIB_DIR="$APK_EXTRACT_DIR/lib/arm64-v8a"
APK_ARM_LIB_DIR="$APK_EXTRACT_DIR/lib/armeabi-v7a"

if [ -z "$PACKAGE_NAME" ]; then
    PACKAGE_NAME="jp.co.cygames.umamusume"
fi
if [ -z "$ACTIVITY_NAME" ]; then
    ACTIVITY_NAME="jp.co.cygames.umamusume_activity.UmamusumeActivity"
fi

clean() {
    echo "-- Cleaning up"
    rm -rf "$APK_EXTRACT_DIR"
    rm -f "$TMP_BASE_APK"
}

if [ "$1" = "clean" ]; then
    clean
    exit
fi

if [ "$RELEASE" = "1" ]; then
    BUILD_TYPE="release"
else
    BUILD_TYPE="debug"
fi

if [ ! -f "$1" ]; then
    echo "Keystore doesn't exist, byebye!"
    exit 1
fi

if [ ! -f "$2" ]; then
    echo "Base APK doesn't exist, byebye!"
    exit 1
fi

if [ -z "$APKSIGNER" ]; then
    APKSIGNER="apksigner"
fi

if ! command -v apktool &> /dev/null; then
    echo "apktool could not be found. Please install it."
    exit 1
fi

echo "-- Building Toolkit"
./tools/android/build.sh

clean

echo "-- Decoding APK with apktool (skipping dex to save time)"
# -s skips decoding classes.dex, keeping them as-is
apktool d -s -f "$2" -o "$APK_EXTRACT_DIR"

echo "-- Injecting External Storage Permissions into AndroidManifest.xml"
python3 -c "
import sys
manifest_path = '$APK_EXTRACT_DIR/AndroidManifest.xml'
with open(manifest_path, 'r', encoding='utf-8') as f:
    content = f.read()

permissions = '''
    <uses-permission android:name=\"android.permission.READ_EXTERNAL_STORAGE\"/>
    <uses-permission android:name=\"android.permission.WRITE_EXTERNAL_STORAGE\"/>
    <uses-permission android:name=\"android.permission.MANAGE_EXTERNAL_STORAGE\"/>
'''

if 'android.permission.READ_EXTERNAL_STORAGE' not in content:
    content = content.replace('<application', permissions + '<application')
    with open(manifest_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print('Permissions injected successfully.')
else:
    print('Permissions already exist.')
"

if [ -d "$APK_ARM64_LIB_DIR" ]; then
    if [ ! -f "$APK_ARM64_LIB_DIR/libmain_orig.so" ]; then
        echo "-- [arm64] Copying libmain_orig.so"
        cp "$APK_ARM64_LIB_DIR/libmain.so" "$APK_ARM64_LIB_DIR/libmain_orig.so"
    fi

    echo "-- [arm64] Copying Toolkit Executable"
    cp "./build/aarch64-linux-android/$BUILD_TYPE/libhachimi.so" "$APK_ARM64_LIB_DIR/libmain.so"
fi

if [ -d "$APK_ARM_LIB_DIR" ]; then
    if [ ! -f "$APK_ARM_LIB_DIR/libmain_orig.so" ]; then
        echo "-- [armv7] Copying libmain_orig.so"
        cp "$APK_ARM_LIB_DIR/libmain.so" "$APK_ARM_LIB_DIR/libmain_orig.so"
    fi

    echo "-- [armv7] Copying Toolkit Executable"
    cp "./build/armv7-linux-androideabi/$BUILD_TYPE/libhachimi.so" "$APK_ARM_LIB_DIR/libmain.so"
fi

echo "-- Repacking APK with apktool"
apktool b "$APK_EXTRACT_DIR" -o "$TMP_BASE_APK"

echo "-- Signing APK"
echo "(Password is securep@ssw0rd816-n if you're using UmaPatcher's keystore)"
\"$APKSIGNER\" sign --ks \"$1\" \"$TMP_BASE_APK\"

echo "-- Installing"
adb shell am force-stop "$PACKAGE_NAME"
adb install -r "$TMP_BASE_APK"

clean

echo "-- Requesting MANAGE_EXTERNAL_STORAGE permission on device"
# Automatically grant MANAGE_EXTERNAL_STORAGE using appops (works on Android 11+)
adb shell appops set "$PACKAGE_NAME" MANAGE_EXTERNAL_STORAGE allow || true

echo "-- Launching"
adb shell am start-activity "$PACKAGE_NAME/$ACTIVITY_NAME"

echo "-- Logcat"
adb logcat |& grep --line-buffered Hachimi
