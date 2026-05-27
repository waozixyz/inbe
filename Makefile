CC = gcc
WINDRES = windres

SRC = src/main.c src/app.c src/ui.c src/theme_meta.c src/theme.c
INBE_DIR = libinbe
INBE_A = $(INBE_DIR)/libinbe.a

RAYLIB_DIR = vendor/raylib/src
RAYLIB_BUILD_DIR = vendor/raylib/build/sdl
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
ANDROID_BUILD_DIR = $(BUILD_DIR)/android
WEB_BUILD_DIR = $(BUILD_DIR)/web
LINUX_ARCHES = x86_64 aarch64
ANDROID_DIST ?= release

# Linux/native target
BINARY_NAME = inbe-$(PLATFORM)-$(ARCH)
TARGET = $(LINUX_BUILD_DIR)/$(BINARY_NAME)
LINUX_INBE_A = $(LINUX_BUILD_DIR)/libinbe.a

# Windows cross-compilation
WIN_CC = x86_64-w64-mingw32-gcc
WIN_AR = x86_64-w64-mingw32-ar
WIN_WINDRES = x86_64-w64-mingw32-windres
WIN_TARGET = $(WINDOWS_BUILD_DIR)/inbe-windows-x86_64.exe
WIN_RAYLIB_BUILD_DIR = $(WINDOWS_BUILD_DIR)/raylib
WIN_RAYLIB_A = $(WIN_RAYLIB_BUILD_DIR)/libraylib.a
WIN_RAYLIB_OBJS = \
	$(WIN_RAYLIB_BUILD_DIR)/rcore.o \
	$(WIN_RAYLIB_BUILD_DIR)/rshapes.o \
	$(WIN_RAYLIB_BUILD_DIR)/rtextures.o \
	$(WIN_RAYLIB_BUILD_DIR)/rtext.o \
	$(WIN_RAYLIB_BUILD_DIR)/raudio.o
WIN_INBE_A = $(WINDOWS_BUILD_DIR)/libinbe.a

# WebAssembly
WEB_CC ?= emcc
WEB_AR ?= emar
WEB_TARGET = $(WEB_BUILD_DIR)/index.html
WEB_RAYLIB_BUILD_DIR = $(WEB_BUILD_DIR)/raylib
WEB_RAYLIB_A = $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
WEB_RAYLIB_OBJS = \
	$(WEB_RAYLIB_BUILD_DIR)/rcore.o \
	$(WEB_RAYLIB_BUILD_DIR)/rshapes.o \
	$(WEB_RAYLIB_BUILD_DIR)/rtextures.o \
	$(WEB_RAYLIB_BUILD_DIR)/rtext.o \
	$(WEB_RAYLIB_BUILD_DIR)/raudio.o
WEB_CFLAGS = -Wall -Wextra -std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1
WEB_SHELL = src/web_shell.html
WEB_LDFLAGS = -sUSE_GLFW=3 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 --shell-file $(WEB_SHELL) --preload-file inbe.ini@inbe.ini --preload-file theme.ini@theme.ini --preload-file themes/sky.ini@themes/sky.ini --preload-file themes/sky_dark.ini@themes/sky_dark.ini --preload-file themes/ocean.ini@themes/ocean.ini --preload-file themes/ocean_dark.ini@themes/ocean_dark.ini --preload-file themes/forest.ini@themes/forest.ini --preload-file themes/forest_dark.ini@themes/forest_dark.ini --preload-file themes/sunset.ini@themes/sunset.ini --preload-file themes/sunset_dark.ini@themes/sunset_dark.ini --preload-file themes/lavender.ini@themes/lavender.ini --preload-file themes/lavender_dark.ini@themes/lavender_dark.ini --preload-file themes/cherry.ini@themes/cherry.ini --preload-file themes/cherry_dark.ini@themes/cherry_dark.ini --preload-file icons/gear.png@icons/gear.png --preload-file icons/x.png@icons/x.png --preload-file icons/manual.png@icons/manual.png --preload-file icons/return.png@icons/return.png --preload-file icons/backward.png@icons/backward.png --preload-file icons/forward.png@icons/forward.png --preload-file icons/play.png@icons/play.png --preload-file icons/pause.png@icons/pause.png --preload-file icons/stat.png@icons/stat.png --preload-file icons/home.png@icons/home.png --preload-file icons/trash.png@icons/trash.png --preload-file icons/telegram.png@icons/telegram.png --preload-file icons/globe.png@icons/globe.png --preload-file icons/stripe.png@icons/stripe.png --preload-file icons/monero.png@icons/monero.png --preload-file assets/angel.jpg@assets/angel.jpg --preload-file assets/begin.jpg@assets/begin.jpg --preload-file assets/sounds/breath-in.ogg@assets/sounds/breath-in.ogg --preload-file assets/sounds/breath-out.ogg@assets/sounds/breath-out.ogg --preload-file assets/sounds/bell.ogg@assets/sounds/bell.ogg
INBE_RAYLIB_CONFIG = $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1

CFLAGS = -Wall -Wextra -std=c99 -Os -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1
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

all: native

native: $(TARGET)

build:
	mkdir -p $(BUILD_DIR)

$(LINUX_BUILD_DIR):
	mkdir -p $@

$(WINDOWS_BUILD_DIR):
	mkdir -p $@

$(WIN_RAYLIB_BUILD_DIR):
	mkdir -p $@

$(ANDROID_BUILD_DIR):
	mkdir -p $@

$(WEB_BUILD_DIR):
	mkdir -p $@

$(WEB_RAYLIB_BUILD_DIR):
	mkdir -p $@

$(RAYLIB_BUILD_DIR): build
	mkdir -p $@

# Native Linux/macOS raylib build
$(RAYLIB_A): | $(RAYLIB_BUILD_DIR)
	$(MAKE) -j1 -C $(RAYLIB_DIR) \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../build/sdl \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		SDL_INCLUDE_PATH="$(RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(RAY_CFLAGS) $(INBE_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

# Build native libinbe, then copy it into build/linux
$(LINUX_INBE_A): FORCE | $(LINUX_BUILD_DIR)
	$(MAKE) -C $(INBE_DIR) clean
	$(MAKE) -C $(INBE_DIR) CC=$(CC) AR=ar
	cp $(INBE_A) $@

# Native executable
$(TARGET): $(SRC) theme.ini inbe.ini $(RAYLIB_A) $(LINUX_INBE_A) | $(LINUX_BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		-Isrc \
		$(RAY_CFLAGS) \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-o $@ \
		$(SRC) \
		$(LINUX_INBE_A) \
		$(RAYLIB_A) \
		$(RAY_LDLIBS) \
		-lm -lpthread -ldl -lrt \
		$(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

linux: $(LINUX_ARCHES:%=linux-%)

linux-x86_64: | $(LINUX_BUILD_DIR)
	$(MAKE) build-linux-arch \
		ARCH_NAME=x86_64 \
		LINUX_CC="$(CC)" \
		LINUX_AR=ar \
		LINUX_RAY_CFLAGS="$(RAY_CFLAGS)" \
		LINUX_RAY_LDLIBS="$(RAY_LDLIBS)" \
		LINUX_RAY_SDL_LDLIBS="$(RAY_SDL_LDLIBS)" \
		LINUX_RAY_SDL_INCLUDE_DIR="$(RAY_SDL_INCLUDE_DIR)"

linux-aarch64: | $(LINUX_BUILD_DIR)
	@if [ -z "$(AARCH64_CC)" ] || [ -z "$(AARCH64_AR)" ] || [ -z "$(AARCH64_RAY_CFLAGS)" ] || [ -z "$(AARCH64_RAY_LDLIBS)" ] || [ -z "$(AARCH64_RAY_SDL_LDLIBS)" ] || [ -z "$(AARCH64_RAY_SDL_INCLUDE_DIR)" ]; then \
		echo "AARCH64 cross-build variables are missing. Enter the flake shell with 'nix develop'."; \
		exit 1; \
	fi
	$(MAKE) build-linux-arch \
		ARCH_NAME=aarch64 \
		LINUX_CC="$(AARCH64_CC)" \
		LINUX_AR="$(AARCH64_AR)" \
		LINUX_RAY_CFLAGS="$(AARCH64_RAY_CFLAGS)" \
		LINUX_RAY_LDLIBS="$(AARCH64_RAY_LDLIBS)" \
		LINUX_RAY_SDL_LDLIBS="$(AARCH64_RAY_SDL_LDLIBS)" \
		LINUX_RAY_SDL_INCLUDE_DIR="$(AARCH64_RAY_SDL_INCLUDE_DIR)"

build-linux-arch:
	@mkdir -p $(LINUX_BUILD_DIR)/obj-$(ARCH_NAME) vendor/raylib/build/sdl-$(ARCH_NAME)
	$(MAKE) -C $(RAYLIB_DIR) clean
	$(MAKE) -j1 -C $(RAYLIB_DIR) \
		CC="$(LINUX_CC)" \
		AR="$(LINUX_AR)" \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../build/sdl-$(ARCH_NAME) \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		SDL_INCLUDE_PATH="$(LINUX_RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(LINUX_RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(LINUX_RAY_CFLAGS) $(INBE_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"
	$(MAKE) -C $(RAYLIB_DIR) clean
	$(LINUX_CC) $(CFLAGS) -I$(INBE_DIR) -c $(INBE_DIR)/inbe.c -o $(LINUX_BUILD_DIR)/obj-$(ARCH_NAME)/inbe.o
	$(LINUX_AR) rcs $(LINUX_BUILD_DIR)/obj-$(ARCH_NAME)/libinbe.a $(LINUX_BUILD_DIR)/obj-$(ARCH_NAME)/inbe.o
	$(LINUX_CC) $(CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		-Isrc \
		$(LINUX_RAY_CFLAGS) \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-o $(LINUX_BUILD_DIR)/inbe-linux-$(ARCH_NAME) \
		$(SRC) \
		$(LINUX_BUILD_DIR)/obj-$(ARCH_NAME)/libinbe.a \
		vendor/raylib/build/sdl-$(ARCH_NAME)/libraylib.a \
		$(LINUX_RAY_LDLIBS) \
		-lm -lpthread -ldl -lrt \
		$(LDFLAGS)

# Windows cross-compilation target
windows: $(WIN_TARGET)

# Windows raylib build
$(WIN_RAYLIB_BUILD_DIR)/%.o: $(RAYLIB_DIR)/%.c | $(WIN_RAYLIB_BUILD_DIR)
	$(WIN_CC) \
		-c $< \
		-o $@ \
		-Wall \
		-D_GNU_SOURCE \
		-DPLATFORM_DESKTOP_WIN32 \
		-DGRAPHICS_API_OPENGL_33 \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-Wno-missing-braces \
		-Werror=pointer-arith \
		-fno-strict-aliasing \
		-std=gnu99 \
		-DUNICODE \
		$(INBE_RAYLIB_CONFIG) \
		-Os \
		-ffunction-sections \
		-fdata-sections \
		-I$(RAYLIB_DIR)

$(WIN_RAYLIB_A): $(WIN_RAYLIB_OBJS)
	$(WIN_AR) rcs $@ $(WIN_RAYLIB_OBJS)

# WebAssembly target
web: $(WEB_TARGET)

$(WEB_RAYLIB_BUILD_DIR)/%.o: $(RAYLIB_DIR)/%.c | $(WEB_RAYLIB_BUILD_DIR)
	$(WEB_CC) \
		-c $< \
		-o $@ \
		-Wall \
		-D_GNU_SOURCE \
		-DPLATFORM_WEB \
		-DGRAPHICS_API_OPENGL_ES2 \
		-Wno-missing-braces \
		-Werror=pointer-arith \
		-fno-strict-aliasing \
		-std=gnu99 \
		-D_DEFAULT_SOURCE \
		$(INBE_RAYLIB_CONFIG) \
		-Os \
		-ffunction-sections \
		-fdata-sections \
		-I$(RAYLIB_DIR)

$(WEB_RAYLIB_A): $(WEB_RAYLIB_OBJS)
	$(WEB_AR) rcs $@ $(WEB_RAYLIB_OBJS)

$(WEB_TARGET): $(SRC) $(INBE_DIR)/inbe.c $(WEB_SHELL) $(WEB_RAYLIB_A) | $(WEB_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		-Isrc \
		-o $@ \
		$(SRC) \
		$(INBE_DIR)/inbe.c \
		$(WEB_RAYLIB_A) \
		$(WEB_LDFLAGS)

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
		-Isrc \
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
	$(MAKE) -C $(INBE_DIR) clean

clean-web:
	rm -rf $(WEB_BUILD_DIR)

clean-raylib:
	$(MAKE) -C $(RAYLIB_DIR) clean
	rm -rf vendor/raylib/build

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
	$(MAKE) linux
	$(MAKE) windows
	$(MAKE) web
	$(MAKE) android-$(ANDROID_DIST)
	$(MAKE) android-bundle
	$(MAKE) dist-linux
	$(MAKE) dist-windows

dist-linux: linux
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
	@cp inbe.ini theme.ini $(LINUX_BUILD_DIR)/dist/inbe-linux/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/themes
	@cp themes/*.ini $(LINUX_BUILD_DIR)/dist/inbe-linux/themes/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/icons
	@cp icons/gear.png icons/x.png icons/manual.png icons/return.png icons/backward.png icons/forward.png icons/play.png icons/pause.png icons/stat.png $(LINUX_BUILD_DIR)/dist/inbe-linux/icons/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/assets
	@cp assets/angel.jpg assets/begin.jpg $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/sounds
	@cp assets/sounds/breath-in.ogg assets/sounds/breath-out.ogg assets/sounds/bell.ogg $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/sounds/
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
	@cp inbe.ini theme.ini $(WINDOWS_BUILD_DIR)/dist/inbe-windows/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/themes
	@cp themes/*.ini $(WINDOWS_BUILD_DIR)/dist/inbe-windows/themes/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/icons
	@cp icons/gear.png icons/x.png icons/manual.png icons/return.png icons/backward.png icons/forward.png icons/play.png icons/pause.png icons/stat.png $(WINDOWS_BUILD_DIR)/dist/inbe-windows/icons/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets
	@cp assets/angel.jpg assets/begin.jpg $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/sounds
	@cp assets/sounds/breath-in.ogg assets/sounds/breath-out.ogg assets/sounds/bell.ogg $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/sounds/
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

android-copy-assets:
	rm -rf $(ANDROID_DIR)/app/src/main/assets
	mkdir -p $(ANDROID_DIR)/app/src/main/assets
	cp inbe.ini $(ANDROID_DIR)/app/src/main/assets/inbe.ini
	cp theme.ini $(ANDROID_DIR)/app/src/main/assets/theme.ini
	mkdir -p $(ANDROID_DIR)/app/src/main/assets/themes
	cp themes/*.ini $(ANDROID_DIR)/app/src/main/assets/themes/
	mkdir -p $(ANDROID_DIR)/app/src/main/assets/icons
	@for icon in icons/*.png; do \
		base=$$(basename "$$icon"); \
		size=$$(identify -format '%wx%h' "$$icon"); \
		magick "$$icon" -filter point -resize "$$size!" "$(ANDROID_DIR)/app/src/main/assets/icons/$$base"; \
	done
	mkdir -p $(ANDROID_DIR)/app/src/main/assets/assets
	cp assets/angel.jpg assets/begin.jpg $(ANDROID_DIR)/app/src/main/assets/assets/
	mkdir -p $(ANDROID_DIR)/app/src/main/assets/assets/sounds
	cp assets/sounds/breath-in.ogg assets/sounds/breath-out.ogg assets/sounds/bell.ogg $(ANDROID_DIR)/app/src/main/assets/assets/sounds/

android-debug:
	$(MAKE) android-copy-assets
	$(GRADLE) -p $(ANDROID_DIR) assembleDebug
	$(MAKE) android-copy-debug-apks

android-release:
	$(MAKE) android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		PASSWORD_VALUE="$(PASSWORD)"; \
	elif [ -t 0 ]; then \
		printf "Keystore password: "; \
		stty -echo; \
		read PASSWORD_VALUE; \
		stty echo; \
		printf "\n"; \
	else \
		echo "Keystore password is required. Run from a terminal or use PASSWORD=your-password."; \
		exit 1; \
	fi; \
	if [ -n "$$PASSWORD_VALUE" ]; then \
		$(GRADLE) -p $(ANDROID_DIR) assembleRelease -Pkeystore.password="$$PASSWORD_VALUE" || exit $$?; \
		$(MAKE) android-copy-release-apks; \
	else \
		echo "Keystore password is required"; \
		exit 1; \
	fi

android-bundle:
	$(MAKE) android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		PASSWORD_VALUE="$(PASSWORD)"; \
	elif [ -t 0 ]; then \
		printf "Keystore password: "; \
		stty -echo; \
		read PASSWORD_VALUE; \
		stty echo; \
		printf "\n"; \
	else \
		echo "Keystore password is required. Run from a terminal or use PASSWORD=your-password."; \
		exit 1; \
	fi; \
	if [ -n "$$PASSWORD_VALUE" ]; then \
		$(GRADLE) -p $(ANDROID_DIR) bundleRelease -Pkeystore.password="$$PASSWORD_VALUE" || exit $$?; \
		$(MAKE) android-copy-bundle; \
	else \
		echo "Keystore password is required"; \
		exit 1; \
	fi

android-copy-bundle: | $(ANDROID_BUILD_DIR)
	@VERSION=$$(grep -m1 '^## \[' CHANGELOG.md | sed 's/^## \[\([^]]*\)\].*/\1/'); \
		BUNDLE=$$(find $(ANDROID_DIR)/app/build/outputs/bundle/release -name "*.aab" 2>/dev/null | head -1); \
		if [ -n "$$BUNDLE" ] && [ -f "$$BUNDLE" ]; then \
			VERSIONED_BUNDLE="$(ANDROID_BUILD_DIR)/inbe-$$VERSION.aab"; \
			echo "Copying and renaming $$BUNDLE to $$VERSIONED_BUNDLE"; \
			cp "$$BUNDLE" "$$VERSIONED_BUNDLE"; \
			ln -sf "$$(basename "$$VERSIONED_BUNDLE")" "$(ANDROID_BUILD_DIR)/inbe-latest.aab"; \
			echo "Created symlink: inbe-latest.aab → $$(basename "$$VERSIONED_BUNDLE")"; \
		else \
			echo "Bundle not found in $(ANDROID_DIR)/app/build/outputs/bundle/release/"; \
			exit 1; \
		fi

android-copy-apks: android-copy-debug-apks android-copy-release-apks

android-copy-debug-apks: | $(ANDROID_BUILD_DIR)
	@for apk in $(ANDROID_DIR)/app/build/outputs/apk/debug/*.apk; do \
		if [ -f "$$apk" ]; then \
			echo "Copying $$apk"; \
			cp "$$apk" "$(ANDROID_BUILD_DIR)/$$(basename "$$apk")"; \
		fi; \
	done

android-copy-release-apks: | $(ANDROID_BUILD_DIR)
	@VERSION=$$(grep INBE_VERSION_STRING src/version.h | grep -o '"[^"]*"' | tr -d '"'); \
	UNIVERSAL_APK=$$(find $(ANDROID_DIR)/app/build/outputs/apk/release -name "app-universal-release.apk" 2>/dev/null); \
	if [ -n "$$UNIVERSAL_APK" ] && [ -f "$$UNIVERSAL_APK" ]; then \
		VERSIONED_APK="$(ANDROID_BUILD_DIR)/inbe-$$VERSION.apk"; \
		echo "Copying and renaming $$UNIVERSAL_APK to $$VERSIONED_APK"; \
		cp "$$UNIVERSAL_APK" "$$VERSIONED_APK"; \
		ln -sf "$$(basename "$$VERSIONED_APK")" "$(ANDROID_BUILD_DIR)/inbe-latest.apk"; \
		echo "Created symlink: inbe-latest.apk → $$(basename "$$VERSIONED_APK")"; \
	else \
		for apk in $(ANDROID_DIR)/app/build/outputs/apk/release/*.apk; do \
			if [ -f "$$apk" ]; then \
				echo "Copying $$apk"; \
				cp "$$apk" "$(ANDROID_BUILD_DIR)/$$(basename "$$apk")"; \
			fi; \
		done; \
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
	rm -rf $(ANDROID_BUILD_DIR)

FORCE:

.PHONY: \
	all \
	native \
	run \
	linux \
	$(LINUX_ARCHES:%=linux-%) \
	build-linux-arch \
	windows \
	web \
	clean \
	clean-linux \
	clean-windows \
	clean-web \
	clean-raylib \
	dist \
	dist-linux \
	dist-windows \
	android-init-signing \
	android-debug \
	android-release \
	android-bundle \
	android-copy-assets \
	android-copy-apks \
	android-copy-debug-apks \
	android-copy-release-apks \
	android-copy-bundle \
	android-install \
	android-install-release \
	android-clean \
	FORCE
