#!/bin/sh

set -eu

if [ $# -gt 0 ]; then
	version=$1
else
	script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
	root_dir=$(dirname -- "$script_dir")
	version=$(sed -n 's/^#define INBE_VERSION_STRING "\([^"]*\)".*/\1/p' "$root_dir/src/core/version.h")
fi

if [ -z "$version" ]; then
	printf 'Error: version is empty\n' >&2
	exit 1
fi

cat <<EOF
inbe-$version.apk
inbe-$version.aab
inbe-web.zip
inbe-chrome-web-store.zip
inbe-firefox-addons.zip
inbe-windows.zip
inbe-windows-setup-$version.exe
inbe-linux-x86_64.AppImage
inbe-linux-aarch64.AppImage
inbe_${version}_amd64.deb
inbe_${version}_arm64.deb
inbe-${version}-1.x86_64.rpm
inbe-${version}-1.aarch64.rpm
inbe_${version}_x86_64.snap
inbe_${version}_aarch64.snap
inbe-${version}-x86_64.flatpak
inbe-${version}-aarch64.flatpak
inbe-${version}-freebsd-x86_64.pkg
EOF
