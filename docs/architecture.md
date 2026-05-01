# Architecture

신세벌식 P2 fcitx5 엔진의 내부 구조.

## 큰 그림

```
                  ┌──────────────────────────────────────┐
                  │  fcitx5 instance (host process)      │
                  │  ─ KeyEvent dispatch                 │
                  │  ─ TriggerKeys (Hangul, …)           │
                  └────────────┬─────────────────────────┘
                               │ KeyEvent (KeySym + state)
                               ▼
   ┌─────────────────────────────────────────────────────┐
   │  sinsebeolsik-p2 addon  (libsinsebeolsik-p2.so)     │
   │                                                     │
   │  engine.cpp ─── InputMethodEngineV2 subclass        │
   │      activate / deactivate / keyEvent / reset       │
   │      ↓                                              │
   │      State 저장(InputContext property)              │
   │      ↓                                              │
   │  keymap.cpp ─── QWERTY → Input 변환                 │
   │      Keymap (TOML rules) + Predicate AST            │
   │      ↓                                              │
   │  automaton.cpp ─ State machine                      │
   │      step / backspace / flush                       │
   │      ↓                                              │
   │  jamo.cpp ── 자모 합성/분해 표 + Unicode 변환       │
   └─────────────────────────────────────────────────────┘
```

## 모듈

### `jamo` — 자모 데이터 + 합성·분해 (의존성 0)

- `enum class Cho/Jung/VJung/Jong` — Unicode 한글 자모 인덱스(U+1100 / U+1161 / U+11A7) 그대로
- `combine_jung(a, b)` / `combine_virtual_jung(va, b)` / `combine_jong(a, b)` — `.ist` UnitMixTable 그대로
- `split_jung(c)` / `split_jong(c)` — 백스페이스 분해
- `compose_syllable(c, j, jo)` — 완성형 음절 (U+AC00..U+D7A3)
- `cho_to_compat / jung_to_compat / jong_to_compat` — 호환 자모 (U+3131..U+318F)
- 순수 함수 only. 테스트는 `tests/jamo_test.cpp`.

### `automaton` — 한글 합성 상태 기계 (jamo만 의존)

- `struct State { cho, jung, jong }`
  - `jung`은 `variant<monostate, Jung, VJung>` — 가상 중성과 실제 중성을 별도 슬롯으로 구분 (P2 핵심)
- `struct Input` = `variant<InputCho, InputJung, InputVJung, InputJong>` — keymap이 분류해 보내는 입력 단위
- `step(state, input) -> { state, commit, preedit }` — 순수 함수
- `backspace(state)` — 자모 1단계 분해 (음절 단위 아님)
- `flush(state)` — 진행 중 음절 강제 commit (포커스 이탈, 비-한글 키 등)
- fcitx5 의존성 없음 → 테스트로 그대로 검증 가능 (`tests/automaton_test.cpp`)

### `keymap` — TOML 키맵 + 갈마들이 평가 (automaton에 의존)

- TOML 정의: `keymaps/sinsebeolsik_p2.toml`
  - `[[key]]` 항목마다 `rules`(우선순위 배열) — 첫 매치 적용
  - 각 rule: `when`(predicate string) + `output`(cho/jung/vjung/jong/passthrough)
- `Keymap::translate(qwerty_key, state) -> optional<Input>` — `nullopt`은 passthrough
- predicate AST: 8개 atom (`D`, `E`, `F`, `E_v_O/U/EU/F`, `E_any_v`) + `! && ||` + 괄호. 작은 RD parser.
- 빌드 시 TOML이 raw string literal로 임베드됨(`embedded_p2_keymap.cpp.in`). 런타임에 사용자 override 발견 시 교체.

### `engine` — fcitx5 어댑터 (keymap + automaton에 의존)

- `Engine : InputMethodEngineV2` — fcitx5 ABI 그대로 따름
- `keyEvent`:
  1. modifier 단독 → 무시
  2. Ctrl/Alt/Super 조합 → flush + passthrough (단축키 충돌 방지)
  3. Backspace → 자동기 BS, 진행 중이면 consume
  4. printable + 자모 매핑 있음 → step + consume
  5. 그 외 (방향키, 비매핑 printable 등) → flush + passthrough
- `P2InputState`(IC property) — input context마다 독립 State
- 시작 시 `try_load_user_keymap()`이 검색 경로(`$SIN3P2_KEYMAP`, XDG, `~/.local/share`, `/usr/share`)를 훑어 override 로드. 실패 시 임베드 사용.

## 데이터 흐름 — 키 한 번

```
host KeyEvent → engine::keyEvent
              → fcitx::Key.keySymToUnicode() = char32_t k
              → keymap.translate(k, state) ───── nullopt → flush, passthrough
                                            └── Input
              → automaton.step(state, input)
                  → { new_state, commit_text, preedit_text }
              → host commit & preedit update
```

## 가상 중성 (VJung) — P2의 핵심 트릭

신세벌식 P2는 한 키로 두 모음을 박는 단축 입력을 위해 **가상 중성** 개념을 쓴다.

- `/`, `i`, `o`, `p` 키가 cho-only 상태(D&&!E&&!F)에서 박히면 가상 중성을 임시로 jung 슬롯에 넣는다 (501..504).
- 다음 키가 합성 가능한 모음이면 `combine_virtual_jung`으로 실제 중성에 합성. 안 되면 가상 중성을 실제로 캐스트하고 그 키는 자기 갈마 조건으로 jong이 된다.
- 여러 갈마 조건(`E_v_O`, `E_v_U`, `E_v_F`)은 *가상* 중성 전용. 실제 ㅗ/ㅜ가 박혀 있을 때는 트리거되지 않는다 — 이게 `jbd` → 웋, `jBd` → 웋 (lowercase d → ㅎ jong)이고 `jod` → 위, `j/d` → 외 (가상 위라 d가 ㅣ jung 갈마)인 차이를 만든다.

자세한 시뮬레이터-검증 동작은 `keymaps/sinsebeolsik_p2.toml` 헤더 주석과 `tests/automaton_test.cpp` 케이스 참고.

## 단위 테스트

- `automaton_test`, `keymap_test`, `jamo_test` — Catch2.
- 모든 .ist 사양 동작은 [pat.im 시뮬레이터](https://ohi.pat.im/?ko=sin3-p2)에서 검증한 시퀀스를 그대로 테스트로 박았다.
- 새 동작 추가 시 시뮬레이터 결과를 oracle로 사용 — 추측으로 코드 작성 금지.
