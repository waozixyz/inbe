.PHONY: all icons uxn wasm4 linux android-release android-debug clean

TARGETS := icons uxn wasm4 linux
ifneq ($(strip $(PASSWORD)),)
TARGETS += android-release
endif

all: $(TARGETS)
ifeq ($(strip $(PASSWORD)),)
	@echo "Skipping Android release; run make PASSWORD=your-password to include it."
endif

icons:
	$(MAKE) -C icons

uxn:
	$(MAKE) -C uxn

wasm4:
	$(MAKE) -C wasm4 all

linux:
	$(MAKE) -C linux all

android-release:
	$(MAKE) -C droid release PASSWORD="$(PASSWORD)"

android-debug:
	$(MAKE) -C droid debug

clean:
	$(MAKE) -C uxn clean
	$(MAKE) -C wasm4 clean
	$(MAKE) -C linux clean
	$(MAKE) -C droid clean
