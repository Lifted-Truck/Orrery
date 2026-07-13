#!/usr/bin/env bash
# validate_au.sh — install the built AU and run auval (the O1b human-run gate).
#
# Machine-local + sandbox-sensitive (global CLAUDE.md build gotchas): writes to
# ~/Library, and codesign/auval/killall hit a sandbox overlay unless run for
# real. Run this in a normal terminal (NOT inside a sandboxed agent step).
#
# Prereq: build the AU first, e.g.
#   cmake -S . -B build-plugin -G "Unix Makefiles" -DORRERY_BUILD_PLUGIN=ON \
#         [-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/juce]
#   cmake --build build-plugin --target Orrery_AU -j
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build-plugin}"
AU="$BUILD/shell/plugin/Orrery_artefacts/Debug/AU/Orrery.component"
DEST="$HOME/Library/Audio/Plug-Ins/Components/Orrery.component"

[ -d "$AU" ] || { echo "AU not built at $AU — build Orrery_AU first (see header)"; exit 1; }

echo "== verifying build seal =="
codesign --verify --deep --strict "$AU" && echo "  build seal OK"

echo "== installing to $DEST =="
rm -rf "$DEST"; cp -R "$AU" "$DEST"
codesign --verify --deep --strict "$DEST" && echo "  installed seal OK"

echo "== resetting AU cache =="
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 1

echo "== auval -v aumu Orry Lftk =="
auval -v aumu Orry Lftk
