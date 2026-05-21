#!/bin/env bash

./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n xyz.waozi.inbe/android.app.NativeActivity



