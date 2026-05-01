#include <catch2/catch_test_macros.hpp>

#include "qwerty_translator.h"

using sin3p2::QwertyTranslator;

// X11 keycode = evdev keycode + 8.
// /usr/include/linux/input-event-codes.h 참고:
//   KEY_A=30, KEY_F=33, KEY_J=36, KEY_Q=16, KEY_K=37, KEY_SEMICOLON=39
namespace {
constexpr int x11(int evdev) { return evdev + 8; }
constexpr int KC_Q = x11(16);
constexpr int KC_F = x11(33);
constexpr int KC_J = x11(36);
constexpr int KC_K = x11(37);
constexpr int KC_A = x11(30);
constexpr int KC_SEMICOLON = x11(39);
}  // namespace

TEST_CASE("QwertyTranslator: 초기화", "[qwerty]") {
    QwertyTranslator t;
    REQUIRE(t.ok());  // xkeyboard-config 미설치 시 실패 — Ubuntu에선 항상 있음
}

TEST_CASE("QwertyTranslator: 기본 letter → US QWERTY keysym", "[qwerty]") {
    QwertyTranslator t;
    REQUIRE(t.ok());
    REQUIRE(t.translate(KC_Q, false, 0) == 'q');
    REQUIRE(t.translate(KC_F, false, 0) == 'f');
    REQUIRE(t.translate(KC_J, false, 0) == 'j');
    REQUIRE(t.translate(KC_K, false, 0) == 'k');
    REQUIRE(t.translate(KC_A, false, 0) == 'a');
    REQUIRE(t.translate(KC_SEMICOLON, false, 0) == ';');
}

TEST_CASE("QwertyTranslator: shift → 대문자 / shifted 기호", "[qwerty]") {
    QwertyTranslator t;
    REQUIRE(t.ok());
    REQUIRE(t.translate(KC_Q, true, 0) == 'Q');
    REQUIRE(t.translate(KC_F, true, 0) == 'F');
    REQUIRE(t.translate(KC_J, true, 0) == 'J');
    REQUIRE(t.translate(KC_A, true, 0) == 'A');
    REQUIRE(t.translate(KC_SEMICOLON, true, 0) == ':');
}

TEST_CASE("QwertyTranslator: 잘못된 keycode → fallback", "[qwerty]") {
    QwertyTranslator t;
    REQUIRE(t.ok());
    REQUIRE(t.translate(0, false, 0xDEAD) == 0xDEAD);
    REQUIRE(t.translate(-1, false, 0xDEAD) == 0xDEAD);
    // 매우 큰 keycode (정의되지 않은 영역)
    REQUIRE(t.translate(999, false, 0xDEAD) == 0xDEAD);
}
