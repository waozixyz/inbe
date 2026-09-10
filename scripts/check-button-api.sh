#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

if rg -n '\b(StyledButton|TextButton|RenderTextButton|IconButton|PaddedIconBtn|IconBtn|OverlayButton|InvisibleButton)\s*\(' src --glob '*.kry' --glob '*.c'; then
    echo 'Use Button(ButtonProps) for buttons; use standard Checkbox/Toggle for their own controls.' >&2
    exit 1
fi
if rg -n 'TitleBar\(' src/screens/elist_screen.kry; then
    echo 'The Lists screen starts with its list tabs, without a separate title bar.' >&2
    exit 1
fi
echo 'Canonical button API and title-free list tabs checks passed'
