#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

status=0

git submodule foreach --recursive '
    dirty=0

    if ! git diff --quiet --ignore-submodules=none; then
        dirty=1
    fi

    if ! git diff --cached --quiet --ignore-submodules=none; then
        dirty=1
    fi

    if [ -n "$(git ls-files --others --exclude-standard)" ]; then
        dirty=1
    fi

    if [ "$dirty" -ne 0 ]; then
        echo "Dirty vendor submodule: $displaypath"
        git status --short
        exit 1
    fi
' || status=1

if [ "$status" -ne 0 ]; then
    echo
    echo "Do not edit vendor from Inbe. Make changes in the owning repo, commit and push there, then update only the submodule pointer here."
    exit 1
fi
