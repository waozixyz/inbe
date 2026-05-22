#!/bin/env bash

# 1. Build the APK. If this fails, exit immediately.
./gradlew :app:assembleDebug && \
\
# 2. If build succeeded, install the APK. If this fails, exit.
adb install -r app/build/outputs/apk/debug/app-debug.apk && \
\
# 3. If install succeeded, start the activity.
adb shell am start -n xyz.waozi.inbe/android.app.NativeActivity