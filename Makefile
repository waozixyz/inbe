.DEFAULT_GOAL := all

APP_NAME := inbe
APP_TITLE := Inner Breeze
ANDROID_APP_ID := xyz.waozi.inbe
ANDROID_ACTIVITY := xyz.waozi.inbe.MainActivity

CC ?= gcc
GRADLE ?= gradle
ARCH := $(shell uname -m)
ANDROID_KEYSTORE ?= $(HOME)/.android/flint-release.keystore
ANDROID_KEY_ALIAS ?= inbe-key

BUILD_DIR := build
BUILD_OBJ_DIR := $(BUILD_DIR)/obj
BUILD_BIN_DIR := $(BUILD_DIR)/bin
BUILD_DIST_DIR := $(BUILD_DIR)/dist
LINUX_OBJ_DIR := $(BUILD_OBJ_DIR)/linux
LINUX_BIN_DIR := $(BUILD_BIN_DIR)/linux
LINUX_DIST_DIR := $(BUILD_DIST_DIR)/linux
LINUX_APPIMAGE_BUILD_DIR := $(BUILD_OBJ_DIR)/appimage/linux
LINUX_APPDIR := $(LINUX_APPIMAGE_BUILD_DIR)/$(APP_NAME).AppDir
LINUX_APPIMAGE_DIR := packaging/linux/appimage
LINUX_APPIMAGE_APPRUN := $(LINUX_APPIMAGE_DIR)/AppRun
LINUX_APPIMAGE_DESKTOP := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).desktop
LINUX_APPIMAGE_ICON := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).png
WINDOWS_OBJ_DIR := $(BUILD_OBJ_DIR)/windows
WINDOWS_BIN_DIR := $(BUILD_BIN_DIR)/windows
WINDOWS_DIST_DIR := $(BUILD_DIST_DIR)/windows
ANDROID_BUILD_DIR := $(BUILD_DIR)/android
WEB_OBJ_DIR := $(BUILD_OBJ_DIR)/web
WEB_DIST_DIR := $(BUILD_DIST_DIR)/web
VERSION_FILE := src/core/version.h
APP_VERSION := $(shell sed -n 's/^#define INBE_VERSION_STRING "\([^"]*\)".*/\1/p' $(VERSION_FILE) 2>/dev/null)

RAYLIB_DIR := vendor/raylib/src
RAYLIB_BUILD_DIR := $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib
RAYLIB_A := $(RAYLIB_BUILD_DIR)/libraylib.a
WIN64_ARCH := x86_64
WIN64_CC ?= $(or $(WIN_CC),x86_64-w64-mingw32-gcc)
WIN64_AR ?= $(or $(WIN_AR),x86_64-w64-mingw32-ar)
WIN64_RANLIB ?= $(or $(WIN_RANLIB),x86_64-w64-mingw32-ranlib)
WIN64_STRIP ?= $(or $(WIN_STRIP),x86_64-w64-mingw32-strip)
WIN32_ARCH := i686
WIN32_CC ?= i686-w64-mingw32-gcc
WIN32_AR ?= i686-w64-mingw32-ar
WIN32_RANLIB ?= i686-w64-mingw32-ranlib
WIN32_STRIP ?= i686-w64-mingw32-strip
WIN64_RAYLIB_BUILD_DIR := $(WINDOWS_OBJ_DIR)/$(WIN64_ARCH)/raylib
WIN64_RAYLIB_A := $(WIN64_RAYLIB_BUILD_DIR)/libraylib.a
WIN32_RAYLIB_BUILD_DIR := $(WINDOWS_OBJ_DIR)/$(WIN32_ARCH)/raylib
WIN32_RAYLIB_A := $(WIN32_RAYLIB_BUILD_DIR)/libraylib.a
WEB_RAYLIB_BUILD_DIR := $(WEB_OBJ_DIR)/raylib
WEB_RAYLIB_A := $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
RAYLIB_SOURCES := $(wildcard $(RAYLIB_DIR)/*.c) $(wildcard $(RAYLIB_DIR)/*.h)

FLINT_DIR := flint
FLINT_ICON_FILES := $(wildcard $(FLINT_DIR)/icons/*.png)
FLINT_ICON_ASSETS_C := $(FLINT_DIR)/src/flint_icon_assets.c
FLINT_SRCS := $(filter-out $(FLINT_ICON_ASSETS_C),$(wildcard $(FLINT_DIR)/src/*.c)) $(FLINT_ICON_ASSETS_C)
FLINT_WEB_SRCS := $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
FLINT_WINDOWS_SRCS := $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
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
FLINT_TEXT_SCALING_TEST := $(TEST_BIN_DIR)/flint_text_scaling_test
ifneq ($(strip $(FLINT_CURL_LDLIBS)),)
FLINT_RUNTIME_ASSET_CFLAGS := -DFLINT_HAS_LIBCURL=1 $(FLINT_CURL_CFLAGS)
FLINT_RUNTIME_ASSET_LDLIBS := $(FLINT_CURL_LDLIBS)
else
FLINT_RUNTIME_ASSET_CFLAGS :=
FLINT_RUNTIME_ASSET_LDLIBS :=
endif

APP_SRCS := \
	src/main.c \
	src/core/breath_engine.c \
	src/app/app.c \
	src/app/device_preferences.c \
	src/session/session.c \
	src/screens/habits_screen.c \
	src/session/meditation_music.c \
	src/core/locale.c \
	src/core/theme.c \
	src/storage/data.c \
	src/storage/storage.c \
	src/third_party/miniz.c \
	src/platform/android/android_device.c \
	src/screens/practice_screen.c \
	src/screens/language_screen.c \
	src/screens/manual_screen.c \
	src/screens/settings/settings_screen.c

LOCALE_FILES := $(wildcard locales/*.txt)
IMAGE_FILES := assets/whm/1.jpg assets/whm/2.jpg
SOUND_FILES := $(wildcard assets/sounds/*.ogg)
FONT_OUTPUTS := assets/fonts/locales.png assets/fonts/locales.dat assets/fonts/locales-8.png assets/fonts/locales-8.dat
OTFCHOP_DIR ?= vendor/otfchop
FONT_TOOL := $(OTFCHOP_DIR)/otfchop
FONT_SOURCE := $(OTFCHOP_DIR)/unifont-17.0.04.otf
EMBEDDED_ASSETS_C := $(BUILD_OBJ_DIR)/$(APP_NAME)_embedded_assets.c
EMBEDDED_ASSET_FILES := $(LOCALE_FILES) $(IMAGE_FILES) $(SOUND_FILES) $(FONT_OUTPUTS)
SRC := $(APP_SRCS) $(EMBEDDED_ASSETS_C)

APP_INCLUDE := -Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/session -Isrc/storage -Isrc/platform/android -Isrc/third_party
APP_RAYLIB_CONFIG := $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DFLINT_EMBEDDED_ONLY=1 $(FLINT_RUNTIME_ASSET_CFLAGS)
WINDOWS_CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DFLINT_EMBEDDED_ONLY=1
WEB_CFLAGS := $(filter-out -std=c99,$(CFLAGS)) -std=gnu99
LDFLAGS := -Wl,--gc-sections -s
WINDOWS_LDFLAGS := -Wl,--gc-sections -static -static-libgcc -mwindows
WINDOWS_LDLIBS := -lgdi32 -lwinmm -lopengl32 -luser32 -lshell32 -lole32 -lcomdlg32 -lcomctl32 -luuid -lm
ifneq ($(strip $(MCFGTHREADS)),)
WIN64_THREAD_LDFLAGS := -L$(MCFGTHREADS)/lib
else
WIN64_THREAD_LDFLAGS :=
endif
ifneq ($(strip $(WIN32_MCFGTHREADS)),)
WIN32_THREAD_LDFLAGS := -L$(WIN32_MCFGTHREADS)/lib
else
WIN32_THREAD_LDFLAGS :=
endif

BINARY_NAME := $(APP_NAME)-linux-$(ARCH)
TARGET := $(LINUX_BIN_DIR)/$(BINARY_NAME)
WIN64_BINARY_NAME := $(APP_NAME)-windows-$(WIN64_ARCH).exe
WIN64_TARGET := $(WINDOWS_BIN_DIR)/$(WIN64_ARCH)/$(WIN64_BINARY_NAME)
WIN32_BINARY_NAME := $(APP_NAME)-windows-$(WIN32_ARCH).exe
WIN32_TARGET := $(WINDOWS_BIN_DIR)/$(WIN32_ARCH)/$(WIN32_BINARY_NAME)
WINDOWS_DIST := $(WINDOWS_DIST_DIR)/$(APP_NAME)-windows.zip
APPIMAGE_NAME := $(APP_NAME)-linux-$(ARCH).AppImage
APPIMAGE_TARGET := $(LINUX_DIST_DIR)/$(APPIMAGE_NAME)
LINUXDEPLOY ?= linuxdeploy
WEB_CC ?= emcc
WEB_AR ?= emar
WEB_CACHE_BUSTER ?= $(shell git rev-parse --short HEAD 2>/dev/null || date +%s)
WEB_TARGET := $(WEB_DIST_DIR)/index.html
WEB_ASSET_FILES := $(shell find web-assets -type f 2>/dev/null)
UNPACKAGED_AUDIO_DIR := unpackaged_assets/audio
MEDITATION_AUDIO_ZIP := web-assets/dl/inbe-meditation-audio-v1.zip

.PHONY: all native run test dist appimage clean clean-linux clean-raylib android-check-keystore android-copy-assets android-debug android-release android-bundle android-install android-install-release android-clean package-unpackaged-assets windows windows64 windows32 web
.NOTPARALLEL: dist windows windows64 windows32 android-release android-bundle

all: native

native: $(TARGET)

dist:
	@password="$(PASSWORD)"; \
	if [ -z "$$password" ]; then \
		printf "Android release keystore password: "; \
		stty -echo; \
		read password; \
		stty echo; \
		printf "\n"; \
	fi; \
	if [ -z "$$password" ]; then \
		echo "Set PASSWORD=your-keystore-password for release builds"; \
		exit 1; \
	fi; \
	$(MAKE) android-check-keystore PASSWORD="$$password" && \
	$(MAKE) package-unpackaged-assets && \
	$(MAKE) web && \
	$(MAKE) appimage && \
	$(MAKE) windows && \
	$(MAKE) android-release PASSWORD="$$password" && \
	$(MAKE) android-bundle PASSWORD="$$password"

appimage: $(APPIMAGE_TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(STORAGE_IMPORT_TEST) $(FLINT_TEXT_SCALING_TEST)
	$(STORAGE_IMPORT_TEST)
	$(FLINT_TEXT_SCALING_TEST)

$(STORAGE_IMPORT_TEST): tests/storage_import_test.c src/storage/storage.c src/storage/storage.h src/screens/habits_screen.c src/screens/habits_screen.h src/third_party/miniz.c src/third_party/miniz.h $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -ffunction-sections -fdata-sections \
		-Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/session -Isrc/storage -Isrc/platform/android -Isrc/third_party -Ivendor/raylib/src $(FLINT_INCLUDE) $(SQLITE_INCLUDE) \
		-o $@ \
		tests/storage_import_test.c src/storage/storage.c src/screens/habits_screen.c src/third_party/miniz.c $(SQLITE_SRC) \
		-Wl,--gc-sections -lm -lpthread -ldl

$(FLINT_TEXT_SCALING_TEST): tests/flint_text_scaling_test.c flint/src/flint_text.c flint/src/flint_clip.c flint/src/flint_scaling.c flint/include/flint_text.h flint/include/flint_clip.h flint/include/flint_scaling.h | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -ffunction-sections -fdata-sections \
		-Iflint/include -Ivendor/raylib/src \
		-o $@ \
		tests/flint_text_scaling_test.c flint/src/flint_text.c flint/src/flint_clip.c flint/src/flint_scaling.c \
		-Wl,--gc-sections -lm

$(BUILD_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR) $(LINUX_APPIMAGE_BUILD_DIR) $(WINDOWS_DIST_DIR) $(ANDROID_BUILD_DIR) $(TEST_BIN_DIR) $(WEB_OBJ_DIR) $(WEB_DIST_DIR):
	mkdir -p $@

$(WINDOWS_BIN_DIR)/$(WIN64_ARCH) $(WINDOWS_BIN_DIR)/$(WIN32_ARCH):
	mkdir -p $@

assets/fonts:
	mkdir -p $@

$(FONT_TOOL): vendor/otfchop/otfchop.c vendor/otfchop/stb_truetype.h vendor/otfchop/stb_image_write.h
	$(MAKE) -C vendor/otfchop otfchop

assets/fonts/locales.png assets/fonts/locales.dat: $(LOCALE_FILES) $(FONT_TOOL) | assets/fonts
	$(FONT_TOOL) --size 16 $(FONT_SOURCE) $(LOCALE_FILES) assets/fonts/locales

assets/fonts/locales-8.png assets/fonts/locales-8.dat: $(LOCALE_FILES) $(FONT_TOOL) | assets/fonts
	$(FONT_TOOL) --size 8 $(FONT_SOURCE) $(LOCALE_FILES) assets/fonts/locales-8

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

$(WIN64_RAYLIB_A): $(RAYLIB_SOURCES)
	rm -rf $(WINDOWS_OBJ_DIR)/$(WIN64_ARCH)/raylib-src
	mkdir -p $(WINDOWS_OBJ_DIR)/$(WIN64_ARCH)/raylib-src $(WIN64_RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(WINDOWS_OBJ_DIR)/$(WIN64_ARCH)/raylib-src/
	$(MAKE) -j1 -C $(WINDOWS_OBJ_DIR)/$(WIN64_ARCH)/raylib-src \
		OS=Windows_NT \
		PLATFORM=PLATFORM_DESKTOP_RGFW \
		GRAPHICS=GRAPHICS_API_OPENGL_11 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		CC="$(WIN64_CC)" \
		AR="$(WIN64_AR)" \
		RANLIB="$(WIN64_RANLIB)" \
		CUSTOM_CFLAGS="$(APP_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

$(WIN32_RAYLIB_A): $(RAYLIB_SOURCES)
	rm -rf $(WINDOWS_OBJ_DIR)/$(WIN32_ARCH)/raylib-src
	mkdir -p $(WINDOWS_OBJ_DIR)/$(WIN32_ARCH)/raylib-src $(WIN32_RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(WINDOWS_OBJ_DIR)/$(WIN32_ARCH)/raylib-src/
	$(MAKE) -j1 -C $(WINDOWS_OBJ_DIR)/$(WIN32_ARCH)/raylib-src \
		OS=Windows_NT \
		PLATFORM=PLATFORM_DESKTOP_RGFW \
		GRAPHICS=GRAPHICS_API_OPENGL_11 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		CC="$(WIN32_CC)" \
		AR="$(WIN32_AR)" \
		RANLIB="$(WIN32_RANLIB)" \
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

$(WIN64_TARGET): Makefile $(SRC) $(FLINT_WINDOWS_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(WIN64_RAYLIB_A) | $(WINDOWS_BIN_DIR)/$(WIN64_ARCH)
	$(WIN64_CC) $(WINDOWS_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		-DPLATFORM_DESKTOP \
		-o $@ \
		$(SRC) \
		$(FLINT_WINDOWS_SRCS) \
		$(SQLITE_SRC) \
		$(WIN64_RAYLIB_A) \
		$(WINDOWS_LDLIBS) \
		$(WIN64_THREAD_LDFLAGS) \
		$(WINDOWS_LDFLAGS)
	$(WIN64_STRIP) $@

$(WIN32_TARGET): Makefile $(SRC) $(FLINT_WINDOWS_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(WIN32_RAYLIB_A) | $(WINDOWS_BIN_DIR)/$(WIN32_ARCH)
	$(WIN32_CC) $(WINDOWS_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		-DPLATFORM_DESKTOP \
		-o $@ \
		$(SRC) \
		$(FLINT_WINDOWS_SRCS) \
		$(SQLITE_SRC) \
		$(WIN32_RAYLIB_A) \
		$(WINDOWS_LDLIBS) \
		$(WIN32_THREAD_LDFLAGS) \
		$(WINDOWS_LDFLAGS)
	$(WIN32_STRIP) $@

$(APPIMAGE_TARGET): $(TARGET) $(LINUX_APPIMAGE_APPRUN) $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPIMAGE_ICON) | $(LINUX_DIST_DIR) $(LINUX_APPIMAGE_BUILD_DIR)
	@command -v linuxdeploy-plugin-appimage >/dev/null || { \
		echo "linuxdeploy-plugin-appimage is missing. Re-enter the flake shell with: nix develop"; \
		exit 1; \
	}
	@command -v appimagetool >/dev/null || { \
		echo "appimagetool is missing. Re-enter the flake shell with: nix develop"; \
		exit 1; \
	}
	rm -rf $(LINUX_APPDIR)
	rm -rf $(LINUX_DIST_DIR)/*.AppDir
	rm -f $(LINUX_DIST_DIR)/*.AppImage
	mkdir -p $(LINUX_APPDIR)/usr/bin $(LINUX_APPDIR)/usr/share/applications $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps
	cp $(TARGET) $(LINUX_APPDIR)/usr/bin/$(APP_NAME)
	cp $(LINUX_APPIMAGE_APPRUN) $(LINUX_APPDIR)/AppRun
	chmod +x $(LINUX_APPDIR)/AppRun
	cp $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPDIR)/$(APP_NAME).desktop
	cp $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPDIR)/usr/share/applications/$(APP_NAME).desktop
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/$(APP_NAME).png
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png
	cd $(LINUX_APPIMAGE_BUILD_DIR) && env -u SOURCE_DATE_EPOCH ARCH=$(ARCH) LDAI_OUTPUT=$(abspath $(APPIMAGE_TARGET)) $(LINUXDEPLOY) \
		--appdir $(APP_NAME).AppDir \
		--executable $(abspath $(LINUX_APPDIR)/usr/bin/$(APP_NAME)) \
		--desktop-file $(abspath $(LINUX_APPIMAGE_DESKTOP)) \
		--icon-file $(abspath $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png) \
		--output appimage
	test -f $@

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

android-check-keystore:
	@if [ -z "$(PASSWORD)" ]; then \
		echo "Set PASSWORD=your-keystore-password for release builds"; \
		exit 1; \
	fi
	@if [ ! -f "$(ANDROID_KEYSTORE)" ]; then \
		echo "Android release keystore not found: $(ANDROID_KEYSTORE)"; \
		exit 1; \
	fi
	@command -v keytool >/dev/null || { \
		echo "keytool is missing. Re-enter the Android/JDK build environment."; \
		exit 1; \
	}
	@if ! keytool -list -keystore "$(ANDROID_KEYSTORE)" -storepass "$(PASSWORD)" -alias "$(ANDROID_KEY_ALIAS)" >/dev/null 2>&1; then \
		echo "Android release keystore password or alias is invalid"; \
		echo "Checked keystore: $(ANDROID_KEYSTORE)"; \
		echo "Checked alias: $(ANDROID_KEY_ALIAS)"; \
		exit 1; \
	fi

android-debug: android-copy-assets
	unset ANDROID_HOME; $(GRADLE) -p droid assembleDebug
	$(MAKE) android-copy-debug-apks

android-release:
	$(MAKE) android-check-keystore PASSWORD="$(PASSWORD)"
	$(MAKE) android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		unset ANDROID_HOME; $(GRADLE) -p droid assembleRelease -Pkeystore.path="$(ANDROID_KEYSTORE)" -Pkeystore.alias="$(ANDROID_KEY_ALIAS)" -Pkeystore.password="$(PASSWORD)" || exit $$?; \
	else \
		echo "Set PASSWORD=your-keystore-password for release builds"; \
		exit 1; \
	fi
	$(MAKE) android-copy-release-apks

android-bundle:
	$(MAKE) android-check-keystore PASSWORD="$(PASSWORD)"
	$(MAKE) android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		unset ANDROID_HOME; $(GRADLE) -p droid bundleRelease -Pkeystore.path="$(ANDROID_KEYSTORE)" -Pkeystore.alias="$(ANDROID_KEY_ALIAS)" -Pkeystore.password="$(PASSWORD)" || exit $$?; \
	else \
		echo "Set PASSWORD=your-keystore-password for bundle builds"; \
		exit 1; \
	fi
	$(MAKE) android-copy-bundle

android-copy-debug-apks: | $(ANDROID_BUILD_DIR)
	@found=0; \
	for apk in droid/app/build/outputs/apk/debug/*.apk; do \
		if [ -f "$$apk" ]; then \
			cp "$$apk" "$(ANDROID_BUILD_DIR)/$$(basename "$$apk")"; \
			found=1; \
		fi; \
	done; \
	if [ "$$found" -eq 0 ]; then \
		echo "No debug APKs were produced"; \
		exit 1; \
	fi

android-copy-release-apks: | $(ANDROID_BUILD_DIR)
	@found=0; \
	for apk in droid/app/build/outputs/apk/release/*.apk; do \
		if [ -f "$$apk" ]; then \
			cp "$$apk" "$(ANDROID_BUILD_DIR)/$$(basename "$$apk")"; \
			found=1; \
		fi; \
	done; \
	if [ "$$found" -eq 0 ]; then \
		echo "No release APKs were produced"; \
		exit 1; \
	fi; \
	if [ -z "$(APP_VERSION)" ]; then \
		echo "Could not read INBE_VERSION_STRING from $(VERSION_FILE)"; \
		exit 1; \
	fi; \
	universal="$$(find droid/app/build/outputs/apk/release -maxdepth 1 -name '*universal*release*.apk' | head -n 1)"; \
	if [ -z "$$universal" ]; then universal="$$(find droid/app/build/outputs/apk/release -maxdepth 1 -name '*release*.apk' | head -n 1)"; fi; \
	if [ -z "$$universal" ]; then \
		echo "No release APK was available for versioned copy"; \
		exit 1; \
	fi; \
	cp "$$universal" "$(ANDROID_BUILD_DIR)/$(APP_NAME)-$(APP_VERSION).apk"; \
	cp "$$universal" "$(ANDROID_BUILD_DIR)/$(APP_NAME)-latest.apk"

android-copy-bundle: | $(ANDROID_BUILD_DIR)
	@found=0; \
	for bundle in droid/app/build/outputs/bundle/release/*.aab; do \
		if [ -f "$$bundle" ]; then \
			cp "$$bundle" "$(ANDROID_BUILD_DIR)/$$(basename "$$bundle")"; \
			found=1; \
		fi; \
	done; \
	if [ "$$found" -eq 0 ]; then \
		echo "No release AAB was produced"; \
		exit 1; \
	fi

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

windows64: $(WIN64_TARGET)

windows32: $(WIN32_TARGET)

windows:
	$(MAKE) windows64
	$(MAKE) windows32
	mkdir -p $(WINDOWS_DIST_DIR)
	rm -f $(WINDOWS_DIST_DIR)/$(APP_NAME)-windows-*.zip
	rm -f $(WINDOWS_DIST)
	cd $(WINDOWS_BIN_DIR) && zip -9 -j $(abspath $(WINDOWS_DIST)) \
		$(WIN64_ARCH)/$(WIN64_BINARY_NAME) \
		$(WIN32_ARCH)/$(WIN32_BINARY_NAME)

web:
	$(MAKE) $(WEB_TARGET)

clean:
	rm -rf build

clean-linux:
	rm -rf $(LINUX_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR)

clean-raylib:
	rm -rf $(RAYLIB_BUILD_DIR) $(LINUX_OBJ_DIR)/*/native/raylib-src vendor/raylib/build

NEEDS_NATIVE_ENV := $(if $(MAKECMDGOALS),$(filter all native run dist appimage,$(MAKECMDGOALS)),native)
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
