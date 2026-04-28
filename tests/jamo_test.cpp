#include <catch2/catch_test_macros.hpp>

#include "jamo.h"

using sinsebeolsik_p2::JamoSlot;
using sinsebeolsik_p2::Syllable;
using sinsebeolsik_p2::classify;
using sinsebeolsik_p2::cho_to_jong;
using sinsebeolsik_p2::compose;
using sinsebeolsik_p2::is_modern_jung;
using sinsebeolsik_p2::jong_to_cho;
using sinsebeolsik_p2::render;
using sinsebeolsik_p2::utf8_encode;

TEST_CASE("classify modern jamo", "[jamo][classify]") {
    REQUIRE(classify(0x1100) == JamoSlot::Cho);   // ㄱ
    REQUIRE(classify(0x1112) == JamoSlot::Cho);   // ㅎ
    REQUIRE(classify(0x1161) == JamoSlot::Jung);  // ㅏ
    REQUIRE(classify(0x1175) == JamoSlot::Jung);  // ㅣ
    REQUIRE(classify(0x11A8) == JamoSlot::Jong);  // ㄱ받침
    REQUIRE(classify(0x11C2) == JamoSlot::Jong);  // ㅎ받침
}

TEST_CASE("classify archaic jung", "[jamo][classify]") {
    REQUIRE(classify(0x119E) == JamoSlot::Jung);  // ㆍ
    REQUIRE(classify(0x11A1) == JamoSlot::Jung);  // ㆎ
    REQUIRE(classify(0x11A2) == JamoSlot::Jung);  // ᆢ
}

TEST_CASE("classify out-of-range", "[jamo][classify]") {
    REQUIRE(classify(0)      == JamoSlot::None);
    REQUIRE(classify(0x10FF) == JamoSlot::None);
    REQUIRE(classify(0x1113) == JamoSlot::None);  // beyond cho range
    REQUIRE(classify(0x1176) == JamoSlot::None);  // gap (modern→archaic jung)
    REQUIRE(classify(0x119D) == JamoSlot::None);  // gap before ㆍ
    REQUIRE(classify(0x11A0) == JamoSlot::None);  // gap between ㆍ and ㆎ
    REQUIRE(classify(0x11A3) == JamoSlot::None);  // beyond ᆢ
    REQUIRE(classify(0x11A7) == JamoSlot::None);  // gap before jong range
    REQUIRE(classify(0x11C3) == JamoSlot::None);  // beyond jong
    REQUIRE(classify(0xAC00) == JamoSlot::None);  // 가 — precomposed, not jamo
}

TEST_CASE("is_modern_jung excludes archaic", "[jamo][classify]") {
    REQUIRE(is_modern_jung(0x1161));         // ㅏ
    REQUIRE(is_modern_jung(0x1175));         // ㅣ
    REQUIRE_FALSE(is_modern_jung(0x119E));   // ㆍ
    REQUIRE_FALSE(is_modern_jung(0x11A1));   // ㆎ
    REQUIRE_FALSE(is_modern_jung(0x11A2));   // ᆢ
    REQUIRE_FALSE(is_modern_jung(0));
    REQUIRE_FALSE(is_modern_jung(0x1100));   // ㄱ초성 — not a jung at all
}

TEST_CASE("jong_to_cho — all 14 simple consonants", "[jamo][slot]") {
    REQUIRE(jong_to_cho(0x11A8) == 0x1100);  // ㄱ
    REQUIRE(jong_to_cho(0x11AB) == 0x1102);  // ㄴ
    REQUIRE(jong_to_cho(0x11AE) == 0x1103);  // ㄷ
    REQUIRE(jong_to_cho(0x11AF) == 0x1105);  // ㄹ
    REQUIRE(jong_to_cho(0x11B7) == 0x1106);  // ㅁ
    REQUIRE(jong_to_cho(0x11B8) == 0x1107);  // ㅂ
    REQUIRE(jong_to_cho(0x11BA) == 0x1109);  // ㅅ
    REQUIRE(jong_to_cho(0x11BC) == 0x110B);  // ㅇ
    REQUIRE(jong_to_cho(0x11BD) == 0x110C);  // ㅈ
    REQUIRE(jong_to_cho(0x11BE) == 0x110E);  // ㅊ
    REQUIRE(jong_to_cho(0x11BF) == 0x110F);  // ㅋ
    REQUIRE(jong_to_cho(0x11C0) == 0x1110);  // ㅌ
    REQUIRE(jong_to_cho(0x11C1) == 0x1111);  // ㅍ
    REQUIRE(jong_to_cho(0x11C2) == 0x1112);  // ㅎ
}

TEST_CASE("jong_to_cho rejects compound jong (split via automaton)", "[jamo][slot]") {
    REQUIRE(jong_to_cho(0x11A9) == 0);  // ㄲ
    REQUIRE(jong_to_cho(0x11AA) == 0);  // ㄳ
    REQUIRE(jong_to_cho(0x11AC) == 0);  // ㄵ
    REQUIRE(jong_to_cho(0x11AD) == 0);  // ㄶ
    REQUIRE(jong_to_cho(0x11B0) == 0);  // ㄺ
    REQUIRE(jong_to_cho(0x11B1) == 0);  // ㄻ
    REQUIRE(jong_to_cho(0x11B2) == 0);  // ㄼ
    REQUIRE(jong_to_cho(0x11B3) == 0);  // ㄽ
    REQUIRE(jong_to_cho(0x11B4) == 0);  // ㄾ
    REQUIRE(jong_to_cho(0x11B5) == 0);  // ㄿ
    REQUIRE(jong_to_cho(0x11B6) == 0);  // ㅀ
    REQUIRE(jong_to_cho(0x11B9) == 0);  // ㅄ
    REQUIRE(jong_to_cho(0x11BB) == 0);  // ㅆ받침 — single glyph but no simple cho-form here
    REQUIRE(jong_to_cho(0)      == 0);
}

TEST_CASE("cho_to_jong roundtrip and rejection", "[jamo][slot]") {
    REQUIRE(cho_to_jong(0x1100) == 0x11A8);  // ㄱ
    REQUIRE(cho_to_jong(0x1102) == 0x11AB);  // ㄴ
    REQUIRE(cho_to_jong(0x1112) == 0x11C2);  // ㅎ
    REQUIRE(cho_to_jong(0x1101) == 0);       // ㄲ초성 — compound, no simple jong
    REQUIRE(cho_to_jong(0x1104) == 0);       // ㄸ초성
    REQUIRE(cho_to_jong(0x1108) == 0);       // ㅃ초성
    REQUIRE(cho_to_jong(0x110A) == 0);       // ㅆ초성
    REQUIRE(cho_to_jong(0x110D) == 0);       // ㅉ초성
    REQUIRE(cho_to_jong(0)      == 0);
}

TEST_CASE("compose — modern syllables", "[jamo][compose]") {
    REQUIRE(compose(0x1100, 0x1161, 0).value()      == 0xAC00);  // 가
    REQUIRE(compose(0x1100, 0x1161, 0x11AB).value() == 0xAC04);  // 간
    REQUIRE(compose(0x1112, 0x1161, 0x11AB).value() == 0xD55C);  // 한
    REQUIRE(compose(0x1112, 0x1175, 0x11C2).value() == 0xD7A3);  // 힣 (last syllable in block)
    REQUIRE(compose(0x1100, 0x1161, 0x11B0).value() == 0xAC09);  // 갉 (compound jong ㄺ)
}

TEST_CASE("compose rejects archaic and malformed", "[jamo][compose]") {
    REQUIRE_FALSE(compose(0x1100, 0x119E, 0).has_value());        // ㆍ jung
    REQUIRE_FALSE(compose(0x1100, 0x11A1, 0).has_value());        // ㆎ
    REQUIRE_FALSE(compose(0x1100, 0x11A2, 0).has_value());        // ᆢ
    REQUIRE_FALSE(compose(0,      0x1161, 0).has_value());        // missing cho
    REQUIRE_FALSE(compose(0x1100, 0,      0).has_value());        // missing jung
    REQUIRE_FALSE(compose(0x1113, 0x1161, 0).has_value());        // out-of-range cho
    REQUIRE_FALSE(compose(0x1100, 0x1176, 0).has_value());        // out-of-range jung
    REQUIRE_FALSE(compose(0x1100, 0x1161, 0x11A7).has_value());   // jong below range
    REQUIRE_FALSE(compose(0x1100, 0x1161, 0x11C3).has_value());   // jong above range
}

TEST_CASE("utf8_encode covers ASCII, BMP, conjoining jamo", "[jamo][utf8]") {
    REQUIRE(utf8_encode(0).empty());                      // sentinel
    REQUIRE(utf8_encode('A')      == "A");                // ASCII
    REQUIRE(utf8_encode(0x80)     == std::string("\xC2\x80", 2));        // 2-byte boundary
    REQUIRE(utf8_encode(0x07FF)   == std::string("\xDF\xBF", 2));        // 2-byte top
    REQUIRE(utf8_encode(0x0800)   == std::string("\xE0\xA0\x80", 3));    // 3-byte boundary
    REQUIRE(utf8_encode(0xAC00)   == std::string("\xEA\xB0\x80", 3));    // 가
    REQUIRE(utf8_encode(0x1100)   == std::string("\xE1\x84\x80", 3));    // ㄱ초성
    REQUIRE(utf8_encode(0x119E)   == std::string("\xE1\x86\x9E", 3));    // ㆍ
    REQUIRE(utf8_encode(0x11A2)   == std::string("\xE1\x86\xA2", 3));    // ᆢ
    REQUIRE(utf8_encode(0x10000)  == std::string("\xF0\x90\x80\x80", 4)); // 4-byte boundary
}

TEST_CASE("render — precomposed when modern", "[jamo][render]") {
    Syllable s;

    // 가
    s = {.cho = 0x1100, .jung = 0x1161};
    REQUIRE(render(s) == utf8_encode(0xAC00));

    // 한
    s = {.cho = 0x1112, .jung = 0x1161, .jong = 0x11AB};
    REQUIRE(render(s) == utf8_encode(0xD55C));

    // 갉 (compound jong)
    s = {.cho = 0x1100, .jung = 0x1161, .jong = 0x11B0};
    REQUIRE(render(s) == utf8_encode(0xAC09));
}

TEST_CASE("render — conjoining-jamo fallback for non-precomposable states", "[jamo][render]") {
    Syllable s;

    // empty buffer
    s = {};
    REQUIRE(render(s).empty());

    // bare jung (legitimate transient state per design §3.6)
    s = {.jung = 0x1161};
    REQUIRE(render(s) == utf8_encode(0x1161));

    // jong-only (legitimate transient state per design §3.6)
    s = {.jong = 0x11A8};
    REQUIRE(render(s) == utf8_encode(0x11A8));

    // archaic ㆍ vowel — ㄱ + ㆍ has no precomposed form
    s = {.cho = 0x1100, .jung = 0x119E};
    REQUIRE(render(s) == utf8_encode(0x1100) + utf8_encode(0x119E));

    // ㆎ as jung (also archaic)
    s = {.cho = 0x1100, .jung = 0x11A1};
    REQUIRE(render(s) == utf8_encode(0x1100) + utf8_encode(0x11A1));

    // cho-only (rare — usually committed before reaching render, but valid input)
    s = {.cho = 0x1100};
    REQUIRE(render(s) == utf8_encode(0x1100));
}

TEST_CASE("Syllable::empty", "[jamo][buffer]") {
    Syllable s;
    REQUIRE(s.empty());

    s.cho = 0x1100;
    REQUIRE_FALSE(s.empty());

    s = {};
    s.jung = 0x1161;
    REQUIRE_FALSE(s.empty());

    s = {};
    s.jong = 0x11A8;
    REQUIRE_FALSE(s.empty());

    // backup slots alone do not make the syllable non-empty —
    // they only have meaning while their primary slot is set.
    s = {};
    s.cho_prev = 0x1100;
    REQUIRE(s.empty());
}
