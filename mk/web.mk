WEB_CC ?= emcc
WEB_AR ?= emar
WEB_TARGET = $(WEB_BUILD_DIR)/index.html
WEB_RAYLIB_BUILD_DIR = $(WEB_BUILD_DIR)/raylib
WEB_RAYLIB_A = $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
WEB_FLINT_SRCS = $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
WEB_RAYLIB_OBJS = \
	$(WEB_RAYLIB_BUILD_DIR)/rcore.o \
	$(WEB_RAYLIB_BUILD_DIR)/rshapes.o \
	$(WEB_RAYLIB_BUILD_DIR)/rtextures.o \
	$(WEB_RAYLIB_BUILD_DIR)/rtext.o \
	$(WEB_RAYLIB_BUILD_DIR)/raudio.o
WEB_CFLAGS = -Wall -Wextra -std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DFLINT_EMBEDDED_ONLY=1
WEB_SHELL = src/web_shell.html
WEB_PUBLIC_FILES = manifest.json $(shell find web-assets -type f 2>/dev/null)
WEB_LDFLAGS = -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -lidbfs.js --shell-file $(WEB_SHELL)

$(WEB_RAYLIB_BUILD_DIR):
	mkdir -p $@

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

$(WEB_RAYLIB_A): $(RAYLIB_SOURCES) $(WEB_RAYLIB_OBJS)
	$(WEB_AR) rcs $@ $(WEB_RAYLIB_OBJS)

$(WEB_TARGET): $(BUILD_MAKEFILES) $(SRC) $(WEB_FLINT_SRCS) $(INBE_DIR)/inbe.c $(WEB_SHELL) $(WEB_RAYLIB_A) $(WEB_PUBLIC_FILES) | $(WEB_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		$(FLINT_INCLUDE) \
		-Isrc -Isrc/android \
		-o $@ \
		$(SRC) \
		$(WEB_FLINT_SRCS) \
		$(INBE_DIR)/inbe.c \
		$(WEB_RAYLIB_A) \
		$(WEB_LDFLAGS)
	cache_buster=$$(find $(WEB_BUILD_DIR) -maxdepth 1 \( -name 'index.js' -o -name 'index.data' -o -name 'index.wasm' \) -type f -print | sort | xargs cksum | cksum | cut -d ' ' -f 1); \
		sed -i "s#src=\"index.js\"#src=\"index.js?v=$$cache_buster\"#; s#WEB_CACHE_BUSTER#$$cache_buster#g" $(WEB_TARGET)
	rsync -a web-assets/ $(WEB_BUILD_DIR)/web-assets/
	cp manifest.json $(WEB_BUILD_DIR)/
