#!/usr/bin/env sh
set -eu

if [ "$#" -ne 4 ]; then
    echo "usage: ADB='adb [-s SERIAL]' $0 <app-id> <activity> <apk-dir> <variant>" >&2
    exit 2
fi

APP_ID=$1
ACTIVITY=$2
APK_DIR=$3
VARIANT=$4
ADB_CMD=${ADB:-adb}

adb_run() {
    # ADB_CMD intentionally supports values such as: adb -s SERIAL
    # shellcheck disable=SC2086
    $ADB_CMD "$@"
}

fail() {
    echo "Android install failed: $*" >&2
    exit 1
}

command -v "$(printf '%s\n' "$ADB_CMD" | awk '{print $1}')" >/dev/null 2>&1 ||
    fail "adb was not found. Set ADB=/path/to/adb or install Android platform-tools."

STATE_OUTPUT=$(adb_run get-state 2>&1) || {
    echo "$STATE_OUTPUT" >&2
    echo >&2
    echo "Visible adb devices:" >&2
    adb_run devices -l >&2 || true
    fail "no single usable device selected. Connect one device, or run with ADB='adb -s SERIAL'."
}

[ "$STATE_OUTPUT" = "device" ] ||
    fail "selected adb target is '$STATE_OUTPUT', not 'device'. Check USB authorization and device state."

ABI=$(adb_run shell getprop ro.product.cpu.abi 2>&1 | tr -d '\r' | head -n 1) || {
    echo "$ABI" >&2
    fail "could not read device ABI."
}
[ -n "$ABI" ] || fail "device ABI is empty."

APK=""
for candidate in \
    "$APK_DIR"/"$VARIANT"/app-*-"$ABI"-"$VARIANT".apk \
    "$APK_DIR"/"$VARIANT"/app-"$ABI"-"$VARIANT".apk \
    "$APK_DIR"/"$VARIANT"/app-*-"$VARIANT".apk \
    "$APK_DIR"/"$VARIANT"/app-"$VARIANT".apk \
    "$APK_DIR"/*/"$VARIANT"/app-*-"$ABI"-"$VARIANT".apk \
    "$APK_DIR"/app-*-"$ABI"-"$VARIANT".apk \
    "$APK_DIR"/*/"$VARIANT"/app-"$ABI"-"$VARIANT".apk \
    "$APK_DIR"/app-"$ABI"-"$VARIANT".apk \
    "$APK_DIR"/*/"$VARIANT"/app-*-"$VARIANT".apk \
    "$APK_DIR"/app-"$VARIANT".apk; do
    if [ -f "$candidate" ]; then
        APK=$candidate
        break
    fi
done
if [ ! -f "$APK" ]; then
    echo "Expected ABI APK below: $APK_DIR/$VARIANT/app-*-$ABI-$VARIANT.apk" >&2
    echo "Expected ABI APK below: $APK_DIR/*/$VARIANT/app-*-$ABI-$VARIANT.apk" >&2
    echo "Expected fallback below: $APK_DIR/$VARIANT/app-*-$VARIANT.apk" >&2
    echo "Expected fallback below: $APK_DIR/*/$VARIANT/app-*-$VARIANT.apk" >&2
    echo "Available APKs:" >&2
    find "$APK_DIR" -type f -name '*.apk' -print >&2 2>/dev/null || true
    fail "no installable $VARIANT APK for device ABI '$ABI'."
fi

echo "Android target: $(adb_run shell getprop ro.product.model 2>/dev/null | tr -d '\r') ($ABI)"
echo "APK: $APK"

INSTALL_OUTPUT=$(adb_run install -r "$APK" 2>&1) || {
    echo "$INSTALL_OUTPUT" >&2
    case "$INSTALL_OUTPUT" in
        *INSTALL_FAILED_UPDATE_INCOMPATIBLE*|*signatures\ do\ not\ match*)
            echo >&2
            echo "The installed $APP_ID package was signed with a different key." >&2
            echo "Use a release APK signed with the same key, or delete the installed app first:" >&2
            echo "  $ADB_CMD shell cmd package uninstall $APP_ID" >&2
            ;;
        *more\ than\ one\ device*|*more\ than\ one\ emulator*)
            echo >&2
            echo "Select one target explicitly, for example:" >&2
            echo "  make android-install ADB='adb -s SERIAL'" >&2
            ;;
    esac
    fail "adb install did not complete."
}
echo "$INSTALL_OUTPUT"

START_OUTPUT=$(adb_run shell am start -n "$APP_ID/$ACTIVITY" 2>&1) || {
    echo "$START_OUTPUT" >&2
    fail "installed APK but could not launch $APP_ID/$ACTIVITY."
}
echo "$START_OUTPUT"

tries=0
while [ "$tries" -lt 12 ]; do
    if adb_run shell pidof "$APP_ID" >/dev/null 2>&1; then
        echo "Android install complete."
        exit 0
    fi
    tries=$((tries + 1))
    sleep 1
done

fail "launch command returned, but $APP_ID is not running after 12 seconds. Check logcat for startup errors."
