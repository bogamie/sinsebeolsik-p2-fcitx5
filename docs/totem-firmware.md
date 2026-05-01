# Totem 38-key Firmware Notes

토템(Totem) 38키 분할 키보드를 본 엔진과 함께 쓰기 위한 QMK 펌웨어 권장 설정.

## 전제

- 펌웨어: **QMK** (Vial 호환)
- 키 갯수: 38 (왼손 19 + 오른손 19, 일반적인 split corne류 cousins)
- 호스트 OS: Linux (X11 또는 Wayland), fcitx5 활성

## 핵심 원칙

엔진은 **물리 scancode가 아닌 KeySym**을 받는다. 즉 펌웨어가 일반 USB HID 키코드를 보내기만 하면 엔진이 알아본다. 토템 특유의 콤보/레이어/홈로우 mod는 **펌웨어 측에서 해결**되어 호스트에 닿을 즈음엔 표준 키 시퀀스로 정규화돼 있어야 한다.

## 권장 base 레이어

QWERTY를 깔되, 신세벌식 P2가 사용하는 38개 위치에 모두 매핑이 있어야 한다. 38키 토템에서 정확히 38개라 짝이 맞는다.

```
왼손 base:                     오른손 base:
  Q W E R T                       Y U I O P
  A S D F G                       H J K L ;
  Z X C V B                       N M , . /
       LCMD LSFT SPC          ENT BSP RSFT
```

본 엔진 키맵은 `'`(apostrophe)도 사용하므로 (ㅌ cho), `;` 옆 위치에 `'` 키가 보장돼야 한다. 38키 base에서 `'`를 어디 둘지 펌웨어에서 결정.

## 필수 매핑

| QWERTY 키 | P2 역할 | 토템 위치 권장 |
|:---:|:---:|:---|
| `;` | ㅂ cho | 오른손 가운데줄 끝 |
| `'` | ㅌ cho | `;`와 같은 줄 옆 (원래 자리) |
| `/` | ㅋ cho / 가상 ㅗ | 오른손 아래줄 끝 |
| `,` `.` | passthrough (구두점) | 그대로 둠 |

## QMK 콤보 / 레이어 — 비추 케이스

다음은 본 엔진과 충돌할 수 있어 **콤보로 만들지 말 것**:

- 두 글쇠 동시 → 다른 한 글쇠 보내는 콤보
  - 신세벌식 P2는 *순차* 입력만 지원. 동시 입력은 꺠진다.
- 알파벳 한 키를 다른 알파벳으로 보내는 펌웨어 매크로 (`y` → `q` 같은 식)
  - 엔진이 keysym 그대로 신뢰하므로 매크로가 들어가면 갈마들이 평가가 어긋난다.

## 권장 케이스

- **홈로우 mod** (HRM): 표준 QMK `MT()` / `LT()` 사용. 호스트는 그냥 normal letter + modifier 조합으로 보임.
- **레이어**: 숫자/기호/펑션을 별도 레이어에 두는 건 OK. 한글 입력 중 숫자 키가 들어오면 엔진은 syllable flush 후 통과시킨다.
- **타이핑 잠김 검사**: HRM 사용 시 `TAPPING_TERM`을 너무 짧게 두면 lowercase 자모가 modifier로 잘못 해석돼서 키 전체가 누락된다. 권장: 200ms 이상.

## Hangul 키

토템에 Hangul(`KC_LANG1`) 키를 별도 두면 fcitx5의 트리거에 바로 등록 가능. 권장 위치:
- 엄지(thumb) cluster의 한 키
- 또는 base 레이어 어딘가에 KC_LANG1 매핑

QMK 키코드: `KC_LNG1` (또는 `KC_LANG1`). USB HID code 0x90.

```c
// keymap.c 예시 — 엄지 클러스터 좌측 끝에 LANG1 추가
[_BASE] = LAYOUT_split_3x5_3(
    ...,
    KC_LNG1, KC_LSFT, KC_SPC,  KC_ENT, KC_BSPC, ...
)
```

호스트 fcitx5에서는 `~/.config/fcitx5/config`의 `[Hotkey/TriggerKeys]`에 `Hangul`이 있으면 이 키 한 번에 IM이 토글된다 (본 프로젝트의 `scripts/setup-fcitx5-trigger.sh`가 자동 등록).

## 디버깅

호스트가 어떤 keysym을 받는지 확인:

```bash
xev   # X11 세션
wev   # wlroots / sway
```

토템에서 키 누른 뒤 `keysym 0xff31 (Hangul)`로 표시되면 OK.

## 참고

- QMK Sinsebeolsik 사용자 사례: 검색해보면 한국 사용자들이 비슷한 split keyboard 구성을 공유한 글이 있다.
- Vial은 Vial-QMK 펌웨어 + Vial GUI로 키맵을 런타임에 바꿀 수 있어 빠른 실험에 좋다. 본 엔진은 펌웨어 무관하게 keysym만 본다.
