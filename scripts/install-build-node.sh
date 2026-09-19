#!/bin/sh
# Explicit container build dependency setup; never invoked by the proof checker.
set -eu

if command -v node >/dev/null 2>&1 && node -e '
    const [major, minor] = process.versions.node.split(".").map(Number);
    process.exit(major > 22 || (major === 22 && minor >= 18) ? 0 : 1);
'; then
    exit 0
fi

destination=${1:?Usage: install-build-node.sh destination}
case "$(uname -sm)" in
    "Linux x86_64")
        archive=node-v22.23.2-linux-x64.tar.xz
        checksum=d60acfe00a2932254bb0ad20e01b0d74397a0875595de719654b214f4b03f307
        ;;
    "Linux aarch64")
        archive=node-v22.23.2-linux-arm64.tar.xz
        checksum=fff4078c5def658577f92c88db7db3bc0072924bfb93fe52c1e744a54e94abb8
        ;;
    *)
        echo 'Install Node.js 22.18 or newer for this build host.' >&2
        exit 1
        ;;
esac

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
curl --fail --location --retry 3 "https://nodejs.org/dist/v22.23.2/$archive" -o "$temporary/$archive"
printf '%s  %s\n' "$checksum" "$temporary/$archive" | sha256sum -c -
tar -xJf "$temporary/$archive" -C "$temporary"
mkdir -p "$destination/bin"
install -m755 "$temporary/${archive%.tar.xz}/bin/node" "$destination/bin/node"
