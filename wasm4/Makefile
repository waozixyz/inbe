CC = $(if $(WASM_CC),$(WASM_CC),clang)
CXX = $(if $(WASM_CXX),$(WASM_CXX),clang++)

WASM_OPT = wasm-opt
WASM_OPT_FLAGS = -Oz --zero-filled-memory --strip-producers --enable-bulk-memory

DEBUG = 0

CFLAGS = -target wasm32 -W -Wall -Wextra -Wno-unused-command-line-argument -Werror -Wno-unused -Wconversion -Wsign-conversion \
         -MMD -MP -nostdlib -ffreestanding -mbulk-memory

ifeq ($(DEBUG), 1)
	CFLAGS += -DDEBUG -O0 -g
else
	CFLAGS += -DNDEBUG -Oz -flto
endif

LDFLAGS = -Wl,-zstack-size=14752 \
          -Wl,--no-entry \
          -Wl,--import-memory \
          -Wl,--allow-undefined \
          -Wl,--export=start \
          -Wl,--export=update \
          -Wl,--initial-memory=65536 \
          -Wl,--max-memory=65536 \
          -Wl,--stack-first

ifeq ($(DEBUG), 1)
	LDFLAGS += -Wl,--export-all,--no-gc-sections
else
	LDFLAGS += -Wl,--strip-all,--gc-sections,--lto-O3
endif

SOURCES = $(wildcard src/*.c) $(wildcard src/*.cpp)
OBJECTS = $(patsubst src/%.c, build/%.o, $(patsubst src/%.cpp, build/%.o, $(SOURCES)))
DEPS = $(OBJECTS:.o=.d)

all: build/cart.wasm bundle dist package

build/cart.wasm: $(OBJECTS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)
ifneq ($(DEBUG), 1)
	@if command -v $(WASM_OPT) >/dev/null 2>&1; then \
		echo "Optimizing cart size with wasm-opt..."; \
		$(WASM_OPT) $(WASM_OPT_FLAGS) $@ -o $@; \
	else \
		echo "Tip: Install 'binaryen' in your shell.nix for smaller builds!"; \
	fi
endif

build/%.o: src/%.c
	@mkdir -p build
	$(CC) -c $< -o $@ $(CFLAGS)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) -c $< -o $@ $(CFLAGS)

.PHONY: clean bundle dist package
clean:
	rm -rf build

bundle: build/cart.wasm
	w4 bundle build/cart.wasm --title "Inner Breeze" --html build/index.html

dist: build/cart.wasm
	w4 bundle build/cart.wasm --title "Inner Breeze" --windows build/inbe-x86_64.exe --linux build/inbe-x86_64

package: dist
	cd build && zip -q -9 inbe-windows.zip inbe-*.exe
	chmod 755 build/inbe-x86_64
	cd build && tar -czf inbe-linux.tar.gz inbe-x86_64

-include $(DEPS)
