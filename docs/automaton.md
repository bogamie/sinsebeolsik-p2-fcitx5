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

**`galmadeuli`** — directional jamo→jamo lookup, sorted for binary search. Each entry is `(from_code, to_code)`. Source: `galmadeuli_3shin_p2[]` in `keyboard_table_galmadeuli.js` (lines 390–433).

For P2 the table has **34 directed entries** organised in three sections:
- 4 cho→jung mappings, **one-way** (i, o, /, p keys' choseong → vowel for the post-cho compound vowel mechanism): ㅁ→ㅡ, ㅊ→ㅜ, ㅋ→ㅗ, ㅍ→ㆍ.
- 15 jung→jong mappings (each modern vowel's key → corresponding 종성 form).
- 15 jong→jung mappings (the inverse of the previous, completing **bidirectional** jung↔jong pairs).

Plus 2 옛한글-toggle-only entries (○→〮, ×→〯) handled separately via `__change_to_yet`. Layer 1 (§3.3) consults this table at most once per keystroke; the choice of branch (apply rewrite vs keep base) depends on input category and current state.

**`combination`** — compound jamo lookup, sorted for binary search. Each entry is `((a, b), result)` packing a 64-bit key. Source: `hangeul_combination_table_default[]` in `keyboard_table_combination.js` (lines 6–32). **26 entries**:
- Choseong doubles (5): ㄲ ㄸ ㅃ ㅆ ㅉ
- Jungseong compounds (9):
  1. ㅘ (ㅗ+ㅏ, U+116A)
  2. ㅙ (ㅗ+ㅐ, U+116B)
  3. ㅚ (ㅗ+ㅣ, U+116C)
  4. ㅝ (ㅜ+ㅓ, U+116F)
  5. ㅞ (ㅜ+ㅔ, U+1170)
  6. ㅟ (ㅜ+ㅣ, U+1171)
  7. ㅢ (ㅡ+ㅣ, U+1174)
  8. ㆎ (ㆍ+ㅣ, U+11A1)
  9. ᆢ (ㆍ+ㆍ, U+11A2 — 쌍아래아)
- Jongseong compounds (12): ㄲ ㄳ ㄵ ㄶ ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ ㅄ

**`jong_split`** — 12-entry decomposition table for compound 종성, used by 도깨비불 (§3.5). Maps a compound jong → `(keep, promote_cho)` where `keep` stays as the closing syllable's 종성 and `promote_cho` becomes the new syllable's 초성. Derived once at engine init from `combination` rules whose result is a JONG: for each `(jong_a, jong_b) → compound`, set `jong_split[compound] = (jong_a, jong_to_cho(jong_b))`. The `jong_to_cho` map is a fixed 14-entry consonant lookup (e.g., ㅅ받침 0x11BA → ㅅ초성 0x1109; ㄱ받침 0x11A8 → ㄱ초성 0x1100). This table is **separate** from `jong_prev` (the buffer slot used for backspace restoration) — the two roles must not be conflated.

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
- If `cur.jung || cur.jong`: commit current `cur`, start new with `cho = code`. (No 도깨비불 here — that fires only on JUNG input following a syllable with 종성; see §3.5.)
- Else if `cur.cho` non-zero:
  - Try `combo = combination[(cur.cho, code)]`. If found: `cur.cho_prev = cur.cho; cur.cho = combo`.
  - Else: commit `cur`, start new with `cho = code`.
- Else: `cur.cho = code`.

**`cat == JUNG`**:
- If `cur.jong` non-zero: **delegate to `apply_dokkaebibul(state, code)` (§3.5)**. The 종성 detaches from the closing syllable into the next syllable's 초성; the closing syllable commits.
- Else if `cur.jung` non-zero:
  - Try `combo = combination[(cur.jung, code)]`. If found: `cur.jung_prev = cur.jung; cur.jung = combo`.
  - Else: commit `cur`, start new with `jung = code` (bare-jung syllable; rendered via conjoining jamo, see §3.6).
- Else: `cur.jung = code`.

**`cat == JONG`** — explicit case discrimination, NO silent overwrite:
- Case A: `!cur.cho || !cur.jung` (no syllable to attach to). Commit any partial `cur`, start new with `jong = code` (jong-only state, see §3.6). For legitimate P2 sequences this rarely fires — Layer 1 (§3.3) typically rewrites such inputs to JUNG via galmadeuli before reaching here.
- Case B: `cur.jong == 0` (first 종성 of this syllable). Set `cur.jong = code`.
- Case C: `cur.jong != 0 AND cur.jong_prev == 0` (single jong, attempt compound).
  - Try `combo = combination[(cur.jong, code)]`. If found: `cur.jong_prev = cur.jong; cur.jong = combo`.
  - Else: no compound rule. Commit `cur`, start new with `jong = code` (jong-only state). Mirrors `ohi.js:1316–1317`.
- Case D: `cur.jong != 0 AND cur.jong_prev != 0` (compound jong already formed). Commit `cur`, start new with `jong = code` (jong-only state). **Existing compound is NOT overwritten.** Mirrors `ohi.js:1311–1317` where `ohiQ[5]` non-zero forces the commit-and-restart branch.

### 3.5 도깨비불 (`apply_dokkaebibul`)

When a vowel is typed and `cur.jong` is filled, the 종성 detaches from the closing syllable and becomes the 초성 of the next syllable. Spec (libhangul-aligned):

> A compound 종성 splits: the **first jamo** stays as 종성 of the closing syllable, the **second jamo** becomes 초성 of the new syllable. A simple (non-compound) 종성 moves wholesale to the new 초성.

Encapsulated as a **separate function** for testability and so the JUNG branch in §3.4 stays small:

```cpp
StepResult apply_dokkaebibul(const State& s, char32_t jung_code);
```

Algorithm (uses `jong_split` from §2.3, NOT `cur.jong_prev`):
```
let closing = copy of s.cur                  // closing syllable
let new_cho

if jong_split.contains(closing.jong):
    let split = jong_split[closing.jong]
    closing.jong = split.keep                // first jamo stays
    new_cho     = split.promote              // second jamo promotes (already in 초성 form)
else:
    new_cho     = jong_to_cho(closing.jong)  // wholesale move, convert 종성→초성 form
    closing.jong = 0

closing.jong_prev = 0                        // backup discarded — closing commits

emit commit_str = render(closing)
new_state.cur = { cho=new_cho, jung=jung_code, jong=0, all *_prev=0 }
return { next_state=new_state, commit=commit_str, preedit=render(new_state.cur), consumed=true }
```

Worked example — `갉 + ㅏ` (compound jong split):
- `cur = { cho=ㄱ, jung=ㅏ, jong=ㄺ, jong_prev=ㄹ (backspace backup) }`
- `jong_split[ㄺ] = { keep=ㄹ, promote=ㄱ초성 }`
- closing → `{ cho=ㄱ, jung=ㅏ, jong=ㄹ }` → "갈"
- new_state.cur → `{ cho=ㄱ초성, jung=ㅏ }` → "가"
- emitted: commit "갈", preedit "가"

Worked example — `갑 + ㅏ` (simple jong, wholesale):
- `cur = { cho=ㄱ, jung=ㅏ, jong=ㅂ받침 }`
- `jong_split` does not contain ㅂ받침 → wholesale
- closing → `{ cho=ㄱ, jung=ㅏ, jong=0 }` → "가"
- new_state.cur → `{ cho=ㅂ초성, jung=ㅏ }` → "바"
- emitted: commit "가", preedit "바"

`jong_split` covers all 12 compound 종성 (ㄲ ㄳ ㄵ ㄶ ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ ㅄ); every other 종성 takes the wholesale branch.

### 3.6 Edge cases and invariants

**Bare-jung syllable** (`{cho=0, jung=v, jong=0}`): legitimate state when a vowel is typed in initial state. Hangul Syllables Block (가–힣) cannot represent it; preedit and commit use the conjoining jamo block (U+1161 etc.). Modern Wayland clients render these correctly. ohi.pat.im behaves the same.

**Jong-only state** (`{cho=0, jung=0, jong≠0}`): a transient state that arises when a 종성-default key cannot attach to the current syllable — produced by §3.4 Cases A/C/D. Rendered via the conjoining jamo block (U+11A8 etc.) in the preedit. ohi.pat.im behaves identically (`ohi.js:1316–1320` produces this exact state). The next input typically resolves it within one keystroke: if a vowel follows, the jong-only commits as a standalone 종성 jamo and the new vowel starts a fresh bare-jung syllable; if another consonant follows, the jong-only commits and the new consonant starts a fresh syllable.

Earlier drafts of this document framed jong-only as an unreachable invariant. That was wrong: jong-only is reachable in legitimate input (e.g., `갈 + g` where ㄹ받침 cannot compound with ㄷ받침). The current design accepts it as a defensive transient state, matches ohi.pat.im, and pins the behavior with tests.

**Claimed vs passthrough — single rule**: a `(keysym, shift_state)` pair is **claimed** if and only if it has a non-zero entry in the TOML `base_keymap`. Uniformly:
- Jamo entries: enter the composition pipeline (Layers 1 and 2).
- Symbol/punctuation entries (e.g., `shift+L` = `·`, `shift+M` = `…`, plain `'` = `'`): commit the current syllable, then emit the symbol as an additional commit. The symbol does NOT enter the syllable buffer.
- Unmapped keys: commit the current syllable AND return `consumed=false` so the application receives the keystroke.

The keymap TOML is the single source of truth for which keys are claimed; no behavior depends on whether the entry "looks like jamo" or "looks like punctuation".

**Modifier combos**: `Ctrl+*` and `Alt+*` always commit the current syllable AND return `consumed=false`, regardless of whether the bare key is claimed. `Ctrl+C` must copy in any IME state.

**Focus loss / IME deactivate**: engine layer calls `commit_and_reset(state)` which emits `cur` rendered as Unicode and zeroes the state. Separate from `step()`.

**Backspace** (`backspace(state) → StepResult`, mirrors `ohi_Backspace` at `ohi.js:343`):
1. Scan slots `[5, 4, 3, 2, 1, 0]` for the highest non-zero index `i`.
2. If `i` is **odd** (1, 3, 5) and `cur[i]` is a valid jamo: slot `i-1` holds a compound and slot `i` is its pre-compound backup. Restore: `cur[i-1] = cur[i]; cur[i] = 0`. (Undoes the last compound formation, e.g., ㄺ → ㄹ.)
3. Else: zero the highest non-zero slot.
4. If the buffer was already empty when backspace was pressed: return `consumed=false` so the application deletes a previously-committed character.

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
# pat.im/1136 covers two revisions: the 2016 layout and the 2018 symbol patch.
# Split here so consumers can tell which axis a future bump touches.
layout_revision = "2016-08-06"
symbol_revision = "2018-04-10"
scope          = "basic-p2"     # base layout only; ancient/extended modes are toggles below
spec_url       = "http://pat.im/1136"
base_layout    = "qwerty"

[base_keymap]
# Each entry: keysym (X11) → unshifted output, shifted output
# 0 means "not claimed" (passthrough)
"a" = [0x11BC, 0x1172]   # ㅇ받침, ㅠ
"b" = [0x11BE, 0x116E]   # ㅊ받침, ㅜ
# ... (94 entries)

[galmadeuli]
# Two sections to match the directional structure of galmadeuli_3shin_p2[]
# (see §2.3). The engine generates inverse entries ONLY for `bidirectional`.

# 4 entries — one-way cho→jung. NO inverse generated (would conflict with
# bidirectional jung↔jong entries; e.g. inverse of ㅡ→ㅁ초성 would shadow
# the ㅡ↔ㄷ받침 pair).
one_way = [
    [0x1106, 0x1173],  # ㅁ초성 → ㅡ
    [0x110E, 0x116E],  # ㅊ초성 → ㅜ
    [0x110F, 0x1169],  # ㅋ초성 → ㅗ
    [0x1111, 0x119E],  # ㅍ초성 → ㆍ
]

# 15 entries — bidirectional jung↔jong. Engine inserts the inverse
# automatically, yielding 30 directed lookups.
bidirectional = [
    [0x1161, 0x11C1],  # ㅏ ↔ ㅍ받침
    [0x1162, 0x11B8],  # ㅐ ↔ ㅂ받침
    [0x1163, 0x11AF],  # ㅑ ↔ ㄹ받침
    [0x1164, 0x11BA],  # ㅒ ↔ ㅅ받침
    [0x1165, 0x11C0],  # ㅓ ↔ ㅌ받침
    [0x1166, 0x11A8],  # ㅔ ↔ ㄱ받침
    [0x1167, 0x11BF],  # ㅕ ↔ ㅋ받침
    [0x1168, 0x11AB],  # ㅖ ↔ ㄴ받침
    [0x1169, 0x11BD],  # ㅗ ↔ ㅈ받침
    [0x116D, 0x11BB],  # ㅛ ↔ ㅆ받침
    [0x116E, 0x11BE],  # ㅜ ↔ ㅊ받침
    [0x1172, 0x11BC],  # ㅠ ↔ ㅇ받침
    [0x1173, 0x11AE],  # ㅡ ↔ ㄷ받침
    [0x1175, 0x11C2],  # ㅣ ↔ ㅎ받침
    [0x119E, 0x11B7],  # ㆍ ↔ ㅁ받침
]
# Total directed lookups: 4 + 30 = 34 (matches galmadeuli_3shin_p2[]).

[combination]
# (a, b) → c
rules = [
    [0x1100, 0x1100, 0x1101],  # ㄱ + ㄱ = ㄲ (cho)
    [0x1169, 0x1161, 0x116A],  # ㅗ + ㅏ = ㅘ
    # ... (all 26 rules)
]

[options]
backspace_mode    = "by_jamo"   # only valid value for v0.1
ancient_hangul    = false       # toggle reserved; flips 2 keys (U, Y) — v0.2
extended_symbols  = false       # toggle reserved; pat.im 확장 기호 layer — v0.2+
```

The TOML is loaded at engine startup and never mutated at runtime.

---

## 6. Acceptance criteria for M2

The pure automaton module is "done" when:

1. `src/automaton.{h,cpp}` and `src/jamo.{h,cpp}` compile against C++20 with no fcitx5 includes.
2. `tests/automaton_test.cpp` runs ≥100 cases covering:
   - All 94 base keymap entries (jamo path + symbol-commit path).
   - All `galmadeuli_3shin_p2[]` entries (4 cho→jung one-way + 15 jung↔jong bidirectional, 34 directed lookups total).
   - All 26 combination rules (5 cho + 9 jung + 12 jong).
   - Backspace decomposition of every compound (5 cho doubles, 9 jung compounds, 12 jong compounds — undoes via backup slot).
   - 도깨비불 with simple jong (one case per 14 modern single 종성 consonants, wholesale move) **and** compound jong (all 12 entries in `jong_split` table, split into keep + promote).
   - Mixed sequences (corpus in §4, expanded to ≥30 entries).
   - Modifier passthrough (Ctrl+C, Alt+Tab, plain unmapped keys) returns `consumed=false` and commits any pending syllable.
   - **Jong-only state coverage**: at least one test for each §3.4 JONG case (A/B/C/D), verifying no silent overwrite of compound jong and that jong-only renders as raw conjoining jamo.
3. CI green on the feature branch.
4. No fcitx5 wiring yet — that is M3.

---

## 7. Open implementation questions (for self, not user)

1. **Test framework**: Catch2 (header-only, easy CMake integration) vs GoogleTest. Lean Catch2 for v0.1 since fcitx5 itself uses Catch2 in some addons.
2. **TOML parser**: `toml++` (single header, C++17, MIT) is the standard pick. Add as `FetchContent` or system package on Ubuntu (`libtomlplusplus-dev`).
3. **char32_t vs uint32_t**: use `char32_t` for jamo codes; it documents intent and the type is exactly `uint_least32_t`.
4. **`std::expected`**: Ubuntu 24.04 ships GCC 13 which has partial `<expected>` support. Use `tl::expected` polyfill (single header) to avoid version friction. Confirmed compatible with C++20 baseline.
5. **Whether to bake the keymap into the binary or load TOML at runtime**: loading TOML is the design intent (per CLAUDE.md "reloadable via fcitx5 config UI"). Cost: parse-time on engine activate. Acceptable.
