.DEFAULT_GOAL := all

APP_NAME := inbe
APP_TITLE := Inner Breeze
ANDROID_APP_ID := xyz.waozi.inbe
ANDROID_ACTIVITY := xyz.waozi.inbe.MainActivity

CC ?= gcc
GRADLE ?= gradle
ARCH := $(shell uname -m)

BUILD_DIR := build
BUILD_OBJ_DIR := $(BUILD_DIR)/obj
BUILD_BIN_DIR := $(BUILD_DIR)/bin
BUILD_DIST_DIR := $(BUILD_DIR)/dist
LINUX_OBJ_DIR := $(BUILD_OBJ_DIR)/linux
LINUX_BIN_DIR := $(BUILD_BIN_DIR)/linux
LINUX_DIST_DIR := $(BUILD_DIST_DIR)/linux
ANDROID_BUILD_DIR := $(BUILD_DIR)/android
WEB_OBJ_DIR := $(BUILD_OBJ_DIR)/web
WEB_DIST_DIR := $(BUILD_DIST_DIR)/web

RAYLIB_DIR := vendor/raylib/src
RAYLIB_BUILD_DIR := $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib
RAYLIB_A := $(RAYLIB_BUILD_DIR)/libraylib.a
WEB_RAYLIB_BUILD_DIR := $(WEB_OBJ_DIR)/raylib
WEB_RAYLIB_A := $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
RAYLIB_SOURCES := $(wildcard $(RAYLIB_DIR)/*.c) $(wildcard $(RAYLIB_DIR)/*.h)

FLINT_DIR := flint
FLINT_ICON_FILES := $(wildcard $(FLINT_DIR)/icons/*.png)
FLINT_ICON_ASSETS_C := $(FLINT_DIR)/src/flint_icon_assets.c
FLINT_SRCS := $(filter-out $(FLINT_ICON_ASSETS_C),$(wildcard $(FLINT_DIR)/src/*.c)) $(FLINT_ICON_ASSETS_C)
FLINT_WEB_SRCS := $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
FLINT_INCLUDE := -I$(FLINT_DIR)/include
FLINT_CURL_CFLAGS ?= $(shell pkg-config --cflags libcurl 2>/dev/null)
FLINT_CURL_LDLIBS ?= $(shell pkg-config --libs libcurl 2>/dev/null)
SQLITE_DIR := vendor/sqlite
SQLITE_BUILD_DIR := $(BUILD_OBJ_DIR)/sqlite
SQLITE_AMALGAMATION_C := $(SQLITE_BUILD_DIR)/sqlite3.c
SQLITE_AMALGAMATION_H := $(SQLITE_BUILD_DIR)/sqlite3.h
SQLITE_SRC := $(SQLITE_AMALGAMATION_C)
SQLITE_INCLUDE := -I$(SQLITE_BUILD_DIR)
TEST_BIN_DIR := $(BUILD_BIN_DIR)/tests
STORAGE_IMPORT_TEST := $(TEST_BIN_DIR)/storage_import_test
ifneq ($(strip $(FLINT_CURL_LDLIBS)),)
FLINT_RUNTIME_ASSET_CFLAGS := -DFLINT_HAS_LIBCURL=1 $(FLINT_CURL_CFLAGS)
FLINT_RUNTIME_ASSET_LDLIBS := $(FLINT_CURL_LDLIBS)
else
FLINT_RUNTIME_ASSET_CFLAGS :=
FLINT_RUNTIME_ASSET_LDLIBS :=
endif

APP_SRCS := \
	src/main.c \
	src/breath_engine.c \
	src/app.c \
	src/app_preferences.c \
	src/app_session.c \
	src/screens/habits_screen.c \
	src/meditation_music.c \
	src/locale.c \
	src/theme.c \
	src/data.c \
	src/storage.c \
	src/miniz.c \
	src/android/android_device.c \
	src/screens/practice_screen.c \
	src/tabs/language_tab.c \
	src/tabs/manual_tab.c \
	src/tabs/settings_tab.c

LOCALE_FILES := $(wildcard locales/*.txt)
IMAGE_FILES := assets/whm/1.jpg assets/whm/2.jpg
SOUND_FILES := $(wildcard assets/sounds/*.ogg)
FONT_OUTPUTS := assets/fonts/locales.png assets/fonts/locales.dat
FONT_TOOL := vendor/otfchop/otfchop
FONT_SOURCE := vendor/otfchop/unifont-17.0.04.otf
EMBEDDED_ASSETS_C := $(BUILD_OBJ_DIR)/$(APP_NAME)_embedded_assets.c
EMBEDDED_ASSET_FILES := $(LOCALE_FILES) $(IMAGE_FILES) $(SOUND_FILES) $(FONT_OUTPUTS)
SRC := $(APP_SRCS) $(EMBEDDED_ASSETS_C)

APP_INCLUDE := -Isrc -Isrc/android
APP_RAYLIB_CONFIG := $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DFLINT_EMBEDDED_ONLY=1 $(FLINT_RUNTIME_ASSET_CFLAGS)
WEB_CFLAGS := $(filter-out -std=c99,$(CFLAGS)) -std=gnu99
LDFLAGS := -Wl,--gc-sections -s

BINARY_NAME := $(APP_NAME)-linux-$(ARCH)
TARGET := $(LINUX_BIN_DIR)/$(BINARY_NAME)
WEB_CC ?= emcc
WEB_AR ?= emar
WEB_CACHE_BUSTER ?= $(shell git rev-parse --short HEAD 2>/dev/null || date +%s)
WEB_TARGET := $(WEB_DIST_DIR)/index.html
WEB_ASSET_FILES := $(shell find web-assets -type f 2>/dev/null)
UNPACKAGED_AUDIO_DIR := unpackaged_assets/audio
MEDITATION_AUDIO_ZIP := web-assets/dl/inbe-meditation-audio-v1.zip

.PHONY: all native run test clean clean-linux clean-raylib android-copy-assets android-debug android-release android-bundle android-install android-install-release android-clean package-unpackaged-assets windows web

all: native

native: $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(STORAGE_IMPORT_TEST)
	$(STORAGE_IMPORT_TEST)

$(STORAGE_IMPORT_TEST): tests/storage_import_test.c src/storage.c src/storage.h src/screens/habits_screen.c src/screens/habits_screen.h src/miniz.c src/miniz.h $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DRINI_IMPLEMENTATION -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES \
		-Isrc -Ivendor/raylib/src $(SQLITE_INCLUDE) \
		-o $@ \
		tests/storage_import_test.c src/storage.c src/screens/habits_screen.c src/miniz.c $(SQLITE_SRC) \
		-lm -lpthread -ldl

$(BUILD_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR) $(ANDROID_BUILD_DIR) $(TEST_BIN_DIR) $(WEB_OBJ_DIR) $(WEB_DIST_DIR):
	mkdir -p $@

assets/fonts:
	mkdir -p $@

$(FONT_TOOL): vendor/otfchop/otfchop.c vendor/otfchop/stb_truetype.h vendor/otfchop/stb_image_write.h
	$(MAKE) -C vendor/otfchop otfchop

$(FONT_OUTPUTS): $(LOCALE_FILES) $(FONT_TOOL) | assets/fonts
	$(FONT_TOOL) $(FONT_SOURCE) $(LOCALE_FILES) assets/fonts/locales

$(EMBEDDED_ASSETS_C): $(EMBEDDED_ASSET_FILES) $(FLINT_DIR)/scripts/embed-assets.sh | $(BUILD_OBJ_DIR)
	sh $(FLINT_DIR)/scripts/embed-assets.sh $@ $(EMBEDDED_ASSET_FILES)

$(FLINT_ICON_ASSETS_C): $(FLINT_ICON_FILES) $(FLINT_DIR)/scripts/embed-icons.sh
	sh $(FLINT_DIR)/scripts/embed-icons.sh $(FLINT_DIR)/icons $@

$(RAYLIB_A): $(RAYLIB_SOURCES)
	rm -rf $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src
	mkdir -p $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src $(RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src/
	$(MAKE) -j1 -C $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		SDL_INCLUDE_PATH="$(RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(RAY_CFLAGS) $(APP_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

$(WEB_RAYLIB_A): $(RAYLIB_SOURCES) | $(WEB_OBJ_DIR)
	rm -rf $(WEB_OBJ_DIR)/raylib-src
	mkdir -p $(WEB_OBJ_DIR)/raylib-src $(WEB_RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(WEB_OBJ_DIR)/raylib-src/
	$(MAKE) -j1 -C $(WEB_OBJ_DIR)/raylib-src \
		PLATFORM=PLATFORM_WEB \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		CC="$(WEB_CC)" \
		AR="$(WEB_AR)" \
		CUSTOM_CFLAGS="$(APP_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

$(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H): $(SQLITE_DIR)/configure $(SQLITE_DIR)/manifest | $(BUILD_OBJ_DIR)
	mkdir -p $(SQLITE_BUILD_DIR)
	cd $(SQLITE_BUILD_DIR) && ../../../$(SQLITE_DIR)/configure
	$(MAKE) -C $(SQLITE_BUILD_DIR) sqlite3.c sqlite3.h

$(TARGET): Makefile $(SRC) $(FLINT_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(RAYLIB_A) | $(LINUX_BIN_DIR)
	$(CC) $(CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		$(RAY_CFLAGS) \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=1 \
		-o $@ \
		$(SRC) \
		$(FLINT_SRCS) \
		$(SQLITE_SRC) \
		$(RAYLIB_A) \
		$(RAY_LDLIBS) \
		$(FLINT_RUNTIME_ASSET_LDLIBS) \
		-lm -lpthread -ldl -lrt \
		$(LDFLAGS)

$(WEB_TARGET): Makefile $(SRC) $(FLINT_WEB_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(WEB_RAYLIB_A) src/web_shell.html manifest.json $(WEB_ASSET_FILES) | $(WEB_DIST_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		-DPLATFORM_WEB \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=1 \
		-o $@ \
		$(SRC) \
		$(FLINT_WEB_SRCS) \
		$(SQLITE_SRC) \
		$(WEB_RAYLIB_A) \
		-sUSE_GLFW=3 \
		-sASYNCIFY \
		-sFORCE_FILESYSTEM=1 \
		-sFETCH=1 \
		-sALLOW_MEMORY_GROWTH=1 \
		--shell-file src/web_shell.html \
		-lidbfs.js \
		-lm
	perl -0pi -e 's/WEB_CACHE_BUSTER/$(WEB_CACHE_BUSTER)/g' $(WEB_DIST_DIR)/index.html
	cp -R web-assets $(WEB_DIST_DIR)/
	cp manifest.json $(WEB_DIST_DIR)/

android-copy-assets:
	$(MAKE) $(FONT_OUTPUTS)
	$(MAKE) $(EMBEDDED_ASSETS_C)
	rm -rf droid/app/src/main/assets
	mkdir -p droid/app/src/main/assets

android-debug: android-copy-assets
	unset ANDROID_HOME; $(GRADLE) -p droid assembleDebug
	$(MAKE) android-copy-debug-apks

android-release: android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		unset ANDROID_HOME; $(GRADLE) -p droid assembleRelease -Pkeystore.password="$(PASSWORD)" || exit $$?; \
	else \
		echo "Set PASSWORD=your-keystore-password for release builds"; \
		exit 1; \
	fi
	$(MAKE) android-copy-release-apks

android-bundle: android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		unset ANDROID_HOME; $(GRADLE) -p droid bundleRelease -Pkeystore.password="$(PASSWORD)" || exit $$?; \
	else \
		echo "Set PASSWORD=your-keystore-password for bundle builds"; \
		exit 1; \
	fi
	$(MAKE) android-copy-bundle

android-copy-debug-apks: | $(ANDROID_BUILD_DIR)
	@for apk in droid/app/build/outputs/apk/debug/*.apk; do \
		if [ -f "$$apk" ]; then cp "$$apk" "$(ANDROID_BUILD_DIR)/$$(basename "$$apk")"; fi; \
	done

android-copy-release-apks: | $(ANDROID_BUILD_DIR)
	@for apk in droid/app/build/outputs/apk/release/*.apk; do \
		if [ -f "$$apk" ]; then cp "$$apk" "$(ANDROID_BUILD_DIR)/$$(basename "$$apk")"; fi; \
	done

android-copy-bundle: | $(ANDROID_BUILD_DIR)
	@for bundle in droid/app/build/outputs/bundle/release/*.aab; do \
		if [ -f "$$bundle" ]; then cp "$$bundle" "$(ANDROID_BUILD_DIR)/$$(basename "$$bundle")"; fi; \
	done

android-install: android-debug
	@ABI=$$(adb shell getprop ro.product.cpu.abi | tr -d '\r'); \
	APK=droid/app/build/outputs/apk/debug/app-$${ABI}-debug.apk; \
	if [ ! -f "$$APK" ]; then APK=droid/app/build/outputs/apk/debug/app-debug.apk; fi; \
	adb install -r "$$APK"; \
	adb shell am start -n $(ANDROID_APP_ID)/$(ANDROID_ACTIVITY)

android-install-release: android-release
	@ABI=$$(adb shell getprop ro.product.cpu.abi | tr -d '\r'); \
	APK=droid/app/build/outputs/apk/release/app-$${ABI}-release.apk; \
	if [ ! -f "$$APK" ]; then APK=droid/app/build/outputs/apk/release/app-release.apk; fi; \
	adb install -r "$$APK"; \
	adb shell am start -n $(ANDROID_APP_ID)/$(ANDROID_ACTIVITY)

android-clean:
	$(GRADLE) -p droid clean
	rm -rf $(ANDROID_BUILD_DIR)

package-unpackaged-assets:
	mkdir -p $(dir $(MEDITATION_AUDIO_ZIP))
	rm -f $(MEDITATION_AUDIO_ZIP)
	cd $(UNPACKAGED_AUDIO_DIR) && find . -mindepth 2 -type f -name '*.ogg' -exec zip -9 -r $(abspath $(MEDITATION_AUDIO_ZIP)) {} + && zip -9 -r $(abspath $(MEDITATION_AUDIO_ZIP)) LICENSE.md MANIFEST.txt

windows:
	@echo "Windows packaging was removed with the Flint CLI layer. Add a focused script when it is needed."
	@exit 1

web:
	$(MAKE) $(WEB_TARGET)

clean:
	rm -rf build

clean-linux:
	rm -rf $(LINUX_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR)

clean-raylib:
	rm -rf $(RAYLIB_BUILD_DIR) $(LINUX_OBJ_DIR)/*/native/raylib-src vendor/raylib/build

NEEDS_NATIVE_ENV := $(if $(MAKECMDGOALS),$(filter all native run,$(MAKECMDGOALS)),native)
ifneq ($(strip $(NEEDS_NATIVE_ENV)),)
ifeq ($(strip $(RAY_CFLAGS)),)
$(error RAY_CFLAGS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_LDLIBS)),)
$(error RAY_LDLIBS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_SDL_LDLIBS)),)
$(error RAY_SDL_LDLIBS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_SDL_INCLUDE_DIR)),)
$(error RAY_SDL_INCLUDE_DIR is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_RAYLIB_CONFIG)),)
$(error RAY_RAYLIB_CONFIG is not set. Enter the ray flake shell with 'nix develop')
endif
endif
