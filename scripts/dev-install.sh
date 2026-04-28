#!/usr/bin/env bash
# Local install for development. No root needed.
#
# Copies the just-built addon, its .conf, and the keymap into XDG_DATA_HOME
# (~/.local/share by default) and the matching plugin dir (~/.local/lib).
# After running this you still need to relaunch fcitx5 with the
# FCITX_ADDON_DIRS env var that prepends ~/.local/lib/fcitx5 — print the
# command at the end.
#
# Usage:
#   ./scripts/dev-install.sh           # uses ./build by default
#   BUILD_DIR=out ./scripts/dev-install.sh
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ ! -f "${REPO_ROOT}/${BUILD_DIR}/src/libsinsebeolsik-p2.so" ]]; then
    echo "no built addon at ${BUILD_DIR}/src/libsinsebeolsik-p2.so" >&2
    echo "run: cmake -S . -B ${BUILD_DIR} -G Ninja && ninja -C ${BUILD_DIR}" >&2
    exit 1
fi

XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
ADDON_LIB_DIR="$HOME/.local/lib/fcitx5"
ADDON_CONF_DIR="${XDG_DATA_HOME}/fcitx5/addon"
KEYMAP_DIR="${XDG_DATA_HOME}/fcitx5/sinsebeolsik-p2"

mkdir -p "${ADDON_LIB_DIR}" "${ADDON_CONF_DIR}" "${KEYMAP_DIR}"

install -m 0644 "${REPO_ROOT}/${BUILD_DIR}/src/libsinsebeolsik-p2.so"  "${ADDON_LIB_DIR}/"
install -m 0644 "${REPO_ROOT}/${BUILD_DIR}/src/sinsebeolsik-p2.conf"   "${ADDON_CONF_DIR}/"
install -m 0644 "${REPO_ROOT}/keymaps/sinsebeolsik_p2.toml"            "${KEYMAP_DIR}/"

echo "installed:"
echo "  ${ADDON_LIB_DIR}/libsinsebeolsik-p2.so"
echo "  ${ADDON_CONF_DIR}/sinsebeolsik-p2.conf"
echo "  ${KEYMAP_DIR}/sinsebeolsik_p2.toml"
echo
echo "to start fcitx5 with this addon, run:"
echo
echo "  pkill fcitx5; sleep 1; FCITX_ADDON_DIRS=\"${ADDON_LIB_DIR}:/usr/lib/x86_64-linux-gnu/fcitx5\" fcitx5 -d"
echo
echo "then add 'Sinsebeolsik P2' in fcitx5-configtool and toggle Hangul mode (default key: Hangul)."
