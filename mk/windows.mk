WIN_CC = x86_64-w64-mingw32-gcc
WIN_AR = x86_64-w64-mingw32-ar
WIN_WINDRES = x86_64-w64-mingw32-windres
WIN_TARGET = $(WINDOWS_BUILD_DIR)/inbe-windows-x86_64.exe
WIN_RAYLIB_BUILD_DIR = $(WINDOWS_BUILD_DIR)/raylib
WIN_RAYLIB_A = $(WIN_RAYLIB_BUILD_DIR)/libraylib.a
WIN_FLINT_SRCS = $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
WIN_RAYLIB_OBJS = \
	$(WIN_RAYLIB_BUILD_DIR)/rcore.o \
	$(WIN_RAYLIB_BUILD_DIR)/rshapes.o \
	$(WIN_RAYLIB_BUILD_DIR)/rtextures.o \
	$(WIN_RAYLIB_BUILD_DIR)/rtext.o \
	$(WIN_RAYLIB_BUILD_DIR)/raudio.o
WIN_INBE_A = $(WINDOWS_BUILD_DIR)/libinbe.a

$(WIN_RAYLIB_BUILD_DIR):
	mkdir -p $@

windows: $(WIN_TARGET)

$(WIN_RAYLIB_BUILD_DIR)/%.o: $(RAYLIB_DIR)/%.c | $(WIN_RAYLIB_BUILD_DIR)
	$(WIN_CC) \
		-c $< \
		-o $@ \
		-Wall \
		-D_GNU_SOURCE \
		-DPLATFORM_DESKTOP_WIN32 \
		-DGRAPHICS_API_OPENGL_33 \
		-DSUPPORT_MODULE_RAUDIO=1 \
		-DSUPPORT_FILEFORMAT_OGG=1 \
		-Wno-missing-braces \
		-Werror=pointer-arith \
		-fno-strict-aliasing \
		-std=gnu99 \
		-DUNICODE \
		$(INBE_RAYLIB_CONFIG) \
		-Os \
		-ffunction-sections \
		-fdata-sections \
		-I$(RAYLIB_DIR)

$(WIN_RAYLIB_A): $(RAYLIB_SOURCES) $(WIN_RAYLIB_OBJS)
	$(WIN_AR) rcs $@ $(WIN_RAYLIB_OBJS)

$(WIN_INBE_A): FORCE | $(WINDOWS_BUILD_DIR)
	$(MAKE) -C $(INBE_DIR) clean
	$(MAKE) -C $(INBE_DIR) CC=$(WIN_CC) AR=$(WIN_AR)
	cp $(INBE_A) $@

$(WIN_TARGET): $(SRC) $(WIN_FLINT_SRCS) $(FONT_FILES) $(WIN_RAYLIB_A) $(WIN_INBE_A) | $(WINDOWS_BUILD_DIR)
	$(WIN_CC) $(CFLAGS) \
		-I$(RAYLIB_DIR) \
		-I$(INBE_DIR) \
		$(FLINT_INCLUDE) \
		-Isrc -Isrc/android \
		-o $@ \
		$(SRC) \
		$(WIN_FLINT_SRCS) \
		$(WIN_INBE_A) \
		$(WIN_RAYLIB_A) \
		-L$(MCFGTHREADS)/lib \
		-lopengl32 -lgdi32 -lwinmm -lws2_32 \
		-mwindows \
		$(LDFLAGS)
