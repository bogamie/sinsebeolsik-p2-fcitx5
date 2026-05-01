#!/usr/bin/env bash
# 개발 사이클 한 번에 — configure(필요 시) → build → tests → install → fcitx5 재시작.
#
# 사용:
#   ./scripts/dev-reload.sh             # 전체 사이클
#   ./scripts/dev-reload.sh --skip-tests # 테스트 생략
#   ./scripts/dev-reload.sh --no-restart # fcitx5 재시작 안 함

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"

SKIP_TESTS=0
NO_RESTART=0
for arg in "$@"; do
    case "${arg}" in
        --skip-tests) SKIP_TESTS=1 ;;
        --no-restart) NO_RESTART=1 ;;
        -h|--help)
            sed -n '2,10p' "$0"
            exit 0 ;;
        *)
            echo "unknown option: ${arg}" >&2; exit 2 ;;
    esac
done

step() { printf '\n\033[1;36m▶ %s\033[0m\n' "$*"; }

# ─── 1. Configure (build dir이 없거나 build.ninja가 없으면) ────────────────
if [[ ! -f "${BUILD}/build.ninja" ]]; then
    step "Configuring CMake (Ninja)"
    cmake -S "${ROOT}" -B "${BUILD}" -G Ninja
fi

# ─── 2. Build ────────────────────────────────────────────────────────────────
step "Building"
cmake --build "${BUILD}" -j

# ─── 3. Tests ────────────────────────────────────────────────────────────────
if [[ "${SKIP_TESTS}" -eq 0 ]]; then
    step "Running tests"
    ctest --test-dir "${BUILD}" --output-on-failure
fi

# ─── 4. Install to ~/.local ──────────────────────────────────────────────────
step "Installing to ~/.local"
ADDON_LIB_DIR="${HOME}/.local/lib/fcitx5"
ADDON_CONF_DIR="${HOME}/.local/share/fcitx5/addon"
KEYMAP_DIR="${HOME}/.local/share/fcitx5/sinsebeolsik-p2"
mkdir -p "${ADDON_LIB_DIR}" "${ADDON_CONF_DIR}" "${KEYMAP_DIR}"
install -m 755 "${BUILD}/src/libsinsebeolsik-p2.so" "${ADDON_LIB_DIR}/"
install -m 644 "${BUILD}/src/sinsebeolsik-p2.conf" "${ADDON_CONF_DIR}/"
install -m 644 "${ROOT}/keymaps/sinsebeolsik_p2.toml" "${KEYMAP_DIR}/"
echo "  ${ADDON_LIB_DIR}/libsinsebeolsik-p2.so"
echo "  ${ADDON_CONF_DIR}/sinsebeolsik-p2.conf"
echo "  ${KEYMAP_DIR}/sinsebeolsik_p2.toml"

# ─── 5. fcitx5 재시작 ────────────────────────────────────────────────────────
if [[ "${NO_RESTART}" -eq 1 ]]; then
    echo
    echo "재시작 생략. 직접 실행:"
    echo "  fcitx5 -r          # 이미 실행 중이면"
    echo "  fcitx5 -d &        # 새로 시작"
    exit 0
fi

step "Restarting fcitx5 with FCITX_ADDON_DIRS"
# fcitx5는 기본적으로 ~/.local/lib/fcitx5/ 를 검색하지 않음.
# 시스템 경로와 함께 우리 경로를 명시적으로 지정해야 .so 발견함.
SYS_ADDON_DIR="/usr/lib/$(uname -m)-linux-gnu/fcitx5"
[[ -d "${SYS_ADDON_DIR}" ]] || SYS_ADDON_DIR="/usr/lib/fcitx5"
export FCITX_ADDON_DIRS="${ADDON_LIB_DIR}:${SYS_ADDON_DIR}"
echo "  FCITX_ADDON_DIRS=${FCITX_ADDON_DIRS}"

# fcitx5-remote -r 은 config만 reload하므로 addon 경로 변경 반영 안 됨.
# kill + 재시작이 필요.
if pgrep -x fcitx5 >/dev/null 2>&1; then
    killall fcitx5 || true
    # 완전히 종료될 때까지 대기 (최대 3초)
    for _ in 1 2 3 4 5 6; do
        pgrep -x fcitx5 >/dev/null 2>&1 || break
        sleep 0.5
    done
fi
(setsid env FCITX_ADDON_DIRS="${FCITX_ADDON_DIRS}" fcitx5 -d </dev/null >/dev/null 2>&1 &)
sleep 0.5

echo
echo "✓ 완료."
echo
echo "주의: 매번 'fcitx5 -d'를 직접 실행하시려면 아래를 ~/.profile 또는 ~/.bashrc 에 추가:"
echo "  export FCITX_ADDON_DIRS=\"${ADDON_LIB_DIR}:${SYS_ADDON_DIR}\""
echo
echo "fcitx5-configtool 에서 Sinsebeolsik P2 추가 후 IM 토글 (보통 Ctrl+Space 또는 Hangul)."
echo
echo "Hangul 키를 트리거로 처음 등록하려면 한 번만:"
echo "  ${ROOT}/scripts/setup-fcitx5-trigger.sh"
