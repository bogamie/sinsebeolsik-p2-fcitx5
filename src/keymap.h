#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sinsebeolsik_p2 {

struct GalmadeuliEntry {
    char32_t from;
    char32_t to;
};

struct CombinationRule {
    char32_t a;
    char32_t b;
    char32_t result;
};

struct JongSplit {
    char32_t keep;     // first jamo, stays as 종성 of the closing syllable
    char32_t promote;  // second jamo, already converted to 초성 form
};

struct Keymap {
    // Metadata
    std::string name;
    std::string layout_revision;
    std::string symbol_revision;
    std::string scope;

    // Indexed by ASCII code 0x21..0x7E. 0 = not claimed (passthrough).
    // Indices outside the printable range are unused.
    std::array<char32_t, 128> base{};

    // Sorted by `from` for binary search.
    std::vector<GalmadeuliEntry> galmadeuli;

    // Sorted by ((a << 32) | b) for binary search.
    std::vector<CombinationRule> combination;

    // Auto-derived at load time from `combination` rules whose result is a
    // compound jong: jong_split[result] = {a, jong_to_cho(b)}.
    // Compound results that don't decompose to a simple cho (e.g., ㅆ받침)
    // are skipped. See docs/automaton.md §3.5.
    std::unordered_map<char32_t, JongSplit> jong_split;

    // Options
    bool ancient_hangul = false;
    bool extended_symbols = false;
};

// O(log n) galmadeuli lookup. Returns 0 if `code` has no entry.
char32_t galmadeuli_lookup(const Keymap& km, char32_t code) noexcept;

// O(log n) combination lookup. Returns 0 if (a, b) has no rule.
char32_t combination_lookup(const Keymap& km, char32_t a, char32_t b) noexcept;

struct LoadResult {
    std::optional<Keymap> keymap;
    std::string error;     // populated only on failure
    bool ok() const noexcept { return keymap.has_value() && error.empty(); }
};

LoadResult load_keymap_from_string(std::string_view toml_text);
LoadResult load_keymap_from_file(const std::string& path);

}  // namespace sinsebeolsik_p2
