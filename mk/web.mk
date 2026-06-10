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
WEB_CFLAGS = -Wall -Wextra -std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1
WEB_SHELL = src/web_shell.html
WEB_SOUND_FILES = $(wildcard assets/sounds/*)
WEB_FONT_FILES = $(FONT_FILES)
WEB_LOCALE_PRELOADS = $(foreach file,$(LOCALE_FILES),--preload-file $(file)@locales/$(notdir $(file)))
WEB_THEME_PRELOADS = $(foreach file,$(THEME_FILES),--preload-file $(file)@themes/$(notdir $(file)))
WEB_SOUND_PRELOADS = $(foreach file,$(WEB_SOUND_FILES),--preload-file $(file)@assets/sounds/$(notdir $(file)))
WEB_FONT_PRELOADS = $(foreach file,$(WEB_FONT_FILES),--preload-file $(file)@assets/fonts/$(notdir $(file)))
WEB_ASSET_FILES = $(CONFIG_FILES) $(IMAGE_FILES) $(LOCALE_FILES) $(THEME_FILES) $(WEB_SOUND_FILES) $(WEB_FONT_FILES)
WEB_LDFLAGS = -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -lidbfs.js --shell-file $(WEB_SHELL) $(WEB_LOCALE_PRELOADS) $(WEB_THEME_PRELOADS) $(WEB_SOUND_PRELOADS) $(WEB_FONT_PRELOADS) --preload-file inbe.ini@inbe.ini --preload-file theme.ini@theme.ini --preload-file assets/angel.jpg@assets/angel.jpg --preload-file assets/begin.jpg@assets/begin.jpg

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

$(WEB_TARGET): $(BUILD_MAKEFILES) $(SRC) $(WEB_FLINT_SRCS) $(INBE_DIR)/inbe.c $(WEB_SHELL) $(WEB_RAYLIB_A) $(WEB_ASSET_FILES) | $(WEB_BUILD_DIR)
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
	cache_buster=$$(cksum $(WEB_BUILD_DIR)/index.js $(WEB_BUILD_DIR)/index.data $(WEB_BUILD_DIR)/index.wasm | cksum | cut -d ' ' -f 1); \
		sed -i "s#src=\"index.js\"#src=\"index.js?v=$$cache_buster\"#; s#WEB_CACHE_BUSTER#$$cache_buster#g" $(WEB_TARGET)
