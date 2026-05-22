.PHONY: help init-signing release debug install install-debug clean uninstall

GRADLEW := ./gradlew

help:
	@echo "Targets:"
	@echo "  make init-signing    - Generate signing keystore (one-time setup)"
	@echo "  make release         - Build release APK"
	@echo "  make debug           - Build debug APK"
	@echo "  make install         - Install release APK to connected device"
	@echo "  make install-debug   - Install debug APK to connected device"
	@echo "  make clean           - Clean build outputs"
	@echo "  make uninstall       - Uninstall app from connected device"

init-signing:
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
	@echo "Add this line to local.properties:"
	@echo "  keystore.path=$$HOME/.android/inbe-release.keystore"

release:
	@if [ -n "$(PASSWORD)" ]; then \
		$(GRADLEW) assembleRelease -Pkeystore.password=$(PASSWORD); \
	else \
		echo "Usage: make release PASSWORD=your-password"; \
		exit 1; \
	fi

debug:
	$(GRADLEW) assembleDebug

install: release
	adb install -r app/build/outputs/apk/release/app-release.apk

install-debug: debug
	adb install -r app/build/outputs/apk/debug/app-debug.apk

clean:
	$(GRADLEW) clean

uninstall:
	adb uninstall xyz.waozi.inbe
