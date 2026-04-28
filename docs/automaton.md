# Hangul Automaton Design

This document specifies the Hangul composition state machine for the Sinsebeolsik P2 fcitx5 addon. The automaton is implemented as a pure C++ module under `src/automaton.{h,cpp}` with no dependency on fcitx5, so it is unit-testable in isolation.

The reference implementations used to derive this design are:
- 박경남(pat.im) **`신세벌식 P2.ist.xml`** — 날개셋 input scheme file (2016.8.6 layout, 2018.4.10 symbol revision)
- 박경남 **ohi.pat.im** / **3beol.gitlab.io** — JavaScript implementation, master branch at `gitlab.com/3beol/3beol.gitlab.io`

The goal is bit-for-bit behavioral equivalence with the ohi.pat.im implementation for any input sequence consisting of the keys defined in `keymaps/sinsebeolsik_p2.toml`.

---

## 1. Scope and non-goals

The automaton handles **only Hangul composition**. Specifically:
- Maps fcitx5 `KeySym` events (limited to keys claimed by the keymap) to Hangul jamo and assembles syllables.
- Emits `(commit, preedit)` pairs that the fcitx5 wiring layer sends to the input context.
- Implements 갈마들이 (alternation), 도깨비불, 합용 자모 composition, and per-jamo backspace.

It does **not**:
- Touch X11/Wayland event loops, fcitx5 lifecycle, or IO. Pure functions only.
- Implement Hangul/English toggle. The engine layer wires the toggle key.
- Implement Hanja conversion or candidate selection.
- Implement 모아치기 (chord input). Sequential only — pat.im's P2 is sequential by design.
- Steal any key not in the TOML keymap. Unmapped keys are returned as "not consumed" (passthrough invariant).

---

## 2. Data model

### 2.1 Jamo unit codes (Unicode Conjoining Jamo block)

The automaton internally uses the **Unicode conjoining jamo** ranges directly, matching the ohi.pat.im internal representation:

| Range | Slot | Meaning | Notes |
|---|---|---|---|
| `0x1100`–`0x1112` | 초성 | 19 modern initials (ㄱ ㄲ ㄴ … ㅎ) | |
| `0x1161`–`0x1175` | 중성 | 21 modern medials (ㅏ … ㅣ) | |
| `0x119E` | 중성 | ㆍ (아래아) | Pre-modern, used by P2 z key |
| `0x11A1`–`0x11A2` | 중성 | ㆎ, ᆢ | Compounds of ㆍ |
| `0x11A8`–`0x11C2` | 종성 | 27 modern finals (ㄱ ㄲ ㄳ … ㅎ) | |

Slot classification function:
```
classify(code):
  if 0x1100 <= code <= 0x1112: return CHO
  if 0x1161 <= code <= 0x1175: return JUNG
  if code == 0x119E: return JUNG
  if 0x11A1 <= code <= 0x11A2: return JUNG
  if 0x11A8 <= code <= 0x11C2: return JONG
  else: return NOT_JAMO
```

### 2.2 Syllable buffer

The composing syllable is held in a 6-slot buffer mirroring ohi.pat.im's `ohiQ`:

```cpp
struct Syllable {
    char32_t cho       = 0;   // [0] current 초성
    char32_t cho_prev  = 0;   // [1] backup before 초성 compound
    char32_t jung      = 0;   // [2] current 중성
    char32_t jung_prev = 0;   // [3] backup before 중성 compound
    char32_t jong      = 0;   // [4] current 종성
    char32_t jong_prev = 0;   // [5] backup before 종성 compound
    bool empty() const { return !cho && !jung && !jong; }
};
```

The backup slots (`*_prev`) hold the pre-compound form so backspace can decompose `ㅘ → ㅗ` instead of deleting the whole vowel. The `*_prev` slot is ONLY populated when a compound was just formed; for plain compositions it stays zero.

State (pure data, no I/O):
```cpp
struct State {
    Syllable cur;     // current composing syllable
};
```

The "automaton state" of the .ist (states 0/1/2/3) is **derived** from `cur`, not stored explicitly:
- 0 (empty): `cur.empty()`
- 1 (초성만): `cur.cho && !cur.jung`
- 2 (초중): `cur.cho && cur.jung && !cur.jong`
- 3 (초중종): `cur.cho && cur.jung && cur.jong`
- (transient) (중성만): `!cur.cho && cur.jung` — happens when a key with default jung is pressed in empty state without a preceding 초성. The reference impl tolerates this.

### 2.3 Static tables

Three tables are baked into the keymap TOML and loaded at engine start:

**`base_keymap`** — `array<char32_t, 94>` indexed by `ascii - 0x21`. Direct ASCII → Unicode mapping (jamo or punctuation passthrough). Source: `K3_3shin_p2[]` in `keyboard_table_hangeul.js`.

**`galmadeuli`** — bidirectional jamo↔jamo lookup, sorted for binary search. Each entry is `(from_code, to_code)`. Both directions are stored as separate entries (e.g., `ㅁ초성→ㅡ` AND `ㅡ→ㅁ초성`). Source: `galmadeuli_3shin_p2[]` in `keyboard_table_galmadeuli.js`.

For P2 the table has 34 entries (4 cho↔jung pairs, 15 jung↔jong pairs, with bidirectional duplicates).

**`combination`** — compound jamo lookup, sorted for binary search. Each entry is `((a, b), result)` packing a 64-bit key. Covers:
- Choseong doubles: ㄲ ㄸ ㅃ ㅆ ㅉ
- Jungseong compounds: ㅘ ㅙ ㅚ ㅝ ㅞ ㅟ ㅢ ㆎ ᆢ
- Jongseong compounds: ㄲ ㄳ ㄵ ㄶ ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ ㅄ

Source: `hangeul_combination_table_default[]` in `keyboard_table_combination.js`. 28 entries total.

---

## 3. Algorithm

### 3.1 Entry point signature

```cpp
struct StepResult {
    State next_state;        // updated state
    std::u32string commit;   // text to commit (zero or more codepoints)
    std::u32string preedit;  // current syllable rendered as Unicode
    bool consumed;           // true = IME claimed this key; false = passthrough
};

StepResult step(const State& s, KeyInput k);
```

`KeyInput` carries a reduced view of the fcitx5 KeyEvent: `keysym`, `is_shift`, `is_ctrl`, `is_alt`. Modifier-bearing keys (Ctrl+anything, Alt+anything) immediately return `consumed=false` without touching state. The TOML keymap defines which (keysym, shift_state) pairs are claimed; everything else is passthrough.

### 3.2 Two-layer composition (mirrors ohi.pat.im)

```
KeyInput
   │
   ▼
[Layer 1: Reclassification]
   ├── base_keymap[keysym] → charCode
   ├── classify(charCode) → cat
   ├── galmadeuli[charCode] → alt_charCode (if exists)
   │
   ├── decide(cat, state) → use base or use alt
   │
   ▼ (effective_code, effective_cat)
[Layer 2: Slot composition]
   ├── if cat==CHO:  apply choseong rules (compound or commit-and-restart)
   ├── if cat==JUNG: apply jungseong rules (compound or commit-and-restart)
   ├── if cat==JONG: apply jongseong rules (compound or commit-and-restart)
   └── else: passthrough or commit-and-output (punctuation while composing)
```

### 3.3 Layer 1 — galmadeuli reclassification

The galmadeuli layer rewrites the input charCode based on the current state. Following the ohi.pat.im algorithm in `ohi_Hangeul_3` (lines 1161–1259):

```
Given (charCode, state):
  cat = classify(charCode)
  alt = galmadeuli_lookup(charCode)        // 0 if not present
  alt_cat = classify(alt) if alt else NONE

  if cat == JUNG:
    if state has cur.jung filled (state >= 2) AND no jong yet:
      attempt compound: combo = combination[(cur.jung, charCode)]
      if combo: emit JUNG combo (handled in layer 2)
      else if alt is JONG: rewrite to (alt, JONG)   // 갈마들이 jung→jong
    else:
      keep as (charCode, JUNG)

  if cat == JONG:
    if state has cur.jong filled (state == 3) AND no jong_prev (no compound yet):
      attempt compound: combo = combination[(cur.jong, charCode)]
      if combo: emit JONG combo (handled in layer 2)
      else: pass through as (charCode, JONG)
    elif !cur.cho || !cur.jung:
      // can't be a 종성, the state lacks a syllable to attach to
      if alt is JUNG: rewrite to (alt, JUNG)        // 갈마들이 jong→jung
      elif alt is CHO: rewrite to (alt, CHO)        // 갈마들이 jong→cho (rare)
      else: keep as (charCode, JONG)                // promotes to new syllable
    else:
      keep as (charCode, JONG)

  if cat == CHO:
    if state has cur.cho filled (state >= 1) AND no cur.jung:
      attempt compound: combo = combination[(cur.cho, charCode)]
      if combo: emit CHO combo
      else if alt is JUNG: rewrite to (alt, JUNG)   // 갈마들이 cho→jung (i,o,/,p)
    else:
      keep as (charCode, CHO)
```

The "attempt compound" branch and the "rewrite via galmadeuli" branch are mutually exclusive — combination is preferred when both apply (ohi.pat.im's `get_combination_value` is checked first for vowels and finals).

### 3.4 Layer 2 — slot composition

After reclassification we have `(code, cat)`. Apply standard 3-set rules:

**`cat == CHO`**:
- If `cur.jung || cur.jong`: commit current `cur`, start new with `cho = code`. (도깨비불 happens here when previous syllable had a 종성 — see §3.5.)
- Else if `cur.cho` non-zero:
  - Try `combo = combination[(cur.cho, code)]`. If found: `cur.cho_prev = cur.cho; cur.cho = combo`.
  - Else: commit `cur`, start new with `cho = code`.
- Else: `cur.cho = code`.

**`cat == JUNG`**:
- If `cur.jong`: invalid state for jung — commit `cur`, start new with `jung = code`. (Hangul Syllables Block requires cho+jung; an isolated jung commits as a Hangul filler `ㅇ` + jung, or stays as bare jung — see §3.6.)
- Else if `cur.jung`:
  - Try `combo = combination[(cur.jung, code)]`. If found: `cur.jung_prev = cur.jung; cur.jung = combo`.
  - Else: commit `cur`, start new with `jung = code`.
- Else: `cur.jung = code`.

**`cat == JONG`**:
- If `!cur.cho || !cur.jung`: invalid (no syllable to attach to). Per ohi.pat.im (line 1316), commit `cur`, start new with `jong = code`. The new syllable is "jong-only" — visually rendered with a filler 초성 (see §3.6) but lives in the buffer for the next event to potentially fix.
- Else if `cur.jong` non-zero AND `!cur.jong_prev`:
  - Try `combo = combination[(cur.jong, code)]`. If found: `cur.jong_prev = cur.jong; cur.jong = combo`.
  - Else: commit `cur`, start new (도깨비불 — see §3.5).
- Else: `cur.jong = code`.

### 3.5 도깨비불 (jongseong → next-syllable choseong)

When a 종성 is followed by a 중성 (vowel), the 종성 detaches and becomes the 초성 of the next syllable. This emerges naturally from the slot rules above:

```
state: { cho=ㄱ, jung=ㅏ, jong=ㅂ }   (= "갑")
input: ㅏ (JUNG)
  Layer 1: cur.jung filled, no jong combo possible → keep as JUNG
  Wait — but jong is filled too. Let me re-check.
```

Actually, the precise handling: when `cat==JUNG` and `cur.jong` is non-zero:

- Pop `cur.jong` (and `cur.jong_prev` if present).
- Commit the rest of `cur` (`cho + jung + 0`, e.g., "가").
- Start new `cur` with `cho = popped jong, jung = code`. (e.g., "바")
- If the popped `jong` was a compound (`jong_prev` non-zero): new `cur.cho = jong_prev`, the higher half of the compound stays on the previous syllable. Example: `갉 + ㅏ → 갈 + 가`, NOT `갉 + ㅏ → 가 + ㄱ가`.

This is the key edge case. Spec from libhangul:
> A compound 종성 splits: the **first jamo** stays as 종성 of the closing syllable, the **second jamo** becomes 초성 of the new syllable.

In our buffer:
- `cur.jong = ㄺ (compound)`, `cur.jong_prev = ㄹ`
- 도깨비불 commits `cho + jung + jong_prev` (= "갈"), new `cur.cho = ㄱ` (the "second jamo" of ㄺ).

The decomposition table is the inverse of `combination` for jong→(jong_keep, jong_promote_to_cho):

| Compound 종성 | Stays as jong | Promotes to cho |
|---|---|---|
| ㄳ | ㄱ | ㅅ |
| ㄵ | ㄴ | ㅈ |
| ㄶ | ㄴ | ㅎ |
| ㄺ | ㄹ | ㄱ |
| ㄻ | ㄹ | ㅁ |
| ㄼ | ㄹ | ㅂ |
| ㄽ | ㄹ | ㅅ |
| ㄾ | ㄹ | ㅌ |
| ㄿ | ㄹ | ㅍ |
| ㅀ | ㄹ | ㅎ |
| ㅄ | ㅂ | ㅅ |

Single 종성 도깨비불 is trivial: jong moves wholesale to new cho.

### 3.6 Edge cases

**Jung-only or jong-only syllable**: Hangul Syllables Block (가–힣) requires cho + jung. When a syllable lacks 초성, ohi.pat.im displays the bare jamo via the conjoining jamo block. We follow the same convention: emit raw conjoining jamo (`U+1161` etc.) in the preedit, NOT a precomposed Hangul Syllable. The text frontend renders these correctly on Wayland and most modern X clients.

When the syllable lacks 종성, normal precomposed Syllable Block is used (e.g., "가" = U+AC00).

**Unmapped key while composing**: any keysym not in the TOML keymap commits the current syllable, then returns `consumed=false` so the unmapped key passes through to the application. Examples: arrow keys, function keys, ASCII punctuation that the keymap doesn't claim.

**Modifier combos**: `Ctrl+*` and `Alt+*` always commit current syllable AND return `consumed=false`. The user expects `Ctrl+C` to copy regardless of IME state.

**Focus loss / IME deactivate**: engine layer calls `commit_and_reset(state)` which emits `cur` rendered as Unicode and zeroes the state. Implemented separately from `step()`.

**Backspace**: separate entry point `backspace(state) → StepResult`. Algorithm (mirrors ohi.pat.im `ohi_Backspace`, line 343):
1. Find highest non-zero slot in buffer (scan ohiQ[5..0]).
2. If that index is **odd** (1, 3, 5) and the value is a valid jamo: this means the corresponding even-slot (0, 2, 4) holds a compound and the odd slot is the pre-compound form. Restore: `even_slot = odd_slot; odd_slot = 0`. (Effectively undoes the last compound formation.)
3. Else: zero the highest slot.
4. If buffer is now empty AND there is no syllable to backspace: return `consumed=false` to delete a previously-committed character.

**Punctuation while composing**: characters like `,` `.` `?` are NOT claimed by the keymap (they pass through unchanged). When typed mid-composition, they trigger commit-then-passthrough at the engine layer.

**`shift+L` = `·` (가운뎃점) etc.**: these ARE claimed by the keymap (they're in the .ist as Korean typographic punctuation). Behavior: commit current syllable, then directly emit the punctuation char as a separate commit. Not assembled into the syllable.

---

## 4. Backspace test corpus (decomposition expectations)

| Input sequence | After backspace |
|---|---|
| `k` (ㄱ초성) | empty |
| `k f` (가) | `ㄱ` (cho only, U+1100) |
| `k f c` (각) | 가 |
| `k k` (ㄲ via combo) | ㄱ |
| `k f e e` (compounds via galmadeuli)... | (varies) |
| `k f w` (ㄱ + ㅏ → ㄹ받침 via galmadeuli, = 갈) | 가 |
| `k f w k` (갈 + ㄱ → ㄺ via combo, = 갉) | 갈 |
| `k f w k`, backspace×1 | 갈 |
| `k f w k`, backspace×2 | 가 |
| `j f` (ㅇ + ㅏ = 아) | ㅇ초성 |

### Doppelgänger compound vowels via virtual unit

| Input | Expected |
|---|---|
| `k i` (ㄱ + i) | 그 (ㄱ + virtual ㅡ via cho→jung galmadeuli) |
| `k o` (ㄱ + o) | 구 (ㄱ + virtual ㅜ) |
| `k /` (ㄱ + /) | 고 (ㄱ + virtual ㅗ) |
| `k p` (ㄱ + p) | 가-with-ㆍ (ㄱ + virtual ㆍ) |
| `k / f` (ㄱ + ㅗ + ㅏ via combo) | 과 |

---

## 5. Keymap TOML format (specified here, implemented in M3)

The keymap file `keymaps/sinsebeolsik_p2.toml` is the source of truth for which keys the engine claims. The automaton consumes a parsed in-memory representation of this file.

```toml
[meta]
name = "Sinsebeolsik P2"
spec_version = "2018-04-10"
spec_url = "http://pat.im/1136"
base_layout = "qwerty"

[base_keymap]
# Each entry: keysym (X11) → unshifted output, shifted output
# 0 means "not claimed" (passthrough)
"a" = [0x11BC, 0x1172]   # ㅇ받침, ㅠ
"b" = [0x11BE, 0x116E]   # ㅊ받침, ㅜ
# ... (94 entries)

[galmadeuli]
# Bidirectional pairs. Engine builds inverse automatically.
pairs = [
    [0x1106, 0x1173],  # ㅁ초성 ↔ ㅡ
    [0x110E, 0x116E],  # ㅊ초성 ↔ ㅜ
    [0x110F, 0x1169],  # ㅋ초성 ↔ ㅗ
    [0x1111, 0x119E],  # ㅍ초성 ↔ ㆍ
    [0x1161, 0x11C1],  # ㅏ ↔ ㅍ받침
    # ... (all 19 pairs)
]

[combination]
# (a, b) → c
rules = [
    [0x1100, 0x1100, 0x1101],  # ㄱ + ㄱ = ㄲ (cho)
    [0x1169, 0x1161, 0x116A],  # ㅗ + ㅏ = ㅘ
    # ... (all 28 rules)
]

[options]
backspace_mode = "by_jamo"      # only valid value for v0.1
ancient_hangul = false           # toggle reserved; flips 2 keys (U, Y) — v0.2
```

The TOML is loaded at engine startup and never mutated at runtime.

---

## 6. Acceptance criteria for M2

The pure automaton module is "done" when:

1. `src/automaton.{h,cpp}` and `src/jamo.{h,cpp}` compile against C++20 with no fcitx5 includes.
2. `tests/automaton_test.cpp` runs ≥100 cases covering:
   - All 94 base keymap entries (including punctuation passthrough).
   - All 19 galmadeuli pairs (both directions).
   - All 28 combination rules (forward).
   - Backspace decomposition of every compound (12 jong, 9 jung, 5 cho).
   - 도깨비불 with single and compound jong (all 11 compound 종성).
   - Mixed sequences (the corpus in §4, expanded to ≥30 entries).
   - Modifier-key passthrough (Ctrl+C, Alt+Tab) returns `consumed=false`.
3. CI green on the feature branch.
4. No fcitx5 wiring yet — that is M3.

---

## 7. Open implementation questions (for self, not user)

1. **Test framework**: Catch2 (header-only, easy CMake integration) vs GoogleTest. Lean Catch2 for v0.1 since fcitx5 itself uses Catch2 in some addons.
2. **TOML parser**: `toml++` (single header, C++17, MIT) is the standard pick. Add as `FetchContent` or system package on Ubuntu (`libtomlplusplus-dev`).
3. **char32_t vs uint32_t**: use `char32_t` for jamo codes; it documents intent and the type is exactly `uint_least32_t`.
4. **`std::expected`**: Ubuntu 24.04 ships GCC 13 which has partial `<expected>` support. Use `tl::expected` polyfill (single header) to avoid version friction. Confirmed compatible with C++20 baseline.
5. **Whether to bake the keymap into the binary or load TOML at runtime**: loading TOML is the design intent (per CLAUDE.md "reloadable via fcitx5 config UI"). Cost: parse-time on engine activate. Acceptable.
