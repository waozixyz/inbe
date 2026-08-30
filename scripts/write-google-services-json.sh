#!/bin/sh
set -eu

out=${1:-droid/app/src/gplay/google-services.json}
mkdir -p "$(dirname "$out")"

if [ -n "${GOOGLE_SERVICES_JSON_BASE64:-}" ]; then
	printf '%s' "$GOOGLE_SERVICES_JSON_BASE64" | base64 -d > "$out"
elif [ -n "${GOOGLE_SERVICES_JSON:-}" ]; then
	printf '%s\n' "$GOOGLE_SERVICES_JSON" > "$out"
elif [ -f "$out" ]; then
	exit 0
else
	printf '%s\n' "Missing GOOGLE_SERVICES_JSON or GOOGLE_SERVICES_JSON_BASE64 for $out" >&2
	exit 1
fi

python3 -m json.tool "$out" >/dev/null
