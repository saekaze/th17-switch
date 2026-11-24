#!/bin/bash
# Fetch the TH17 runtime (github.com/Ritosa2333/eagler-touhou, ports/th17) at
# the commit in UPSTREAM_COMMIT into upstream/ and apply patches/native.patch.
#
# Only the C++ runtime is checked out (ports/th17/web/cpp/{game,sdl},
# ports/th17/portable and the shared common/include headers); none of the
# repository's game content or web launcher is downloaded.
#
# Usage: scripts/fetch_upstream.sh [dest-dir]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${1:-${ROOT}/upstream}"
COMMIT="$(tr -d ' \n' < "${ROOT}/UPSTREAM_COMMIT")"
URL="https://github.com/Ritosa2333/eagler-touhou.git"

if [ ! -d "${DEST}/.git" ]; then
    git init -q "${DEST}"
    git -C "${DEST}" remote add origin "${URL}"
fi
git -C "${DEST}" config core.sparseCheckout true
printf '%s\n' \
    '/ports/th17/web/cpp/game/' \
    '/ports/th17/web/cpp/sdl/' \
    '/ports/th17/portable/sdl/' \
    '/ports/th17/portable/input/' \
    '/ports/th17/THIRD-PARTY-NOTICES.txt' \
    '/common/include/eagler/text/' \
    '/common/include/eagler/thprac/' \
    '/common/include/eagler/audio/' \
    '/common/include/eagler/input/' \
    > "${DEST}/.git/info/sparse-checkout"
git -C "${DEST}" fetch -q --depth 1 --filter=blob:none origin "${COMMIT}"
git -C "${DEST}" checkout -q --force FETCH_HEAD
[ ! -s "${ROOT}/patches/native.patch" ] || git -C "${DEST}" apply --whitespace=nowarn "${ROOT}/patches/native.patch"
echo "upstream ${COMMIT} ready in ${DEST}"
