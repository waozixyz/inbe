CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SIZE_CFLAGS = -Os -ffunction-sections -fdata-sections
SIZE_LDFLAGS = -Wl,--gc-sections -s

RAYLIB_DIR = ../vendor/raylib/src
RAYLIB_BUILD_DIR = ../vendor/raylib/build
RAYLIB_SDL_A = $(RAYLIB_BUILD_DIR)/sdl/libraylib.a
RAYLIB_DRM_PKGS = libdrm gbm egl glesv2
RAYLIB_DRM_CFLAGS = $(shell pkg-config --cflags $(RAYLIB_DRM_PKGS) 2>/dev/null)
RAYLIB_DRM_LDLIBS = $(shell pkg-config --libs $(RAYLIB_DRM_PKGS) 2>/dev/null)
SDL2_CFLAGS = $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL2_LDLIBS = $(shell pkg-config --libs sdl2 2>/dev/null)
SDL2_INCLUDE_DIR = $(shell pkg-config --variable=includedir sdl2 2>/dev/null | sed 's,/SDL2$$,,')
RAYLIB_MIN_CONFIG = -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=0

DRM_CFLAGS = -I$(RAYLIB_DIR) $(RAYLIB_DRM_CFLAGS) $(SIZE_CFLAGS)

SDL_CFLAGS = -I$(RAYLIB_DIR) $(SDL2_CFLAGS) $(RAYLIB_DRM_CFLAGS) $(SIZE_CFLAGS)
SDL_LDFLAGS = $(RAYLIB_SDL_A) $(SDL2_LDLIBS) $(RAYLIB_DRM_LDLIBS) -lm -lpthread -ldl -lrt $(SIZE_LDFLAGS)

TARGET = main
SRC = main.c
ANDROID_DIR = android
GRADLEW = ../droid/gradlew
NIX_DEVELOP = nix develop path:. --command

all: sdl

sdl: $(TARGET)

$(RAYLIB_BUILD_DIR)/sdl:
	mkdir -p $@

$(RAYLIB_SDL_A): FORCE | $(RAYLIB_BUILD_DIR)/sdl
	$(MAKE) -C $(RAYLIB_DIR) clean
	$(MAKE) -j1 -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP_SDL GRAPHICS=GRAPHICS_API_OPENGL_ES2 RAYLIB_LIBTYPE=STATIC RAYLIB_RELEASE_PATH=../build/sdl RAYLIB_MODULE_AUDIO=FALSE RAYLIB_MODULE_MODELS=FALSE SDL_INCLUDE_PATH="$(SDL2_INCLUDE_DIR)" SDL_LIBRARIES="$(SDL2_LDLIBS)" CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(SDL2_CFLAGS) $(RAYLIB_DRM_CFLAGS) $(RAYLIB_MIN_CONFIG) $(SIZE_CFLAGS)"

$(TARGET): $(SRC) $(RAYLIB_SDL_A)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $(TARGET) $(SRC) $(SDL_LDFLAGS)

clean:
	rm -f $(TARGET)

clean-raylib:
	$(MAKE) -C $(RAYLIB_DIR) clean
	rm -rf $(RAYLIB_BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

android-init-signing:
	@echo "Generating release keystore..."
	@mkdir -p ~/.android
	$(NIX_DEVELOP) keytool -genkeypair -v -storetype PKCS12 \
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
	$(NIX_DEVELOP) $(GRADLEW) -p $(ANDROID_DIR) assembleDebug

android-release:
	@if [ -n "$(PASSWORD)" ]; then \
		$(NIX_DEVELOP) $(GRADLEW) -p $(ANDROID_DIR) assembleRelease -Pkeystore.password=$(PASSWORD); \
	else \
		echo "Usage: make android-release PASSWORD=your-password"; \
		exit 1; \
	fi

android-install: android-debug
	adb install -r $(ANDROID_DIR)/app/build/outputs/apk/debug/app-debug.apk

android-install-release: android-release
	adb install -r $(ANDROID_DIR)/app/build/outputs/apk/release/app-release.apk

android-clean:
	$(NIX_DEVELOP) $(GRADLEW) -p $(ANDROID_DIR) clean

FORCE:

.PHONY: all sdl clean clean-raylib run android-init-signing android-debug android-release android-install android-install-release android-clean FORCE
