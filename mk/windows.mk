WIN_CC ?= x86_64-w64-mingw32-gcc
WIN_AR ?= x86_64-w64-mingw32-ar
WIN_RANLIB ?= x86_64-w64-mingw32-ranlib
WIN_WINDRES ?= x86_64-w64-mingw32-windres
WIN32_CC ?= i686-w64-mingw32-gcc
WIN32_AR ?= i686-w64-mingw32-ar
WIN32_RANLIB ?= i686-w64-mingw32-ranlib
WIN32_WINDRES ?= i686-w64-mingw32-windres
WINDOWS_ARCHES ?= x86_64 i686
WINDOWS_RENDERERS ?= opengl rlsw
WIN_x86_64_CC = $(WIN_CC)
WIN_x86_64_AR = $(WIN_AR)
WIN_x86_64_RANLIB = $(WIN_RANLIB)
WIN_x86_64_WINDRES = $(WIN_WINDRES)
WIN_x86_64_MCFGTHREADS = $(MCFGTHREADS)
WIN_i686_CC = $(WIN32_CC)
WIN_i686_AR = $(WIN32_AR)
WIN_i686_RANLIB = $(WIN32_RANLIB)
WIN_i686_WINDRES = $(WIN32_WINDRES)
WIN_i686_MCFGTHREADS = $(WIN32_MCFGTHREADS)
WIN_RESOURCE = windows/inbe.rc
WIN_ICON = windows/inbe.ico
WIN_FLINT_SRCS = $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))

WIN_opengl_TARGET_PREFIX = inbe-windows-
WIN_opengl_TARGET_SUFFIX =
WIN_opengl_PLATFORM = PLATFORM_DESKTOP
WIN_opengl_GRAPHICS = GRAPHICS_API_OPENGL_21
WIN_opengl_LDLIBS = -lopengl32 -lgdi32 -lwinmm -lws2_32 -luser32
WIN_rlsw_TARGET_PREFIX = inbe-windows-rlsw-
WIN_rlsw_TARGET_SUFFIX =
WIN_rlsw_PLATFORM = PLATFORM_DESKTOP_WIN32
WIN_rlsw_GRAPHICS = GRAPHICS_API_OPENGL_SOFTWARE
WIN_rlsw_LDLIBS = -lopengl32 -lgdi32 -lwinmm -lws2_32 -luser32 -lshcore

WINDOWS_TARGETS = $(foreach arch,$(WINDOWS_ARCHES),$(foreach renderer,$(WINDOWS_RENDERERS),$(WINDOWS_BIN_DIR)/$(WIN_$(renderer)_TARGET_PREFIX)$(arch)$(WIN_$(renderer)_TARGET_SUFFIX).exe))

$(WINDOWS_OBJ_DIR)/%:
	mkdir -p $@

$(WINDOWS_OBJ_DIR)/%/raylib: | $(WINDOWS_OBJ_DIR)/%
	mkdir -p $@

windows: $(WINDOWS_TARGETS)

define WINDOWS_ARCH_COMMON_RULES
$(WINDOWS_OBJ_DIR)/$(1)/libinbe.a: $(INBE_DIR)/inbe.c $(INBE_DIR)/inbe.h | $(WINDOWS_OBJ_DIR)/$(1)
	$(MAKE) -C $(INBE_DIR) clean
	$(MAKE) -C $(INBE_DIR) CC=$$(WIN_$(1)_CC) AR=$$(WIN_$(1)_AR) RANLIB=$$(WIN_$(1)_RANLIB)
	cp $(INBE_A) $$@

$(WINDOWS_OBJ_DIR)/$(1)/inbe.res: $(WIN_RESOURCE) $(WIN_ICON) | $(WINDOWS_OBJ_DIR)/$(1)
	$$(WIN_$(1)_WINDRES) -Iwindows -O coff $$< $$@
endef

define WINDOWS_ARCH_RENDERER_RULES
$(WINDOWS_OBJ_DIR)/$(1)/$(2)/raylib/libraylib.a: $(RAYLIB_SOURCES) | $(WINDOWS_OBJ_DIR)/$(1)/$(2)/raylib
	$(MAKE) -C $(RAYLIB_DIR) clean RAYLIB_RELEASE_PATH=../../../$(WINDOWS_OBJ_DIR)/$(1)/$(2)/raylib
	$(MAKE) -j1 -C $(RAYLIB_DIR) \
		CC="$$(WIN_$(1)_CC)" \
		AR="$$(WIN_$(1)_AR)" \
		PLATFORM=$$(WIN_$(2)_PLATFORM) \
		PLATFORM_OS=WINDOWS \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../../../$(WINDOWS_OBJ_DIR)/$(1)/$(2)/raylib \
		RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=FALSE \
		GRAPHICS=$$(WIN_$(2)_GRAPHICS) \
		CUSTOM_CFLAGS="$(INBE_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"
	@for obj in $(RAYLIB_DIR)/*.o; do \
		[ -e "$$$$obj" ] || continue; \
		mv "$$$$obj" $(WINDOWS_OBJ_DIR)/$(1)/$(2)/raylib/; \
	done

$(WINDOWS_BIN_DIR)/$$(WIN_$(2)_TARGET_PREFIX)$(1)$$(WIN_$(2)_TARGET_SUFFIX).exe: $(SRC) $(WIN_FLINT_SRCS) $(FONT_FILES) $(EMBEDDED_ASSETS_C) $(WINDOWS_OBJ_DIR)/$(1)/$(2)/raylib/libraylib.a $(WINDOWS_OBJ_DIR)/$(1)/libinbe.a $(WINDOWS_OBJ_DIR)/$(1)/inbe.res | $(WINDOWS_BIN_DIR)
	$$(WIN_$(1)_CC) $(CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		$(FLINT_INCLUDE) \
		-Isrc -Isrc/android \
		-o $$@ \
		$(SRC) \
		$(WIN_FLINT_SRCS) \
		$(WINDOWS_OBJ_DIR)/$(1)/libinbe.a \
		$(WINDOWS_OBJ_DIR)/$(1)/$(2)/raylib/libraylib.a \
		$(WINDOWS_OBJ_DIR)/$(1)/inbe.res \
		-L$$(WIN_$(1)_MCFGTHREADS)/lib \
		$$(WIN_$(2)_LDLIBS) \
		-mwindows \
		-static -static-libgcc \
		$(LDFLAGS)
endef

$(foreach arch,$(WINDOWS_ARCHES),$(eval $(call WINDOWS_ARCH_COMMON_RULES,$(arch))))
$(foreach arch,$(WINDOWS_ARCHES),$(foreach renderer,$(WINDOWS_RENDERERS),$(eval $(call WINDOWS_ARCH_RENDERER_RULES,$(arch),$(renderer)))))
