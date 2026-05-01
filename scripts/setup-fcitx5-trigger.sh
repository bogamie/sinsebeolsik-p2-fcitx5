#!/usr/bin/env bash
# fcitx5 트리거 키에 Hangul (KeySym 0xff31, KC_LANG1)을 추가한다.
#
# fcitx5의 트리거 키는 글로벌 설정이라 엔진이 자체적으로 박을 수 없고,
# 사용자의 ~/.config/fcitx5/config 의 [Hotkey/TriggerKeys] 섹션을 통해서 정해진다.
# 이 스크립트는 그 항목을 멱등적으로 끼워넣는다.
#
# fcitx5의 키 목록 포맷은 다음과 같다:
#   [Hotkey/TriggerKeys]
#   0=Ctrl+space
#   1=Zenkaku_Hankaku
#   2=Hangul
#
# 사용:
#   ./scripts/setup-fcitx5-trigger.sh         # 적용
#   ./scripts/setup-fcitx5-trigger.sh --dry   # 결과만 출력
#
# 안전성:
#   - 이미 Hangul이 등록돼 있으면 변경 없음.
#   - 다른 키들은 보존되고 Hangul이 다음 빈 인덱스에 append된다.
#   - 섹션이 없으면 새로 만든다.

set -euo pipefail

DRY=0
case "${1:-}" in
    --dry|--dry-run) DRY=1 ;;
    -h|--help)
        sed -n '2,22p' "$0"; exit 0 ;;
    "") ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
esac

CONFIG="${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/config"
mkdir -p "$(dirname "$CONFIG")"
[[ -f "$CONFIG" ]] || : > "$CONFIG"

# 이미 Hangul 등록되어 있는지: [Hotkey/TriggerKeys] 블록 안에 `=Hangul`이 있나.
already_registered() {
    awk '
        /^\[Hotkey\/TriggerKeys\]/ { in_block=1; next }
        /^\[/ { in_block=0 }
        in_block && /^[0-9]+=Hangul[[:space:]]*$/ { found=1 }
        END { exit found ? 0 : 1 }
    ' "$CONFIG"
}

if already_registered; then
    echo "이미 Hangul이 [Hotkey/TriggerKeys]에 등록되어 있습니다 — 변경 없음."
    exit 0
fi

apply() {
    if [[ "$DRY" -eq 1 ]]; then
        echo "--- proposed $CONFIG ---"
        cat
        echo "--- end ---"
    else
        # 임시 파일에 쓴 뒤 mv (writer 도중에 fcitx5가 read하면 안 되니까).
        TMP="$(mktemp "${CONFIG}.XXXXXX")"
        cat > "$TMP"
        mv "$TMP" "$CONFIG"
        echo "✓ Hangul을 [Hotkey/TriggerKeys]에 추가했습니다 — $CONFIG"
    fi
}

if grep -q '^\[Hotkey/TriggerKeys\]' "$CONFIG"; then
    # 기존 인덱스 다음 번호로 추가.
    NEXT_IDX=$(awk '
        /^\[Hotkey\/TriggerKeys\]/ { in_block=1; next }
        /^\[/ { in_block=0 }
        in_block && /^[0-9]+=/ {
            n = $0
            sub(/=.*/, "", n)
            if (n+0 > max) max = n+0
        }
        END { print max+1 }
    ' "$CONFIG")
    # 섹션 안의 줄들을 버퍼링해서 마지막 `N=Value` 직후에 Hangul을 삽입한다.
    # (블록 끝에 빈 줄이 있어도 Hangul이 엔트리 사이에 깔끔하게 들어가도록)
    awk -v idx="$NEXT_IDX" '
        function flush_block(   i, last_kv) {
            last_kv = 0
            for (i=1; i<=n; i++) if (buf[i] ~ /^[0-9]+=/) last_kv = i
            for (i=1; i<=n; i++) {
                print buf[i]
                if (i == last_kv) print idx "=Hangul"
            }
            if (last_kv == 0) print idx "=Hangul"
            n = 0
        }
        /^\[Hotkey\/TriggerKeys\]/ { in_block=1; print; next }
        /^\[/ {
            if (in_block) { flush_block(); in_block=0 }
            print; next
        }
        in_block { buf[++n] = $0; next }
        { print }
        END { if (in_block) flush_block() }
    ' "$CONFIG" | apply
else
    # 섹션이 없음 → 끝에 새 섹션 append.
    {
        cat "$CONFIG"
        # 끝에 빈 줄 보장
        if [[ -s "$CONFIG" ]] && [[ "$(tail -c1 "$CONFIG" | xxd -p)" != "0a" ]]; then
            echo
        fi
        [[ -s "$CONFIG" ]] && echo
        echo "[Hotkey/TriggerKeys]"
        echo "0=Hangul"
    } | apply
fi

if [[ "$DRY" -eq 0 ]]; then
    echo
    echo "변경 적용 — fcitx5 재시작:"
    echo "  fcitx5-remote -r           # 이미 실행 중일 때"
    echo "  killall fcitx5; fcitx5 -d  # 또는 완전 재시작"
fi
