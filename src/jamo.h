#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sinsebeolsik_p2 {

enum class JamoSlot {
    None,
    Cho,
    Jung,
    Jong,
};

// Classify a Unicode codepoint:
//   Cho:  U+1100..U+1112        — 19 modern initials
//   Jung: U+1161..U+1175         — 21 modern medials
//         U+119E                 — ㆍ (아래아)
//         U+11A1..U+11A2         — ㆎ, ᆢ
//   Jong: U+11A8..U+11C2         — 27 modern finals (incl. compounds)
//   None: anything else
JamoSlot classify(char32_t code) noexcept;

// True iff `code` is a modern medial that participates in the precomposed
// Hangul Syllables Block (U+AC00..U+D7A3). Excludes archaic ㆍ ㆎ ᆢ.
bool is_modern_jung(char32_t code) noexcept;

// Map a 종성-form consonant to its 초성-form counterpart for the 14 simple
// consonants. Compound 종성 (ㄲ ㄳ ㄺ ...) return 0 — split via automaton's
// jong_split table, not here.
char32_t jong_to_cho(char32_t code) noexcept;

// Inverse of jong_to_cho. Returns 0 for codes that don't have a matching
// simple 종성 form (e.g., the compound choseongs ㄲ/ㄸ/ㅃ/ㅆ/ㅉ).
char32_t cho_to_jong(char32_t code) noexcept;

// Six-slot syllable buffer. Mirrors ohi.pat.im's `ohiQ`.
// The *_prev slots are non-zero only when the corresponding current slot
// holds a compound whose pre-compound form is recoverable by backspace.
struct Syllable {
    char32_t cho       = 0;
    char32_t cho_prev  = 0;
    char32_t jung      = 0;
    char32_t jung_prev = 0;
    char32_t jong      = 0;
    char32_t jong_prev = 0;

    bool empty() const noexcept { return !cho && !jung && !jong; }
};

// Compose into a precomposed Hangul Syllable codepoint (U+AC00..U+D7A3).
// Pass 0 for `jong` if absent. Returns nullopt when the components don't
// all fit the modern syllables block — in particular when:
//   - cho is absent or out of U+1100..U+1112,
//   - jung is absent, archaic, or out of U+1161..U+1175,
//   - jong (when non-zero) is out of U+11A8..U+11C2.
std::optional<char32_t> compose(char32_t cho,
                                char32_t jung,
                                char32_t jong = 0) noexcept;

// Render a syllable buffer as UTF-8 for preedit/commit. Uses precomposed
// Hangul Syllables when feasible; otherwise emits raw conjoining jamo
// (transient bare-jung, jong-only, ㆍ vowels, etc.). Returns "" for empty.
std::string render(const Syllable& s);

// Encode a single codepoint as UTF-8. Returns "" when cp == 0
// (the engine's "empty slot" sentinel — never a real input).
std::string utf8_encode(char32_t cp);

}  // namespace sinsebeolsik_p2
