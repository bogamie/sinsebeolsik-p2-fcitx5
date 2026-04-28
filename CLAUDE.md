# Project: Sinsebeolsik P2 Hangul IME for fcitx5

## What this project is

A fcitx5 input method engine (addon) that provides **Sinsebeolsik P2 (신세벌식 P2)** Hangul input on Linux, with first-class support for the **Totem 38-key split keyboard** layout and graceful fallback to standard 60%+ keyboards.

English input is **out of scope** for this engine. Canary layout for English will be handled separately at the XKB layer; this engine only handles Hangul composition when the IME is active.

## Goals (in order of priority)

1. Working Sinsebeolsik P2 Hangul input on Ubuntu 24.04 with fcitx5, validated on the Totem 38-key keyboard
2. Forward compatibility with Ubuntu 26.04 LTS (GNOME 50, Wayland-only, `zwp_input_method_v2`)
3. Clean addon architecture so additional Hangul layouts (두벌식, 세벌식 최종, 신세벌식 변종) can be added later as separate keymaps
4. Standard 60%/TKL/full-size keyboard support as a non-blocking secondary target
5. Public-quality GitHub repo: clear README in Korean and English, MIT license, CI builds, contribution-friendly

## Non-goals

- Do not implement an IBus engine in v1 (fcitx5 only). IBus port is a possible future addon.
- Do not implement English layout switching inside the engine. Canary stays in XKB.
- Do not implement Hanja conversion in v1 (note it as a future enhancement).
- Do not vendor libhangul. Link against the system package.

## Target environment

- **Development**: KVM virtual machine running Ubuntu 24.04 LTS
- **Display server**: Both X11 and Wayland sessions must work; prioritize Wayland correctness
- **Compositors to test**: GNOME on Wayland (default), KDE Plasma Wayland (best fcitx5 support), and at least one wlroots-based compositor (sway) if feasible
- **fcitx5 version**: System package on 24.04 (5.1.x). Build must also succeed against 26.04 packages — keep CMake checks version-aware.
- **Compiler**: GCC default on 24.04; do not use C++23-only features

## Architecture decisions (already made — do not relitigate)

- **Language**: C++20. fcitx5 is C++, addon API is C++, this matches the ecosystem.
- **Hangul automaton**: Implement from scratch in this project. Do NOT depend on libhangul's automaton for Sinsebeolsik P2 — libhangul's P2 support is incomplete. We will have our own state machine and our own keymap format.
- **Keymap format**: TOML files under `keymaps/`. One file per layout (e.g. `sinsebeolsik_p2.toml`). Loaded at engine startup; reloadable via fcitx5 config UI.
- **Keyboard adaptation layer**: Logical key codes from fcitx5 (`KeySym` + modifiers), NOT raw scancodes. The Totem 38-key firmware (QMK assumed) emits standard USB HID codes via combos/layers/home-row mods, so the host sees a normal keyboard. The engine should be keyboard-agnostic at the input layer; Totem-specific behavior lives in documentation and recommended firmware config, not in the engine code.
- **Hangul/English toggle**: Bind to `Hangul` key (KC_LANG1, KeySym `Hangul` / 0xff31) by default. Provide config for alternate triggers (right Alt, custom combo). Do not steal `Super+Space` — GNOME claims it.

## Repository layout (target)

```
sinsebeolsik-p2-fcitx5/
├── CLAUDE.md                  # this file
├── README.md                  # English
├── README.ko.md               # Korean
├── LICENSE                    # MIT
├── CMakeLists.txt
├── .github/
│   └── workflows/
│       └── build.yml          # CI: build on Ubuntu 24.04 and 26.04
├── src/
│   ├── engine.cpp / .h        # fcitx5 InputMethodEngine subclass
│   ├── automaton.cpp / .h     # Hangul composition state machine
│   ├── keymap.cpp / .h        # TOML keymap loader
│   ├── jamo.cpp / .h          # Jamo combination tables, Unicode normalization
│   └── addon.conf.in          # fcitx5 addon descriptor
├── keymaps/
│   └── sinsebeolsik_p2.toml
├── tests/
│   ├── automaton_test.cpp     # unit tests for composition logic
│   └── corpus/                # input sequence → expected output fixtures
├── docs/
│   ├── architecture.md
│   ├── totem-firmware.md      # QMK config recommendations for Totem 38
│   └── adding-a-keymap.md
└── packaging/
    └── debian/                # debuild files for .deb packaging
```

## Coding standards

- C++20, four-space indent, `snake_case` for functions and variables, `PascalCase` for types
- Headers use `#pragma once`
- No exceptions across addon boundary; use `std::expected` (or `tl::expected` polyfill) internally
- All user-facing strings localizable via fcitx5's `_()` macro
- Comments and commit messages in English; user-facing docs in Korean primary, English secondary
- Run `clang-format` (config to match fcitx5 upstream style) before committing
- No `using namespace std;` in headers

## Hangul automaton: critical correctness rules

These are non-negotiable. The automaton must handle:

1. **초성–중성–종성 sequencing**: A jamo can only attach if the current syllable state allows it
2. **종성 분리 (도깨비불)**: When a jongseong is followed by a moeum, the jongseong becomes the choseong of the next syllable
3. **복합 자모**: Compound jamo like ㄳ, ㄵ, ㄶ, ㄺ, ㄻ, ㄼ, ㄽ, ㄾ, ㄿ, ㅀ, ㅄ, ㅘ, ㅙ, ㅚ, ㅝ, ㅞ, ㅟ, ㅢ
4. **신세벌식 P2 specifics**: P2 uses a **moa-chigi (모아치기) friendly** layout — research the exact P2 spec before implementing. The choseong/jungseong/jongseong key positions differ from 신세벌식 2003/2012.
5. **백스페이스**: Deletes one jamo at a time, not one syllable. This is standard Hangul IME behavior.
6. **커밋 타이밍**: Commit syllable on space, punctuation, layout switch, focus loss, or non-Hangul key.

Write the automaton as a pure function `(state, key) -> (new_state, commit_text, preedit_text)`. No fcitx5 dependencies in `automaton.cpp/h`. This makes it unit-testable without a running input method framework.

## Test strategy

- Unit tests for `automaton.cpp` using a corpus of `(input_sequence, expected_committed, expected_preedit)` triples
- Integration test script that launches fcitx5 in a nested session and pipes input
- CI runs unit tests on every push; integration tests on PRs against `main`

## Workflow rules for Claude Code

- **Always read existing files before editing them.** Use the view tool first.
- **One concern per commit.** Keep diffs small and reviewable.
- **Update tests in the same commit as the code change.** No "tests later" commits.
- **Never push directly to `main`.** Work on feature branches; user reviews PRs.
- **When uncertain about Sinsebeolsik P2 behavior, ASK.** Do not guess. The user is the domain expert here.
- **When uncertain about fcitx5 API, search the fcitx5 GitHub repo or its docs.** Do not invent function signatures.
- **Do not run `apt install` without confirmation.** List required packages and let the user install.
- **Do not modify `keymaps/sinsebeolsik_p2.toml` without explicit instruction.** This is the user's source of truth for the layout.
- **Korean text in code comments is fine when referring to specific jamo or layout names.** Don't translate "신세벌식" to "Sinsebeolsik" inside a code comment that's discussing the jamo table — keep it readable for Korean contributors.

## Milestones

### M1: Skeleton (target: end of week 1)
- Repo scaffolded, CMake builds an empty fcitx5 addon that registers itself
- README.md and README.ko.md drafts
- CI green on Ubuntu 24.04
- Engine appears in `fcitx5-configtool` but does nothing useful

### M2: Automaton (target: end of week 2)
- `automaton.cpp` implements full Hangul composition rules
- 100+ unit test cases passing, covering edge cases (도깨비불, 복합 자모, 백스페이스)
- No fcitx5 integration yet — pure logic

### M3: P2 Keymap + fcitx5 Wiring (target: end of week 3)
- `keymaps/sinsebeolsik_p2.toml` complete and validated against published P2 spec
- Engine processes real keystrokes, commits syllables, shows preedit
- Manual testing on Ubuntu 24.04 in both X11 and Wayland sessions

### M4: Polish + Public Release (target: end of week 4)
- Hangul/English toggle works reliably
- Configuration UI integration (fcitx5-configtool entries)
- `.deb` packaging
- Documentation: architecture.md, totem-firmware.md, adding-a-keymap.md
- v0.1.0 tagged release on GitHub

### Post-v0.1
- Ubuntu 26.04 testing and any needed fixes
- Additional keymaps (두벌식 standard, 세벌식 최종) as proof of extensibility
- Optional: IBus frontend addon

## Open questions to resolve before M2

The following must be answered (by user or by research) before automaton implementation:

1. Which exact Sinsebeolsik P2 specification version to target? (2014 / 2015 / latest community spec)
2. Behavior on shift+vowel: P2 has shift-layer keys — confirm full table
3. 갈마들이 (alternation) rules: P2 uses position-based jamo selection — list authoritative source
4. Does the user want 모아치기 (chord/simultaneous press) support, or sequential only?

When you (Claude Code) reach M2, present these questions to the user and wait for answers before writing the automaton.

## Reference projects (read these before implementing similar logic)

- **kime** (`Riey/kime` on GitHub): Rust Hangul IME with fcitx5 frontend and Sinsebeolsik support. Best modern reference for automaton structure.
- **libhangul** (`libhangul/libhangul`): C library, the canonical Hangul IME reference. Read its automaton (`hangulinputcontext.c`) for correctness rules even though we won't depend on it.
- **fcitx5-hangul** (`fcitx/fcitx5-hangul`): Minimal example of how to wrap a Hangul backend in fcitx5.
- **fcitx5** (`fcitx/fcitx5`): The framework itself. `src/lib/fcitx/` and `src/modules/` are essential reading.

When citing patterns from these in code or docs, attribute them.

## License & contribution

- MIT license
- Contributor sign-off via DCO (Developer Certificate of Origin) — `git commit -s`
- Code of Conduct: Contributor Covenant 2.1
- Issue templates for bug report and keymap proposal

## Communication preferences

- The user (Bogamie) is a Korean undergraduate with strong systems programming background. Skip basic explanations of C/C++/Linux concepts.
- The user prefers concise 개조식 (outline) style for technical notes; reserve prose for design discussions.
- When the user pushes back on a design decision, reconsider seriously rather than defending the original choice — they have domain knowledge (especially on Hangul layouts) that may not be in your training data.
