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
	$(MAKE) $(FONT_FILES)
	$(MAKE) $(EMBEDDED_ASSETS_C)
	rm -rf $(ANDROID_DIR)/app/src/main/assets
	mkdir -p $(ANDROID_DIR)/app/src/main/assets

android-debug:
	$(MAKE) android-copy-assets
	unset ANDROID_HOME; $(GRADLE) -p $(ANDROID_DIR) assembleDebug
	$(MAKE) android-copy-debug-apks

android-release:
	$(MAKE) android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		PASSWORD_VALUE="$(PASSWORD)"; \
		unset ANDROID_HOME; $(GRADLE) -p $(ANDROID_DIR) assembleRelease -Pkeystore.password="$$PASSWORD_VALUE" || exit $$?; \
		$(MAKE) android-copy-release-apks; \
	elif [ -t 0 ]; then \
		printf "Keystore password: "; \
		stty -echo; \
		read PASSWORD_VALUE; \
		stty echo; \
		printf "\n"; \
		if [ -n "$$PASSWORD_VALUE" ]; then \
			unset ANDROID_HOME; $(GRADLE) -p $(ANDROID_DIR) assembleRelease -Pkeystore.password="$$PASSWORD_VALUE" || exit $$?; \
			$(MAKE) android-copy-release-apks; \
		else \
			echo "Keystore password is required"; \
			exit 1; \
		fi; \
	else \
		echo "Keystore password is required. Run from a terminal or use PASSWORD=your-password."; \
		exit 1; \
	fi

android-bundle:
	$(MAKE) android-copy-assets
	@if [ -n "$(PASSWORD)" ]; then \
		PASSWORD_VALUE="$(PASSWORD)"; \
		unset ANDROID_HOME; $(GRADLE) -p $(ANDROID_DIR) bundleRelease -Pkeystore.password="$$PASSWORD_VALUE" || exit $$?; \
		$(MAKE) android-copy-bundle; \
	elif [ -t 0 ]; then \
		printf "Keystore password: "; \
		stty -echo; \
		read PASSWORD_VALUE; \
		stty echo; \
		printf "\n"; \
		if [ -n "$$PASSWORD_VALUE" ]; then \
			unset ANDROID_HOME; $(GRADLE) -p $(ANDROID_DIR) bundleRelease -Pkeystore.password="$$PASSWORD_VALUE" || exit $$?; \
			$(MAKE) android-copy-bundle; \
		else \
			echo "Keystore password is required"; \
			exit 1; \
		fi; \
	else \
		echo "Keystore password is required. Run from a terminal or use PASSWORD=your-password."; \
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
			echo "Created symlink: inbe-latest.aab -> $$(basename "$$VERSIONED_BUNDLE")"; \
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
		echo "Created symlink: inbe-latest.apk -> $$(basename "$$VERSIONED_APK")"; \
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
