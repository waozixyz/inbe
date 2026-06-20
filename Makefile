.DEFAULT_GOAL := all

APP_NAME := inbe
APP_TITLE := Inner Breeze
ANDROID_APP_ID := xyz.waozi.inbe
ANDROID_ACTIVITY := xyz.waozi.inbe.MainActivity

CC ?= gcc
CMAKE ?= $(shell if [ -x /usr/bin/cmake ]; then echo /usr/bin/cmake; else command -v cmake; fi)
GRADLE ?= gradle
ARCH := $(shell uname -m)
ANDROID_KEYSTORE ?= $(HOME)/.android/flint-release.keystore
ANDROID_KEY_ALIAS ?= inbe-key

BUILD_DIR := build
BUILD_OBJ_DIR := $(BUILD_DIR)/obj
BUILD_BIN_DIR := $(BUILD_DIR)/bin
BUILD_DIST_DIR := $(BUILD_DIR)/dist
VENDOR_BUILD_DIR := vendor-builds
LINUX_OBJ_DIR := $(BUILD_OBJ_DIR)/linux
LINUX_BIN_DIR := $(BUILD_BIN_DIR)/linux
LINUX_DIST_DIR := $(BUILD_DIST_DIR)/linux
LINUX_APPIMAGE_BUILD_DIR := $(BUILD_OBJ_DIR)/appimage/linux
LINUX_APPDIR := $(LINUX_APPIMAGE_BUILD_DIR)/$(APP_NAME).AppDir
LINUX_APPIMAGE_DIR := packaging/linux/appimage
LINUX_APPIMAGE_APPRUN := $(LINUX_APPIMAGE_DIR)/AppRun
LINUX_APPIMAGE_DESKTOP := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).desktop
LINUX_APPIMAGE_ICON := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).png
LINUX_APPIMAGE_APPDATA := $(LINUX_APPIMAGE_DIR)/$(APP_NAME).appdata.xml
CLICK_PACKAGE ?= inbe
CLICK_TITLE ?= $(APP_TITLE)
CLICK_MAINTAINER ?= Waozi Project <waozi@waozi.xyz>
CLICK_ARCH ?= arm64
CLICK_FRAMEWORK ?= ubuntu-sdk-20.04
CLICK_POLICY_VERSION ?= 20.04
CLICK_INCLUDE_METAINFO ?= 0
CLICK_DIR := packaging/click
CLICK_MANIFEST_TEMPLATE := $(CLICK_DIR)/manifest.json.in
CLICK_CONTROL_TEMPLATE := $(CLICK_DIR)/control.in
CLICK_APPARMOR_TEMPLATE := $(CLICK_DIR)/inbe.apparmor.in
CLICK_DESKTOP_TEMPLATE := $(CLICK_DIR)/inbe.desktop.in
CLICK_METAINFO_TEMPLATE := $(CLICK_DIR)/inbe.metainfo.xml.in
CLICK_RUNNER := $(CLICK_DIR)/run-inbe.sh
CLICK_BUILD_DIR := $(BUILD_OBJ_DIR)/click/$(CLICK_ARCH)
CLICK_ROOT := $(CLICK_BUILD_DIR)/$(CLICK_PACKAGE)
CLICK_CONTROL_DIR := $(CLICK_BUILD_DIR)/control
CLICK_BIN_DIR := $(BUILD_BIN_DIR)/click/$(CLICK_ARCH)
CLICK_DIST_DIR := $(BUILD_DIST_DIR)/click
CLICK_TARGET = $(CLICK_DIST_DIR)/$(CLICK_PACKAGE)_$(APP_VERSION)_$(CLICK_ARCH).click
CLICK_BIN := $(CLICK_BIN_DIR)/$(APP_NAME)
CLICK_BIN_INPUT := $(if $(strip $(CLICK_BIN_SOURCE)),$(CLICK_BIN_SOURCE),$(CLICK_BIN))
CLICK_RAYLIB_BUILD_DIR := $(VENDOR_BUILD_DIR)/click/$(CLICK_ARCH)/raylib
CLICK_RAYLIB_A := $(CLICK_RAYLIB_BUILD_DIR)/libraylib.a
CLICK_PATCHELF_INTERPRETER ?= /lib/ld-linux-aarch64.so.1
CLICK_RUNTIME_LIBS ?= $(AARCH64_CLICK_RUNTIME_LIBS)
WINDOWS_OBJ_DIR := $(BUILD_OBJ_DIR)/windows
WINDOWS_BIN_DIR := $(BUILD_BIN_DIR)/windows
WINDOWS_DIST_DIR := $(BUILD_DIST_DIR)/windows
ANDROID_BUILD_DIR := $(BUILD_DIR)/android
WEB_OBJ_DIR := $(BUILD_OBJ_DIR)/web
WEB_DIST_DIR := $(BUILD_DIST_DIR)/web
CHROME_WEB_STORE_DIR := $(BUILD_DIST_DIR)/chrome-web-store
VERSION_FILE := src/core/version.h
APP_VERSION := $(shell sed -n 's/^#define INBE_VERSION_STRING "\([^"]*\)".*/\1/p' $(VERSION_FILE) 2>/dev/null)

RAYLIB_DIR := vendor/raylib/src
RAYLIB_BUILD_DIR := $(VENDOR_BUILD_DIR)/linux/$(ARCH)/raylib
RAYLIB_A := $(RAYLIB_BUILD_DIR)/libraylib.a
WIN64_ARCH := x86_64
WIN64_CC ?= $(or $(WIN_CC),x86_64-w64-mingw32-gcc)
WIN64_AR ?= $(or $(WIN_AR),x86_64-w64-mingw32-ar)
WIN64_RANLIB ?= $(or $(WIN_RANLIB),x86_64-w64-mingw32-ranlib)
WIN64_STRIP ?= $(or $(WIN_STRIP),x86_64-w64-mingw32-strip)
WIN64_CC_PATH := $(shell command -v $(WIN64_CC) 2>/dev/null || printf '%s' $(WIN64_CC))
WIN64_AR_PATH := $(shell command -v $(WIN64_AR) 2>/dev/null || printf '%s' $(WIN64_AR))
WIN64_RANLIB_PATH := $(shell command -v $(WIN64_RANLIB) 2>/dev/null || printf '%s' $(WIN64_RANLIB))
WIN32_ARCH := i686
WIN32_CC ?= i686-w64-mingw32-gcc
WIN32_AR ?= i686-w64-mingw32-ar
WIN32_RANLIB ?= i686-w64-mingw32-ranlib
WIN32_STRIP ?= i686-w64-mingw32-strip
WIN32_CC_PATH := $(shell command -v $(WIN32_CC) 2>/dev/null || printf '%s' $(WIN32_CC))
WIN32_AR_PATH := $(shell command -v $(WIN32_AR) 2>/dev/null || printf '%s' $(WIN32_AR))
WIN32_RANLIB_PATH := $(shell command -v $(WIN32_RANLIB) 2>/dev/null || printf '%s' $(WIN32_RANLIB))
WIN64_RAYLIB_BUILD_DIR := $(VENDOR_BUILD_DIR)/windows/$(WIN64_ARCH)/raylib
WIN64_RAYLIB_A := $(WIN64_RAYLIB_BUILD_DIR)/libraylib.a
WIN32_RAYLIB_BUILD_DIR := $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/raylib
WIN32_RAYLIB_A := $(WIN32_RAYLIB_BUILD_DIR)/libraylib.a
WIN64_CURL_BUILD_DIR := $(VENDOR_BUILD_DIR)/windows/$(WIN64_ARCH)/curl
WIN64_CURL_INCLUDE_DIR := $(WIN64_CURL_BUILD_DIR)/include
WIN64_CURL_A := $(WIN64_CURL_BUILD_DIR)/lib/libcurl.a
WIN32_CURL_BUILD_DIR := $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/curl
WIN32_CURL_INCLUDE_DIR := $(WIN32_CURL_BUILD_DIR)/include
WIN32_CURL_A := $(WIN32_CURL_BUILD_DIR)/lib/libcurl.a
WEB_RAYLIB_BUILD_DIR := $(VENDOR_BUILD_DIR)/web/raylib
WEB_RAYLIB_A := $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
RAYLIB_SOURCES := $(shell find $(RAYLIB_DIR) -type f \( -name '*.c' -o -name '*.h' \))

FLINT_DIR := flint
FLINT_ICON_FILES := $(wildcard $(FLINT_DIR)/icons/*.png)
FLINT_ICON_ASSETS_C := $(FLINT_DIR)/src/flint_icon_assets.c
FLINT_SRCS := $(filter-out $(FLINT_ICON_ASSETS_C),$(wildcard $(FLINT_DIR)/src/*.c) $(wildcard $(FLINT_DIR)/src/ui/*.c)) $(FLINT_ICON_ASSETS_C)
FLINT_WEB_SRCS := $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
FLINT_WINDOWS_SRCS := $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
FLINT_INCLUDE := -I$(FLINT_DIR)/include
CURL_DIR := vendor/curl
CURL_BUILD_DIR := $(VENDOR_BUILD_DIR)/linux/$(ARCH)/curl
CURL_INCLUDE_DIR := $(CURL_BUILD_DIR)/include
CURL_LIB_DIR := $(CURL_BUILD_DIR)/lib64
CURL_SO := $(CURL_LIB_DIR)/libcurl.so
CURL_PROTOCOL_CHECK := $(CURL_BUILD_DIR)/.protocols-ok
OPENSSL_ROOT_DIR ?= $(shell pkg-config --variable=prefix openssl 2>/dev/null)
OPENSSL_CMAKE_OPT := $(if $(strip $(OPENSSL_ROOT_DIR)),-DOPENSSL_ROOT_DIR=$(OPENSSL_ROOT_DIR),)
OPENSSL_INCLUDE_CMAKE_OPT := $(if $(strip $(OPENSSL_INCLUDE_DIR)),-DOPENSSL_INCLUDE_DIR=$(OPENSSL_INCLUDE_DIR),)
OPENSSL_SSL_CMAKE_OPT := $(if $(strip $(OPENSSL_SSL_LIBRARY)),-DOPENSSL_SSL_LIBRARY=$(OPENSSL_SSL_LIBRARY),)
OPENSSL_CRYPTO_CMAKE_OPT := $(if $(strip $(OPENSSL_CRYPTO_LIBRARY)),-DOPENSSL_CRYPTO_LIBRARY=$(OPENSSL_CRYPTO_LIBRARY),)
OPENSSL_LIB_DIR := $(patsubst %/,%,$(dir $(OPENSSL_SSL_LIBRARY)))
OPENSSL_LDLIBS := $(if $(strip $(OPENSSL_SSL_LIBRARY) $(OPENSSL_CRYPTO_LIBRARY)),-Xlinker -rpath -Xlinker $(OPENSSL_LIB_DIR) $(OPENSSL_SSL_LIBRARY) $(OPENSSL_CRYPTO_LIBRARY),$(or $(shell pkg-config --libs openssl 2>/dev/null),-lssl -lcrypto))
FLINT_CURL_CFLAGS := -I$(CURL_INCLUDE_DIR)
FLINT_CURL_LDLIBS := -L$(CURL_LIB_DIR) -Wl,-rpath,$(abspath $(CURL_LIB_DIR)) -lcurl $(OPENSSL_LDLIBS)
FLINT_CURL_VERSION_NUM ?= $(shell printf '%s\n' '#include <curl/curlver.h>' 'LIBCURL_VERSION_NUM' | $(CC) -I$(CURL_DIR)/include -E -P - 2>/dev/null | tail -n 1)
FLINT_CURL_VERSION_HEX := $(patsubst 0x%,%,$(FLINT_CURL_VERSION_NUM))
SQLITE_DIR := vendor/sqlite
SQLITE_BUILD_DIR := $(VENDOR_BUILD_DIR)/sqlite
SQLITE_AMALGAMATION_C := $(SQLITE_BUILD_DIR)/sqlite3.c
SQLITE_AMALGAMATION_H := $(SQLITE_BUILD_DIR)/sqlite3.h
SQLITE_SRC := $(SQLITE_AMALGAMATION_C)
SQLITE_INCLUDE := -I$(SQLITE_BUILD_DIR)
LIBOQS_DIR := vendor/liboqs
LIBOQS_BUILD_DIR := $(VENDOR_BUILD_DIR)/linux/$(ARCH)/liboqs
LIBOQS_A := $(LIBOQS_BUILD_DIR)/lib/liboqs.a
LIBOQS_INCLUDE := -I$(LIBOQS_BUILD_DIR)/include
WEB_LIBOQS_BUILD_DIR := $(VENDOR_BUILD_DIR)/web/liboqs
WEB_LIBOQS_A := $(WEB_LIBOQS_BUILD_DIR)/lib/liboqs.a
WEB_LIBOQS_INCLUDE := -I$(WEB_LIBOQS_BUILD_DIR)/include
TEST_BIN_DIR := $(BUILD_BIN_DIR)/tests
STORAGE_IMPORT_TEST := $(TEST_BIN_DIR)/storage_import_test
LOCALE_KEYS_TEST := $(TEST_BIN_DIR)/locale_keys_test
SYNC_URL_TEST := $(TEST_BIN_DIR)/sync_url_test
SYNC_ACCOUNT_TEST := $(TEST_BIN_DIR)/sync_account_test
FLINT_RUNTIME_ASSET_CFLAGS := $(FLINT_CURL_CFLAGS)
FLINT_RUNTIME_ASSET_LDLIBS := $(FLINT_CURL_LDLIBS)

APP_SRCS := \
	src/main.c \
	src/core/breath_engine.c \
	src/app/app.c \
	src/app/app_settings.c \
	src/app/device_preferences.c \
	src/practices/practice_registry.c \
	src/practices/whm/whm_practice.c \
	src/practices/whm/whm_session.c \
	src/practices/whm/whm_manual.c \
	src/practices/whm/whm_config.c \
	src/practices/meditation/meditation_practice.c \
	src/practices/meditation/meditation_session.c \
	src/practices/meditation/meditation_manual.c \
	src/practices/meditation/meditation_config.c \
	src/screens/habits_screen.c \
	src/screens/statistics_screen.c \
	src/screens/habits/edit.c \
	src/screens/habits/session.c \
	src/practices/meditation/meditation_music.c \
	src/core/theme.c \
	src/storage/data.c \
	src/storage/db.c \
	src/storage/import.c \
	src/storage/storage.c \
	src/storage/sync_account.c \
	src/storage/sync_client.c \
	src/third_party/miniz.c \
	src/platform/android/android_device.c \
	src/screens/practice_screen.c \
	src/screens/language_screen.c \
	src/screens/manual_screen.c \
	src/screens/settings/settings_screen.c \
	src/screens/settings/settings_device.c \
	src/screens/settings/settings_theme.c \
	src/screens/settings/settings_data.c \
	src/screens/settings/settings_sync_account.c

LOCALE_FILES := $(wildcard locales/*.txt)
IMAGE_FILES := assets/practices/whm/1.jpg assets/practices/whm/2.jpg assets/practices/meditation/1.jpg
SOUND_FILES := $(wildcard assets/sounds/*.ogg)
FONT_OUTPUTS := assets/fonts/locales.png assets/fonts/locales.dat
OTFCHOP_DIR ?= vendor/otfchop
FONT_TOOL := $(OTFCHOP_DIR)/otfchop
FONT_SOURCE := $(OTFCHOP_DIR)/unifont-17.0.04.otf
EMBEDDED_ASSETS_C := $(BUILD_OBJ_DIR)/$(APP_NAME)_embedded_assets.c
EMBEDDED_ASSET_FILES := $(LOCALE_FILES) $(IMAGE_FILES) $(SOUND_FILES) $(FONT_OUTPUTS)
SRC := $(APP_SRCS) $(EMBEDDED_ASSETS_C)

APP_INCLUDE := -Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/practices -Isrc/practices/whm -Isrc/practices/meditation -Isrc/storage -Isrc/platform/android -Isrc/third_party
APP_RAYLIB_CONFIG := $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DFLINT_EMBEDDED_ONLY=1 $(FLINT_RUNTIME_ASSET_CFLAGS)
WINDOWS_CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DFLINT_EMBEDDED_ONLY=1
WEB_CFLAGS := $(filter-out -std=c99,$(CFLAGS)) -std=gnu99
CLICK_CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DFLINT_EMBEDDED_ONLY=1 $(AARCH64_FLINT_CURL_CFLAGS)
LDFLAGS := -Wl,--gc-sections -s
WINDOWS_LDFLAGS := -Wl,--gc-sections -static -static-libgcc -mwindows
WINDOWS_LDLIBS := -lgdi32 -lwinmm -lopengl32 -luser32 -lshell32 -lole32 -lcomdlg32 -lcomctl32 -luuid -lwininet -lws2_32 -liphlpapi -lcrypt32 -lsecur32 -lbcrypt -ladvapi32 -lm
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
WEB_CACHE_BUSTER ?= $(shell if git diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then git rev-parse --short HEAD 2>/dev/null; else date +%s; fi)
WEB_TARGET := $(WEB_DIST_DIR)/index.html
WEB_DIST_ZIP := $(BUILD_DIST_DIR)/$(APP_NAME)-web.zip
WEB_APP_URL ?= https://inbe.waozi.xyz/
CHROME_WEB_STORE_ZIP := $(BUILD_DIST_DIR)/$(APP_NAME)-chrome-web-store.zip
CHROME_WEB_STORE_MANIFEST := packaging/chrome-web-store/manifest.json
CHROME_WEB_STORE_WORKER := packaging/chrome-web-store/service_worker.js
CHROME_WEB_STORE_ICON := web-assets/icons/icon-512x512.png
MAGICK ?= magick
WEB_ASSET_FILES := $(shell find web-assets -type f 2>/dev/null)
UNPACKAGED_AUDIO_DIR := unpackaged_assets/audio
UNPACKAGED_AUDIO_FILES := $(shell find $(UNPACKAGED_AUDIO_DIR) -type f 2>/dev/null)
MEDITATION_AUDIO_ZIP := web-assets/dl/inbe-meditation-audio-v1.zip

.PHONY: all native run screenshot test dist appimage click click-verify vendor-prebuilds vendor-prebuilds-native vendor-prebuilds-web vendor-prebuilds-windows clean clean-linux clean-vendor-builds android-check-keystore android-copy-assets android-local-properties android-debug android-release android-bundle android-install android-install-release android-clean package-unpackaged-assets windows-runtime-assets-check windows windows64 windows32 web chrome-web-store
.NOTPARALLEL: dist windows windows64 windows32 android-release android-bundle click

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
	$(MAKE) chrome-web-store && \
	$(MAKE) click && \
	$(MAKE) appimage && \
	$(MAKE) windows && \
	$(MAKE) android-release PASSWORD="$$password" && \
	$(MAKE) android-bundle PASSWORD="$$password"

appimage: $(APPIMAGE_TARGET)

click: $(CLICK_TARGET)

click-verify: $(CLICK_TARGET)
	@command -v clickable >/dev/null || { \
		echo "clickable is missing. Re-enter the flake shell with: nix develop"; \
		exit 1; \
	}
	clickable review $(CLICK_TARGET)

vendor-prebuilds: vendor-prebuilds-native vendor-prebuilds-web vendor-prebuilds-windows

vendor-prebuilds-native: $(RAYLIB_A) $(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H) $(LIBOQS_A) $(CURL_PROTOCOL_CHECK)

vendor-prebuilds-web: $(WEB_RAYLIB_A) $(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H) $(WEB_LIBOQS_A)

vendor-prebuilds-windows: $(WIN64_RAYLIB_A) $(WIN32_RAYLIB_A) $(WIN64_CURL_A) $(WIN32_CURL_A) $(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H)

run: $(TARGET)
	./$(TARGET)

screenshot: $(TARGET)
	./scripts/generate-screenshots.sh "$(TARGET)"

test: $(STORAGE_IMPORT_TEST) $(LOCALE_KEYS_TEST) $(SYNC_URL_TEST) $(SYNC_ACCOUNT_TEST)
	$(STORAGE_IMPORT_TEST)
	$(LOCALE_KEYS_TEST)
	$(SYNC_URL_TEST)
	$(SYNC_ACCOUNT_TEST)

$(STORAGE_IMPORT_TEST): tests/storage_import_test.c src/storage/storage.c src/storage/db.c src/storage/import.c src/storage/storage.h src/storage/db.h src/storage/import.h src/screens/habits_screen.c src/screens/habits/edit.c src/screens/habits/session.c src/screens/habits_screen.h src/screens/habits/habits.h src/third_party/miniz.c src/third_party/miniz.h $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -ffunction-sections -fdata-sections \
		-Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/practices -Isrc/practices/whm -Isrc/practices/meditation -Isrc/storage -Isrc/platform/android -Isrc/third_party -Ivendor/raylib/src $(FLINT_INCLUDE) $(SQLITE_INCLUDE) \
		-o $@ \
		tests/storage_import_test.c src/storage/storage.c src/storage/db.c src/storage/import.c src/screens/habits_screen.c src/screens/habits/edit.c src/screens/habits/session.c src/third_party/miniz.c $(SQLITE_SRC) \
		-Wl,--gc-sections -lm -lpthread -ldl

$(LOCALE_KEYS_TEST): tests/locale_keys_test.c $(LOCALE_FILES) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE \
		-o $@ \
		tests/locale_keys_test.c

$(SYNC_URL_TEST): tests/sync_url_test.c src/storage/sync_client.c src/storage/sync_client.h $(CURL_PROTOCOL_CHECK) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -Wno-unused-function -std=c99 -D_DEFAULT_SOURCE -DINBE_SYNC_CLIENT_TESTS -ffunction-sections -fdata-sections \
		-Isrc/storage -Isrc -Ivendor/raylib/src $(FLINT_CURL_CFLAGS) -o $@ \
		tests/sync_url_test.c src/storage/sync_client.c \
		-Wl,--gc-sections $(FLINT_CURL_LDLIBS)

$(SYNC_ACCOUNT_TEST): tests/sync_account_test.c src/storage/sync_account.c src/storage/sync_account.h src/storage/storage.c src/storage/db.c src/storage/import.c src/storage/storage.h src/storage/db.h src/storage/import.h src/third_party/miniz.c src/third_party/miniz.h $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -ffunction-sections -fdata-sections \
		-Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/practices -Isrc/practices/whm -Isrc/practices/meditation -Isrc/storage -Isrc/platform/android -Isrc/third_party -Ivendor/raylib/src $(SQLITE_INCLUDE) \
		-o $@ \
		tests/sync_account_test.c src/storage/sync_account.c src/storage/storage.c src/storage/db.c src/storage/import.c src/third_party/miniz.c $(SQLITE_SRC) \
		-Wl,--gc-sections -lm -lpthread -ldl

$(BUILD_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR) $(LINUX_APPIMAGE_BUILD_DIR) $(CLICK_BIN_DIR) $(CLICK_BUILD_DIR) $(CLICK_DIST_DIR) $(WINDOWS_DIST_DIR) $(ANDROID_BUILD_DIR) $(TEST_BIN_DIR) $(WEB_OBJ_DIR) $(WEB_DIST_DIR) $(CHROME_WEB_STORE_DIR):
	mkdir -p $@

$(WINDOWS_BIN_DIR)/$(WIN64_ARCH) $(WINDOWS_BIN_DIR)/$(WIN32_ARCH):
	mkdir -p $@

assets/fonts:
	mkdir -p $@

$(FONT_TOOL): vendor/otfchop/otfchop.c vendor/otfchop/stb_truetype.h vendor/otfchop/stb_image_write.h
	$(MAKE) -C vendor/otfchop otfchop

assets/fonts/locales.png assets/fonts/locales.dat: $(LOCALE_FILES) $(FONT_TOOL) | assets/fonts
	$(FONT_TOOL) $(FONT_SOURCE) $(LOCALE_FILES) assets/fonts/locales

$(EMBEDDED_ASSETS_C): $(EMBEDDED_ASSET_FILES) $(FLINT_DIR)/scripts/embed-assets.sh | $(BUILD_OBJ_DIR)
	sh $(FLINT_DIR)/scripts/embed-assets.sh $@ $(EMBEDDED_ASSET_FILES)

$(FLINT_ICON_ASSETS_C): $(FLINT_ICON_FILES) $(FLINT_DIR)/scripts/embed-icons.sh
	sh $(FLINT_DIR)/scripts/embed-icons.sh $(FLINT_DIR)/icons $@

$(RAYLIB_A): $(RAYLIB_SOURCES)
	rm -rf $(VENDOR_BUILD_DIR)/linux/$(ARCH)/raylib-src
	mkdir -p $(VENDOR_BUILD_DIR)/linux/$(ARCH)/raylib-src $(RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(VENDOR_BUILD_DIR)/linux/$(ARCH)/raylib-src/
	$(MAKE) -j1 -C $(VENDOR_BUILD_DIR)/linux/$(ARCH)/raylib-src \
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
	rm -rf $(VENDOR_BUILD_DIR)/web/raylib-src
	mkdir -p $(VENDOR_BUILD_DIR)/web/raylib-src $(WEB_RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(VENDOR_BUILD_DIR)/web/raylib-src/
	$(MAKE) -j1 -C $(VENDOR_BUILD_DIR)/web/raylib-src \
		PLATFORM=PLATFORM_WEB \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		CC="$(WEB_CC)" \
		AR="$(WEB_AR)" \
		CUSTOM_CFLAGS="$(APP_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

$(CLICK_RAYLIB_A): $(RAYLIB_SOURCES)
	rm -rf $(VENDOR_BUILD_DIR)/click/$(CLICK_ARCH)/raylib-src
	mkdir -p $(VENDOR_BUILD_DIR)/click/$(CLICK_ARCH)/raylib-src $(CLICK_RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(VENDOR_BUILD_DIR)/click/$(CLICK_ARCH)/raylib-src/
	$(MAKE) -j1 -C $(VENDOR_BUILD_DIR)/click/$(CLICK_ARCH)/raylib-src \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		CC="$(AARCH64_CC)" \
		AR="$(AARCH64_AR)" \
		RANLIB="$(AARCH64_RANLIB)" \
		SDL_INCLUDE_PATH="$(AARCH64_RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(AARCH64_RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(AARCH64_RAY_CFLAGS) $(APP_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

$(WIN64_RAYLIB_A): $(RAYLIB_SOURCES)
	rm -rf $(VENDOR_BUILD_DIR)/windows/$(WIN64_ARCH)/raylib-src
	mkdir -p $(VENDOR_BUILD_DIR)/windows/$(WIN64_ARCH)/raylib-src $(WIN64_RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(VENDOR_BUILD_DIR)/windows/$(WIN64_ARCH)/raylib-src/
	$(MAKE) -j1 -C $(VENDOR_BUILD_DIR)/windows/$(WIN64_ARCH)/raylib-src \
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
	rm -rf $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/raylib-src
	mkdir -p $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/raylib-src $(WIN32_RAYLIB_BUILD_DIR)
	cp -R $(RAYLIB_DIR)/. $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/raylib-src/
	$(MAKE) -j1 -C $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/raylib-src \
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
	cd $(SQLITE_BUILD_DIR) && $(abspath $(SQLITE_DIR))/configure
	$(MAKE) -C $(SQLITE_BUILD_DIR) sqlite3.c sqlite3.h

$(LIBOQS_A): $(LIBOQS_DIR)/CMakeLists.txt | $(BUILD_OBJ_DIR)
	$(CMAKE) -S $(LIBOQS_DIR) -B $(LIBOQS_BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=MinSizeRel \
		-DBUILD_SHARED_LIBS=OFF \
		-DOQS_BUILD_ONLY_LIB=ON \
		-DOQS_USE_OPENSSL=OFF \
		-DOQS_MINIMAL_BUILD=SIG_ml_dsa_44
	$(CMAKE) --build $(LIBOQS_BUILD_DIR) --target oqs

$(WEB_LIBOQS_A): $(LIBOQS_DIR)/CMakeLists.txt | $(BUILD_OBJ_DIR)
	emcmake $(CMAKE) -S $(LIBOQS_DIR) -B $(WEB_LIBOQS_BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=MinSizeRel \
		-DBUILD_SHARED_LIBS=OFF \
		-DOQS_BUILD_ONLY_LIB=ON \
		-DOQS_USE_OPENSSL=OFF \
		-DOQS_DIST_BUILD=OFF \
		-DOQS_OPT_TARGET=generic \
		-DOQS_MINIMAL_BUILD=SIG_ml_dsa_44
	$(CMAKE) --build $(WEB_LIBOQS_BUILD_DIR) --target oqs

$(CURL_SO): $(CURL_DIR)/CMakeLists.txt
	$(CMAKE) -S $(CURL_DIR) -B $(CURL_BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=MinSizeRel \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(CURL_BUILD_DIR)) \
		-DCMAKE_INSTALL_LIBDIR=lib64 \
		-DBUILD_CURL_EXE=OFF \
		-DBUILD_SHARED_LIBS=ON \
		-DBUILD_STATIC_LIBS=OFF \
		-DCURL_USE_OPENSSL=ON \
		$(OPENSSL_CMAKE_OPT) \
		$(OPENSSL_INCLUDE_CMAKE_OPT) \
		$(OPENSSL_SSL_CMAKE_OPT) \
		$(OPENSSL_CRYPTO_CMAKE_OPT) \
		-DCURL_DISABLE_WEBSOCKETS=OFF \
		-DCURL_DISABLE_INSTALL=OFF \
		-DCURL_DISABLE_LDAP=ON \
		-DCURL_DISABLE_LDAPS=ON \
		-DCURL_DISABLE_DICT=ON \
		-DCURL_DISABLE_FILE=ON \
		-DCURL_DISABLE_FTP=ON \
		-DCURL_DISABLE_GOPHER=ON \
		-DCURL_DISABLE_IMAP=ON \
		-DCURL_DISABLE_MQTT=ON \
		-DCURL_DISABLE_POP3=ON \
		-DCURL_DISABLE_RTSP=ON \
		-DCURL_DISABLE_SMB=ON \
		-DCURL_DISABLE_SMTP=ON \
		-DCURL_DISABLE_TELNET=ON \
		-DCURL_DISABLE_TFTP=ON \
		-DCURL_DISABLE_LIBCURL_OPTION=ON \
		-DCURL_ZLIB=OFF \
		-DCURL_BROTLI=OFF \
		-DCURL_ZSTD=OFF \
		-DCURL_USE_LIBPSL=OFF \
		-DCURL_USE_LIBSSH2=OFF \
		-DCURL_USE_GSSAPI=OFF \
		-DUSE_NGHTTP2=OFF \
		-DUSE_LIBIDN2=OFF \
		-DENABLE_CURL_MANUAL=OFF \
		-DBUILD_EXAMPLES=OFF \
		-DBUILD_LIBCURL_DOCS=OFF \
		-DBUILD_MISC_DOCS=OFF \
		-DBUILD_TESTING=OFF
	$(CMAKE) --build $(CURL_BUILD_DIR) --target install

$(CURL_PROTOCOL_CHECK): $(CURL_SO)
	@$(CURL_BUILD_DIR)/bin/curl-config --protocols | grep -Eq '(^|[[:space:]])WS([[:space:]]|$$)' || { echo "vendored libcurl was built without WS protocol support"; exit 1; }
	@$(CURL_BUILD_DIR)/bin/curl-config --protocols | grep -Eq '(^|[[:space:]])WSS([[:space:]]|$$)' || { echo "vendored libcurl was built without WSS protocol support"; exit 1; }
	@touch $@

$(WIN64_CURL_A): $(CURL_DIR)/CMakeLists.txt
	rm -rf $(WIN64_CURL_BUILD_DIR)
	$(CMAKE) -S $(CURL_DIR) -B $(WIN64_CURL_BUILD_DIR) \
		-DCMAKE_SYSTEM_NAME=Windows \
		-DCMAKE_C_COMPILER=$(WIN64_CC_PATH) \
		-DCMAKE_AR=$(WIN64_AR_PATH) \
		-DCMAKE_RANLIB=$(WIN64_RANLIB_PATH) \
		-DCMAKE_EXE_LINKER_FLAGS="$(WIN64_THREAD_LDFLAGS)" \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(WIN64_CURL_BUILD_DIR)) \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_STATIC_LIBS=ON \
		-DBUILD_CURL_EXE=OFF \
		-DCURL_STATICLIB=ON \
		-DCURL_USE_SCHANNEL=ON \
		-DCURL_USE_OPENSSL=OFF \
		-DCURL_USE_LIBPSL=OFF \
		-DCURL_USE_LIBSSH2=OFF \
		-DCURL_USE_GSSAPI=OFF \
		-DUSE_NGHTTP2=OFF \
		-DUSE_LIBIDN2=OFF \
		-DCURL_DISABLE_LDAP=ON \
		-DCURL_DISABLE_LDAPS=ON \
		-DCURL_DISABLE_SMB=ON \
		-DENABLE_CURL_MANUAL=OFF \
		-DBUILD_EXAMPLES=OFF \
		-DBUILD_LIBCURL_DOCS=OFF \
		-DBUILD_MISC_DOCS=OFF \
		-DBUILD_TESTING=OFF
	$(CMAKE) --build $(WIN64_CURL_BUILD_DIR) --target install

$(WIN32_CURL_A): $(CURL_DIR)/CMakeLists.txt
	rm -rf $(WIN32_CURL_BUILD_DIR)
	$(CMAKE) -S $(CURL_DIR) -B $(WIN32_CURL_BUILD_DIR) \
		-DCMAKE_SYSTEM_NAME=Windows \
		-DCMAKE_C_COMPILER=$(WIN32_CC_PATH) \
		-DCMAKE_AR=$(WIN32_AR_PATH) \
		-DCMAKE_RANLIB=$(WIN32_RANLIB_PATH) \
		-DCMAKE_EXE_LINKER_FLAGS="$(WIN32_THREAD_LDFLAGS)" \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(WIN32_CURL_BUILD_DIR)) \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_STATIC_LIBS=ON \
		-DBUILD_CURL_EXE=OFF \
		-DCURL_STATICLIB=ON \
		-DCURL_USE_SCHANNEL=ON \
		-DCURL_USE_OPENSSL=OFF \
		-DCURL_USE_LIBPSL=OFF \
		-DCURL_USE_LIBSSH2=OFF \
		-DCURL_USE_GSSAPI=OFF \
		-DUSE_NGHTTP2=OFF \
		-DUSE_LIBIDN2=OFF \
		-DCURL_DISABLE_LDAP=ON \
		-DCURL_DISABLE_LDAPS=ON \
		-DCURL_DISABLE_SMB=ON \
		-DENABLE_CURL_MANUAL=OFF \
		-DBUILD_EXAMPLES=OFF \
		-DBUILD_LIBCURL_DOCS=OFF \
		-DBUILD_MISC_DOCS=OFF \
		-DBUILD_TESTING=OFF
	$(CMAKE) --build $(WIN32_CURL_BUILD_DIR) --target install

$(TARGET): Makefile $(SRC) $(FLINT_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(RAYLIB_A) $(LIBOQS_A) $(CURL_PROTOCOL_CHECK) | $(LINUX_BIN_DIR)
	$(CC) $(CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(LIBOQS_INCLUDE) \
		-I$(RAYLIB_DIR) \
		$(RAY_CFLAGS) \
		-DINBE_HAS_LIBOQS=1 \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=1 \
		-o $@ \
		$(SRC) \
		$(FLINT_SRCS) \
		$(SQLITE_SRC) \
		$(RAYLIB_A) \
		$(LIBOQS_A) \
		$(RAY_LDLIBS) \
		$(FLINT_RUNTIME_ASSET_LDLIBS) \
		-lm -lpthread -ldl -lrt \
		$(LDFLAGS)

$(CLICK_BIN): Makefile $(SRC) $(FLINT_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(CLICK_RAYLIB_A) | $(CLICK_BIN_DIR)
	$(AARCH64_CC) $(CLICK_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		$(AARCH64_RAY_CFLAGS) \
		-DPLATFORM_DESKTOP \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=1 \
		-o $@ \
		$(SRC) \
		$(FLINT_SRCS) \
		$(SQLITE_SRC) \
		$(CLICK_RAYLIB_A) \
		$(AARCH64_RAY_LDLIBS) \
		$(AARCH64_FLINT_CURL_LDLIBS) \
		-lm -lpthread -ldl -lrt \
		$(LDFLAGS)
	@if command -v patchelf >/dev/null; then \
		patchelf --set-interpreter "$(CLICK_PATCHELF_INTERPRETER)" --set-rpath '$$ORIGIN/../lib' $@; \
	fi

$(CLICK_TARGET): $(CLICK_BIN_INPUT) $(CLICK_MANIFEST_TEMPLATE) $(CLICK_CONTROL_TEMPLATE) $(CLICK_APPARMOR_TEMPLATE) $(CLICK_DESKTOP_TEMPLATE) $(CLICK_METAINFO_TEMPLATE) $(CLICK_RUNNER) $(LINUX_APPIMAGE_ICON) | $(CLICK_BUILD_DIR) $(CLICK_DIST_DIR)
	@command -v ar >/dev/null || { \
		echo "ar is missing. Re-enter the flake shell with: nix develop"; \
		exit 1; \
	}
	rm -rf $(CLICK_ROOT) $(CLICK_CONTROL_DIR)
	rm -f $(CLICK_DIST_DIR)/$(CLICK_PACKAGE)_*_$(CLICK_ARCH).click
	rm -f $(CLICK_BUILD_DIR)/debian-binary $(CLICK_BUILD_DIR)/control.tar.gz $(CLICK_BUILD_DIR)/data.tar.gz
	mkdir -p $(CLICK_ROOT)/usr/bin $(CLICK_ROOT)/usr/lib $(CLICK_ROOT)/usr/share/applications $(CLICK_ROOT)/usr/share/icons/hicolor/512x512/apps $(CLICK_ROOT)/usr/share/metainfo
	mkdir -p $(CLICK_CONTROL_DIR)
	cp $(CLICK_BIN_INPUT) $(CLICK_ROOT)/usr/bin/$(APP_NAME)
	cp $(CLICK_RUNNER) $(CLICK_ROOT)/run-inbe.sh
	chmod +x $(CLICK_ROOT)/run-inbe.sh $(CLICK_ROOT)/usr/bin/$(APP_NAME)
	@for lib in $(CLICK_RUNTIME_LIBS); do \
		if [ -f "$$lib" ]; then \
			cp -L "$$lib" $(CLICK_ROOT)/usr/lib/; \
		fi; \
	done
	@if command -v patchelf >/dev/null; then \
		for elf in $(CLICK_ROOT)/usr/lib/*.so*; do \
			if [ -f "$$elf" ]; then patchelf --set-rpath '$$ORIGIN' "$$elf" >/dev/null 2>&1 || true; fi; \
		done; \
	fi
	sed -e 's#__CLICK_PACKAGE__#$(CLICK_PACKAGE)#g' \
		-e 's#__CLICK_TITLE__#$(CLICK_TITLE)#g' \
		-e 's#__APP_VERSION__#$(APP_VERSION)#g' \
		-e 's#__CLICK_ARCH__#$(CLICK_ARCH)#g' \
		-e 's#__CLICK_FRAMEWORK__#$(CLICK_FRAMEWORK)#g' \
		-e 's#__CLICK_MAINTAINER__#$(CLICK_MAINTAINER)#g' \
		$(CLICK_MANIFEST_TEMPLATE) > $(CLICK_ROOT)/manifest.json
	cp $(CLICK_ROOT)/manifest.json $(CLICK_CONTROL_DIR)/manifest
	installed_size=$$(du -sk $(CLICK_ROOT) | awk '{ print $$1 }'); \
	sed -e 's#__CLICK_PACKAGE__#$(CLICK_PACKAGE)#g' \
		-e 's#__APP_VERSION__#$(APP_VERSION)#g' \
		-e 's#__CLICK_ARCH__#$(CLICK_ARCH)#g' \
		-e 's#__CLICK_TITLE__#$(CLICK_TITLE)#g' \
		-e 's#__CLICK_MAINTAINER__#$(CLICK_MAINTAINER)#g' \
		-e "s#__INSTALLED_SIZE__#$$installed_size#g" \
		$(CLICK_CONTROL_TEMPLATE) > $(CLICK_CONTROL_DIR)/control
	printf '%s\n' '#! /bin/sh' \
		'echo "Click packages may not be installed directly using dpkg."' \
		'echo "Use '\''click install'\'' instead."' \
		'exit 1' > $(CLICK_CONTROL_DIR)/preinst
	sed -e 's#__CLICK_POLICY_VERSION__#$(CLICK_POLICY_VERSION)#g' \
		$(CLICK_APPARMOR_TEMPLATE) > $(CLICK_ROOT)/inbe.apparmor
	sed -e 's#__CLICK_TITLE__#$(CLICK_TITLE)#g' \
		$(CLICK_DESKTOP_TEMPLATE) > $(CLICK_ROOT)/inbe.desktop
	@if [ "$(CLICK_INCLUDE_METAINFO)" = "1" ]; then \
		sed -e 's#__CLICK_PACKAGE__#$(CLICK_PACKAGE)#g' \
			-e 's#__APP_VERSION__#$(APP_VERSION)#g' \
			-e 's#__RELEASE_DATE__#$(shell date -u +%Y-%m-%d)#g' \
			$(CLICK_METAINFO_TEMPLATE) > $(CLICK_ROOT)/usr/share/metainfo/$(CLICK_PACKAGE).metainfo.xml; \
	fi
	cp $(LINUX_APPIMAGE_ICON) $(CLICK_ROOT)/inbe.png
	cp $(LINUX_APPIMAGE_ICON) $(CLICK_ROOT)/usr/share/icons/hicolor/512x512/apps/inbe.png
	cd $(CLICK_ROOT) && find . -type f -printf '%P\n' | LC_ALL=C sort | xargs md5sum > $(abspath $(CLICK_CONTROL_DIR)/md5sums)
	printf '2.0\n' > $(CLICK_BUILD_DIR)/debian-binary
	tar -C $(CLICK_CONTROL_DIR) --sort=name --owner=0 --group=0 --numeric-owner -czf $(abspath $(CLICK_BUILD_DIR)/control.tar.gz) control manifest md5sums preinst
	tar -C $(CLICK_ROOT) --sort=name --owner=0 --group=0 --numeric-owner -czf $(abspath $(CLICK_BUILD_DIR)/data.tar.gz) inbe.apparmor inbe.desktop inbe.png manifest.json run-inbe.sh usr
	cd $(CLICK_BUILD_DIR) && ar rcs $(abspath $@) debian-binary control.tar.gz data.tar.gz
	test -f $@
	ar t $@ | grep -qx debian-binary
	ar t $@ | grep -qx control.tar.gz
	ar t $@ | grep -qx data.tar.gz

$(WIN64_TARGET): Makefile $(SRC) $(FLINT_WINDOWS_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(WIN64_RAYLIB_A) $(WIN64_CURL_A) | $(WINDOWS_BIN_DIR)/$(WIN64_ARCH)
	$(WIN64_CC) $(WINDOWS_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		-I$(WIN64_CURL_INCLUDE_DIR) \
		-I$(RAYLIB_DIR) \
		-DPLATFORM_DESKTOP \
		-DCURL_STATICLIB \
		-o $@ \
		$(SRC) \
		$(FLINT_WINDOWS_SRCS) \
		$(SQLITE_SRC) \
		$(WIN64_RAYLIB_A) \
		$(WIN64_CURL_A) \
		$(WINDOWS_LDLIBS) \
		$(WIN64_THREAD_LDFLAGS) \
		$(WINDOWS_LDFLAGS)
	$(WIN64_STRIP) $@

$(WIN32_TARGET): Makefile $(SRC) $(FLINT_WINDOWS_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(WIN32_RAYLIB_A) $(WIN32_CURL_A) | $(WINDOWS_BIN_DIR)/$(WIN32_ARCH)
	$(WIN32_CC) $(WINDOWS_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		-I$(WIN32_CURL_INCLUDE_DIR) \
		-I$(RAYLIB_DIR) \
		-DPLATFORM_DESKTOP \
		-DCURL_STATICLIB \
		-o $@ \
		$(SRC) \
		$(FLINT_WINDOWS_SRCS) \
		$(SQLITE_SRC) \
		$(WIN32_RAYLIB_A) \
		$(WIN32_CURL_A) \
		$(WINDOWS_LDLIBS) \
		$(WIN32_THREAD_LDFLAGS) \
		$(WINDOWS_LDFLAGS)
	$(WIN32_STRIP) $@

$(APPIMAGE_TARGET): $(TARGET) $(LINUX_APPIMAGE_APPRUN) $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPIMAGE_ICON) $(LINUX_APPIMAGE_APPDATA) | $(LINUX_DIST_DIR) $(LINUX_APPIMAGE_BUILD_DIR)
	@command -v linuxdeploy-plugin-appimage >/dev/null || { \
		echo "linuxdeploy-plugin-appimage is missing. Re-enter the flake shell with: nix develop"; \
		exit 1; \
	}
	@command -v appimagetool >/dev/null || { \
		echo "appimagetool is missing. Re-enter the flake shell with: nix develop"; \
		exit 1; \
	}
	@command -v patchelf >/dev/null || { \
		echo "patchelf is missing. Re-enter the flake shell with: nix develop"; \
		exit 1; \
	}
	rm -rf $(LINUX_APPDIR)
	rm -rf $(LINUX_DIST_DIR)/*.AppDir
	rm -f $(LINUX_DIST_DIR)/*.AppImage
	mkdir -p $(LINUX_APPDIR)/usr/bin $(LINUX_APPDIR)/usr/lib $(LINUX_APPDIR)/usr/share/applications $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps $(LINUX_APPDIR)/usr/share/metainfo
	cp $(TARGET) $(LINUX_APPDIR)/usr/bin/$(APP_NAME)
	patchelf --set-interpreter /lib64/ld-linux-x86-64.so.2 $(LINUX_APPDIR)/usr/bin/$(APP_NAME)
	cp $(LINUX_APPIMAGE_APPRUN) $(LINUX_APPDIR)/AppRun
	chmod +x $(LINUX_APPDIR)/AppRun
	@loader=$$(LC_ALL=C readelf -l $(TARGET) | sed -n 's#.*Requesting program interpreter: \(.*\)]#\1#p'); \
	if printf '%s\n' "$$loader" | grep -q '^/nix/store/.*glibc.*/'; then \
		glibc_lib_dir=$$(dirname "$$loader"); \
		echo "Bundling glibc loader: $$loader"; \
		cp "$$loader" $(LINUX_APPDIR)/usr/lib/; \
		for lib in libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libresolv.so.2 libnss_files.so.2; do \
			if [ -f "$$glibc_lib_dir/$$lib" ]; then \
				cp "$$glibc_lib_dir/$$lib" $(LINUX_APPDIR)/usr/lib/; \
				chmod u+w $(LINUX_APPDIR)/usr/lib/$$lib; \
			fi; \
		done; \
		chmod u+w $(LINUX_APPDIR)/usr/lib/$$(basename "$$loader"); \
	else \
		echo "Not bundling glibc; build interpreter is $$loader"; \
	fi
	cp $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPDIR)/$(APP_NAME).desktop
	cp $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPDIR)/usr/share/applications/$(APP_NAME).desktop
	cp $(LINUX_APPIMAGE_APPDATA) $(LINUX_APPDIR)/usr/share/metainfo/$(APP_NAME).appdata.xml
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/$(APP_NAME).png
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png
	cd $(LINUX_APPIMAGE_BUILD_DIR) && env -u SOURCE_DATE_EPOCH ARCH=$(ARCH) LDAI_OUTPUT=$(abspath $(APPIMAGE_TARGET)) $(LINUXDEPLOY) \
		--appdir $(APP_NAME).AppDir \
		--executable $(abspath $(LINUX_APPDIR)/usr/bin/$(APP_NAME)) \
		--desktop-file $(abspath $(LINUX_APPIMAGE_DESKTOP)) \
		--icon-file $(abspath $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png) \
		--output appimage
	test -f $@

$(WEB_TARGET): Makefile $(SRC) $(FLINT_WEB_SRCS) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_OUTPUTS) $(EMBEDDED_ASSETS_C) $(WEB_RAYLIB_A) $(WEB_LIBOQS_A) src/web_shell.html manifest.json $(WEB_ASSET_FILES) $(MEDITATION_AUDIO_ZIP) | $(WEB_DIST_DIR)
	rm -f $(WEB_DIST_DIR)/index.data
	$(WEB_CC) $(WEB_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(WEB_LIBOQS_INCLUDE) \
		-I$(RAYLIB_DIR) \
		-DINBE_HAS_LIBOQS=1 \
		-DPLATFORM_WEB \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=1 \
		-o $@ \
		$(SRC) \
		$(FLINT_WEB_SRCS) \
		$(SQLITE_SRC) \
		$(WEB_RAYLIB_A) \
		$(WEB_LIBOQS_A) \
		-sUSE_GLFW=3 \
		-sASYNCIFY \
		-sFORCE_FILESYSTEM=1 \
		-sFETCH=1 \
		-sALLOW_MEMORY_GROWTH=1 \
		-sSTACK_SIZE=8388608 \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_app_web_get_play_in_background,_app_web_set_backgrounded,_app_web_background_tick \
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

android-local-properties:
	@if [ -f droid/local.properties ]; then sed -i '/^ndk\.dir=/d' droid/local.properties; fi
	@if [ -d /mnt/storage/Android/Sdk/ndk/28.2.13676358 ]; then \
		if [ -f droid/local.properties ]; then \
			sed -i 's#^sdk\.dir=.*#sdk.dir=/mnt/storage/Android/Sdk#' droid/local.properties; \
			sed -i 's#^cmake\.dir=.*#cmake.dir=/mnt/storage/Android/Sdk/cmake/3.22.1#' droid/local.properties; \
		else \
			printf 'sdk.dir=/mnt/storage/Android/Sdk\ncmake.dir=/mnt/storage/Android/Sdk/cmake/3.22.1\n' > droid/local.properties; \
		fi; \
	fi

android-debug: android-copy-assets android-local-properties
	unset ANDROID_HOME; $(GRADLE) -p droid assembleDebug
	$(MAKE) android-copy-debug-apks

android-release:
	$(MAKE) android-check-keystore PASSWORD="$(PASSWORD)"
	$(MAKE) android-copy-assets
	$(MAKE) android-local-properties
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
	$(MAKE) android-local-properties
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

$(MEDITATION_AUDIO_ZIP): $(UNPACKAGED_AUDIO_FILES)
	mkdir -p $(dir $(MEDITATION_AUDIO_ZIP))
	rm -f $(MEDITATION_AUDIO_ZIP)
	cd $(UNPACKAGED_AUDIO_DIR) && find . -mindepth 2 -type f -name '*.ogg' -exec zip -9 -r $(abspath $(MEDITATION_AUDIO_ZIP)) {} + && zip -9 -r $(abspath $(MEDITATION_AUDIO_ZIP)) LICENSE.md MANIFEST.txt

package-unpackaged-assets: $(MEDITATION_AUDIO_ZIP)

windows-runtime-assets-check:
	@:

windows64: windows-runtime-assets-check $(WIN64_TARGET)

windows32: windows-runtime-assets-check $(WIN32_TARGET)

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
	rm -f $(WEB_DIST_ZIP)
	cd $(WEB_DIST_DIR) && zip -9 -r $(abspath $(WEB_DIST_ZIP)) .

chrome-web-store: $(CHROME_WEB_STORE_ZIP)

$(CHROME_WEB_STORE_ZIP): $(WEB_TARGET) $(CHROME_WEB_STORE_MANIFEST) $(CHROME_WEB_STORE_WORKER) $(CHROME_WEB_STORE_ICON) | $(CHROME_WEB_STORE_DIR)
	rm -rf $(CHROME_WEB_STORE_DIR)
	mkdir -p $(CHROME_WEB_STORE_DIR)/icons
	cp -R $(WEB_DIST_DIR)/. $(CHROME_WEB_STORE_DIR)/
	sed -e 's#__APP_VERSION__#$(APP_VERSION)#g' \
		$(CHROME_WEB_STORE_MANIFEST) > $(CHROME_WEB_STORE_DIR)/manifest.json
	cp $(CHROME_WEB_STORE_WORKER) $(CHROME_WEB_STORE_DIR)/service_worker.js
	$(MAGICK) $(CHROME_WEB_STORE_ICON) -filter point -resize 16x16 $(CHROME_WEB_STORE_DIR)/icons/icon-16.png
	$(MAGICK) $(CHROME_WEB_STORE_ICON) -filter point -resize 32x32 $(CHROME_WEB_STORE_DIR)/icons/icon-32.png
	$(MAGICK) $(CHROME_WEB_STORE_ICON) -filter point -resize 48x48 $(CHROME_WEB_STORE_DIR)/icons/icon-48.png
	$(MAGICK) $(CHROME_WEB_STORE_ICON) -filter point -resize 128x128 $(CHROME_WEB_STORE_DIR)/icons/icon-128.png
	rm -f $(CHROME_WEB_STORE_ZIP)
	cd $(CHROME_WEB_STORE_DIR) && zip -9 -r $(abspath $(CHROME_WEB_STORE_ZIP)) .

clean:
	rm -rf build

clean-linux:
	rm -rf $(LINUX_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR)

clean-vendor-builds:
	rm -rf $(VENDOR_BUILD_DIR)

NEEDS_NATIVE_ENV := $(if $(MAKECMDGOALS),$(filter all native run dist appimage vendor-prebuilds vendor-prebuilds-native,$(MAKECMDGOALS)),native)
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
ifeq ($(strip $(FLINT_CURL_LDLIBS)),)
$(error libcurl metadata is missing. Sync is required; enter the flake shell with 'nix develop' or set FLINT_CURL_CFLAGS/FLINT_CURL_LDLIBS explicitly)
endif
ifneq ($(shell v='$(FLINT_CURL_VERSION_HEX)'; if [ "$$v" = 075600 ] || [ "$$v" \> 075600 ]; then echo yes; fi),yes)
$(error libcurl >= 7.86.0 is required for websocket sync; found LIBCURL_VERSION_NUM=$(FLINT_CURL_VERSION_NUM))
endif
endif

NEEDS_CLICK_ENV := $(filter click click-verify,$(MAKECMDGOALS))
ifneq ($(strip $(NEEDS_CLICK_ENV)),)
ifeq ($(strip $(CLICK_BIN_SOURCE)),)
ifeq ($(strip $(AARCH64_CC)),)
$(error AARCH64_CC is not set. Enter the ray flake shell with 'nix develop' or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_AR)),)
$(error AARCH64_AR is not set. Enter the ray flake shell with 'nix develop' or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RANLIB)),)
$(error AARCH64_RANLIB is not set. Enter the ray flake shell with 'nix develop' or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RAY_CFLAGS)),)
$(error AARCH64_RAY_CFLAGS is not set. Enter the ray flake shell with 'nix develop' or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RAY_LDLIBS)),)
$(error AARCH64_RAY_LDLIBS is not set. Enter the ray flake shell with 'nix develop' or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RAY_SDL_INCLUDE_DIR)),)
$(error AARCH64_RAY_SDL_INCLUDE_DIR is not set. Enter the ray flake shell with 'nix develop' or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_FLINT_CURL_LDLIBS)),)
$(error AARCH64_FLINT_CURL_LDLIBS is not set. Enter the ray flake shell with 'nix develop' or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
endif
endif
