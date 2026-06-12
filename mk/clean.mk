clean:
	rm -rf build
	$(MAKE) -C $(INBE_DIR) clean

clean-linux:
	rm -rf $(LINUX_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR)
	$(MAKE) -C $(INBE_DIR) clean

clean-windows:
	rm -rf $(WINDOWS_OBJ_DIR) $(WINDOWS_BIN_DIR) $(WINDOWS_DIST_DIR)
	$(MAKE) -C $(INBE_DIR) clean

clean-web:
	rm -rf $(WEB_BUILD_DIR)

clean-raylib:
	$(MAKE) -C $(RAYLIB_DIR) clean
	rm -rf vendor/raylib/build
