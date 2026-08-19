#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 PACKAGE_FILE [EXTRA_PACKAGE ...]" >&2
  exit 2
fi

package_file="$1"
shift

if [ ! -f "$package_file" ]; then
  echo "package file not found: $package_file" >&2
  exit 2
fi

APT_CACHE_DIR="${APT_CACHE_DIR:-/tmp/apt-cache}"
codename="$(
  . /etc/os-release
  printf '%s' "${VERSION_CODENAME:-jammy}"
)"

# GitHub-hosted Ubuntu runners sometimes use an Azure mirrorlist that stalls.
# Pin Ubuntu packages to the canonical archive before running apt.
sudo tee /etc/apt/sources.list.d/ubuntu.sources >/dev/null <<EOF
Types: deb
URIs: https://archive.ubuntu.com/ubuntu
Suites: $codename ${codename}-updates ${codename}-backports ${codename}-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

mapfile -t raw_packages < "$package_file"
packages=()
for package in "${raw_packages[@]}" "$@"; do
  if [ -z "$package" ] || [[ "$package" == \#* ]]; then
    continue
  fi
  packages+=("$package")
done

mkdir -p "$APT_CACHE_DIR/partial"
sudo apt-get \
  -o Acquire::Retries=3 \
  -o Acquire::http::Timeout=20 \
  -o Acquire::https::Timeout=20 \
  update
if [ "${#packages[@]}" -gt 0 ]; then
  sudo apt-get \
    -o Acquire::Retries=3 \
    -o Acquire::http::Timeout=20 \
    -o Acquire::https::Timeout=20 \
    -o Dir::Cache::archives="$APT_CACHE_DIR" \
    install -y "${packages[@]}"
fi
sudo chown -R "$USER:$USER" "$APT_CACHE_DIR"
