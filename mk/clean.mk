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
