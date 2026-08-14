.DEFAULT_GOAL := all

APP_NAME := inbe
APP_TITLE := Inner Breeze
ANDROID_APP_ID := xyz.waozi.inbe
ANDROID_ACTIVITY := xyz.waozi.inbe.MainActivity

CC ?= cc
CMAKE ?= $(shell if [ -x /usr/bin/cmake ]; then echo /usr/bin/cmake; else command -v cmake; fi)
GRADLE ?= droid/gradlew
ADB ?= adb
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
NATIVE_PLATFORM := $(if $(filter FreeBSD,$(UNAME_S)),freebsd,$(if $(filter Linux,$(UNAME_S)),linux,$(shell printf '%s' "$(UNAME_S)" | tr '[:upper:]' '[:lower:]')))
ARCH := $(if $(filter amd64,$(UNAME_M)),x86_64,$(UNAME_M))
ANDROID_SDK ?= $(if $(ANDROID_SDK_ROOT),$(ANDROID_SDK_ROOT),$(if $(ANDROID_HOME),$(ANDROID_HOME),$(shell sed -n 's/^sdk\.dir=//p' droid/local.properties 2>/dev/null | head -n 1)))
ANDROID_CMAKE_DIR ?= $(ANDROID_SDK)/cmake/3.22.1
ANDROID_AAPT2 ?= $(shell if [ -n "$(ANDROID_SDK)" ]; then find "$(ANDROID_SDK)/build-tools" -mindepth 2 -maxdepth 2 -type f -name aapt2 -perm -111 2>/dev/null | sort | tail -n 1; fi)
ANDROID_GRADLE_ARGS := $(if $(ANDROID_AAPT2),-Pandroid.aapt2FromMavenOverride="$(ANDROID_AAPT2)",)
ANDROID_JAVA_HOME ?= $(shell for dir in /usr/local/openjdk17 /usr/lib/jvm/java-17-openjdk-amd64 /usr/lib/jvm/java-17-openjdk; do if [ -x "$$dir/bin/java" ]; then printf "%s\n" "$$dir"; break; fi; done)
ANDROID_GRADLE_ENV := unset ANDROID_HOME; $(if $(ANDROID_JAVA_HOME),JAVA_HOME="$(ANDROID_JAVA_HOME)" PATH="$(ANDROID_JAVA_HOME)/bin:$$PATH")
ANDROID_KEYSTORE ?= $(HOME)/.android/kryon-release.keystore
ANDROID_KEY_ALIAS ?= inbe-key

BUILD_DIR := build
BUILD_OBJ_DIR := $(BUILD_DIR)/obj
BUILD_BIN_DIR := $(BUILD_DIR)/bin
BUILD_DIST_DIR := $(BUILD_DIR)/dist
VENDOR_BUILD_DIR := vendor-builds
NATIVE_OBJ_DIR := $(BUILD_OBJ_DIR)/$(NATIVE_PLATFORM)
NATIVE_BIN_DIR := $(BUILD_BIN_DIR)/$(NATIVE_PLATFORM)
NATIVE_DIST_DIR := $(BUILD_DIST_DIR)/$(NATIVE_PLATFORM)
NATIVE_VENDOR_BUILD_DIR := $(VENDOR_BUILD_DIR)/$(NATIVE_PLATFORM)/$(ARCH)
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
DEB_BUILD_DIR := $(BUILD_OBJ_DIR)/deb
DEB_ROOT := $(DEB_BUILD_DIR)/root
DEB_DIST_DIR := $(BUILD_DIST_DIR)/deb
DEB_ARCH ?= $(if $(filter x86_64 amd64,$(ARCH)),amd64,$(if $(filter aarch64 arm64,$(ARCH)),arm64,$(ARCH)))
DEB_BIN_SOURCE ?=
DEB_BIN_INPUT = $(if $(strip $(DEB_BIN_SOURCE)),$(DEB_BIN_SOURCE),$(if $(filter linux,$(NATIVE_PLATFORM)),$(TARGET),))
DEB_PACKAGE_NAME ?= $(APP_NAME)
DEB_MAINTAINER ?= $(APP_MAINTAINER)
DEB_SECTION ?= utils
DEB_PRIORITY ?= optional
DEB_DEPENDS ?= libc6, libsdl2-2.0-0, libgtk-3-0, libcurl4, libdrm2, libgbm1, libegl1, libgles2, hicolor-icon-theme
DEB_TARGET = $(DEB_DIST_DIR)/$(DEB_PACKAGE_NAME)_$(APP_VERSION)_$(DEB_ARCH).deb
DEB_TARGET_PREREQS = Makefile $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPIMAGE_ICON) $(LINUX_APPIMAGE_APPDATA) $(VERSION_FILE) $(if $(filter linux,$(NATIVE_PLATFORM)),$(TARGET),)
RPM_BUILD_DIR := $(BUILD_OBJ_DIR)/rpm
RPM_TOPDIR := $(RPM_BUILD_DIR)/rpmbuild
RPM_DIST_DIR := $(BUILD_DIST_DIR)/rpm
RPM_ARCH ?= $(if $(filter x86_64 amd64,$(ARCH)),x86_64,$(if $(filter aarch64 arm64,$(ARCH)),aarch64,$(ARCH)))
RPM_BIN_SOURCE ?=
RPM_BIN_INPUT = $(if $(strip $(RPM_BIN_SOURCE)),$(RPM_BIN_SOURCE),$(if $(filter linux,$(NATIVE_PLATFORM)),$(TARGET),))
RPM_PACKAGE_NAME ?= $(APP_NAME)
RPM_RELEASE ?= 1
RPM_LICENSE ?= BSD-3-Clause
RPM_REQUIRES ?= glibc, SDL2, gtk3, libcurl, libdrm, mesa-libgbm, mesa-libEGL, mesa-libGLES, hicolor-icon-theme
RPM_SPEC := $(RPM_BUILD_DIR)/$(RPM_PACKAGE_NAME).spec
RPM_TARGET = $(RPM_DIST_DIR)/$(RPM_PACKAGE_NAME)-$(APP_VERSION)-$(RPM_RELEASE).$(RPM_ARCH).rpm
RPM_TARGET_PREREQS = Makefile $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPIMAGE_ICON) $(LINUX_APPIMAGE_APPDATA) $(VERSION_FILE) $(if $(filter linux,$(NATIVE_PLATFORM)),$(TARGET),)
PODMAN ?= $(shell if [ "$(UNAME_S)" = "FreeBSD" ] && [ "$$(id -u)" != "0" ]; then \
	if command -v doas >/dev/null 2>&1; then printf 'doas podman'; \
	elif command -v sudo >/dev/null 2>&1; then printf 'sudo podman'; \
	else printf 'podman'; fi; \
else printf 'podman'; fi)
PODMAN_RUN_PLATFORM ?= $(if $(filter FreeBSD,$(UNAME_S)),--os linux --arch amd64,)
PODMAN_RUN_NETWORK ?= $(if $(filter FreeBSD,$(UNAME_S)),--network host,)
SNAP_BUILD_DIR := $(BUILD_OBJ_DIR)/snap
SNAP_DIST_DIR := $(BUILD_DIST_DIR)/snap
SNAP_IMAGE ?= ghcr.io/canonical/snapcraft:8_core22
SNAP_ENTRYPOINT ?= /bin/sh
SNAP_APT_CACHE_VOLUME ?= $(APP_NAME)-snap-apt-cache
SNAP_ROOT_CACHE_VOLUME ?= $(APP_NAME)-snap-root-cache
SNAP_CACHE_VOLUMES := $(SNAP_APT_CACHE_VOLUME) $(SNAP_ROOT_CACHE_VOLUME)
SNAP_TARGET = $(SNAP_DIST_DIR)/$(APP_NAME)_$(APP_VERSION)_$(ARCH).snap
FLATPAK_BUILD_DIR := $(BUILD_OBJ_DIR)/flatpak
FLATPAK_DIST_DIR := $(BUILD_DIST_DIR)/flatpak
FLATPAK_IMAGE ?= ghcr.io/flathub-infra/flatpak-github-actions:gnome-46
FLATPAK_MANIFEST = packaging/flatpak/$(APP_ID).yml
FLATPAK_TARGET = $(FLATPAK_DIST_DIR)/$(APP_NAME)-$(APP_VERSION)-$(ARCH).flatpak
APP_ID := $(ANDROID_APP_ID)
APP_COMMENT := Syncable breathing, meditation, and habit practice app
APP_DESC := Inner Breeze is a free, open-source practice app for breathing, meditation, and habit tracking.
APP_CATEGORIES := Utility;Education;
APP_MAINTAINER := Waozi <waozi@waozi.xyz>
APP_WWW := https://inbe.waozi.xyz/
APP_ORIGIN := games/inbe
APP_LICENSE := BSD3CLAUSE
APP_DESKTOP := $(LINUX_APPIMAGE_DESKTOP)
APP_ICON := $(LINUX_APPIMAGE_ICON)
APP_ICON_SIZE := 512x512
APP_METAINFO := $(LINUX_APPIMAGE_APPDATA)
FREEBSD_PKG_DEPS := curl:ftp/curl gtk3:x11-toolkits/gtk30 hicolor-icon-theme:misc/hicolor-icon-theme libdrm:graphics/libdrm mesa-libs:graphics/mesa-libs sdl2:devel/sdl20 sqlite3:databases/sqlite3
CLICK_PACKAGE ?= inbe
CLICK_ID ?= inbe
CLICK_TITLE ?= $(APP_TITLE)
CLICK_MAINTAINER ?= Waozi <waozi@waozi.xyz>
CLICK_ARCH ?= arm64
CLICK_FRAMEWORK ?= ubuntu-sdk-20.04
CLICK_POLICY_VERSION ?= 20.04
CLICK_INCLUDE_METAINFO ?= 1
CLICK_DIR := packaging/click
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
CLICK_LIBOQS_BUILD_DIR := $(VENDOR_BUILD_DIR)/click/$(CLICK_ARCH)/liboqs
CLICK_LIBOQS_A := $(CLICK_LIBOQS_BUILD_DIR)/lib/liboqs.a
CLICK_LIBOQS_INCLUDE := -I$(CLICK_LIBOQS_BUILD_DIR)/include
CLICK_PATCHELF_INTERPRETER ?= /lib/ld-linux-aarch64.so.1
CLICK_RUNTIME_LIBS ?= $(AARCH64_CLICK_RUNTIME_LIBS)
AARCH64_CC ?= $(shell command -v aarch64-linux-gnu-gcc 2>/dev/null || command -v aarch64-linux-musl-gcc 2>/dev/null)
AARCH64_AR ?= $(shell command -v aarch64-linux-gnu-ar 2>/dev/null || command -v aarch64-linux-musl-ar 2>/dev/null)
AARCH64_RANLIB ?= $(shell command -v aarch64-linux-gnu-ranlib 2>/dev/null || command -v aarch64-linux-musl-ranlib 2>/dev/null)
WINDOWS_OBJ_DIR := $(BUILD_OBJ_DIR)/windows
WINDOWS_BIN_DIR := $(BUILD_BIN_DIR)/windows
WINDOWS_DIST_DIR := $(BUILD_DIST_DIR)/windows
ANDROID_BUILD_DIR := $(BUILD_DIR)/android
WEB_OBJ_DIR := $(BUILD_OBJ_DIR)/web
WEB_DIST_DIR := $(BUILD_DIST_DIR)/web
CHROME_WEB_STORE_DIR := $(BUILD_DIST_DIR)/chrome-web-store
VERSION_FILE := src/core/version.h
APP_VERSION := $(shell awk '/INBE_VERSION_STRING/ { print $$3; exit }' $(VERSION_FILE) 2>/dev/null | tr -d '"')

KRYON_DIR ?= vendor/kryon
RAYLIB_DIR = $(KRYON_DIR)/vendor/raylib/src
RAYLIB_BUILD_DIR := $(NATIVE_VENDOR_BUILD_DIR)/raylib
RAYLIB_A := $(RAYLIB_BUILD_DIR)/libraylib.a
WIN64_ARCH := x86_64
WIN64_CC ?= $(or $(WIN_CC),x86_64-w64-mingw32-gcc)
WIN64_AR ?= $(or $(WIN_AR),x86_64-w64-mingw32-ar)
WIN64_RANLIB ?= $(or $(WIN_RANLIB),x86_64-w64-mingw32-ranlib)
WIN64_STRIP ?= $(or $(WIN_STRIP),x86_64-w64-mingw32-strip)
WIN64_CMAKE_SYSTEM_PROCESSOR ?= x86_64
WIN64_CC_PATH := $(shell command -v $(WIN64_CC) 2>/dev/null || printf '%s' $(WIN64_CC))
WIN64_AR_PATH := $(shell command -v $(WIN64_AR) 2>/dev/null || printf '%s' $(WIN64_AR))
WIN64_RANLIB_PATH := $(shell command -v $(WIN64_RANLIB) 2>/dev/null || printf '%s' $(WIN64_RANLIB))
WIN32_ARCH := i686
WIN32_CC ?= i686-w64-mingw32-gcc
WIN32_AR ?= i686-w64-mingw32-ar
WIN32_RANLIB ?= i686-w64-mingw32-ranlib
WIN32_STRIP ?= i686-w64-mingw32-strip
WIN32_CMAKE_SYSTEM_PROCESSOR ?= x86
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
WIN64_LIBOQS_BUILD_DIR := $(VENDOR_BUILD_DIR)/windows/$(WIN64_ARCH)/liboqs
WIN64_LIBOQS_A := $(WIN64_LIBOQS_BUILD_DIR)/lib/liboqs.a
WIN64_LIBOQS_INCLUDE := -I$(WIN64_LIBOQS_BUILD_DIR)/include
WIN32_CURL_BUILD_DIR := $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/curl
WIN32_CURL_INCLUDE_DIR := $(WIN32_CURL_BUILD_DIR)/include
WIN32_CURL_A := $(WIN32_CURL_BUILD_DIR)/lib/libcurl.a
WIN32_LIBOQS_BUILD_DIR := $(VENDOR_BUILD_DIR)/windows/$(WIN32_ARCH)/liboqs
WIN32_LIBOQS_A := $(WIN32_LIBOQS_BUILD_DIR)/lib/liboqs.a
WIN32_LIBOQS_INCLUDE := -I$(WIN32_LIBOQS_BUILD_DIR)/include
WEB_RAYLIB_BUILD_DIR := $(VENDOR_BUILD_DIR)/web/raylib
WEB_RAYLIB_A := $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
RAYLIB_SOURCES := $(shell find $(RAYLIB_DIR) -type f \( -name '*.c' -o -name '*.h' \))

KRYON_ICON_DIRS := icons language payments platforms tiles pfp
KRYON_ICON_FILES := $(foreach dir,$(KRYON_ICON_DIRS),$(wildcard $(KRYON_DIR)/$(dir)/*.png))
KRYON_ICON_ASSETS_C := $(KRYON_DIR)/src/ui/ui_icon_assets.c
KRYON_ICON_STAMP := $(BUILD_OBJ_DIR)/kryon-icons.sha256
KRYON_SRCS := $(filter-out $(KRYON_ICON_ASSETS_C),$(shell find $(KRYON_DIR)/src -type f -name '*.c' | LC_ALL=C sort)) $(KRYON_ICON_ASSETS_C)
KRYON_WEB_SRCS := $(KRYON_SRCS)
KRYON_WINDOWS_SRCS := $(filter-out $(KRYON_DIR)/src/file_dialog/file_dialog.c,$(KRYON_SRCS))
KRYON_CLICK_SRCS := $(filter-out $(KRYON_DIR)/src/file_dialog/file_dialog.c,$(KRYON_SRCS))
KRYON_INCLUDE := -I$(KRYON_DIR)/include
KRYON_SYNC_ACCOUNT_C := $(KRYON_DIR)/src/ksync/ksync_account.c
KRYON_SYNC_C := $(KRYON_DIR)/src/ksync/ksync_sync.c
KRYON_SYNC_TRANSPORT_C := $(KRYON_DIR)/src/ksync/ksync_transport.c
KRYON_SYNC_ACCOUNT_H := $(KRYON_DIR)/include/ksync_account.h
KRYON_ALLOW_ICON_REGEN ?= 0
KRYON_ICON_ASSETS_DEPS := $(if $(filter 1,$(KRYON_ALLOW_ICON_REGEN)),$(KRYON_ICON_STAMP) $(KRYON_DIR)/scripts/embed-icons.sh,)
KRYON_VENDOR_BUILD_DIR := $(NATIVE_VENDOR_BUILD_DIR)
KRYON_LIBOQS_BUILD_DIR := $(KRYON_VENDOR_BUILD_DIR)/liboqs
KRYON_WEB_LIBOQS_BUILD_DIR := $(VENDOR_BUILD_DIR)/web/liboqs
KRYON_CURL_BUILD_DIR := $(KRYON_VENDOR_BUILD_DIR)/curl
KRYON_CURL_REQUIRE_WEBSOCKETS := 1
KRYON_CURL_EXTRA_CMAKE_FLAGS := \
	-DCURL_DISABLE_WEBSOCKETS=OFF \
	-DCURL_DISABLE_INSTALL=OFF \
	-DCURL_DISABLE_DICT=ON \
	-DCURL_DISABLE_FILE=ON \
	-DCURL_DISABLE_FTP=ON \
	-DCURL_DISABLE_GOPHER=ON \
	-DCURL_DISABLE_IMAP=ON \
	-DCURL_DISABLE_MQTT=ON \
	-DCURL_DISABLE_POP3=ON \
	-DCURL_DISABLE_RTSP=ON \
	-DCURL_DISABLE_SMTP=ON \
	-DCURL_DISABLE_TELNET=ON \
	-DCURL_DISABLE_TFTP=ON \
	-DCURL_DISABLE_LIBCURL_OPTION=ON \
	-DCURL_ZLIB=OFF \
	-DCURL_BROTLI=OFF \
	-DCURL_ZSTD=OFF
# inbe is a UI-only app: drop the 2D physics subsystem (Box2D) entirely. The
# flag must precede the vendor.mk include (which defaults it to 1); the source
# filter and define are applied right after it. KRYON_WEB/WINDOWS/CLICK_SRCS
# were snapshotted from KRYON_SRCS above (before this filter), so re-apply it
# to every build variant -- otherwise the kryon physics .c files (which need
# box2d.h) leak into the web/Windows/click builds and break them.
KRYON_WITH_PHYSICS ?= 0
include $(KRYON_DIR)/mk/vendor.mk
KRYON_INCLUDE += $(KRYON_PHYSICS_CPPFLAGS)
KRYON_SRCS := $(filter-out $(KRYON_PHYSICS_SRCS),$(KRYON_SRCS))
KRYON_WEB_SRCS := $(filter-out $(KRYON_PHYSICS_SRCS),$(KRYON_WEB_SRCS))
KRYON_WINDOWS_SRCS := $(filter-out $(KRYON_PHYSICS_SRCS),$(KRYON_WINDOWS_SRCS))
KRYON_CLICK_SRCS := $(filter-out $(KRYON_PHYSICS_SRCS),$(KRYON_CLICK_SRCS))
KRYON_LIBOQS_CPU_FEATURE_CMAKE_FLAGS ?= \
	-DOQS_USE_ADX_INSTRUCTIONS=OFF \
	-DOQS_USE_AES_INSTRUCTIONS=OFF \
	-DOQS_USE_AVX_INSTRUCTIONS=OFF \
	-DOQS_USE_AVX2_INSTRUCTIONS=OFF \
	-DOQS_USE_AVX512_INSTRUCTIONS=OFF \
	-DOQS_USE_AVX512BW_INSTRUCTIONS=OFF \
	-DOQS_USE_AVX512DQ_INSTRUCTIONS=OFF \
	-DOQS_USE_AVX512F_INSTRUCTIONS=OFF \
	-DOQS_USE_BMI1_INSTRUCTIONS=OFF \
	-DOQS_USE_BMI2_INSTRUCTIONS=OFF \
	-DOQS_USE_FMA_INSTRUCTIONS=OFF \
	-DOQS_USE_PCLMULQDQ_INSTRUCTIONS=OFF \
	-DOQS_USE_POPCNT_INSTRUCTIONS=OFF \
	-DOQS_USE_SSE_INSTRUCTIONS=OFF \
	-DOQS_USE_SSE2_INSTRUCTIONS=OFF \
	-DOQS_USE_SSE3_INSTRUCTIONS=OFF \
	-DOQS_USE_VPCLMULQDQ_INSTRUCTIONS=OFF
CURL_DIR := $(KRYON_CURL_DIR)
CURL_BUILD_DIR := $(KRYON_CURL_BUILD_DIR)
CURL_INCLUDE_DIR := $(KRYON_CURL_INCLUDE_DIR)
CURL_LIB_DIR := $(KRYON_CURL_LIB_DIR)
CURL_SO := $(KRYON_CURL_SO)
CURL_PROTOCOL_CHECK := $(KRYON_CURL_PROTOCOL_CHECK)
KRYON_CURL_VERSION_NUM ?= $(shell printf '%b\n' '\043include <curl/curlver.h>' 'LIBCURL_VERSION_NUM' | $(CC) -I$(KRYON_CURL_DIR)/include -E -P - 2>/dev/null | tail -n 1)
KRYON_CURL_VERSION_HEX := $(patsubst 0x%,%,$(KRYON_CURL_VERSION_NUM))
SQLITE_DIR := vendor/sqlite
SQLITE_BUILD_DIR := $(VENDOR_BUILD_DIR)/sqlite
SQLITE_AMALGAMATION_C := $(SQLITE_BUILD_DIR)/sqlite3.c
SQLITE_AMALGAMATION_H := $(SQLITE_BUILD_DIR)/sqlite3.h
SQLITE_SRC := $(SQLITE_AMALGAMATION_C)
SQLITE_INCLUDE := -I$(SQLITE_BUILD_DIR)
LIBOQS_DIR := $(KRYON_LIBOQS_DIR)
LIBOQS_BUILD_DIR := $(KRYON_LIBOQS_BUILD_DIR)
LIBOQS_A := $(KRYON_LIBOQS_A)
LIBOQS_INCLUDE := $(KRYON_LIBOQS_INCLUDE)
WEB_LIBOQS_BUILD_DIR := $(KRYON_WEB_LIBOQS_BUILD_DIR)
WEB_LIBOQS_A := $(KRYON_WEB_LIBOQS_A)
WEB_LIBOQS_INCLUDE := -I$(WEB_LIBOQS_BUILD_DIR)/include
TEST_BIN_DIR := $(BUILD_BIN_DIR)/tests
STORAGE_IMPORT_TEST := $(TEST_BIN_DIR)/storage_import_test
LOCALE_KEYS_TEST := $(TEST_BIN_DIR)/locale_keys_test
SYNC_URL_TEST := $(TEST_BIN_DIR)/sync_url_test
SYNC_ACCOUNT_TEST := $(TEST_BIN_DIR)/sync_account_test
SYNC_REVIEW_TEST := $(TEST_BIN_DIR)/sync_review_test
FONT_LOCALE_TEST := $(TEST_BIN_DIR)/font_locale_test
GUIDE_OVERLAY_TEST := $(TEST_BIN_DIR)/guide_overlay_test
APP_BOTTOM_NAV_TEST := $(TEST_BIN_DIR)/app_bottom_nav_test
BREATH_TIMING_TEST := $(TEST_BIN_DIR)/breath_timing_test
TESTS := $(STORAGE_IMPORT_TEST) $(LOCALE_KEYS_TEST) $(SYNC_URL_TEST) $(SYNC_ACCOUNT_TEST) $(SYNC_REVIEW_TEST) $(FONT_LOCALE_TEST) $(GUIDE_OVERLAY_TEST) $(APP_BOTTOM_NAV_TEST) $(BREATH_TIMING_TEST)
RUNTIME_ASSET_CFLAGS := -DHAS_LIBCURL=1 $(KRYON_CURL_CFLAGS)
RUNTIME_ASSET_LDLIBS := $(KRYON_CURL_LDLIBS)

APP_SRCS := \
	src/main.c \
	$(sort $(wildcard src/app/*.c)) \
	src/storage/storage.c \
	src/storage/sync_client.c \
	src/third_party/miniz.c \
	src/platform/android/android_device.c

ifeq ($(NATIVE_PLATFORM),linux)
# Prefer AppIndicator (visible on GNOME/KDE via StatusNotifierItem); fall back to
# the GTK GtkStatusIcon backend when only GTK3 is available. GtkStatusIcon is
# deprecated and invisible on stock GNOME, so install libayatana-appindicator3-dev
# for a reliable tray icon on modern desktops.
DESKTOP_TRAY_PKG := $(shell if pkg-config --exists ayatana-appindicator3-0.1; then printf '%s' ayatana-appindicator3-0.1; elif pkg-config --exists appindicator3-0.1; then printf '%s' appindicator3-0.1; elif pkg-config --exists gtk+-3.0; then printf '%s' gtk+-3.0; fi)
ifeq ($(filter ayatana-appindicator3-0.1 appindicator3-0.1,$(DESKTOP_TRAY_PKG)),)
DESKTOP_TRAY_DEFINE := -DINBE_DESKTOP_TRAY_GTK_STATUS_ICON -DKRYON_DESKTOP_TRAY_GTK_STATUS_ICON
else ifeq ($(filter ayatana-appindicator3-0.1,$(DESKTOP_TRAY_PKG)),ayatana-appindicator3-0.1)
DESKTOP_TRAY_DEFINE := -DINBE_DESKTOP_TRAY_AYATANA -DKRYON_DESKTOP_TRAY_AYATANA
else
DESKTOP_TRAY_DEFINE := -DINBE_DESKTOP_TRAY_APPINDICATOR -DKRYON_DESKTOP_TRAY_APPINDICATOR
endif
endif
ifeq ($(NATIVE_PLATFORM),freebsd)
DESKTOP_TRAY_PKG := $(shell if pkg-config --exists gtk+-3.0; then printf '%s' gtk+-3.0; fi)
DESKTOP_TRAY_DEFINE := -DINBE_DESKTOP_TRAY_GTK_STATUS_ICON -DKRYON_DESKTOP_TRAY_GTK_STATUS_ICON
endif
ifneq ($(strip $(DESKTOP_TRAY_PKG)),)
APP_SRCS += src/platform/inbe_desktop_tray.c
DESKTOP_TRAY_CFLAGS := $(shell pkg-config --cflags $(DESKTOP_TRAY_PKG)) -DINBE_DESKTOP_TRAY_ENABLED -DKRYON_DESKTOP_TRAY_ENABLED $(DESKTOP_TRAY_DEFINE)
DESKTOP_TRAY_LDLIBS := $(shell pkg-config --libs $(DESKTOP_TRAY_PKG))
endif

ifneq ($(filter linux freebsd,$(NATIVE_PLATFORM)),)
SYSTEM_THEME_PKG := $(shell if pkg-config --exists gtk+-3.0; then printf '%s' gtk+-3.0; fi)
FILE_DIALOG_PKG := gtk+-3.0
endif
ifneq ($(filter all native install run run-fresh screenshot dist appimage vendor-prebuilds vendor-prebuilds-native,$(MAKECMDGOALS))$(if $(MAKECMDGOALS),,default),)
ifeq ($(strip $(SYSTEM_THEME_PKG)),)
$(error gtk+-3.0 is required for native desktop file dialogs)
endif
endif
ifneq ($(strip $(SYSTEM_THEME_PKG)),)
SYSTEM_THEME_CFLAGS := $(shell pkg-config --cflags $(SYSTEM_THEME_PKG)) -DSYSTEM_THEME_GTK
SYSTEM_THEME_LDLIBS := $(shell pkg-config --libs $(SYSTEM_THEME_PKG))
endif

LOCALE_FILES := $(wildcard locales/*.txt)
IMAGE_FILES := assets/app/icon.png assets/easteregg/art.png assets/easteregg/waozi.png assets/practices/whm/1.png assets/practices/whm/2.png assets/practices/meditation/1.png assets/pet/egg1.png $(wildcard assets/practices/*/banner.png) assets/practices/sunsalutation/poses_man_sheet.png assets/practices/sunsalutation/poses_woman_sheet.png
SOUND_FILES := $(wildcard assets/sounds/*.ogg)
FONT_SUBSET_DIR := assets/fonts/subset
FONT_SUBSET_CORPUS := locales assets/fonts/input_common.txt
FONT_FILES := \
	$(FONT_SUBSET_DIR)/NotoSans-Inbe-Regular.ttf \
	$(FONT_SUBSET_DIR)/NotoSansSC-Inbe-Regular.otf \
	$(FONT_SUBSET_DIR)/NotoSansJP-Inbe-Regular.otf \
	$(FONT_SUBSET_DIR)/NotoSansKR-Inbe-Regular.otf \
	$(FONT_SUBSET_DIR)/NotoSansTC-Inbe-Regular.otf
EMBEDDED_ASSETS_C := $(BUILD_OBJ_DIR)/$(APP_NAME)_embedded_assets.c
EMBEDDED_ASSET_FILES := $(LOCALE_FILES) $(IMAGE_FILES) $(SOUND_FILES) $(FONT_FILES)
KC ?= $(KRYON_DIR)/build/bin/kc
KRY_GEN_DIR := $(BUILD_DIR)/kryon/generated
KRY_SRCS := $(shell find src -type f -name '*.kry' 2>/dev/null | LC_ALL=C sort)
KRY_GEN_SRCS := $(patsubst %.kry,$(KRY_GEN_DIR)/%.c,$(KRY_SRCS))
KRY_GEN_HDRS := $(patsubst %.kry,$(KRY_GEN_DIR)/%.h,$(KRY_SRCS))
KRY_GEN_SRCS := $(filter-out $(KRY_GEN_DIR)/src/storage/db.c,$(KRY_GEN_SRCS)) $(KRY_GEN_DIR)/src/storage/db_impl.c
KRY_GEN_HDRS := $(filter-out $(KRY_GEN_DIR)/src/storage/db.h,$(KRY_GEN_HDRS)) $(KRY_GEN_DIR)/src/storage/db_impl.h
KRY_GEN_SRCS := $(filter-out $(KRY_GEN_DIR)/src/storage/import.c,$(KRY_GEN_SRCS)) $(KRY_GEN_DIR)/src/storage/import_impl.c
KRY_GEN_HDRS := $(filter-out $(KRY_GEN_DIR)/src/storage/import.h,$(KRY_GEN_HDRS)) $(KRY_GEN_DIR)/src/storage/import_impl.h
KRY_PROJECT_HDR := $(KRY_GEN_DIR)/kryon_project.h
KRY_PROJECT_C := $(KRY_GEN_DIR)/kryon_project.c
KRY_GEN_STAMP := $(KRY_GEN_DIR)/.fresh
SRC := $(APP_SRCS) $(KRY_GEN_SRCS) $(EMBEDDED_ASSETS_C)
KRYON_HOST_APP_SRCS := $(filter-out src/main.c src/platform/inbe_desktop_tray.c,$(APP_SRCS)) $(KRY_GEN_SRCS) $(KRY_PROJECT_C)
KRYON_HOST_RUNTIME_SRCS := $(KRYON_DIR)/src/core/embedded_assets.c
KRYON_HOST_SRC := $(KRYON_HOST_APP_SRCS) $(KRYON_HOST_RUNTIME_SRCS) $(EMBEDDED_ASSETS_C)
WEB_APP_SRCS := $(filter-out src/platform/inbe_desktop_tray.c,$(APP_SRCS))
WEB_SRC := $(WEB_APP_SRCS) $(KRY_GEN_SRCS) $(EMBEDDED_ASSETS_C)

APP_INCLUDE := -Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/practices -Isrc/practices/whm -Isrc/practices/meditation -Isrc/practices/sun_salutation -Isrc/storage -Isrc/platform -Isrc/platform/android -Isrc/third_party
APP_INCLUDE += $(KRYON_INCLUDE)
APP_INCLUDE += -I$(KRY_GEN_DIR)
APP_INCLUDE += -I$(KRY_GEN_DIR)/src
RAY_PKGS ?= sdl2 libdrm gbm egl glesv2
RAY_SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
RAY_SDL_LDLIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
RAY_GL_CFLAGS ?= $(shell pkg-config --cflags libdrm gbm egl glesv2 2>/dev/null)
RAY_GL_LDLIBS ?= $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null)
RAY_CFLAGS ?= $(strip $(RAY_SDL_CFLAGS) $(RAY_GL_CFLAGS))
RAY_LDLIBS ?= $(strip $(RAY_SDL_LDLIBS) $(RAY_GL_LDLIBS))
RAY_SDL_INCLUDE_DIR ?= $(shell pkg-config --variable=includedir sdl2 2>/dev/null | sed 's,/SDL2$$,,')
RAY_RAYLIB_CONFIG ?= -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=1
ifeq ($(NATIVE_PLATFORM),freebsd)
KRYON_RAYLIB_AUDIO_PERIOD_FRAMES ?= 128
KRYON_RAYLIB_AUDIO_PERIODS ?= 2
endif
KRYON_RAYLIB_AUDIO_PERIOD_CONFIG := $(if $(strip $(KRYON_RAYLIB_AUDIO_PERIOD_FRAMES)),-DAUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES=$(KRYON_RAYLIB_AUDIO_PERIOD_FRAMES),)
KRYON_RAYLIB_AUDIO_PERIODS_CONFIG := $(if $(strip $(KRYON_RAYLIB_AUDIO_PERIODS)),-DAUDIO_DEVICE_PERIODS=$(KRYON_RAYLIB_AUDIO_PERIODS),)
APP_RAYLIB_CONFIG := $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=%,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=0 $(KRYON_RAYLIB_AUDIO_PERIOD_CONFIG) $(KRYON_RAYLIB_AUDIO_PERIODS_CONFIG)
COMMON_CFLAGS := -Wall -Wextra -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DUI_EMBEDDED_ONLY=1
CFLAGS := $(COMMON_CFLAGS) -std=c99 $(RUNTIME_ASSET_CFLAGS) $(SYSTEM_THEME_CFLAGS) $(DESKTOP_TRAY_CFLAGS)
NATIVE_SYSTEM_LDLIBS := -lm -lpthread $(if $(filter linux,$(NATIVE_PLATFORM)),-ldl -lrt,) $(SYSTEM_THEME_LDLIBS)
WINDOWS_CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DUI_EMBEDDED_ONLY=1
WEB_CFLAGS := $(filter-out -Os,$(COMMON_CFLAGS)) -O1 -std=gnu99
CLICK_CFLAGS := -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DUI_EMBEDDED_ONLY=1 -DINBE_DISABLE_KRYON_FILE_DIALOG -DHAS_LIBCURL=1 $(AARCH64_KRYON_CURL_CFLAGS)
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

BINARY_NAME := $(APP_NAME)-$(NATIVE_PLATFORM)-$(ARCH)
TARGET := $(NATIVE_BIN_DIR)/$(BINARY_NAME)
KRYON_HOST_TARGET := $(BUILD_DIR)/kryon/app_host.so
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
WEB_RANLIB ?= emranlib
include $(KRYON_DIR)/mk/raylib.mk
KRYON_SRCS += $(KRYON_RAYLIB_WRAPPERS_C)
KRYON_WEB_SRCS += $(KRYON_RAYLIB_WRAPPERS_C)
KRYON_WINDOWS_SRCS += $(KRYON_RAYLIB_WRAPPERS_C)
KRYON_CLICK_SRCS += $(KRYON_RAYLIB_WRAPPERS_C)
WEB_CACHE_BUSTER ?= $(shell if git diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then git rev-parse --short HEAD 2>/dev/null; else date +%s; fi)
WEB_TARGET := $(WEB_DIST_DIR)/index.html
WEB_APP_SCRIPT := <script>window.__inbeLoadApp("index.js?v=$(WEB_CACHE_BUSTER)")</script>
WEB_JS_TARGET := $(WEB_DIST_DIR)/index.js
WEB_BOOT_JS := src/web_boot.js
WEB_DIST_ZIP := $(BUILD_DIST_DIR)/$(APP_NAME)-web.zip
WEB_SMOKE_BROWSER ?= auto
WEB_SMOKE_TEST := scripts/web-smoke-test.mjs
WEB_APP_URL ?= https://inbe.waozi.xyz/
CHROME_WEB_STORE_ZIP := $(BUILD_DIST_DIR)/$(APP_NAME)-chrome-web-store.zip
CHROME_WEB_STORE_MANIFEST := packaging/chrome-web-store/manifest.json
CHROME_WEB_STORE_WORKER := packaging/chrome-web-store/service_worker.js
CHROME_WEB_STORE_ICON_DIR := packaging/chrome-web-store/icons
CHROME_WEB_STORE_ICONS := \
	$(CHROME_WEB_STORE_ICON_DIR)/icon-16.png \
	$(CHROME_WEB_STORE_ICON_DIR)/icon-32.png \
	$(CHROME_WEB_STORE_ICON_DIR)/icon-48.png \
	$(CHROME_WEB_STORE_ICON_DIR)/icon-128.png
FIREFOX_ADDONS_DIR := $(BUILD_DIST_DIR)/firefox-addons
FIREFOX_ADDONS_ZIP := $(BUILD_DIST_DIR)/$(APP_NAME)-firefox-addons.zip
FIREFOX_ADDONS_SOURCE_ZIP := $(BUILD_DIST_DIR)/$(APP_NAME)-firefox-addons-source.zip
FIREFOX_ADDONS_MANIFEST := packaging/firefox-addons/manifest.json
FIREFOX_ADDONS_BACKGROUND := packaging/firefox-addons/background.js
FIREFOX_ADDONS_LOADER := packaging/firefox-addons/extension_loader.js
FIREFOX_ADDONS_INDEX := $(FIREFOX_ADDONS_DIR)/index.html
FIREFOX_ADDONS_APP_SCRIPT := <script src="extension_loader.js"></script>
FIREFOX_ADDONS_ICON_DIR := $(CHROME_WEB_STORE_ICON_DIR)
FIREFOX_ADDONS_ICONS := $(CHROME_WEB_STORE_ICONS)
ADDONS_LINTER ?= npx --yes addons-linter
WEB_ASSET_FILES := $(shell find web-assets site-icons -type f 2>/dev/null)
UNPACKAGED_AUDIO_DIR := unpackaged_assets/audio
UNPACKAGED_AUDIO_FILES := $(shell find $(UNPACKAGED_AUDIO_DIR) -type f 2>/dev/null)
MEDITATION_AUDIO_ZIP := web-assets/dl/inbe-meditation-audio-v1.zip
MEDITATION_AUDIO_TRACKS := \
	Elijah_K/deep-meditation.ogg \
	Elijah_K/path-of-meditation.ogg \
	Elijah_K/truth-of-silence.ogg

include $(KRYON_DIR)/mk/package-freebsd.mk

.PHONY: all native kryon-host install install-user uninstall stage package-freebsd deb package-deb deb-check rpm package-rpm rpm-check snap package-snap snap-cache-clean flatpak package-flatpak podman-check validate-desktop run run-fresh screenshot test ci dist appimage click click-verify vendor-prebuilds vendor-prebuilds-native vendor-prebuilds-web vendor-prebuilds-windows font-subsets font-bundle-check clean clean-linux clean-native clean-vendor-builds android-avd android-check-keystore android-copy-assets android-local-properties android-debug android-release android-bundle android-install android-install-release android-clean validate-meditation-audio package-unpackaged-assets windows-runtime-assets-check windows windows64 windows32 web web-tools-check web-smoke-test web-smoke-test-firefox web-smoke-test-librewolf site chrome-web-store firefox-addons firefox-addons-lint firefox-addons-source-zip verify-firefox-addons
.NOTPARALLEL: dist windows windows64 windows32 android-release android-bundle click deb package-deb rpm package-rpm snap package-snap flatpak package-flatpak

all: native

native: $(TARGET)

kryon-host: $(KRYON_HOST_TARGET)

$(KC): $(KRYON_DIR)/cmd/kc/kc.c
	$(MAKE) -C $(KRYON_DIR) build/bin/kc

$(KRY_GEN_STAMP): Makefile $(KC) $(KRY_SRCS)
	rm -rf $(KRY_GEN_DIR)
	mkdir -p $(KRY_GEN_DIR)
	$(KC) --root $(abspath .) -o $(KRY_GEN_DIR) $(abspath $(KRY_SRCS))
	touch $@

$(KRY_GEN_SRCS) $(KRY_GEN_HDRS) $(KRY_PROJECT_HDR) $(KRY_PROJECT_C): $(KRY_GEN_STAMP)

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
	$(MAKE) firefox-addons && \
	$(MAKE) click && \
	$(MAKE) appimage && \
	$(MAKE) windows && \
	$(MAKE) android-release PASSWORD="$$password" && \
	$(MAKE) android-bundle PASSWORD="$$password"

appimage: $(APPIMAGE_TARGET)

deb package-deb: $(DEB_TARGET)

rpm package-rpm: $(RPM_TARGET)

snap package-snap: $(SNAP_TARGET)

snap-cache-clean: podman-check
	$(PODMAN) volume rm -f $(SNAP_CACHE_VOLUMES)

flatpak package-flatpak: $(FLATPAK_TARGET)

click: $(CLICK_TARGET)

click-verify: $(CLICK_TARGET)
	@command -v clickable >/dev/null || { \
		echo "clickable is missing. Install clickable or put it on PATH."; \
		exit 1; \
	}
	clickable review $(CLICK_TARGET)

web-tools-check:
	@missing=0; \
	for tool in "$(WEB_CC)" "$(WEB_AR)" "$(WEB_RANLIB)" emcmake; do \
		if ! command -v "$$tool" >/dev/null 2>&1; then \
			echo "Missing web build tool: $$tool"; \
			missing=1; \
		fi; \
	done; \
	if [ "$$missing" -ne 0 ]; then \
		echo ""; \
		echo "Install Emscripten for this host or run the web build from an environment where Emscripten is on PATH."; \
		echo ""; \
		echo "Required tools: WEB_CC=$(WEB_CC), WEB_AR=$(WEB_AR), WEB_RANLIB=$(WEB_RANLIB), emcmake."; \
		exit 1; \
	fi

vendor-prebuilds: vendor-prebuilds-native vendor-prebuilds-web vendor-prebuilds-windows

vendor-prebuilds-native: $(RAYLIB_A) $(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H) $(LIBOQS_A) $(CURL_PROTOCOL_CHECK)

vendor-prebuilds-web: web-tools-check $(WEB_RAYLIB_A) $(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H) $(WEB_LIBOQS_A)

vendor-prebuilds-windows: $(WIN64_RAYLIB_A) $(WIN32_RAYLIB_A) $(WIN64_CURL_A) $(WIN32_CURL_A) $(WIN64_LIBOQS_A) $(WIN32_LIBOQS_A) $(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H)

run: $(TARGET)
	./$(TARGET)

run-fresh: $(TARGET)
	@root=$$(mktemp -d /tmp/inbe-fresh.XXXXXX); \
	echo "INBE_DATA_ROOT=$$root"; \
	INBE_FORCE_DARK_MODE=1 INBE_DATA_ROOT="$$root" ./$(TARGET)

screenshot: $(TARGET)
	./scripts/generate-screenshots.sh "$(TARGET)"


.SILENT: test $(TESTS) font-bundle-check

## Local parity with the ci.yml gate: unit tests plus the web build (emcc).
## Run before pushing to catch web-only breakage -- e.g. code under
## #if defined(PLATFORM_WEB) -- that `make test` (desktop-native) misses.
ci: test web

test: $(TESTS) font-bundle-check
	echo "== Inbe tests =="; \
	status=0; \
	for test_bin in $(TESTS); do \
		name=$$(basename "$$test_bin"); \
		log=$$(mktemp /tmp/inbe-test.XXXXXX); \
		printf "%-28s" "$$name"; \
		if "$$test_bin" >"$$log" 2>&1; then \
			echo "PASS"; \
			rm -f "$$log"; \
		else \
			echo "FAIL"; \
			cat "$$log"; \
			rm -f "$$log"; \
			status=1; \
			break; \
		fi; \
	done; \
	if [ "$$status" -eq 0 ]; then \
		echo "== PASS: all Inbe tests =="; \
	else \
		echo "== FAIL: Inbe tests =="; \
	fi; \
	exit "$$status"

font-bundle-check:
	for font in $(FONT_FILES); do \
		case "$$font" in \
			$(KRYON_DIR)/fonts/noto/*) \
				echo "Full Noto font must not be embedded: $$font"; \
				exit 1; \
				;; \
		esac; \
	done

font-subsets:
	$(MAKE) -C $(KRYON_DIR) font-subsets \
		FONT_SUBSET_OUT_DIR="$(abspath $(FONT_SUBSET_DIR))" \
		FONT_SUBSET_SOURCE_DIR="$(abspath $(KRYON_DIR)/fonts/noto)" \
		FONT_SUBSET_PREFIX=Inbe \
		FONT_SUBSET_CORPUS="$(abspath locales) $(abspath assets/fonts/input_common.txt)"

$(STORAGE_IMPORT_TEST): tests/storage_import_test.c tests/test_locale_stub.c src/storage/storage.c $(KRY_GEN_DIR)/src/storage/storage_sessions.c $(KRY_GEN_DIR)/src/storage/sync_review.c $(KRY_GEN_DIR)/src/storage/db_impl.c $(KRY_GEN_DIR)/src/storage/import_impl.c src/storage/storage.h src/storage/db.h src/storage/import.h $(KRY_GEN_DIR)/src/screens/habits_screen.c $(KRY_GEN_DIR)/src/screens/habits/edit.c $(KRY_GEN_DIR)/src/screens/habits/session.c src/screens/habits_screen.h src/screens/habits/habits.h src/third_party/miniz.c src/third_party/miniz.h $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -ffunction-sections -fdata-sections \
		-Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/practices -Isrc/practices/whm -Isrc/practices/meditation -Isrc/storage -Isrc/platform/android -Isrc/third_party $(KRYON_INCLUDE) -I$(KRY_GEN_DIR) -I$(KRY_GEN_DIR)/src $(SQLITE_INCLUDE) \
		-o $@ \
		tests/storage_import_test.c tests/test_locale_stub.c src/storage/storage.c $(KRY_GEN_DIR)/src/storage/storage_sessions.c $(KRY_GEN_DIR)/src/storage/sync_review.c $(KRY_GEN_DIR)/src/storage/db_impl.c $(KRY_GEN_DIR)/src/storage/import_impl.c $(KRY_GEN_DIR)/src/screens/habits_screen.c $(KRY_GEN_DIR)/src/screens/habits/edit.c $(KRY_GEN_DIR)/src/screens/habits/session.c src/third_party/miniz.c $(SQLITE_SRC) \
		-Wl,--gc-sections $(NATIVE_SYSTEM_LDLIBS)

$(LOCALE_KEYS_TEST): tests/locale_keys_test.c $(LOCALE_FILES) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE \
		-o $@ \
		tests/locale_keys_test.c

$(SYNC_URL_TEST): tests/sync_url_test.c src/storage/sync_client.c src/storage/sync_client.h $(KRYON_SYNC_C) $(KRYON_SYNC_TRANSPORT_C) $(KRYON_SYNC_ACCOUNT_C) $(CURL_PROTOCOL_CHECK) $(LIBOQS_A) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -Wno-unused-function -std=c99 -D_DEFAULT_SOURCE -DINBE_SYNC_CLIENT_TESTS -DHAS_LIBOQS=1 -ffunction-sections -fdata-sections \
		-Isrc/storage -Isrc -Isrc/core $(KRYON_INCLUDE) $(KRYON_CURL_CFLAGS) $(LIBOQS_INCLUDE) -o $@ \
		tests/sync_url_test.c src/storage/sync_client.c $(KRYON_SYNC_C) $(KRYON_SYNC_TRANSPORT_C) $(KRYON_SYNC_ACCOUNT_C) \
		$(LIBOQS_A) -Wl,--gc-sections $(KRYON_CURL_LDLIBS) $(NATIVE_SYSTEM_LDLIBS)

$(SYNC_ACCOUNT_TEST): tests/sync_account_test.c tests/test_locale_stub.c $(KRY_GEN_DIR)/src/storage/sync_account.c src/storage/sync_account.h $(KRYON_SYNC_ACCOUNT_C) $(KRYON_SYNC_ACCOUNT_H) src/storage/storage.c $(KRY_GEN_DIR)/src/storage/storage_sessions.c $(KRY_GEN_DIR)/src/storage/sync_review.c $(KRY_GEN_DIR)/src/storage/db_impl.c $(KRY_GEN_DIR)/src/storage/import_impl.c src/storage/storage.h src/storage/db.h src/storage/import.h src/third_party/miniz.c src/third_party/miniz.h $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(LIBOQS_A) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DHAS_LIBOQS=1 -ffunction-sections -fdata-sections \
		-Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/practices -Isrc/practices/whm -Isrc/practices/meditation -Isrc/storage -Isrc/platform/android -Isrc/third_party $(KRYON_INCLUDE) -I$(KRY_GEN_DIR) -I$(KRY_GEN_DIR)/src $(LIBOQS_INCLUDE) $(SQLITE_INCLUDE) \
		-o $@ \
		tests/sync_account_test.c tests/test_locale_stub.c $(KRY_GEN_DIR)/src/storage/sync_account.c $(KRYON_SYNC_ACCOUNT_C) src/storage/storage.c $(KRY_GEN_DIR)/src/storage/storage_sessions.c $(KRY_GEN_DIR)/src/storage/sync_review.c $(KRY_GEN_DIR)/src/storage/db_impl.c $(KRY_GEN_DIR)/src/storage/import_impl.c src/third_party/miniz.c $(SQLITE_SRC) \
		$(LIBOQS_A) -Wl,--gc-sections $(NATIVE_SYSTEM_LDLIBS)

$(SYNC_REVIEW_TEST): tests/sync_review_test.c tests/test_locale_stub.c src/storage/storage.c $(KRY_GEN_DIR)/src/storage/storage_sessions.c $(KRY_GEN_DIR)/src/storage/sync_review.c $(KRY_GEN_DIR)/src/storage/db_impl.c $(KRY_GEN_DIR)/src/storage/import_impl.c src/storage/storage.h src/storage/db.h src/storage/import.h $(KRY_GEN_DIR)/src/screens/habits_screen.c src/screens/habits_screen.h src/third_party/miniz.c src/third_party/miniz.h $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -ffunction-sections -fdata-sections \
		-Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/practices -Isrc/practices/whm -Isrc/practices/meditation -Isrc/storage -Isrc/platform/android -Isrc/third_party $(KRYON_INCLUDE) -I$(KRY_GEN_DIR) -I$(KRY_GEN_DIR)/src $(SQLITE_INCLUDE) \
		-o $@ \
		tests/sync_review_test.c tests/test_locale_stub.c src/storage/storage.c $(KRY_GEN_DIR)/src/storage/storage_sessions.c $(KRY_GEN_DIR)/src/storage/sync_review.c $(KRY_GEN_DIR)/src/storage/db_impl.c $(KRY_GEN_DIR)/src/storage/import_impl.c $(KRY_GEN_DIR)/src/screens/habits_screen.c src/third_party/miniz.c $(SQLITE_SRC) \
		-Wl,--gc-sections $(NATIVE_SYSTEM_LDLIBS)

$(FONT_LOCALE_TEST): tests/font_locale_test.c $(FONT_FILES) | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE \
		-DKRYON_DIR=\"$(KRYON_DIR)\" \
		-o $@ \
		tests/font_locale_test.c

$(GUIDE_OVERLAY_TEST): tests/guide_overlay_test.c $(KRYON_DIR)/src/ui/guide.c $(KRYON_DIR)/include/ui.h | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE \
		-I$(KRYON_DIR) $(KRYON_INCLUDE) \
		-o $@ \
		tests/guide_overlay_test.c

$(APP_BOTTOM_NAV_TEST): tests/app_bottom_nav_test.c src/app/app_nav.h src/app/app.h $(KRY_GEN_DIR)/src/app/app_nav.c $(KRY_GEN_DIR)/src/app/customize_nav.c $(KRY_GEN_DIR)/src/widgets/bottom_nav.c $(KRYON_DIR)/include/ui.h | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE \
		-Isrc -Isrc/app -Isrc/core -Isrc/screens -Isrc/screens/settings -Isrc/storage -Isrc/platform/android $(KRYON_INCLUDE) -I$(KRY_GEN_DIR) -I$(KRY_GEN_DIR)/src \
		-o $@ \
		tests/app_bottom_nav_test.c \
		$(KRY_GEN_DIR)/src/app/customize_nav.c \
		$(KRY_GEN_DIR)/src/widgets/bottom_nav.c

$(BREATH_TIMING_TEST): tests/breath_timing_test.c $(KRY_GEN_DIR)/src/core/breath_engine.c $(KRY_GEN_DIR)/src/core/types.c | $(TEST_BIN_DIR)
	$(CC) -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE \
		-Isrc/core $(KRYON_INCLUDE) -I$(KRY_GEN_DIR) -I$(KRY_GEN_DIR)/src \
		-o $@ \
		tests/breath_timing_test.c \
		$(KRY_GEN_DIR)/src/core/breath_engine.c

$(sort $(BUILD_OBJ_DIR) $(NATIVE_OBJ_DIR) $(NATIVE_BIN_DIR) $(NATIVE_DIST_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR) $(LINUX_APPIMAGE_BUILD_DIR) $(DEB_BUILD_DIR) $(DEB_DIST_DIR) $(RPM_BUILD_DIR) $(RPM_DIST_DIR) $(SNAP_BUILD_DIR) $(SNAP_DIST_DIR) $(FLATPAK_BUILD_DIR) $(FLATPAK_DIST_DIR) $(CLICK_BIN_DIR) $(CLICK_BUILD_DIR) $(CLICK_DIST_DIR) $(WINDOWS_DIST_DIR) $(ANDROID_BUILD_DIR) $(TEST_BIN_DIR) $(WEB_OBJ_DIR) $(WEB_DIST_DIR) $(CHROME_WEB_STORE_DIR) $(FIREFOX_ADDONS_DIR)):
	mkdir -p $@

$(WINDOWS_BIN_DIR)/$(WIN64_ARCH) $(WINDOWS_BIN_DIR)/$(WIN32_ARCH):
	mkdir -p $@

FORCE:

$(EMBEDDED_ASSETS_C): Makefile $(EMBEDDED_ASSET_FILES) $(KRYON_DIR)/scripts/embed-assets.sh | $(BUILD_OBJ_DIR)
	sh $(KRYON_DIR)/scripts/embed-assets.sh $@ $(EMBEDDED_ASSET_FILES)

$(KRYON_ICON_STAMP): FORCE $(KRYON_ICON_FILES) | $(BUILD_OBJ_DIR)
	@tmp="$@.tmp"; \
	for dir in $(KRYON_ICON_DIRS); do find "$(KRYON_DIR)/$$dir" -maxdepth 1 -type f -name '*.png' 2>/dev/null; done | LC_ALL=C sort | while IFS= read -r file; do sha256sum "$$file"; done > "$$tmp"; \
	if ! cmp -s "$$tmp" "$@"; then mv "$$tmp" "$@"; else rm "$$tmp"; fi

$(KRYON_ICON_ASSETS_C): $(KRYON_ICON_ASSETS_DEPS)
	@if [ "$(KRYON_ALLOW_ICON_REGEN)" != "1" ]; then \
		if [ ! -f "$@" ]; then \
			echo "Missing Kryon icon assets: $@"; \
			echo "Regenerate icons in the root Kryon checkout and update vendor/kryon intentionally."; \
			exit 1; \
		fi; \
		exit 0; \
	fi
	cd $(KRYON_DIR) && sh scripts/embed-icons.sh "$(KRYON_ICON_DIRS)" src/ui/ui_icon_assets.c

$(WEB_RAYLIB_A): web-tools-check
$(WEB_LIBOQS_A): web-tools-check

$(CLICK_LIBOQS_A): $(LIBOQS_DIR)/CMakeLists.txt
	rm -rf $(CLICK_LIBOQS_BUILD_DIR)
	$(CMAKE) -S $(LIBOQS_DIR) -B $(CLICK_LIBOQS_BUILD_DIR) \
		-DCMAKE_SYSTEM_NAME=Linux \
		-DCMAKE_SYSTEM_PROCESSOR=aarch64 \
		-DCMAKE_C_COMPILER=$(AARCH64_CC) \
		-DCMAKE_AR=$(AARCH64_AR) \
		-DCMAKE_RANLIB=$(AARCH64_RANLIB) \
		-DCMAKE_BUILD_TYPE=$(KRYON_LIBOQS_BUILD_TYPE) \
		-DBUILD_SHARED_LIBS=OFF \
		-DOQS_BUILD_ONLY_LIB=ON \
		-DOQS_USE_OPENSSL=OFF \
		-DOQS_DIST_BUILD=OFF \
		-DOQS_OPT_TARGET=generic \
		$(KRYON_LIBOQS_CPU_FEATURE_CMAKE_FLAGS) \
		-DOQS_MINIMAL_BUILD=$(KRYON_LIBOQS_MINIMAL_BUILD)
	$(CMAKE) --build $(CLICK_LIBOQS_BUILD_DIR) --target oqs

$(SQLITE_AMALGAMATION_C) $(SQLITE_AMALGAMATION_H): $(SQLITE_DIR)/configure $(SQLITE_DIR)/manifest | $(BUILD_OBJ_DIR)
	mkdir -p $(SQLITE_BUILD_DIR)
	cd $(SQLITE_BUILD_DIR) && $(abspath $(SQLITE_DIR))/configure
	$(MAKE) -C $(SQLITE_BUILD_DIR) sqlite3.c sqlite3.h

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

$(WIN64_LIBOQS_A): $(LIBOQS_DIR)/CMakeLists.txt
	rm -rf $(WIN64_LIBOQS_BUILD_DIR)
	$(CMAKE) -S $(LIBOQS_DIR) -B $(WIN64_LIBOQS_BUILD_DIR) \
		-DCMAKE_SYSTEM_NAME=Windows \
		-DCMAKE_SYSTEM_PROCESSOR=$(WIN64_CMAKE_SYSTEM_PROCESSOR) \
		-DCMAKE_C_COMPILER=$(WIN64_CC_PATH) \
		-DCMAKE_AR=$(WIN64_AR_PATH) \
		-DCMAKE_RANLIB=$(WIN64_RANLIB_PATH) \
		-DCMAKE_EXE_LINKER_FLAGS="$(WIN64_THREAD_LDFLAGS)" \
		-DCMAKE_BUILD_TYPE=$(KRYON_LIBOQS_BUILD_TYPE) \
		-DBUILD_SHARED_LIBS=OFF \
		-DOQS_BUILD_ONLY_LIB=ON \
		-DOQS_USE_OPENSSL=OFF \
		-DOQS_DIST_BUILD=OFF \
		-DOQS_OPT_TARGET=generic \
		$(KRYON_LIBOQS_CPU_FEATURE_CMAKE_FLAGS) \
		-DOQS_MINIMAL_BUILD=$(KRYON_LIBOQS_MINIMAL_BUILD)
	$(CMAKE) --build $(WIN64_LIBOQS_BUILD_DIR) --target oqs

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

$(WIN32_LIBOQS_A): $(LIBOQS_DIR)/CMakeLists.txt
	rm -rf $(WIN32_LIBOQS_BUILD_DIR)
	$(CMAKE) -S $(LIBOQS_DIR) -B $(WIN32_LIBOQS_BUILD_DIR) \
		-DCMAKE_SYSTEM_NAME=Windows \
		-DCMAKE_SYSTEM_PROCESSOR=$(WIN32_CMAKE_SYSTEM_PROCESSOR) \
		-DCMAKE_C_COMPILER=$(WIN32_CC_PATH) \
		-DCMAKE_AR=$(WIN32_AR_PATH) \
		-DCMAKE_RANLIB=$(WIN32_RANLIB_PATH) \
		-DCMAKE_EXE_LINKER_FLAGS="$(WIN32_THREAD_LDFLAGS)" \
		-DCMAKE_BUILD_TYPE=$(KRYON_LIBOQS_BUILD_TYPE) \
		-DBUILD_SHARED_LIBS=OFF \
		-DOQS_BUILD_ONLY_LIB=ON \
		-DOQS_USE_OPENSSL=OFF \
		-DOQS_DIST_BUILD=OFF \
		-DOQS_OPT_TARGET=generic \
		$(KRYON_LIBOQS_CPU_FEATURE_CMAKE_FLAGS) \
		-DOQS_MINIMAL_BUILD=$(KRYON_LIBOQS_MINIMAL_BUILD)
	$(CMAKE) --build $(WIN32_LIBOQS_BUILD_DIR) --target oqs

$(TARGET): Makefile $(SRC) $(KRYON_SRCS) $(KRYON_ICON_STAMP) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_FILES) $(EMBEDDED_ASSETS_C) $(RAYLIB_A) $(LIBOQS_A) $(CURL_PROTOCOL_CHECK) | $(NATIVE_BIN_DIR)
	$(CC) $(CFLAGS) \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(LIBOQS_INCLUDE) \
		$(RAY_CFLAGS) \
		-DHAS_LIBOQS=1 \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=0 \
		-o $@ \
		$(SRC) \
		$(KRYON_SRCS) \
		$(SQLITE_SRC) \
		$(RAYLIB_A) \
		$(LIBOQS_A) \
		$(RAY_LDLIBS) \
		$(RUNTIME_ASSET_LDLIBS) \
		$(DESKTOP_TRAY_LDLIBS) \
		$(NATIVE_SYSTEM_LDLIBS) \
		$(LDFLAGS)

$(KRYON_HOST_TARGET): Makefile $(KRYON_HOST_SRC) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_FILES) $(EMBEDDED_ASSETS_C) | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC -shared \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(LIBOQS_INCLUDE) \
		$(RAY_CFLAGS) \
		-DHAS_LIBOQS=1 \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=0 \
		-o $@ \
		$(KRYON_HOST_SRC) \
		$(SQLITE_SRC) \
		$(NATIVE_SYSTEM_LDLIBS) \
		-Wl,-Bsymbolic \
		-Wl,--allow-shlib-undefined \
		$(LDFLAGS)

$(CLICK_BIN): Makefile $(SRC) $(KRYON_CLICK_SRCS) $(KRYON_ICON_STAMP) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_FILES) $(EMBEDDED_ASSETS_C) $(CLICK_RAYLIB_A) $(CLICK_LIBOQS_A) | $(CLICK_BIN_DIR)
	$(AARCH64_CC) $(CLICK_CFLAGS) \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(CLICK_LIBOQS_INCLUDE) \
		$(AARCH64_RAY_CFLAGS) \
		-DHAS_LIBOQS=1 \
		-DPLATFORM_DESKTOP \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=0 \
		-o $@ \
		$(SRC) \
		$(KRYON_CLICK_SRCS) \
		$(SQLITE_SRC) \
		$(CLICK_RAYLIB_A) \
		$(CLICK_LIBOQS_A) \
		$(AARCH64_RAY_LDLIBS) \
		$(AARCH64_KRYON_CURL_LDLIBS) \
		-lm -lpthread -ldl -lrt \
		$(LDFLAGS)
	@if command -v patchelf >/dev/null; then \
		patchelf --set-interpreter "$(CLICK_PATCHELF_INTERPRETER)" --set-rpath '$$ORIGIN/../lib' $@; \
	fi

$(CLICK_TARGET): Makefile $(CLICK_BIN_INPUT) $(CLICK_DIR)/inbe.apparmor $(CLICK_DIR)/inbe.desktop $(CLICK_DIR)/inbe.metainfo.xml $(CLICK_RUNNER) $(LINUX_APPIMAGE_ICON) $(VERSION_FILE) | $(CLICK_BUILD_DIR) $(CLICK_DIST_DIR)
	@command -v click >/dev/null || { \
		echo "click is missing. Install click or put it on PATH."; \
		exit 1; \
	}
	rm -rf $(CLICK_ROOT)
	rm -f $(CLICK_DIST_DIR)/$(CLICK_PACKAGE)_*_$(CLICK_ARCH).click
	rm -f $(CLICK_DIST_DIR)/$(CLICK_ID)_*_$(CLICK_ARCH).click
	rm -f $(CLICK_ID)_$(APP_VERSION)_$(CLICK_ARCH).click
	mkdir -p $(CLICK_ROOT)/usr/bin $(CLICK_ROOT)/usr/lib $(CLICK_ROOT)/usr/share/applications $(CLICK_ROOT)/usr/share/icons/hicolor/512x512/apps $(CLICK_ROOT)/usr/share/metainfo
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
	printf '%s\n' \
		'{' \
		'  "name": "$(CLICK_ID)",' \
		'  "title": "$(CLICK_TITLE)",' \
		'  "version": "$(APP_VERSION)",' \
		'  "architecture": "$(CLICK_ARCH)",' \
		'  "framework": "$(CLICK_FRAMEWORK)",' \
		'  "description": "Syncable breathing, meditation, and habit practice app.",' \
		'  "maintainer": "$(CLICK_MAINTAINER)",' \
		'  "hooks": {' \
		'    "$(APP_NAME)": {' \
		'      "apparmor": "$(APP_NAME).apparmor",' \
		'      "desktop": "$(APP_NAME).desktop"' \
		'    }' \
		'  }' \
		'}' \
		> $(CLICK_ROOT)/manifest.json
	cp $(CLICK_DIR)/inbe.apparmor $(CLICK_ROOT)/inbe.apparmor
	cp $(CLICK_DIR)/inbe.desktop $(CLICK_ROOT)/inbe.desktop
	@if [ "$(CLICK_INCLUDE_METAINFO)" = "1" ]; then \
		mkdir -p $(CLICK_ROOT)/usr/share/metainfo; \
		sed -e 's/<release version="[^"]*"/<release version="$(APP_VERSION)"/' $(CLICK_DIR)/inbe.metainfo.xml > $(CLICK_ROOT)/usr/share/metainfo/$(CLICK_ID).metainfo.xml; \
	fi
	cp $(LINUX_APPIMAGE_ICON) $(CLICK_ROOT)/inbe.png
	cp $(LINUX_APPIMAGE_ICON) $(CLICK_ROOT)/usr/share/icons/hicolor/512x512/apps/inbe.png
	click build $(CLICK_ROOT) $(CLICK_DIST_DIR)
	@if [ -f "$(CLICK_DIST_DIR)/$(CLICK_ID)_$(APP_VERSION)_$(CLICK_ARCH).click" ]; then \
		mv "$(CLICK_DIST_DIR)/$(CLICK_ID)_$(APP_VERSION)_$(CLICK_ARCH).click" "$(CLICK_TARGET)"; \
	elif [ -f "$(CLICK_ID)_$(APP_VERSION)_$(CLICK_ARCH).click" ]; then \
		mv "$(CLICK_ID)_$(APP_VERSION)_$(CLICK_ARCH).click" "$(CLICK_TARGET)"; \
	fi
	test -f $@

$(WIN64_TARGET): Makefile $(SRC) $(KRYON_WINDOWS_SRCS) $(KRYON_ICON_STAMP) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_FILES) $(EMBEDDED_ASSETS_C) $(WIN64_RAYLIB_A) $(WIN64_CURL_A) $(WIN64_LIBOQS_A) | $(WINDOWS_BIN_DIR)/$(WIN64_ARCH)
	$(WIN64_CC) $(WINDOWS_CFLAGS) \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(WIN64_LIBOQS_INCLUDE) \
		-I$(WIN64_CURL_INCLUDE_DIR) \
		-DHAS_LIBOQS=1 \
		-DPLATFORM_DESKTOP \
		-DCURL_STATICLIB \
		-o $@ \
		$(SRC) \
		$(KRYON_WINDOWS_SRCS) \
		$(SQLITE_SRC) \
		$(WIN64_RAYLIB_A) \
		$(WIN64_CURL_A) \
		$(WIN64_LIBOQS_A) \
		$(WINDOWS_LDLIBS) \
		$(WIN64_THREAD_LDFLAGS) \
		$(WINDOWS_LDFLAGS)
	$(WIN64_STRIP) $@

$(WIN32_TARGET): Makefile $(SRC) $(KRYON_WINDOWS_SRCS) $(KRYON_ICON_STAMP) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_FILES) $(EMBEDDED_ASSETS_C) $(WIN32_RAYLIB_A) $(WIN32_CURL_A) $(WIN32_LIBOQS_A) | $(WINDOWS_BIN_DIR)/$(WIN32_ARCH)
	$(WIN32_CC) $(WINDOWS_CFLAGS) \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(WIN32_LIBOQS_INCLUDE) \
		-I$(WIN32_CURL_INCLUDE_DIR) \
		-DHAS_LIBOQS=1 \
		-DPLATFORM_DESKTOP \
		-DCURL_STATICLIB \
		-o $@ \
		$(SRC) \
		$(KRYON_WINDOWS_SRCS) \
		$(SQLITE_SRC) \
		$(WIN32_RAYLIB_A) \
		$(WIN32_CURL_A) \
		$(WIN32_LIBOQS_A) \
		$(WINDOWS_LDLIBS) \
		$(WIN32_THREAD_LDFLAGS) \
		$(WINDOWS_LDFLAGS)
	$(WIN32_STRIP) $@

$(APPIMAGE_TARGET): $(TARGET) $(LINUX_APPIMAGE_APPRUN) $(LINUX_APPIMAGE_DESKTOP) $(LINUX_APPIMAGE_ICON) $(LINUX_APPIMAGE_APPDATA) | $(LINUX_DIST_DIR) $(LINUX_APPIMAGE_BUILD_DIR)
	@command -v linuxdeploy-plugin-appimage >/dev/null || { \
		echo "linuxdeploy-plugin-appimage is missing. Install it or put it on PATH."; \
		exit 1; \
	}
	@command -v appimagetool >/dev/null || { \
		echo "appimagetool is missing. Install it or put it on PATH."; \
		exit 1; \
	}
	@command -v patchelf >/dev/null || { \
		echo "patchelf is missing. Install it or put it on PATH."; \
		exit 1; \
	}
	rm -rf $(LINUX_APPDIR)
	rm -rf $(LINUX_DIST_DIR)/*.AppDir
	rm -f $(LINUX_DIST_DIR)/*.AppImage
	mkdir -p $(LINUX_APPDIR)/usr/bin $(LINUX_APPDIR)/usr/lib $(LINUX_APPDIR)/usr/share/applications $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps $(LINUX_APPDIR)/usr/share/metainfo $(LINUX_APPDIR)/usr/share/appdata
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
	cp $(LINUX_APPIMAGE_APPDATA) $(LINUX_APPDIR)/usr/share/metainfo/$(ANDROID_APP_ID).metainfo.xml
	cp $(LINUX_APPIMAGE_APPDATA) $(LINUX_APPDIR)/usr/share/appdata/$(ANDROID_APP_ID).appdata.xml
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/$(APP_NAME).png
	cp $(LINUX_APPIMAGE_ICON) $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png
	@# Manually copy critical X11/OpenGL libraries that linuxdeploy might miss
	@for lib in libX11.so.6 libXext.so.6 libdrm.so.2 libgbm.so.1 libEGL.so.1 libGLESv2.so.2 libGLdispatch.so.0 libglapi.so.0; do \
		found=$$(find /usr/lib /lib -name "$$lib" 2>/dev/null | head -n 1); \
		if [ -n "$$found" ]; then \
			echo "Copying $$lib from $$found"; \
			cp "$$found" $(LINUX_APPDIR)/usr/lib/ 2>/dev/null || true; \
		fi; \
	done
	@# Detect if running on NixOS (by checking if loader is in /nix/store)
	@loader=$$(LC_ALL=C readelf -l $(TARGET) | sed -n 's#.*Requesting program interpreter: \(.*\)]#\1#p'); \
	if printf '%s\n' "$$loader" | grep -q '^/nix/store/.*glibc.*/'; then \
		LIBRARY_FLAGS=""; \
		echo "Building on NixOS - linuxdeploy will auto-detect libraries from /nix/store"; \
	else \
		LIBRARY_FLAGS=""; \
		echo "Building on FHS system - linuxdeploy will auto-detect libraries"; \
	fi; \
	cd $(LINUX_APPIMAGE_BUILD_DIR) && env -u SOURCE_DATE_EPOCH ARCH=$(ARCH) LDAI_OUTPUT=$(abspath $(APPIMAGE_TARGET)) $(LINUXDEPLOY) \
		--appdir $(APP_NAME).AppDir \
		--executable $(abspath $(LINUX_APPDIR)/usr/bin/$(APP_NAME)) \
		--desktop-file $(abspath $(LINUX_APPIMAGE_DESKTOP)) \
		--icon-file $(abspath $(LINUX_APPDIR)/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png) \
		$$LIBRARY_FLAGS \
		--output appimage
	test -f $@

deb-check:
	@command -v dpkg-deb >/dev/null 2>&1 || { \
		echo "dpkg-deb is missing. On FreeBSD install it with: pkg install dpkg"; \
		echo "To build a Debian package on FreeBSD, pass DEB_BIN_SOURCE=/path/to/linux/inbe."; \
		exit 1; \
	}
	@if [ -z "$(strip $(DEB_BIN_INPUT))" ]; then \
		echo "No Linux binary is available for the Debian package."; \
		echo "Run this target on Linux, or on FreeBSD pass DEB_BIN_SOURCE=/path/to/linux/inbe."; \
		exit 1; \
	fi

$(DEB_TARGET): $(DEB_TARGET_PREREQS) deb-check | $(DEB_BUILD_DIR) $(DEB_DIST_DIR)
	rm -rf $(DEB_ROOT)
	mkdir -p $(DEB_ROOT)/DEBIAN $(DEB_ROOT)/usr/bin $(DEB_ROOT)/usr/share/applications $(DEB_ROOT)/usr/share/icons/hicolor/512x512/apps $(DEB_ROOT)/usr/share/metainfo
	cp $(DEB_BIN_INPUT) $(DEB_ROOT)/usr/bin/$(APP_NAME)
	chmod 755 $(DEB_ROOT)/usr/bin/$(APP_NAME)
	cp $(LINUX_APPIMAGE_DESKTOP) $(DEB_ROOT)/usr/share/applications/$(APP_NAME).desktop
	cp $(LINUX_APPIMAGE_ICON) $(DEB_ROOT)/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png
	sed -e 's/<release version="[^"]*"/<release version="$(APP_VERSION)"/' \
		$(LINUX_APPIMAGE_APPDATA) > $(DEB_ROOT)/usr/share/metainfo/$(ANDROID_APP_ID).metainfo.xml
	@installed_size=$$(find $(DEB_ROOT)/usr -type f -exec wc -c {} + | awk '$$2 != "total" { bytes += $$1 } END { print int((bytes + 1023) / 1024) }'); \
	{ \
		printf 'Package: %s\n' '$(DEB_PACKAGE_NAME)'; \
		printf 'Version: %s\n' '$(APP_VERSION)'; \
		printf 'Architecture: %s\n' '$(DEB_ARCH)'; \
		printf 'Maintainer: %s\n' '$(DEB_MAINTAINER)'; \
		printf 'Section: %s\n' '$(DEB_SECTION)'; \
		printf 'Priority: %s\n' '$(DEB_PRIORITY)'; \
		printf 'Installed-Size: %s\n' "$$installed_size"; \
		printf 'Depends: %s\n' '$(DEB_DEPENDS)'; \
		printf 'Homepage: %s\n' '$(APP_WWW)'; \
		printf 'Description: %s\n' '$(APP_COMMENT)'; \
		printf ' %s\n' '$(APP_DESC)'; \
	} > $(DEB_ROOT)/DEBIAN/control
	rm -f $(DEB_DIST_DIR)/$(DEB_PACKAGE_NAME)_*_$(DEB_ARCH).deb
	dpkg-deb --build --root-owner-group $(DEB_ROOT) $(DEB_TARGET)
	test -f $@

rpm-check:
	@command -v rpmbuild >/dev/null 2>&1 || { \
		echo "rpmbuild is missing. On FreeBSD install it with: pkg install rpm4"; \
		echo "To build an RPM package on FreeBSD, pass RPM_BIN_SOURCE=/path/to/linux/inbe."; \
		exit 1; \
	}
	@if [ -z "$(strip $(RPM_BIN_INPUT))" ]; then \
		echo "No Linux binary is available for the RPM package."; \
		echo "Run this target on Linux, or on FreeBSD pass RPM_BIN_SOURCE=/path/to/linux/inbe."; \
		exit 1; \
	fi

$(RPM_TARGET): $(RPM_TARGET_PREREQS) rpm-check | $(RPM_BUILD_DIR) $(RPM_DIST_DIR)
	rm -rf $(RPM_TOPDIR)
	mkdir -p $(RPM_TOPDIR)/BUILD $(RPM_TOPDIR)/BUILDROOT $(RPM_TOPDIR)/RPMS $(RPM_TOPDIR)/SOURCES $(RPM_TOPDIR)/SPECS $(RPM_TOPDIR)/SRPMS
	{ \
		printf '%s\n' 'Name: $(RPM_PACKAGE_NAME)'; \
		printf '%s\n' 'Version: $(APP_VERSION)'; \
		printf '%s\n' 'Release: $(RPM_RELEASE)%{?dist}'; \
		printf '%s\n' 'Summary: $(APP_COMMENT)'; \
		printf '%s\n' 'License: $(RPM_LICENSE)'; \
		printf '%s\n' 'URL: $(APP_WWW)'; \
		printf '%s\n' 'Requires: $(RPM_REQUIRES)'; \
		printf '%s\n' ''; \
		printf '%s\n' '%description'; \
		printf '%s\n' '$(APP_DESC)'; \
		printf '%s\n' ''; \
		printf '%s\n' '%prep'; \
		printf '%s\n' ''; \
		printf '%s\n' '%build'; \
		printf '%s\n' ''; \
		printf '%s\n' '%install'; \
		printf '%s\n' 'rm -rf %{buildroot}'; \
		printf '%s\n' 'mkdir -p %{buildroot}/usr/bin %{buildroot}/usr/share/applications %{buildroot}/usr/share/icons/hicolor/512x512/apps %{buildroot}/usr/share/metainfo'; \
		printf '%s\n' 'cp "$(abspath $(RPM_BIN_INPUT))" %{buildroot}/usr/bin/$(APP_NAME)'; \
		printf '%s\n' 'chmod 755 %{buildroot}/usr/bin/$(APP_NAME)'; \
		printf '%s\n' 'cp "$(abspath $(LINUX_APPIMAGE_DESKTOP))" %{buildroot}/usr/share/applications/$(APP_NAME).desktop'; \
		printf '%s\n' 'cp "$(abspath $(LINUX_APPIMAGE_ICON))" %{buildroot}/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png'; \
		printf '%s\n' 'sed -e '\''s/<release version="[^"]*"/<release version="$(APP_VERSION)"/'\'' "$(abspath $(LINUX_APPIMAGE_APPDATA))" > %{buildroot}/usr/share/metainfo/$(ANDROID_APP_ID).metainfo.xml'; \
		printf '%s\n' ''; \
		printf '%s\n' '%files'; \
		printf '%s\n' '/usr/bin/$(APP_NAME)'; \
		printf '%s\n' '/usr/share/applications/$(APP_NAME).desktop'; \
		printf '%s\n' '/usr/share/icons/hicolor/512x512/apps/$(APP_NAME).png'; \
		printf '%s\n' '/usr/share/metainfo/$(ANDROID_APP_ID).metainfo.xml'; \
	} > $(RPM_SPEC)
	rpmbuild -bb $(RPM_SPEC) \
		--target $(RPM_ARCH) \
		--define '_topdir $(abspath $(RPM_TOPDIR))' \
		--define '_build_id_links none'
	rm -f $(RPM_DIST_DIR)/$(RPM_PACKAGE_NAME)-*-$(RPM_RELEASE).$(RPM_ARCH).rpm
	created=$$(find $(RPM_TOPDIR)/RPMS -type f -name '$(RPM_PACKAGE_NAME)-$(APP_VERSION)-$(RPM_RELEASE)*.$(RPM_ARCH).rpm' | head -n 1); \
	if [ -z "$$created" ]; then echo "rpmbuild did not produce an RPM"; exit 1; fi; \
	cp "$$created" $(RPM_TARGET)
	test -f $@

podman-check:
	@$(PODMAN) --version >/dev/null 2>&1 || { \
		echo "podman check failed. Install podman, run with root privileges on FreeBSD, or set PODMAN=/path/to/podman."; \
		exit 1; \
	}

$(SNAP_TARGET): Makefile packaging/snap/snap/snapcraft.yaml | $(SNAP_BUILD_DIR) $(SNAP_DIST_DIR) podman-check
	rm -f $(SNAP_DIST_DIR)/*.snap
	$(PODMAN) run $(PODMAN_RUN_PLATFORM) $(PODMAN_RUN_NETWORK) --rm --privileged \
		-v "$(SNAP_APT_CACHE_VOLUME):/var/cache/apt/archives" \
		-v "$(SNAP_ROOT_CACHE_VOLUME):/root/.cache" \
		-v "$(abspath .):/work" \
		-w /work \
		--entrypoint "$(SNAP_ENTRYPOINT)" \
		$(SNAP_IMAGE) \
		-lc 'set -eu; printf "%s\n" "APT::Cache-Start \"100000000\";" > /etc/apt/apt.conf.d/99cache-start; apt-get update; rm -rf /tmp/inbe-snap; cp -a /work /tmp/inbe-snap; cd /tmp/inbe-snap; rm -rf build snap; mkdir snap; cp packaging/snap/snap/snapcraft.yaml snap/snapcraft.yaml; sed -i "s/^version:.*/version: '\''$(APP_VERSION)'\''/" snap/snapcraft.yaml; snapcraft pack --destructive-mode; cp *.snap /work/$(SNAP_DIST_DIR)/'
	created=$$(find $(SNAP_DIST_DIR) -maxdepth 1 -type f -name '*.snap' | head -n 1); \
	if [ -z "$$created" ]; then echo "snapcraft did not produce a snap"; exit 1; fi; \
	mv "$$created" $(SNAP_TARGET)
	test -f $@

$(FLATPAK_TARGET): Makefile $(FLATPAK_MANIFEST) | $(FLATPAK_BUILD_DIR) $(FLATPAK_DIST_DIR) podman-check
	rm -f $(FLATPAK_DIST_DIR)/*.flatpak
	$(PODMAN) run $(PODMAN_RUN_PLATFORM) $(PODMAN_RUN_NETWORK) --rm --privileged \
		-v "$(abspath .):/work" \
		-w /work \
		$(FLATPAK_IMAGE) \
		sh -lc 'set -eu; rm -rf .flatpak-builder $(FLATPAK_BUILD_DIR)/repo $(FLATPAK_BUILD_DIR)/build-dir; flatpak-builder --disable-rofiles-fuse --force-clean --repo=$(FLATPAK_BUILD_DIR)/repo $(FLATPAK_BUILD_DIR)/build-dir $(FLATPAK_MANIFEST) || { rm -rf $(FLATPAK_BUILD_DIR)/repo $(FLATPAK_BUILD_DIR)/build-dir vendor-builds/linux build/bin/linux; make vendor-prebuilds-native; make native; flatpak build-init $(FLATPAK_BUILD_DIR)/build-dir $(APP_ID) org.gnome.Sdk org.gnome.Platform 46; install -D -m755 "$$(find build/bin/linux -maxdepth 1 -type f -name '\''inbe-linux-*'\'' | head -n 1)" $(FLATPAK_BUILD_DIR)/build-dir/files/bin/inbe; install -D -m644 packaging/linux/appimage/inbe.desktop $(FLATPAK_BUILD_DIR)/build-dir/files/share/applications/$(APP_ID).desktop; sed -i '\''s/^Icon=.*/Icon=$(APP_ID)/'\'' $(FLATPAK_BUILD_DIR)/build-dir/files/share/applications/$(APP_ID).desktop; install -D -m644 packaging/linux/appimage/inbe.png $(FLATPAK_BUILD_DIR)/build-dir/files/share/icons/hicolor/512x512/apps/$(APP_ID).png; install -D -m644 packaging/linux/appimage/inbe.appdata.xml $(FLATPAK_BUILD_DIR)/build-dir/files/share/metainfo/$(APP_ID).metainfo.xml; flatpak build-finish --share=ipc --share=network --socket=fallback-x11 --socket=wayland --socket=pulseaudio --device=dri --filesystem=home $(FLATPAK_BUILD_DIR)/build-dir; flatpak build-export $(FLATPAK_BUILD_DIR)/repo $(FLATPAK_BUILD_DIR)/build-dir; }; flatpak build-bundle $(FLATPAK_BUILD_DIR)/repo $(FLATPAK_TARGET) $(APP_ID)'
	test -f $@

$(WEB_JS_TARGET): Makefile $(WEB_SRC) $(KRYON_WEB_SRCS) $(KRYON_ICON_STAMP) $(SQLITE_SRC) $(SQLITE_AMALGAMATION_H) $(FONT_FILES) $(EMBEDDED_ASSETS_C) $(WEB_RAYLIB_A) $(WEB_LIBOQS_A) web-tools-check | $(WEB_DIST_DIR)
	rm -f $(WEB_DIST_DIR)/index.data
	$(WEB_CC) $(WEB_CFLAGS) \
		$(APP_INCLUDE) \
		$(KRYON_INCLUDE) \
		$(SQLITE_INCLUDE) \
		$(WEB_LIBOQS_INCLUDE) \
		-DHAS_LIBOQS=1 \
		-DPLATFORM_WEB \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-DSUPPORT_FILEFORMAT_MP3=0 \
		-o $(WEB_JS_TARGET) \
		$(WEB_SRC) \
		$(KRYON_WEB_SRCS) \
		$(SQLITE_SRC) \
		$(WEB_RAYLIB_A) \
		$(WEB_LIBOQS_A) \
		-sUSE_GLFW=3 \
		-sASYNCIFY \
		-sFORCE_FILESYSTEM=1 \
		-sFETCH=1 \
		-sALLOW_MEMORY_GROWTH=0 \
		-sINITIAL_MEMORY=268435456 \
		-sSTACK_SIZE=33554432 \
		-sGLOBAL_BASE=67108864 \
		-sASYNCIFY_STACK_SIZE=1048576 \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_app_web_get_play_in_background,_app_web_set_backgrounded,_app_web_background_tick,_app_web_launch_practice,_app_web_test_save_onboarding_state,_app_web_test_onboarding_state,_app_web_test_sync_key_state,_app_web_test_import_sync_key \
		-lidbfs.js \
		-lm

$(WEB_TARGET): src/web_shell.html $(WEB_BOOT_JS) $(WEB_JS_TARGET) manifest.json $(WEB_ASSET_FILES) $(MEDITATION_AUDIO_ZIP) validate-meditation-audio | $(WEB_DIST_DIR)
	perl -0pe 's#\{\{\{ APP_SCRIPT \}\}\}#$(WEB_APP_SCRIPT)#g; s/WEB_CACHE_BUSTER/$(WEB_CACHE_BUSTER)/g' src/web_shell.html > $@
	cp $(WEB_BOOT_JS) $(WEB_DIST_DIR)/index_boot.js
	perl -0pi -e 's/WEB_CACHE_BUSTER/$(WEB_CACHE_BUSTER)/g' $(WEB_DIST_DIR)/index_boot.js
	rm -rf $(WEB_DIST_DIR)/web-assets $(WEB_DIST_DIR)/site-icons
	cp -R web-assets $(WEB_DIST_DIR)/
	cp -R site-icons $(WEB_DIST_DIR)/
	cp manifest.json $(WEB_DIST_DIR)/webmanifest.json

android-copy-assets:
	$(MAKE) $(FONT_FILES)
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
	@if [ -z "$(ANDROID_SDK)" ]; then \
		echo "Set ANDROID_SDK_ROOT or ANDROID_HOME to your Android SDK path."; \
		exit 1; \
	fi
	@if [ ! -d "$(ANDROID_SDK)" ]; then \
		echo "Android SDK not found: $(ANDROID_SDK)"; \
		echo "Set ANDROID_SDK_ROOT or ANDROID_HOME to your Android SDK path."; \
		exit 1; \
	fi
	@printf 'sdk.dir=%s\ncmake.dir=%s\n' "$(ANDROID_SDK)" "$(ANDROID_CMAKE_DIR)" > droid/local.properties

android-debug: android-copy-assets android-local-properties
	$(ANDROID_GRADLE_ENV) $(GRADLE) -p droid assembleDebug $(ANDROID_GRADLE_ARGS)
	$(MAKE) android-copy-debug-apks

android-release:
	$(MAKE) android-check-keystore PASSWORD="$(PASSWORD)"
	$(MAKE) android-copy-assets
	$(MAKE) android-local-properties
	@if [ -n "$(PASSWORD)" ]; then \
		$(ANDROID_GRADLE_ENV) $(GRADLE) -p droid assembleRelease -Pkeystore.path="$(ANDROID_KEYSTORE)" -Pkeystore.alias="$(ANDROID_KEY_ALIAS)" -Pkeystore.password="$(PASSWORD)" $(ANDROID_GRADLE_ARGS) || exit $$?; \
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
		$(ANDROID_GRADLE_ENV) $(GRADLE) -p droid bundleRelease -Pkeystore.path="$(ANDROID_KEYSTORE)" -Pkeystore.alias="$(ANDROID_KEY_ALIAS)" -Pkeystore.password="$(PASSWORD)" $(ANDROID_GRADLE_ARGS) || exit $$?; \
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
	@ABI=$$($(ADB) shell getprop ro.product.cpu.abi | tr -d '\r'); \
	APK=droid/app/build/outputs/apk/debug/app-$${ABI}-debug.apk; \
	if [ ! -f "$$APK" ]; then APK=droid/app/build/outputs/apk/debug/app-debug.apk; fi; \
	$(ADB) install -r "$$APK"; \
	$(ADB) shell am start -n $(ANDROID_APP_ID)/$(ANDROID_ACTIVITY)

android-install-release: android-release
	@ABI=$$($(ADB) shell getprop ro.product.cpu.abi | tr -d '\r'); \
	APK=droid/app/build/outputs/apk/release/app-$${ABI}-release.apk; \
	if [ ! -f "$$APK" ]; then APK=droid/app/build/outputs/apk/release/app-release.apk; fi; \
	$(ADB) install -r "$$APK"; \
	$(ADB) shell am start -n $(ANDROID_APP_ID)/$(ANDROID_ACTIVITY)

android-avd:
	@if [ "$(UNAME_S)" = "FreeBSD" ]; then \
		HOME="$${ANDROID_TOOL_HOME:-/tmp/inbe-android-home}" ANDROID_SDK_ROOT="$${ANDROID_SDK_WORK_ROOT:-$${ANDROID_SDK_ROOT:-$${ANDROID_HOME:-/tmp/android-sdk}}}" ANDROID_HOME="$${ANDROID_SDK_WORK_ROOT:-$${ANDROID_SDK_ROOT:-$${ANDROID_HOME:-/tmp/android-sdk}}}" bash scripts/emulator.sh; \
	else \
		bash scripts/emulator.sh; \
	fi
	@adb_cmd="$$HOME/.android-sdk-writable/platform-tools/adb"; \
	if [ "$(UNAME_S)" = "FreeBSD" ]; then adb_cmd="$${ANDROID_SDK_WORK_ROOT:-$${ANDROID_SDK_ROOT:-$${ANDROID_HOME:-/tmp/android-sdk}}}/platform-tools/adb"; fi; \
	if [ ! -x "$$adb_cmd" ]; then adb_cmd="$${ANDROID_SDK_ROOT:-$${ANDROID_HOME}}/platform-tools/adb"; fi; \
	if [ ! -x "$$adb_cmd" ]; then adb_cmd=adb; fi; \
	$(MAKE) android-install ADB="$$adb_cmd -e"

android-smoke:
	@mkdir -p build/android
	sh scripts/android-smoke-test.sh "$(ANDROID_SDK)" "$(ANDROID_APP_ID)" "$(ANDROID_ACTIVITY)"

android-clean:
	$(GRADLE) -p droid clean $(ANDROID_GRADLE_ARGS)
	rm -rf $(ANDROID_BUILD_DIR)

validate-meditation-audio:
	@set -e; \
	for file in $(MEDITATION_AUDIO_TRACKS); do \
		path="$(UNPACKAGED_AUDIO_DIR)/$$file"; \
		if [ ! -f "$$path" ]; then \
			echo "Missing meditation audio track: $$path"; \
			exit 1; \
		fi; \
		size=$$(wc -c < "$$path" | tr -d ' '); \
		magic=$$(dd if="$$path" bs=4 count=1 2>/dev/null); \
		if [ "$$magic" != "OggS" ] || [ "$$size" -lt 4096 ]; then \
			echo "Invalid meditation audio track: $$path"; \
			echo "Expected a real Ogg file. If this is a Git LFS pointer, run git lfs pull."; \
			exit 1; \
		fi; \
	done

$(MEDITATION_AUDIO_ZIP): $(UNPACKAGED_AUDIO_FILES)
	$(MAKE) validate-meditation-audio
	mkdir -p $(dir $(MEDITATION_AUDIO_ZIP))
	rm -f $(MEDITATION_AUDIO_ZIP)
	cd $(UNPACKAGED_AUDIO_DIR) && zip -9 -r $(abspath $(MEDITATION_AUDIO_ZIP)) $(MEDITATION_AUDIO_TRACKS) LICENSE.md MANIFEST.txt

package-unpackaged-assets: validate-meditation-audio $(MEDITATION_AUDIO_ZIP)

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
	$(MAKE) validate-meditation-audio
	$(MAKE) $(WEB_TARGET)
	$(MAKE) web-smoke-test
	rm -f $(WEB_DIST_ZIP)
	cd $(WEB_DIST_DIR) && zip -9 -r $(abspath $(WEB_DIST_ZIP)) .

web-smoke-test: $(WEB_SMOKE_TEST)
	WEB_SMOKE_BROWSER="$(WEB_SMOKE_BROWSER)" node $(WEB_SMOKE_TEST) $(WEB_DIST_DIR)

web-smoke-test-firefox: $(WEB_SMOKE_TEST)
	WEB_SMOKE_BROWSER="firefox" node $(WEB_SMOKE_TEST) $(WEB_DIST_DIR)

web-smoke-test-librewolf: $(WEB_SMOKE_TEST)
	WEB_SMOKE_BROWSER="librewolf" WEB_SMOKE_ALLOW_WEBGL_DISABLED=1 node $(WEB_SMOKE_TEST) $(WEB_DIST_DIR)

site: web
	sh site/build.sh

chrome-web-store: $(CHROME_WEB_STORE_ZIP)

$(CHROME_WEB_STORE_ZIP): $(WEB_TARGET) $(CHROME_WEB_STORE_MANIFEST) $(CHROME_WEB_STORE_WORKER) $(CHROME_WEB_STORE_ICONS) | $(CHROME_WEB_STORE_DIR)
	rm -rf $(CHROME_WEB_STORE_DIR)
	mkdir -p $(CHROME_WEB_STORE_DIR)/icons
	cp -R $(WEB_DIST_DIR)/. $(CHROME_WEB_STORE_DIR)/
	sed -e 's#__APP_VERSION__#$(APP_VERSION)#g' \
		$(CHROME_WEB_STORE_MANIFEST) > $(CHROME_WEB_STORE_DIR)/manifest.json
	cp $(CHROME_WEB_STORE_WORKER) $(CHROME_WEB_STORE_DIR)/service_worker.js
	cp $(CHROME_WEB_STORE_ICONS) $(CHROME_WEB_STORE_DIR)/icons/
	rm -f $(CHROME_WEB_STORE_ZIP)
	cd $(CHROME_WEB_STORE_DIR) && zip -9 -r $(abspath $(CHROME_WEB_STORE_ZIP)) .

firefox-addons: $(FIREFOX_ADDONS_ZIP)

$(FIREFOX_ADDONS_ZIP): $(WEB_TARGET) src/web_shell.html $(FIREFOX_ADDONS_MANIFEST) $(FIREFOX_ADDONS_BACKGROUND) $(FIREFOX_ADDONS_LOADER) $(FIREFOX_ADDONS_ICONS) | $(FIREFOX_ADDONS_DIR)
	rm -rf $(FIREFOX_ADDONS_DIR)
	mkdir -p $(FIREFOX_ADDONS_DIR)/icons
	cp -R $(WEB_DIST_DIR)/. $(FIREFOX_ADDONS_DIR)/
	sed -e 's#__APP_VERSION__#$(APP_VERSION)#g' \
		$(FIREFOX_ADDONS_MANIFEST) > $(FIREFOX_ADDONS_DIR)/manifest.json
	cp $(FIREFOX_ADDONS_BACKGROUND) $(FIREFOX_ADDONS_DIR)/background.js
	cp $(FIREFOX_ADDONS_LOADER) $(FIREFOX_ADDONS_DIR)/extension_loader.js
	perl -0pe 's#\{\{\{ APP_SCRIPT \}\}\}#$(FIREFOX_ADDONS_APP_SCRIPT)#g; s/WEB_CACHE_BUSTER/$(WEB_CACHE_BUSTER)/g' src/web_shell.html > $(FIREFOX_ADDONS_INDEX)
	cp $(FIREFOX_ADDONS_ICONS) $(FIREFOX_ADDONS_DIR)/icons/
	rm -f $(FIREFOX_ADDONS_ZIP)
	cd $(FIREFOX_ADDONS_DIR) && zip -9 -r $(abspath $(FIREFOX_ADDONS_ZIP)) .

firefox-addons-lint: $(FIREFOX_ADDONS_ZIP)
	$(ADDONS_LINTER) $(FIREFOX_ADDONS_ZIP)

firefox-addons-source-zip:
	sh scripts/build-firefox-addons-source-zip.sh $(FIREFOX_ADDONS_SOURCE_ZIP)

verify-firefox-addons: firefox-addons-lint firefox-addons-source-zip

clean:
	rm -rf build

clean-linux:
	rm -rf $(LINUX_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR)

clean-native:
	rm -rf $(NATIVE_OBJ_DIR) $(NATIVE_BIN_DIR) $(NATIVE_DIST_DIR) $(NATIVE_VENDOR_BUILD_DIR)

clean-vendor-builds:
	rm -rf $(VENDOR_BUILD_DIR)

NEEDS_DEB_NATIVE := $(if $(strip $(DEB_BIN_SOURCE)),,$(if $(filter linux,$(NATIVE_PLATFORM)),$(filter deb package-deb,$(MAKECMDGOALS))))
NEEDS_RPM_NATIVE := $(if $(strip $(RPM_BIN_SOURCE)),,$(if $(filter linux,$(NATIVE_PLATFORM)),$(filter rpm package-rpm,$(MAKECMDGOALS))))
NEEDS_NATIVE_ENV := $(if $(MAKECMDGOALS),$(filter all native install install-user stage package-freebsd run run-fresh dist appimage vendor-prebuilds vendor-prebuilds-native,$(MAKECMDGOALS)) $(NEEDS_DEB_NATIVE) $(NEEDS_RPM_NATIVE),native)
ifneq ($(strip $(NEEDS_NATIVE_ENV)),)
ifeq ($(strip $(RAY_CFLAGS)),)
$(error RAY_CFLAGS is not set. Install pkg-config metadata for $(RAY_PKGS), or set RAY_CFLAGS explicitly)
endif
ifeq ($(strip $(RAY_LDLIBS)),)
$(error RAY_LDLIBS is not set. Install pkg-config metadata for $(RAY_PKGS), or set RAY_LDLIBS explicitly)
endif
ifeq ($(strip $(RAY_SDL_LDLIBS)),)
$(error RAY_SDL_LDLIBS is not set. Install SDL2 development files or set RAY_SDL_LDLIBS explicitly)
endif
ifeq ($(strip $(RAY_SDL_INCLUDE_DIR)),)
$(error RAY_SDL_INCLUDE_DIR is not set. Install SDL2 development files or set RAY_SDL_INCLUDE_DIR explicitly)
endif
ifeq ($(strip $(RAY_RAYLIB_CONFIG)),)
$(error RAY_RAYLIB_CONFIG is not set. Set RAY_RAYLIB_CONFIG explicitly)
endif
ifeq ($(strip $(KRYON_CURL_LDLIBS)),)
$(error libcurl metadata is missing. Install libcurl pkg-config metadata or set KRYON_CURL_CFLAGS/KRYON_CURL_LDLIBS explicitly)
endif
ifneq ($(shell v='$(KRYON_CURL_VERSION_HEX)'; if [ "$$v" = 075600 ] || [ "$$v" \> 075600 ]; then echo yes; fi),yes)
$(error libcurl >= 7.86.0 is required for websocket sync; found LIBCURL_VERSION_NUM=$(KRYON_CURL_VERSION_NUM))
endif
endif

NEEDS_CLICK_ENV := $(filter click click-verify,$(MAKECMDGOALS))
ifneq ($(strip $(NEEDS_CLICK_ENV)),)
ifeq ($(strip $(CLICK_BIN_SOURCE)),)
ifeq ($(strip $(AARCH64_CC)),)
$(error AARCH64_CC is not set. Install an AArch64 cross compiler, set AARCH64_CC, or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_AR)),)
$(error AARCH64_AR is not set. Install AArch64 binutils, set AARCH64_AR, or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RANLIB)),)
$(error AARCH64_RANLIB is not set. Install AArch64 binutils, set AARCH64_RANLIB, or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RAY_CFLAGS)),)
$(error AARCH64_RAY_CFLAGS is not set. Set AARCH64_RAY_CFLAGS for your cross sysroot, or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RAY_LDLIBS)),)
$(error AARCH64_RAY_LDLIBS is not set. Set AARCH64_RAY_LDLIBS for your cross sysroot, or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_RAY_SDL_INCLUDE_DIR)),)
$(error AARCH64_RAY_SDL_INCLUDE_DIR is not set. Set AARCH64_RAY_SDL_INCLUDE_DIR for your cross sysroot, or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
ifeq ($(strip $(AARCH64_KRYON_CURL_LDLIBS)),)
$(error AARCH64_KRYON_CURL_LDLIBS is not set. Set AARCH64_KRYON_CURL_LDLIBS for your cross sysroot, or pass CLICK_BIN_SOURCE=/path/to/inbe)
endif
endif
endif
