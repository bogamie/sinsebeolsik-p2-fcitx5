#include "automaton.h"

namespace sinsebeolsik_p2 {

namespace {

// ---------------------------------------------------------------------------
// Helpers — small, self-contained primitives used across the step branches.
// ---------------------------------------------------------------------------

// Build a result that commits `s.cur`, resets the buffer, and signals
// the engine to ALSO let the keystroke pass through (e.g., Ctrl+C,
// unmapped key). `claimed=false` instructs the engine wrapper to not
// swallow the event.
StepResult commit_then_passthrough(const State& s) {
    StepResult r;
    r.commit   = render(s.cur);
    r.next.cur = Syllable{};
    r.consumed = false;
    return r;
}

// Build a result that commits `s.cur` and starts a new syllable holding
// just the given jamo in `slot`. Used by §3.4 commit-and-restart paths.
StepResult commit_and_start_with(const State& s,
                                 char32_t code,
                                 JamoSlot slot) {
    StepResult r;
    r.commit = render(s.cur);
    Syllable n;
    switch (slot) {
        case JamoSlot::Cho:  n.cho  = code; break;
        case JamoSlot::Jung: n.jung = code; break;
        case JamoSlot::Jong: n.jong = code; break;
        case JamoSlot::None: break;  // unreachable on legitimate input
    }
    r.next.cur = n;
    r.preedit  = render(n);
    r.consumed = true;
    return r;
}

// Build a result that updates `s` in place (no commit) and returns it.
StepResult update(const State& s, Syllable next_cur) {
    StepResult r;
    r.next.cur = next_cur;
    r.preedit  = render(next_cur);
    r.consumed = true;
    return r;
}

// Apply 도깨비불 split per docs §3.5. Caller has verified that the
// closing syllable has cho+jung filled and jong non-zero.
StepResult apply_dokkaebibul(const State& s,
                             const Keymap& km,
                             char32_t jung_code) {
    Syllable closing = s.cur;
    char32_t new_cho = 0;

    auto split_it = km.jong_split.find(closing.jong);
    if (split_it != km.jong_split.end()) {
        // Compound jong — split into keep + promote.
        closing.jong = split_it->second.keep;
        new_cho      = split_it->second.promote;
    } else {
        // Simple jong — wholesale move to next 초성.
        new_cho      = jong_to_cho(closing.jong);
        closing.jong = 0;
    }
    closing.jong_prev = 0;  // backup discarded — closing commits

    StepResult r;
    r.commit = render(closing);
    Syllable n;
    n.cho  = new_cho;
    n.jung = jung_code;
    r.next.cur = n;
    r.preedit  = render(n);
    r.consumed = true;
    return r;
}

// ---------------------------------------------------------------------------
// Layer 1 — galmadeuli reclassification (docs §3.3).
// Returns the (possibly rewritten) (code, cat) pair handed to Layer 2.
// ---------------------------------------------------------------------------

struct Reclassified { char32_t code; JamoSlot cat; };

Reclassified layer1_reclassify(const State& s,
                               const Keymap& km,
                               char32_t code,
                               JamoSlot cat) {
    if (cat == JamoSlot::Jung) {
        // Post-jung in a still-open syllable: prefer compound; if no
        // compound rule, swap to the JONG alternate via galmadeuli.
        if (s.cur.jung != 0 && s.cur.jong == 0 && s.cur.jung_prev == 0) {
            if (combination_lookup(km, s.cur.jung, code) == 0) {
                char32_t alt = galmadeuli_lookup(km, code);
                if (alt != 0 && classify(alt) == JamoSlot::Jong) {
                    return {alt, JamoSlot::Jong};
                }
            }
        }
        return {code, cat};
    }

    if (cat == JamoSlot::Jong) {
        // Cannot attach a 종성 to a syllable that lacks 초성 or 중성:
        // route the keystroke to the corresponding JUNG (or rarely CHO)
        // via galmadeuli so we never produce a degenerate compound.
        if (s.cur.cho == 0 || s.cur.jung == 0) {
            char32_t alt = galmadeuli_lookup(km, code);
            if (alt != 0) {
                JamoSlot alt_cat = classify(alt);
                if (alt_cat == JamoSlot::Jung || alt_cat == JamoSlot::Cho) {
                    return {alt, alt_cat};
                }
            }
        }
        return {code, cat};
    }

    if (cat == JamoSlot::Cho) {
        // Post-cho with no jung yet: prefer cho compound; else swap to
        // the JUNG alternate (the i/o/p/slash 갈마들이).
        if (s.cur.cho != 0 && s.cur.jung == 0 && s.cur.cho_prev == 0) {
            if (combination_lookup(km, s.cur.cho, code) == 0) {
                char32_t alt = galmadeuli_lookup(km, code);
                if (alt != 0 && classify(alt) == JamoSlot::Jung) {
                    return {alt, JamoSlot::Jung};
                }
            }
        }
        return {code, cat};
    }

    return {code, cat};
}

// ---------------------------------------------------------------------------
// Layer 2 — slot composition (docs §3.4). One function per category.
// ---------------------------------------------------------------------------

StepResult step_cho(const State& s, const Keymap& km, char32_t code) {
    // Non-empty syllable that already has either a vowel/jong OR a
    // compound 초성 already formed — commit and start fresh.
    if (s.cur.jung || s.cur.jong || s.cur.cho_prev) {
        return commit_and_start_with(s, code, JamoSlot::Cho);
    }
    if (s.cur.cho != 0) {
        char32_t combo = combination_lookup(km, s.cur.cho, code);
        if (combo != 0) {
            Syllable n = s.cur;
            n.cho_prev = s.cur.cho;
            n.cho      = combo;
            return update(s, n);
        }
        return commit_and_start_with(s, code, JamoSlot::Cho);
    }
    Syllable n = s.cur;
    n.cho = code;
    return update(s, n);
}

StepResult step_jung(const State& s, const Keymap& km, char32_t code) {
    // 도깨비불 path — current syllable has cho+jung+jong, vowel arrives.
    if (s.cur.jong != 0 && s.cur.cho != 0 && s.cur.jung != 0) {
        return apply_dokkaebibul(s, km, code);
    }
    // Edge: jong-only or cho+jong without jung. No real syllable to
    // detach from — commit cur as-is, start new with the vowel.
    if (s.cur.jong != 0) {
        return commit_and_start_with(s, code, JamoSlot::Jung);
    }
    // Compound jung already formed — no chained compounds, commit & restart.
    if (s.cur.jung_prev != 0) {
        return commit_and_start_with(s, code, JamoSlot::Jung);
    }
    if (s.cur.jung != 0) {
        char32_t combo = combination_lookup(km, s.cur.jung, code);
        if (combo != 0) {
            Syllable n = s.cur;
            n.jung_prev = s.cur.jung;
            n.jung      = combo;
            return update(s, n);
        }
        return commit_and_start_with(s, code, JamoSlot::Jung);
    }
    Syllable n = s.cur;
    n.jung = code;
    return update(s, n);
}

StepResult step_jong(const State& s, const Keymap& km, char32_t code) {
    // Case A — cannot attach (no 초성 or no 중성). Commit any partial,
    // start new jong-only syllable. (Layer 1 already tried galmadeuli;
    // we reach here only when no JUNG/CHO alternate exists for this
    // code — defensive.)
    if (s.cur.cho == 0 || s.cur.jung == 0) {
        return commit_and_start_with(s, code, JamoSlot::Jong);
    }

    // Case B — first 종성 of this syllable.
    if (s.cur.jong == 0) {
        Syllable n = s.cur;
        n.jong = code;
        return update(s, n);
    }

    // Case C — single 종성, attempt compound.
    if (s.cur.jong_prev == 0) {
        char32_t combo = combination_lookup(km, s.cur.jong, code);
        if (combo != 0) {
            Syllable n = s.cur;
            n.jong_prev = s.cur.jong;
            n.jong      = combo;
            return update(s, n);
        }
        return commit_and_start_with(s, code, JamoSlot::Jong);
    }

    // Case D — compound 종성 already formed. Commit, start new (jong-only).
    // The existing compound is NOT silently overwritten.
    return commit_and_start_with(s, code, JamoSlot::Jong);
}

}  // namespace

// ---------------------------------------------------------------------------
// Public entry points.
// ---------------------------------------------------------------------------

StepResult step(const State& s, const Keymap& km, KeyInput k) {
    // Modifier combos (Ctrl/Alt/Super) always pass through, committing
    // any pending syllable. The user expects Ctrl+C to copy regardless
    // of IME state.
    if (k.ctrl || k.alt || k.super) {
        return commit_then_passthrough(s);
    }

    // Reject anything outside printable ASCII — those are function keys,
    // arrow keys, or input we don't map.
    if (k.keysym < 0x21 || k.keysym > 0x7E) {
        return commit_then_passthrough(s);
    }

    char32_t code = km.base[k.keysym];
    if (code == 0) {
        // Not claimed — passthrough (e.g., shift+I in 2018-04-10 P2).
        return commit_then_passthrough(s);
    }

    JamoSlot cat = classify(code);

    // Symbol/punctuation entry: claim the key, commit current syllable,
    // emit the symbol as an additional commit. The symbol does not
    // enter the syllable buffer.
    if (cat == JamoSlot::None) {
        StepResult r;
        r.commit   = render(s.cur);
        r.commit  += utf8_encode(code);
        r.next.cur = Syllable{};
        r.consumed = true;
        return r;
    }

    auto rc = layer1_reclassify(s, km, code, cat);

    switch (rc.cat) {
        case JamoSlot::Cho:  return step_cho (s, km, rc.code);
        case JamoSlot::Jung: return step_jung(s, km, rc.code);
        case JamoSlot::Jong: return step_jong(s, km, rc.code);
        case JamoSlot::None: break;  // unreachable post-Layer 1
    }
    return commit_then_passthrough(s);
}

StepResult backspace(const State& s) {
    StepResult r;
    if (s.cur.empty()) {
        r.consumed = false;   // let the app delete a previously-committed char
        return r;
    }

    Syllable next = s.cur;

    // Slot order matches the 6-tuple ohiQ[0..5] = cho, cho_prev, jung,
    // jung_prev, jong, jong_prev. Walk from highest to find the most
    // recent jamo to retract.
    char32_t* slots[6] = {
        &next.cho, &next.cho_prev,
        &next.jung, &next.jung_prev,
        &next.jong, &next.jong_prev,
    };
    int last = -1;
    for (int i = 0; i < 6; ++i) {
        if (*slots[i] != 0) last = i;
    }
    if (last < 0) {
        r.consumed = false;
        return r;
    }

    if ((last & 1) != 0) {
        // Odd slot = backup. Restore the corresponding even slot — i.e.,
        // peel the most recent compound (ㄺ → ㄹ받침, ㅘ → ㅗ, ㄲ → ㄱ).
        *slots[last - 1] = *slots[last];
        *slots[last]     = 0;
    } else {
        *slots[last] = 0;
    }

    r.next.cur = next;
    r.preedit  = render(next);
    r.consumed = true;
    return r;
}

StepResult commit_and_reset(const State& s) {
    StepResult r;
    r.commit   = render(s.cur);
    r.next.cur = Syllable{};
    r.consumed = true;
    return r;
}

}  // namespace sinsebeolsik_p2
