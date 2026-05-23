CC = gcc
CFLAGS = -Wall -Wextra -O2
ARCH := $(shell uname -m)
LOTUS_DIR = ../liblotus
LOTUS_LIB = $(LOTUS_DIR)/liblotus.a
LOTUS_INC = -I$(LOTUS_DIR)
SRC_DIR = src
BUILD_DIR = build

CFLAGS += $(LOTUS_INC)

all: $(LOTUS_LIB) $(BUILD_DIR)/inbe-$(ARCH) $(BUILD_DIR)/inbe-linux.tar.gz

$(BUILD_DIR)/inbe-$(ARCH): $(SRC_DIR)/main.c $(SRC_DIR)/graphics.c $(SRC_DIR)/common.c $(SRC_DIR)/graphics.h $(SRC_DIR)/common.h $(LOTUS_LIB)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main.c $(SRC_DIR)/graphics.c $(SRC_DIR)/common.c $(LOTUS_LIB) -lX11

$(BUILD_DIR)/inbe-linux.tar.gz: $(BUILD_DIR)/inbe-$(ARCH)
	find $(BUILD_DIR) -maxdepth 1 -type f \( -name 'inbe-*' -o -name 'inbefb-*' \) ! -name '*.tar.gz' -exec chmod 755 {} +
	cd $(BUILD_DIR) && tar -czf inbe-linux.tar.gz $$(find . -maxdepth 1 -type f \( -name 'inbe-*' \) ! -name '*.tar.gz' -printf '%f\n' | sort)

$(LOTUS_LIB):
	$(MAKE) -C $(LOTUS_DIR)

clean:
	rm -f $(BUILD_DIR)/inbe $(BUILD_DIR)/inbe_* $(BUILD_DIR)/inbe-*
	$(MAKE) -C $(LOTUS_DIR) clean

.PHONY: all clean
