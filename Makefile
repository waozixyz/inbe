CC = gcc
WINDRES = windres

SRC = src/main.c
INBE_DIR = libinbe
INBE_A = $(INBE_DIR)/libinbe.a

RAYLIB_DIR = ../vendor/raylib/src
RAYLIB_BUILD_DIR = ../vendor/raylib/build/sdl
RAYLIB_A = $(RAYLIB_BUILD_DIR)/libraylib.a

ANDROID_DIR = droid
GRADLE = gradle
ANDROID_APP_ID = xyz.waozi.inbe
ANDROID_ACTIVITY = android.app.NativeActivity

# Detect platform and architecture
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

# Build output dirs
BUILD_DIR = build
LINUX_BUILD_DIR = $(BUILD_DIR)/linux
WINDOWS_BUILD_DIR = $(BUILD_DIR)/windows

# Linux/native target
BINARY_NAME = inbe-$(PLATFORM)-$(ARCH)
TARGET = $(LINUX_BUILD_DIR)/$(BINARY_NAME)
LINUX_INBE_A = $(LINUX_BUILD_DIR)/libinbe.a

# Windows cross-compilation
WIN_CC = x86_64-w64-mingw32-gcc
WIN_AR = x86_64-w64-mingw32-ar
WIN_WINDRES = x86_64-w64-mingw32-windres
WIN_TARGET = $(WINDOWS_BUILD_DIR)/inbe-windows-x86_64.exe
WIN_RAYLIB_A = ../vendor/raylib/build/desktop/libraylib.a
WIN_INBE_A = $(WINDOWS_BUILD_DIR)/libinbe.a

CFLAGS = -Wall -Wextra -std=c99 -Os -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections -s

.NOTPARALLEL:

# Required shell vars from nix develop
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

all: $(TARGET)

build:
	mkdir -p $(BUILD_DIR)

$(LINUX_BUILD_DIR):
	mkdir -p $@

$(WINDOWS_BUILD_DIR):
	mkdir -p $@

$(RAYLIB_BUILD_DIR): build
	mkdir -p $@

# Native Linux/macOS raylib build
$(RAYLIB_A): FORCE | $(RAYLIB_BUILD_DIR)
	$(MAKE) -C $(RAYLIB_DIR) clean
	$(MAKE) -j1 -C $(RAYLIB_DIR) \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../build/sdl \
		RAYLIB_MODULE_AUDIO=FALSE \
		RAYLIB_MODULE_MODELS=FALSE \
		SDL_INCLUDE_PATH="$(RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(RAY_CFLAGS) $(RAY_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"
	$(MAKE) -C $(RAYLIB_DIR) clean

# Build native libinbe, then copy it into build/linux
$(LINUX_INBE_A): FORCE | $(LINUX_BUILD_DIR)
	$(MAKE) -C $(INBE_DIR) clean
	$(MAKE) -C $(INBE_DIR) CC=$(CC) AR=ar
	cp $(INBE_A) $@

# Native executable
$(TARGET): $(SRC) $(RAYLIB_A) $(LINUX_INBE_A) | $(LINUX_BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		$(RAY_CFLAGS) \
		-o $@ \
		$(SRC) \
		$(LINUX_INBE_A) \
		$(RAYLIB_A) \
		$(RAY_LDLIBS) \
		-lm -lpthread -ldl -lrt \
		$(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

# Windows cross-compilation target
windows: $(WIN_TARGET)

# Windows raylib build
$(WIN_RAYLIB_A): FORCE | $(WINDOWS_BUILD_DIR)
	mkdir -p ../vendor/raylib/build/desktop
	$(MAKE) -C $(RAYLIB_DIR) clean
	$(MAKE) -j1 -C $(RAYLIB_DIR) \
		CC=$(WIN_CC) \
		AR=$(WIN_AR) \
		WINDRES=$(WIN_WINDRES) \
		OS=Windows_NT \
		PLATFORM=PLATFORM_DESKTOP_WIN32 \
		GRAPHICS=GRAPHICS_API_OPENGL_33 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../build/desktop \
		RAYLIB_MODULE_AUDIO=FALSE \
		RAYLIB_MODULE_MODELS=FALSE \
		CUSTOM_CFLAGS="-DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=0 -Os -ffunction-sections -fdata-sections"
	$(MAKE) -C $(RAYLIB_DIR) clean

# Build Windows libinbe, then copy it into build/windows
$(WIN_INBE_A): FORCE | $(WINDOWS_BUILD_DIR)
	$(MAKE) -C $(INBE_DIR) clean
	$(MAKE) -C $(INBE_DIR) CC=$(WIN_CC) AR=$(WIN_AR)
	cp $(INBE_A) $@

# Windows executable
$(WIN_TARGET): $(SRC) $(WIN_RAYLIB_A) $(WIN_INBE_A) | $(WINDOWS_BUILD_DIR)
	$(WIN_CC) $(CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		-o $@ \
		$(SRC) \
		$(WIN_INBE_A) \
		$(WIN_RAYLIB_A) \
		-L$(MCFGTHREADS)/lib \
		-lopengl32 -lgdi32 -lwinmm \
		-mwindows \
		$(LDFLAGS)

# Cleaning
clean:
	rm -rf build
	$(MAKE) -C $(INBE_DIR) clean

clean-linux:
	rm -rf $(LINUX_BUILD_DIR)
	$(MAKE) -C $(INBE_DIR) clean

clean-windows:
	rm -rf $(WINDOWS_BUILD_DIR)
	rm -rf ../vendor/raylib/build/desktop
	$(MAKE) -C $(INBE_DIR) clean

clean-raylib:
	$(MAKE) -C $(RAYLIB_DIR) clean
	rm -rf ../vendor/raylib/build

# Distribution targets
#
# These create ONE archive per platform:
#
#   build/linux/inbe-linux.tar.gz
#   build/windows/inbe-windows.zip
#
# Each archive includes every matching binary already present in that
# platform's build folder.

dist:
	$(MAKE) all
	$(MAKE) windows
	$(MAKE) dist-linux
	$(MAKE) dist-windows

dist-linux:
	@echo "Creating Linux tar.gz package with all Linux arch binaries..."
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux
	@for bin in $(LINUX_BUILD_DIR)/inbe-linux-*; do \
		if [ -f "$$bin" ] && [ -x "$$bin" ]; then \
			echo "Adding $$bin"; \
			cp "$$bin" "$(LINUX_BUILD_DIR)/dist/inbe-linux/$$(basename "$$bin")"; \
		fi; \
	done
	@if [ -z "$$(find $(LINUX_BUILD_DIR)/dist/inbe-linux -type f 2>/dev/null)" ]; then \
		echo "No Linux binaries found in $(LINUX_BUILD_DIR)"; \
		exit 1; \
	fi
	@cd $(LINUX_BUILD_DIR)/dist && tar -czf ../inbe-linux.tar.gz inbe-linux/
	@rm -rf $(LINUX_BUILD_DIR)/dist
	@echo "Created $(LINUX_BUILD_DIR)/inbe-linux.tar.gz"

dist-windows:
	@echo "Creating Windows zip package with all Windows arch binaries..."
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows
	@for bin in $(WINDOWS_BUILD_DIR)/inbe-windows-*.exe; do \
		if [ -f "$$bin" ]; then \
			echo "Adding $$bin"; \
			cp "$$bin" "$(WINDOWS_BUILD_DIR)/dist/inbe-windows/$$(basename "$$bin")"; \
		fi; \
	done
	@if [ -z "$$(find $(WINDOWS_BUILD_DIR)/dist/inbe-windows -type f 2>/dev/null)" ]; then \
		echo "No Windows binaries found in $(WINDOWS_BUILD_DIR)"; \
		exit 1; \
	fi
	@cd $(WINDOWS_BUILD_DIR)/dist && zip -r ../inbe-windows.zip inbe-windows/
	@rm -rf $(WINDOWS_BUILD_DIR)/dist
	@echo "Created $(WINDOWS_BUILD_DIR)/inbe-windows.zip"
	
# Android targets
android-init-signing:
	@echo "Generating release keystore..."
	@mkdir -p ~/.android
	keytool -genkeypair -v -storetype PKCS12 \
		-keystore ~/.android/inbe-release.keystore \
		-alias inbe-key \
		-keyalg RSA \
		-keysize 4096 \
		-validity 10000
	@echo ""
	@echo "Keystore created at ~/.android/inbe-release.keystore"
	@echo ""
	@echo "Add this line to $(ANDROID_DIR)/local.properties:"
	@echo "  keystore.path=$$HOME/.android/inbe-release.keystore"

android-debug:
	$(GRADLE) -p $(ANDROID_DIR) assembleDebug

android-release:
	@if [ -n "$(PASSWORD)" ]; then \
		$(GRADLE) -p $(ANDROID_DIR) assembleRelease -Pkeystore.password=$(PASSWORD); \
	else \
		echo "Usage: make android-release PASSWORD=your-password"; \
		exit 1; \
	fi

android-install: android-debug
	@ABI=$$(adb shell getprop ro.product.cpu.abi | tr -d '\r'); \
	echo "Device ABI: $$ABI"; \
	APK=$(ANDROID_DIR)/app/build/outputs/apk/debug/app-$${ABI}-debug.apk; \
	if [ ! -f "$$APK" ]; then \
		echo "APK not found for $$ABI, trying universal..."; \
		APK=$(ANDROID_DIR)/app/build/outputs/apk/debug/app-debug.apk; \
	fi; \
	echo "Installing: $$APK"; \
	adb install -r "$$APK"
	adb shell am start -n $(ANDROID_APP_ID)/$(ANDROID_ACTIVITY)

android-install-release: android-release
	@ABI=$$(adb shell getprop ro.product.cpu.abi | tr -d '\r'); \
	echo "Device ABI: $$ABI"; \
	APK=$(ANDROID_DIR)/app/build/outputs/apk/release/app-$${ABI}-release.apk; \
	if [ ! -f "$$APK" ]; then \
		echo "APK not found for $$ABI, trying universal..."; \
		APK=$(ANDROID_DIR)/app/build/outputs/apk/release/app-release.apk; \
	fi; \
	echo "Installing: $$APK"; \
	adb install -r "$$APK"
	adb shell am start -n $(ANDROID_APP_ID)/$(ANDROID_ACTIVITY)

android-clean:
	$(GRADLE) -p $(ANDROID_DIR) clean

FORCE:

.PHONY: \
	all \
	run \
	windows \
	clean \
	clean-linux \
	clean-windows \
	clean-raylib \
	dist \
	dist-linux \
	dist-windows \
	android-init-signing \
	android-debug \
	android-release \
	android-install \
	android-install-release \
	android-clean \
	FORCE
