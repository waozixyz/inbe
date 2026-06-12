.DEFAULT_GOAL := all

FLINT_DIR ?= vendor/flint
FLINT_PROJECT ?= flint.toml
ANDROID_ACTIVITY ?= xyz.waozi.inbe.MainActivity

include $(FLINT_DIR)/mk/project.mk
