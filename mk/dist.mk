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
	@cp $(CONFIG_FILES) $(LINUX_BUILD_DIR)/dist/inbe-linux/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/locales
	@cp $(LOCALE_FILES) $(LINUX_BUILD_DIR)/dist/inbe-linux/locales/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/themes
	@cp $(THEME_FILES) $(LINUX_BUILD_DIR)/dist/inbe-linux/themes/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/icons
	@cp $(ICON_FILES) $(LINUX_BUILD_DIR)/dist/inbe-linux/icons/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/assets
	@cp $(IMAGE_FILES) $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/fonts
	@cp $(FONT_FILES) $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/fonts/
	@mkdir -p $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/sounds
	@cp $(SOUND_FILES) $(LINUX_BUILD_DIR)/dist/inbe-linux/assets/sounds/
	@cd $(LINUX_BUILD_DIR)/dist && tar -czf ../inbe-linux.tar.gz inbe-linux/
	@rm -rf $(LINUX_BUILD_DIR)/dist
	@echo "Created $(LINUX_BUILD_DIR)/inbe-linux.tar.gz"

install: dist-linux
	@echo "Installing inbe to $(INSTALL_DIR)..."
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(BIN_DIR)
	@tar -xzf $(TARBALL) -C $(INSTALL_DIR)
	@ARCH=$$(uname -m); \
	if [ "$$ARCH" != "x86_64" ] && [ "$$ARCH" != "aarch64" ]; then \
		echo "Warning: Unsupported architecture $$ARCH, defaulting to x86_64"; \
		ARCH="x86_64"; \
	fi; \
	BINARY_PATH="$(INSTALL_DIR)/inbe-linux/inbe-linux-$$ARCH"; \
	if [ ! -f "$$BINARY_PATH" ]; then \
		echo "Error: Binary not found: $$BINARY_PATH"; \
		echo "Available binaries:"; \
		ls $(INSTALL_DIR)/inbe-linux/inbe-linux-* 2>/dev/null || echo "  None"; \
		exit 1; \
	fi; \
	if [ -L $(BIN_DIR)/inbe ]; then \
		echo "Removing existing symlink: $(BIN_DIR)/inbe"; \
		rm $(BIN_DIR)/inbe; \
	elif [ -e $(BIN_DIR)/inbe ]; then \
		echo "Error: $(BIN_DIR)/inbe exists and is not a symlink"; \
		echo "Please remove it manually and try again"; \
		exit 1; \
	fi; \
	ln -s "$$BINARY_PATH" $(BIN_DIR)/inbe && \
	echo "Created symlink: $(BIN_DIR)/inbe -> $$BINARY_PATH" && \
	echo "" && \
	echo "Installation complete!" && \
	echo "  Binary: $(BIN_DIR)/inbe" && \
	echo "  Data: $(INSTALL_DIR)/inbe-linux/" && \
	echo "" && \
	echo "Run 'inbe' to start the application"

uninstall:
	@echo "Uninstalling inbe..."
	@if [ -L $(BIN_DIR)/inbe ]; then \
		echo "Removing symlink: $(BIN_DIR)/inbe"; \
		rm $(BIN_DIR)/inbe; \
	elif [ -e $(BIN_DIR)/inbe ]; then \
		echo "Warning: $(BIN_DIR)/inbe exists but is not a symlink"; \
		echo "Skipping symlink removal"; \
	fi
	@if [ -d $(INSTALL_DIR) ]; then \
		echo "Removing directory: $(INSTALL_DIR)"; \
		rm -rf $(INSTALL_DIR); \
	else \
		echo "Install directory not found: $(INSTALL_DIR)"; \
	fi
	@echo "Uninstall complete"

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
	@cp $(CONFIG_FILES) $(WINDOWS_BUILD_DIR)/dist/inbe-windows/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/locales
	@cp $(LOCALE_FILES) $(WINDOWS_BUILD_DIR)/dist/inbe-windows/locales/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/themes
	@cp $(THEME_FILES) $(WINDOWS_BUILD_DIR)/dist/inbe-windows/themes/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/icons
	@cp $(WINDOWS_ICON_FILES) $(WINDOWS_BUILD_DIR)/dist/inbe-windows/icons/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets
	@cp $(IMAGE_FILES) $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/fonts
	@cp $(FONT_FILES) $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/fonts/
	@mkdir -p $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/sounds
	@cp $(SOUND_FILES) $(WINDOWS_BUILD_DIR)/dist/inbe-windows/assets/sounds/
	@cd $(WINDOWS_BUILD_DIR)/dist && zip -r ../inbe-windows.zip inbe-windows/
	@rm -rf $(WINDOWS_BUILD_DIR)/dist
	@echo "Created $(WINDOWS_BUILD_DIR)/inbe-windows.zip"
