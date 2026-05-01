# Adding a New Keymap

본 엔진은 P2 외 다른 한글 배열을 추가할 수 있도록 keymap 파일과 코드를 분리해 놓았다.

## TL;DR

1. `keymaps/<your_layout>.toml` 작성
2. 시뮬레이터/스펙 oracle로 동작 검증한 시퀀스를 `tests/keymap_test.cpp`에 케이스로 박기
3. 엔진 시작 시 `~/.local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`(현 위치) 자리에 새 파일을 두면 자동 로드 — *현 v1은 layout 이름이 고정*. 다중 배열 지원은 후속 작업.

## TOML 스키마

`keymaps/sinsebeolsik_p2.toml` 헤더 주석이 1차 레퍼런스. 핵심 요약:

```toml
[meta]
name = "Layout Display Name"

[[key]]
key = "h"                                   # QWERTY 한 글자 (ASCII printable)
rules = [
    { output = { cho = "N" } }              # 무조건 매치 → ㄴ 초성
]

[[key]]
key = "/"
rules = [
    { when = "D && !E && !F",
      output = { vjung = "O" } },           # 조건부 — cho-only일 때만
    { output = { cho = "K" } }              # 폴스루 — 그 외엔 ㅋ 초성
]
```

### output 슬롯 (정확히 1개)

| 키 | 값 | 의미 |
|:---|:---|:---|
| `cho` | `Cho` enum 이름 (G GG N D DD R M B BB S SS O J JJ C K T P H) | 초성 입력 |
| `jung` | `Jung` enum 이름 (A AE YA YAE EO E YEO YE O WA WAE OI YO U UEO WE WI YU EU EUI I) | 중성 입력 |
| `vjung` | `VJung` enum (O U EU F) | 가상 중성 (P2 단축 입력용) |
| `jong` | `Jong` enum (G GG GS N NJ NH D R RG RM RB RS RT RP RH M B BS S SS O J C K T P H) | 종성 입력 |
| `passthrough` | `true` | 자모가 아님 (현 음절 flush 후 호스트로 통과) |

### `when` predicate 문법

```
atom :=  D | E | F | E_v_O | E_v_U | E_v_EU | E_v_F | E_any_v
unary := "!" unary | atom | "(" expr ")"
and  := unary ("&&" unary)*
or   := and   ("||" and)*
expr := or
```

| atom | 의미 |
|:---|:---|
| `D` | cho 슬롯 채워짐 |
| `E` | jung 슬롯 채워짐 (real 또는 virtual) |
| `F` | jong 슬롯 채워짐 |
| `E_v_O` | jung == VJung::O (가상 ㅗ) |
| `E_v_U` | jung == VJung::U (가상 ㅜ) |
| `E_v_EU` | jung == VJung::EU (가상 ㅡ) |
| `E_v_F` | jung == VJung::F (가상 ㆍ) |
| `E_any_v` | jung이 가상 중성 (어느 것이든) |

## 갈마들이가 없는 단순 배열

두벌식이나 세벌식 최종처럼 위치 의존이 거의 없는 배열은 모든 rule이 무조건 매치 형태. predicate AST를 안 써도 OK.

```toml
[[key]]
key = "k"
rules = [{ output = { cho = "G" } }]   # ㄱ — 항상 cho

[[key]]
key = "h"
rules = [{ output = { jung = "A" } }]  # ㅏ — 항상 jung (두벌식 식)
```

## 새 가상 중성을 도입하려면

자모 enum (`jamo.h`) 자체에 손을 대야 한다. 현 `VJung`은 P2의 4개 (O/U/EU/F)에 1:1 대응. 더 추가하려면:

1. `jamo.h`의 `enum class VJung`에 항목 추가
2. `jamo.cpp`의 `combine_virtual_jung` 분기 + `virtual_to_real` 분기 갱신
3. `keymap.cpp`의 `name_to_vjung()` 분기 추가
4. predicate atom (`E_v_*`)이 더 필요하면 `Predicate::AtomKind` 확장 + `predicates_of` + `name_to_atom` + 평가 분기

이건 본 엔진 v1 범위 밖 — 옛한글 입력기를 만든다거나 할 때 고려.

## 검증 절차 (필수)

1. 배열 spec / 시뮬레이터에서 핵심 시퀀스 5–10개를 골라 입력→출력 표를 만듦.
2. 그 표를 `tests/keymap_test.cpp`에 `TEST_CASE` 한 개씩 박음.
3. `./scripts/dev-reload.sh`로 빌드+테스트 통과 확인.
4. 사용자 dir에 새 keymap 파일 떨군 뒤 fcitx5 재시작, 실제 타이핑으로 spot-check.

`키맵 동작은 시뮬레이터/스펙이 oracle이지 추측이 oracle이 아니다.` 신세벌식 계열은 특히 갈마들이/가상 중성에서 직관과 어긋나는 동작이 많아서 매번 검증해야 한다.

## 후속 — 다중 배열 동시 지원

현 v1은 keymap 파일을 1개만 로드한다 (process-global default). 다중 배열을 같은 fcitx5 세션에서 토글하려면:

- engine.cpp에 layout switch 핸들러 추가
- 각 layout에 fcitx5 InputMethodEntry 별도 등록
- `Keymap` 인스턴스를 layout별로 보관

설계는 이미 `Keymap` 클래스가 분리돼 있어 어렵지 않다. 후속 마일스톤에서 다룰 예정.
