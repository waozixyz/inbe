dist:
	@# Get password once for both Android builds
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
	if [ -z "$$PASSWORD_VALUE" ]; then \
		echo "Keystore password is required"; \
		exit 1; \
	fi; \
	echo "Building Android release APK (validating password)..."; \
	$(MAKE) android-release PASSWORD="$$PASSWORD_VALUE" || exit $$?; \
	echo "Building Android bundle..."; \
	$(MAKE) android-bundle PASSWORD="$$PASSWORD_VALUE" || exit $$?; \
	echo "✓ Android builds complete, proceeding with desktop builds..."; \
	$(MAKE) linux || exit $$?; \
	$(MAKE) windows || exit $$?; \
	$(MAKE) web || exit $$?; \
	$(MAKE) dist-linux || exit $$?; \
	$(MAKE) dist-windows || exit $$?

dist-linux: linux | $(LINUX_DIST_DIR)
	@echo "Creating Linux tar.gz package with all Linux arch binaries..."
	@for bin in $(LINUX_BIN_DIR)/inbe-linux-*; do \
		if [ -f "$$bin" ] && [ -x "$$bin" ]; then \
			echo "Adding $$bin"; \
		fi; \
	done
	@if [ -z "$$(find $(LINUX_BIN_DIR) -maxdepth 1 -type f -name 'inbe-linux-*' -executable 2>/dev/null)" ]; then \
		echo "No Linux binaries found in $(LINUX_BIN_DIR)"; \
		exit 1; \
	fi
	@rm -f $(TARBALL)
	@find $(LINUX_BIN_DIR) -maxdepth 1 -type f -name 'inbe-linux-*' -executable -printf '%f\n' | sort > $(LINUX_DIST_DIR)/inbe-linux.files
	@tar -czf $(abspath $(TARBALL)) -C $(LINUX_BIN_DIR) -T $(abspath $(LINUX_DIST_DIR)/inbe-linux.files)
	@rm -f $(LINUX_DIST_DIR)/inbe-linux.files
	@echo "Created $(TARBALL)"

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
	BINARY_PATH="$(INSTALL_DIR)/inbe-linux-$$ARCH"; \
	if [ ! -f "$$BINARY_PATH" ]; then \
		echo "Error: Binary not found: $$BINARY_PATH"; \
		echo "Available binaries:"; \
		ls $(INSTALL_DIR)/inbe-linux-* 2>/dev/null || echo "  None"; \
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
	echo "  Data: $(INSTALL_DIR)" && \
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

dist-windows: windows | $(WINDOWS_DIST_DIR)
	@echo "Creating Windows zip package with all Windows arch binaries..."
	@for bin in $(WINDOWS_BIN_DIR)/inbe-windows-*.exe; do \
		if [ -f "$$bin" ]; then \
			echo "Adding $$bin"; \
		fi; \
	done
	@if [ -z "$$(find $(WINDOWS_BIN_DIR) -maxdepth 1 -type f -name 'inbe-windows-*.exe' 2>/dev/null)" ]; then \
		echo "No Windows binaries found in $(WINDOWS_BIN_DIR)"; \
		exit 1; \
	fi
	@rm -f $(WINDOWS_DIST_DIR)/inbe-windows.zip
	@cd $(WINDOWS_BIN_DIR) && zip -j $(abspath $(WINDOWS_DIST_DIR)/inbe-windows.zip) inbe-windows-*.exe
	@echo "Created $(WINDOWS_DIST_DIR)/inbe-windows.zip"
