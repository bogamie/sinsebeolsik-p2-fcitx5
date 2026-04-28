#pragma once

#include <cstdint>
#include <string>

#include "jamo.h"
#include "keymap.h"

namespace sinsebeolsik_p2 {

// Reduced view of an fcitx5 KeyEvent. The automaton operates on this so
// the pure-logic core does not transitively depend on libfcitx5.
//
// Shift state is intentionally absent — it is already encoded in the
// keysym ('a' (0x61) vs 'A' (0x41)). Modifier flags are passed so we
// can punt on Ctrl/Alt/Super combos without parsing the keymap.
struct KeyInput {
    char32_t keysym = 0;     // X11 KeySym, treated as ASCII for our keymap
    bool     ctrl   = false;
    bool     alt    = false;
    bool     super  = false;
};

// Engine-facing state: just the syllable buffer for now. fcitx5 lifecycle
// state lives in the engine wrapper.
struct State {
    Syllable cur;
};

struct StepResult {
    State       next;
    std::string commit;     // text to commit (UTF-8); may be empty
    std::string preedit;    // current syllable rendered as UTF-8
    bool        consumed = false;  // false → engine passes the event through
};

// Pure step. Given current state, the loaded keymap, and an input event,
// produce the next (state, commit, preedit, consumed). No I/O, no globals.
StepResult step(const State& s, const Keymap& km, KeyInput k);

// Backspace is a separate entry point (mirrors `ohi_Backspace` in
// ohi.js:343). Unwinds via the *_prev backup slots so compound jamo
// decomposes one step at a time before deleting the underlying jamo.
StepResult backspace(const State& s);

// Commit any pending syllable and zero the state. Used on focus loss,
// IME deactivate, or any explicit flush trigger.
StepResult commit_and_reset(const State& s);

}  // namespace sinsebeolsik_p2
