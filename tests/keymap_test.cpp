#include <catch2/catch_test_macros.hpp>

#include <string>

#include "keymap.h"

using sinsebeolsik_p2::Keymap;
using sinsebeolsik_p2::combination_lookup;
using sinsebeolsik_p2::galmadeuli_lookup;
using sinsebeolsik_p2::load_keymap_from_file;
using sinsebeolsik_p2::load_keymap_from_string;

namespace {

// Tests load the bundled P2 keymap once. The path is provided by CMake
// via -DP2_KEYMAP_PATH so the build directory location is independent.
const std::string& p2_path() {
    static const std::string path = P2_KEYMAP_PATH;
    return path;
}

const Keymap& p2_keymap() {
    static const Keymap k = [] {
        auto r = load_keymap_from_file(p2_path());
        REQUIRE(r.ok());
        return *std::move(r.keymap);
    }();
    return k;
}

}  // namespace

TEST_CASE("loader: malformed TOML rejected", "[keymap][load]") {
    auto r = load_keymap_from_string("this is = not valid [toml");
    REQUIRE_FALSE(r.ok());
    REQUIRE_FALSE(r.error.empty());
}

TEST_CASE("loader: missing base_keymap rejected", "[keymap][load]") {
    auto r = load_keymap_from_string(R"([meta]
name = "x"
)");
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.error.find("base_keymap") != std::string::npos);
}

TEST_CASE("loader: ascii out of range rejected", "[keymap][load]") {
    auto r = load_keymap_from_string(R"(base_keymap = [ [0x20, 0x20] ]
)");
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.error.find("range") != std::string::npos);
}

TEST_CASE("loader: minimal valid keymap", "[keymap][load]") {
    // base_keymap MUST come before any [section] header — TOML scopes bare
    // keys to the most recent table, so writing `[meta] ... base_keymap=`
    // would land base_keymap under meta.base_keymap.
    auto r = load_keymap_from_string(R"(
base_keymap = [
    [0x21, 0x21],
    [0x6B, 0x1100],
]

[meta]
name = "tiny"
)");
    INFO("error: " << r.error);
    REQUIRE(r.ok());
    REQUIRE(r.keymap->name == "tiny");
    REQUIRE(r.keymap->base[0x21] == 0x21);
    REQUIRE(r.keymap->base[0x6B] == 0x1100);
    // Anything else is 0 (passthrough).
    REQUIRE(r.keymap->base[0x6C] == 0);
}

TEST_CASE("loader: galmadeuli bidirectional inverses are auto-generated",
          "[keymap][load]") {
    auto r = load_keymap_from_string(R"(
base_keymap = [ [0x21, 0x21] ]

[galmadeuli]
one_way = [
    [0x1106, 0x1173],
]
bidirectional = [
    [0x1161, 0x11C1],
]
)");
    REQUIRE(r.ok());
    const auto& km = *r.keymap;
    // one_way: only forward
    REQUIRE(galmadeuli_lookup(km, 0x1106) == 0x1173);
    REQUIRE(galmadeuli_lookup(km, 0x1173) == 0);   // ← no auto inverse for one_way
    // bidirectional: both directions
    REQUIRE(galmadeuli_lookup(km, 0x1161) == 0x11C1);
    REQUIRE(galmadeuli_lookup(km, 0x11C1) == 0x1161);
    // unrelated lookup misses
    REQUIRE(galmadeuli_lookup(km, 0x1100) == 0);
}

TEST_CASE("loader: duplicate `from` is rejected (one_way + bidi inverse collision)",
          "[keymap][load]") {
    // ㅡ → ㅁ초성 from one_way's auto-inverse would shadow ㅡ → ㄷ받침
    // from bidirectional. We DON'T auto-invert one_way, but if a TOML
    // accidentally lists the same `from` twice (e.g., user error), the
    // loader must catch it.
    auto r = load_keymap_from_string(R"(
base_keymap = [ [0x21, 0x21] ]

[galmadeuli]
one_way = [
    [0x1106, 0x1173],
    [0x1106, 0x1100],   # duplicate `from`
]
)");
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.error.find("duplicate") != std::string::npos);
}

TEST_CASE("loader: combination + jong_split derivation",
          "[keymap][load]") {
    auto r = load_keymap_from_string(R"(
base_keymap = [ [0x21, 0x21] ]

[combination]
rules = [
    [0x1100, 0x1100, 0x1101],   # ㄱ + ㄱ = ㄲ (cho — not a jong_split entry)
    [0x11AF, 0x11A8, 0x11B0],   # ㄹ받침 + ㄱ받침 = ㄺ → split keep=ㄹ, promote=ㄱ초성
    [0x1169, 0x1161, 0x116A],   # ㅗ + ㅏ = ㅘ (jung — not a jong_split entry)
]
)");
    REQUIRE(r.ok());
    const auto& km = *r.keymap;

    // forward combination lookup
    REQUIRE(combination_lookup(km, 0x1100, 0x1100) == 0x1101);   // ㄲ
    REQUIRE(combination_lookup(km, 0x11AF, 0x11A8) == 0x11B0);   // ㄺ
    REQUIRE(combination_lookup(km, 0x1169, 0x1161) == 0x116A);   // ㅘ
    // miss
    REQUIRE(combination_lookup(km, 0x1100, 0x1102) == 0);

    // jong_split: only the JONG result populates this map.
    REQUIRE(km.jong_split.size() == 1);
    auto it = km.jong_split.find(0x11B0);
    REQUIRE(it != km.jong_split.end());
    REQUIRE(it->second.keep == 0x11AF);     // ㄹ받침
    REQUIRE(it->second.promote == 0x1100);  // ㄱ초성

    // ㄲ (cho double) does NOT appear — its 'a' is a cho, not a jong.
    REQUIRE(km.jong_split.find(0x1101) == km.jong_split.end());
    // ㅘ (jung compound) does NOT appear.
    REQUIRE(km.jong_split.find(0x116A) == km.jong_split.end());
}

// ---------------------------------------------------------------------------
// Integration with the bundled P2 keymap. Verifies the file parses and
// matches a sample of expected entries from the 3beol/ohi reference.
// ---------------------------------------------------------------------------

TEST_CASE("P2: file loads cleanly", "[keymap][p2]") {
    auto r = load_keymap_from_file(p2_path());
    INFO("error: " << r.error);
    REQUIRE(r.ok());
}

TEST_CASE("P2: meta", "[keymap][p2]") {
    const auto& km = p2_keymap();
    REQUIRE(km.name == "Sinsebeolsik P2");
    REQUIRE(km.layout_revision == "2016-08-06");
    REQUIRE(km.symbol_revision == "2018-04-10");
    REQUIRE(km.scope == "basic-p2");
    REQUIRE_FALSE(km.ancient_hangul);
    REQUIRE_FALSE(km.extended_symbols);
}

TEST_CASE("P2: base_keymap representative entries", "[keymap][p2]") {
    const auto& km = p2_keymap();

    // Letter row — lowercase
    REQUIRE(km.base['k']  == 0x1100);  // ㄱ초성
    REQUIRE(km.base['h']  == 0x1102);  // ㄴ초성
    REQUIRE(km.base[';']  == 0x1107);  // ㅂ초성
    REQUIRE(km.base['j']  == 0x110B);  // ㅇ초성
    REQUIRE(km.base['m']  == 0x1112);  // ㅎ초성
    REQUIRE(km.base['z']  == 0x11B7);  // ㅁ받침
    REQUIRE(km.base['f']  == 0x11C1);  // ㅍ받침

    // Letter row — uppercase (shift = vowel)
    REQUIRE(km.base['F']  == 0x1161);  // ㅏ
    REQUIRE(km.base['Z']  == 0x119E);  // ㆍ
    REQUIRE(km.base['G']  == 0x1173);  // ㅡ

    // Symbol passthroughs
    REQUIRE(km.base['1']  == 0x31);    // 1
    REQUIRE(km.base['!']  == 0x21);    // !
    REQUIRE(km.base['(']  == 0x28);    // (
    REQUIRE(km.base[',']  == 0x2C);    // ,

    // 2018-04-10 symbol layer
    REQUIRE(km.base['"']  == 0x2F);    // " → /
    REQUIRE(km.base['\''] == 0x1110);  // ' → ㅌ초성
    REQUIRE(km.base['/']  == 0x110F);  // / → ㅋ초성
    REQUIRE(km.base['L']  == 0x00B7);  // shift+L → ·
    REQUIRE(km.base['M']  == 0x2026);  // shift+M → …
    REQUIRE(km.base['N']  == 0x2015);  // shift+N → ―

    // Removed in 2018-04-10 (formerly vowels on shift+I/O)
    REQUIRE(km.base['I']  == 0);       // unclaimed
    REQUIRE(km.base['O']  == 0);       // unclaimed
}

TEST_CASE("P2: galmadeuli — cho→jung one-way", "[keymap][p2]") {
    const auto& km = p2_keymap();

    REQUIRE(galmadeuli_lookup(km, 0x1106) == 0x1173);  // ㅁ초성 → ㅡ
    REQUIRE(galmadeuli_lookup(km, 0x110E) == 0x116E);  // ㅊ초성 → ㅜ
    REQUIRE(galmadeuli_lookup(km, 0x110F) == 0x1169);  // ㅋ초성 → ㅗ
    REQUIRE(galmadeuli_lookup(km, 0x1111) == 0x119E);  // ㅍ초성 → ㆍ

    // Other cho (not in the 4-entry table) miss.
    REQUIRE(galmadeuli_lookup(km, 0x1100) == 0);       // ㄱ초성
    REQUIRE(galmadeuli_lookup(km, 0x1102) == 0);       // ㄴ초성
}

TEST_CASE("P2: galmadeuli — jung↔jong bidirectional", "[keymap][p2]") {
    const auto& km = p2_keymap();

    // forward: jung → jong
    REQUIRE(galmadeuli_lookup(km, 0x1161) == 0x11C1);  // ㅏ → ㅍ받침
    REQUIRE(galmadeuli_lookup(km, 0x1175) == 0x11C2);  // ㅣ → ㅎ받침
    REQUIRE(galmadeuli_lookup(km, 0x119E) == 0x11B7);  // ㆍ → ㅁ받침

    // reverse: jong → jung (auto-generated)
    REQUIRE(galmadeuli_lookup(km, 0x11C1) == 0x1161);  // ㅍ받침 → ㅏ
    REQUIRE(galmadeuli_lookup(km, 0x11C2) == 0x1175);  // ㅎ받침 → ㅣ
    REQUIRE(galmadeuli_lookup(km, 0x11B7) == 0x119E);  // ㅁ받침 → ㆍ

    // ㅡ ↔ ㄷ받침 — important: must NOT collide with cho→jung's ㅁ초성→ㅡ.
    // The one_way lookup ㅡ→? would only return something if ㅡ had an
    // auto-inverse, which we explicitly don't generate. So ㅡ as `from`
    // should only return ㄷ받침 (from the bidirectional pair).
    REQUIRE(galmadeuli_lookup(km, 0x1173) == 0x11AE);  // ㅡ → ㄷ받침
    REQUIRE(galmadeuli_lookup(km, 0x11AE) == 0x1173);  // ㄷ받침 → ㅡ
}

TEST_CASE("P2: combination — counts and key entries", "[keymap][p2]") {
    const auto& km = p2_keymap();

    REQUIRE(km.combination.size() == 26);  // 5 + 9 + 12 per docs §2.3

    // cho doubles
    REQUIRE(combination_lookup(km, 0x1100, 0x1100) == 0x1101);  // ㄲ
    REQUIRE(combination_lookup(km, 0x110C, 0x110C) == 0x110D);  // ㅉ

    // jung compounds
    REQUIRE(combination_lookup(km, 0x1169, 0x1161) == 0x116A);  // ㅘ
    REQUIRE(combination_lookup(km, 0x1173, 0x1175) == 0x1174);  // ㅢ
    REQUIRE(combination_lookup(km, 0x119E, 0x1175) == 0x11A1);  // ㆎ
    REQUIRE(combination_lookup(km, 0x119E, 0x119E) == 0x11A2);  // ᆢ (쌍아래아)

    // jong compounds
    REQUIRE(combination_lookup(km, 0x11AF, 0x11A8) == 0x11B0);  // ㄺ
    REQUIRE(combination_lookup(km, 0x11A8, 0x11BA) == 0x11AA);  // ㄳ
    REQUIRE(combination_lookup(km, 0x11B8, 0x11BA) == 0x11B9);  // ㅄ

    // misses
    REQUIRE(combination_lookup(km, 0x1100, 0x1102) == 0);       // ㄱ + ㄴ — no rule
    REQUIRE(combination_lookup(km, 0x1161, 0x1162) == 0);       // ㅏ + ㅐ — no rule
}

TEST_CASE("P2: jong_split has all 11 derivable compound jongs",
          "[keymap][p2]") {
    const auto& km = p2_keymap();

    // ㄲ (compound jong) exists in combination but its 'b' is ㄱ받침
    // (which has a cho-form), so it IS in jong_split.
    REQUIRE(km.jong_split.size() == 12);  // all 12 jong compounds

    // Spot-check the canonical examples
    auto check = [&](char32_t compound, char32_t keep, char32_t promote) {
        auto it = km.jong_split.find(compound);
        REQUIRE(it != km.jong_split.end());
        REQUIRE(it->second.keep    == keep);
        REQUIRE(it->second.promote == promote);
    };

    check(0x11AA, 0x11A8, 0x1109);  // ㄳ → keep ㄱ받침, promote ㅅ초성
    check(0x11AC, 0x11AB, 0x110C);  // ㄵ → keep ㄴ받침, promote ㅈ초성
    check(0x11AD, 0x11AB, 0x1112);  // ㄶ → keep ㄴ받침, promote ㅎ초성
    check(0x11B0, 0x11AF, 0x1100);  // ㄺ → keep ㄹ받침, promote ㄱ초성
    check(0x11B1, 0x11AF, 0x1106);  // ㄻ → keep ㄹ, promote ㅁ초성
    check(0x11B2, 0x11AF, 0x1107);  // ㄼ → ㄹ, ㅂ
    check(0x11B3, 0x11AF, 0x1109);  // ㄽ → ㄹ, ㅅ
    check(0x11B4, 0x11AF, 0x1110);  // ㄾ → ㄹ, ㅌ
    check(0x11B5, 0x11AF, 0x1111);  // ㄿ → ㄹ, ㅍ
    check(0x11B6, 0x11AF, 0x1112);  // ㅀ → ㄹ, ㅎ
    check(0x11B9, 0x11B8, 0x1109);  // ㅄ → ㅂ, ㅅ
    check(0x11A9, 0x11A8, 0x1100);  // ㄲ받침 → ㄱ받침, ㄱ초성

    // Choseong doubles (ㄲ ㄸ ㅃ ㅆ ㅉ) and jung compounds (ㅘ ...) are NOT
    // in jong_split — they have wrong slot category.
    REQUIRE(km.jong_split.find(0x1101) == km.jong_split.end());  // ㄲ초성
    REQUIRE(km.jong_split.find(0x116A) == km.jong_split.end());  // ㅘ
}
