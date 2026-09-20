#!/usr/bin/env bash
# Storage vocabulary lives only in the proved laws package:
# laws/storage_layout -> scripts/generate-storage-layout.mjs -> storage_layout.h.
# Hand-typed copies in src/ drift during renames and orphan user data.
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v rg >/dev/null 2>&1; then
    echo 'Storage literal checks require ripgrep (rg).' >&2
    exit 1
fi

status=0

check_pattern() {
    local pattern="$1"
    local message="$2"
    if rg -n "$pattern" src --glob '*.kry' --glob '*.c' --glob '*.h'; then
        echo "$message" >&2
        status=1
    fi
}

check_pattern '"(breathing|inbe)\.db"' \
    'Database names come from storage_layout.h (STORAGE_DB_NAME / STORAGE_DB_NAME_LEGACY).'
check_pattern '"(breathing|inbe)-data/' \
    'Export and import entry names come from storage_layout.h (storage_import_entries).'
check_pattern '"breathing-data-sqlite"' \
    'The export metadata format string comes from storage_layout.h (STORAGE_EXPORT_META_FORMAT).'
check_pattern '"/home/(breathing|inbe)"' \
    'Web home paths come from storage_layout.h (STORAGE_WEB_HOME / STORAGE_WEB_HOME_LEGACY).'
check_pattern '"BreathSession"' \
    'Directory names come from storage_layout.h (STORAGE_DIR_NAME / STORAGE_DIR_LEGACY_*).'
check_pattern '"(breathing|inbe)-sessions\.csv"|"breathing-web-export' \
    'Export artifact names come from storage_layout.h (STORAGE_EXPORT_*).'
check_pattern '\bjoin_path2\b' \
    'Use storage_join_path (src/storage/db.kry) for path assembly.'
check_pattern 'snprintf\([^;]*"(breathing|inbe)["/]' \
    'Paths are assembled from storage_layout.h constants, not hand-typed names.'

if [ "$status" -ne 0 ]; then
    echo 'Storage vocabulary must be defined only by the proved storage layout laws.' >&2
    exit 1
fi
echo 'Storage literal checks passed'
