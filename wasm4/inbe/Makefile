CC = clang
CXX = clang++

WASM_OPT = wasm-opt
WASM_OPT_FLAGS = -Oz --zero-filled-memory --strip-producers --enable-bulk-memory

DEBUG = 0

CFLAGS = -target wasm32 -W -Wall -Wextra -Werror -Wno-unused -Wconversion -Wsign-conversion \
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

all: build/cart.wasm

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

.PHONY: clean bundle dist
clean:
	rm -rf build

bundle: build/cart.wasm
	w4 bundle build/cart.wasm --title "Inner Breeze" --html ../inbe.html

dist: build/cart.wasm
	cp build/cart.wasm ../inbe.wasm
	w4 bundle build/cart.wasm --title "Inner Breeze" --windows ../inbe_x86_64.exe --linux ../inbe_x86_64

.PHONY: all
all: build/cart.wasm

-include $(DEPS)