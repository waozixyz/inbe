CC = gcc
WINDRES = windres

SRC = src/main.c src/app.c src/app_session.c src/locale.c src/theme_meta.c src/theme.c src/data.c src/miniz.c src/android/android_share.c src/tabs/history_tab.c src/tabs/language_tab.c src/tabs/manual_tab.c src/tabs/settings_tab.c
INBE_DIR = libinbe
INBE_A = $(INBE_DIR)/libinbe.a

RAYLIB_DIR = vendor/raylib/src
RAYLIB_BUILD_DIR = vendor/raylib/build/sdl
RAYLIB_A = $(RAYLIB_BUILD_DIR)/libraylib.a
RAYLIB_SOURCES = $(wildcard $(RAYLIB_DIR)/*.c) $(wildcard $(RAYLIB_DIR)/*.h)

FLINT_DIR = vendor/flint
FLINT_SRCS = $(wildcard $(FLINT_DIR)/src/*.c)
FLINT_INCLUDE = -I$(FLINT_DIR)/include

ANDROID_DIR = droid
GRADLE = gradle
ANDROID_APP_ID = xyz.waozi.inbe
ANDROID_ACTIVITY = android.app.NativeActivity

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
else ifeq ($(UNAME_S),Darwin)
    PLATFORM = macos
else
    PLATFORM = unknown
endif

ifeq ($(UNAME_M),x86_64)
    ARCH = x86_64
else ifeq ($(UNAME_M),aarch64)
    ARCH = aarch64
else ifeq ($(UNAME_M),armv7l)
    ARCH = arm
else
    ARCH = $(UNAME_M)
endif

BUILD_DIR = build
LINUX_BUILD_DIR = $(BUILD_DIR)/linux
WINDOWS_BUILD_DIR = $(BUILD_DIR)/windows
ANDROID_BUILD_DIR = $(BUILD_DIR)/android
WEB_BUILD_DIR = $(BUILD_DIR)/web
LINUX_ARCHES = x86_64 aarch64
ANDROID_DIST ?= release

INSTALL_DIR = $(HOME)/.local/share/inbe
BIN_DIR = $(HOME)/bin
TARBALL = $(LINUX_BUILD_DIR)/inbe-linux.tar.gz

CONFIG_FILES = inbe.ini theme.ini
LOCALE_FILES = $(wildcard locales/*.txt)
THEME_FILES = $(wildcard $(FLINT_DIR)/themes/*.ini)
IMAGE_FILES = assets/angel.jpg assets/begin.jpg
SOUND_FILES = assets/sounds/breath-in.ogg assets/sounds/breath-out.ogg assets/sounds/bell.ogg
FONT_OUTPUTS = assets/fonts/locales.png assets/fonts/locales.dat
FONT_FILES = $(FONT_OUTPUTS)
FONT_TOOL = vendor/otfchop/otfchop
FONT_SOURCE = vendor/otfchop/unifont-17.0.04.otf
BUILD_MAKEFILES = Makefile mk/common.mk mk/native.mk mk/windows.mk mk/web.mk mk/android.mk mk/dist.mk mk/clean.mk

assets/fonts:
	mkdir -p $@

$(FONT_OUTPUTS): $(LOCALE_FILES) $(FONT_TOOL) | assets/fonts
	$(FONT_TOOL) $(FONT_SOURCE) $(LOCALE_FILES) assets/fonts/locales

$(FONT_TOOL): vendor/otfchop/otfchop.c vendor/otfchop/stb_truetype.h vendor/otfchop/stb_image_write.h
	$(MAKE) -C vendor/otfchop otfchop

INBE_RAYLIB_CONFIG = $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1

CFLAGS = -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
LDFLAGS = -Wl,--gc-sections -s

.NOTPARALLEL:

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

build:
	mkdir -p $(BUILD_DIR)

$(LINUX_BUILD_DIR):
	mkdir -p $@

$(WINDOWS_BUILD_DIR):
	mkdir -p $@

$(ANDROID_BUILD_DIR):
	mkdir -p $@

$(WEB_BUILD_DIR):
	mkdir -p $@

$(RAYLIB_BUILD_DIR): build
	mkdir -p $@
