.DEFAULT_GOAL := all

include mk/common.mk
include mk/native.mk
include mk/windows.mk
include mk/web.mk
include mk/android.mk
include mk/dist.mk
include mk/clean.mk

FORCE:

.PHONY: \
	all \
	native \
	run \
	linux \
	$(LINUX_ARCHES:%=linux-%) \
	build-linux-arch \
	windows \
	web \
	clean \
	clean-linux \
	clean-windows \
	clean-web \
	clean-raylib \
	dist \
	dist-linux \
	dist-windows \
	install \
	uninstall \
	android-init-signing \
	android-debug \
	android-release \
	android-bundle \
	android-copy-assets \
	android-copy-apks \
	android-copy-debug-apks \
	android-copy-release-apks \
	android-copy-bundle \
	android-install \
	android-install-release \
	android-clean \
	FORCE
