CC = gcc
TARGET = build/main
SRC = src/main.c
INBE_DIR = libinbe
INBE_A = $(INBE_DIR)/libinbe.a

RAYLIB_DIR = ../vendor/raylib/src
RAYLIB_BUILD_DIR = ../vendor/raylib/build/sdl
RAYLIB_A = $(RAYLIB_BUILD_DIR)/libraylib.a

ANDROID_DIR = droid
GRADLEW = ./droid/gradlew
ANDROID_APP_ID = xyz.waozi.inbe
ANDROID_ACTIVITY = android.app.NativeActivity

CFLAGS = -Wall -Wextra -std=c99 -Os -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections -s

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
	mkdir -p $@

$(RAYLIB_BUILD_DIR): build
	mkdir -p $@

$(RAYLIB_A): FORCE | $(RAYLIB_BUILD_DIR)
	$(MAKE) -C $(RAYLIB_DIR) clean
	$(MAKE) -j1 -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP_SDL GRAPHICS=GRAPHICS_API_OPENGL_ES2 RAYLIB_LIBTYPE=STATIC RAYLIB_RELEASE_PATH=../build/sdl RAYLIB_MODULE_AUDIO=FALSE RAYLIB_MODULE_MODELS=FALSE SDL_INCLUDE_PATH="$(RAY_SDL_INCLUDE_DIR)" SDL_LIBRARIES="$(RAY_SDL_LDLIBS)" CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(RAY_CFLAGS) $(RAY_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

$(INBE_A):
	$(MAKE) -C $(INBE_DIR)

$(TARGET): $(SRC) $(RAYLIB_A) $(INBE_A) | build
	$(CC) $(CFLAGS) -I$(RAYLIB_DIR) -I$(INBE_DIR) $(RAY_CFLAGS) -o $@ $(SRC) $(INBE_A) $(RAYLIB_A) $(RAY_LDLIBS) -lm -lpthread -ldl -lrt $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
	$(MAKE) -C $(INBE_DIR) clean

clean-raylib:
	$(MAKE) -C $(RAYLIB_DIR) clean
	rm -rf ../vendor/raylib/build

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
	$(GRADLEW) -p $(ANDROID_DIR) assembleDebug

android-release:
	@if [ -n "$(PASSWORD)" ]; then \
		$(GRADLEW) -p $(ANDROID_DIR) assembleRelease -Pkeystore.password=$(PASSWORD); \
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
	$(GRADLEW) -p $(ANDROID_DIR) clean

FORCE:

.PHONY: all run clean clean-raylib android-init-signing android-debug android-release android-install android-install-release android-clean FORCE $(INBE_A)
