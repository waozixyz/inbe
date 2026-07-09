#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/dist/click"

if [ -f "$ROOT_DIR/.env" ]; then
    set -a
    # shellcheck disable=SC1091
    . "$ROOT_DIR/.env"
    set +a
fi

OPENSTORE_BASE_URL="${OPENSTORE_BASE_URL:-https://open-store.io}"
OPENSTORE_APP_ID="${OPENSTORE_APP_ID:-inbe}"
OPENSTORE_CHANNEL="${OPENSTORE_CHANNEL:-focal}"
OPENSTORE_API_KEY="${OPENSTORE_API_KEY:-}"
CHANGELOG="${CHANGELOG:-}"
CLICK_FILE="${1:-}"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Error: '$1' command not found" >&2
        exit 1
    fi
}

if [ -z "$OPENSTORE_API_KEY" ]; then
    echo "Error: OPENSTORE_API_KEY is not set" >&2
    exit 1
fi

need_cmd curl
need_cmd jq

if [ -z "$CLICK_FILE" ]; then
    CLICK_FILE="$(ls -t "$BUILD_DIR"/*.click 2>/dev/null | head -1 || true)"
fi

if [ -z "$CLICK_FILE" ] || [ ! -f "$CLICK_FILE" ]; then
    echo "Error: Click file not found. Build first with: make click" >&2
    exit 1
fi

if [ -z "$CHANGELOG" ]; then
    app_version="$(sed -n 's/^#define INBE_VERSION_STRING "\([^"]*\)".*/\1/p' "$ROOT_DIR/src/core/version.h" 2>/dev/null || true)"
    CHANGELOG="Release ${app_version:-latest}"
fi

response_file="$(mktemp)"
trap 'rm -f "$response_file"' EXIT

api_url="$OPENSTORE_BASE_URL/api/v3/manage/$OPENSTORE_APP_ID"
revision_url="$api_url/revision"

http_code="$(curl -sS "$api_url?apikey=$OPENSTORE_API_KEY" -o "$response_file" -w '%{http_code}')"

if ! jq -e '.success == true' "$response_file" >/dev/null; then
    echo "Error: OpenStore auth probe failed (HTTP $http_code)" >&2
    jq -r '.message // "Unknown error"' "$response_file" >&2
    exit 1
fi

managed_app_id="$(jq -r '.data.id // empty' "$response_file")"
if [ "$managed_app_id" != "$OPENSTORE_APP_ID" ]; then
    echo "Error: API key can access '$managed_app_id', expected '$OPENSTORE_APP_ID'" >&2
    exit 1
fi

echo "OpenStore access OK for $OPENSTORE_APP_ID"
echo "Uploading $(basename "$CLICK_FILE") to channel $OPENSTORE_CHANNEL"

http_code="$(curl -sS -X POST \
    -F "file=@$CLICK_FILE" \
    -F "channel=$OPENSTORE_CHANNEL" \
    -F "changelog=$CHANGELOG" \
    "$revision_url?apikey=$OPENSTORE_API_KEY" \
    -o "$response_file" \
    -w '%{http_code}')"

if ! jq -e '.success == true' "$response_file" >/dev/null; then
    echo "Error: OpenStore revision upload failed (HTTP $http_code)" >&2
    jq -r '.message // "Unknown error"' "$response_file" >&2
    jq -r '.data.reasons[]? // empty' "$response_file" >&2
    exit 1
fi

version="$(jq -r '.data.version // empty' "$response_file")"
architectures="$(jq -r '.data.architectures // [] | join(",")' "$response_file")"
echo "OpenStore revision uploaded: version=$version architectures=$architectures"
