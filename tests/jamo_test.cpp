#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <tuple>

#include "jamo.h"

using namespace sin3p2;

TEST_CASE("compose_syllable produces correct precomposed Hangul", "[jamo][compose]") {
    REQUIRE(compose_syllable(Cho::G, Jung::A, Jong::None) == U'가');
    REQUIRE(compose_syllable(Cho::G, Jung::A, Jong::R)    == U'갈');
    REQUIRE(compose_syllable(Cho::H, Jung::A, Jong::N)    == U'한');
    REQUIRE(compose_syllable(Cho::G, Jung::WA, Jong::None) == U'과');
    REQUIRE(compose_syllable(Cho::O, Jung::U, Jong::H)    == U'웋');
    REQUIRE(compose_syllable(Cho::O, Jung::WI, Jong::None) == U'위');
    REQUIRE(compose_syllable(Cho::O, Jung::EUI, Jong::None) == U'의');
    REQUIRE(compose_syllable(Cho::G, Jung::A, Jong::RG)   == U'갉');
    // 음절 표 끝점: cho=H(18), jung=I(20), jong=H(27) → 힣 (U+D7A3)
    REQUIRE(compose_syllable(Cho::H, Jung::I, Jong::H)    == U'힣');
}

TEST_CASE("combine_jung covers all .ist UnitMix vowel pairs", "[jamo][combine]") {
    // ㅗ 기반 (3종)
    REQUIRE(combine_jung(Jung::O, Jung::A)  == Jung::WA);
    REQUIRE(combine_jung(Jung::O, Jung::AE) == Jung::WAE);
    REQUIRE(combine_jung(Jung::O, Jung::I)  == Jung::OI);
    // ㅜ 기반 (3종)
    REQUIRE(combine_jung(Jung::U, Jung::EO) == Jung::UEO);
    REQUIRE(combine_jung(Jung::U, Jung::E)  == Jung::WE);
    REQUIRE(combine_jung(Jung::U, Jung::I)  == Jung::WI);
    // ㅡ 기반 (1종)
    REQUIRE(combine_jung(Jung::EU, Jung::I) == Jung::EUI);

    // 조합 불가 — UnitMix에 없는 쌍
    REQUIRE_FALSE(combine_jung(Jung::A, Jung::I).has_value());
    REQUIRE_FALSE(combine_jung(Jung::O, Jung::U).has_value());
    REQUIRE_FALSE(combine_jung(Jung::EO, Jung::A).has_value());
}

TEST_CASE("combine_virtual_jung mirrors real combinations", "[jamo][combine][virtual]") {
    // 가상 ㅗ — '/' 키
    REQUIRE(combine_virtual_jung(VJung::O, Jung::A)  == Jung::WA);
    REQUIRE(combine_virtual_jung(VJung::O, Jung::AE) == Jung::WAE);
    REQUIRE(combine_virtual_jung(VJung::O, Jung::I)  == Jung::OI);
    // 가상 ㅜ — 'o' 키
    REQUIRE(combine_virtual_jung(VJung::U, Jung::EO) == Jung::UEO);
    REQUIRE(combine_virtual_jung(VJung::U, Jung::E)  == Jung::WE);
    REQUIRE(combine_virtual_jung(VJung::U, Jung::I)  == Jung::WI);
    // 가상 ㅡ — 'i' 키
    REQUIRE(combine_virtual_jung(VJung::EU, Jung::I) == Jung::EUI);

    // 합성 안 되는 조합 — 시뮬레이터에서 j/r → 옽 같은 케이스를 만드는 원인
    REQUIRE_FALSE(combine_virtual_jung(VJung::O, Jung::EO).has_value());
    REQUIRE_FALSE(combine_virtual_jung(VJung::U, Jung::A).has_value());
    REQUIRE_FALSE(combine_virtual_jung(VJung::EU, Jung::A).has_value());
}

TEST_CASE("combine_jong builds clusters per .ist", "[jamo][combine][cluster]") {
    REQUIRE(combine_jong(Jong::G, Jong::G) == Jong::GG);
    REQUIRE(combine_jong(Jong::G, Jong::S) == Jong::GS);
    REQUIRE(combine_jong(Jong::N, Jong::J) == Jong::NJ);
    REQUIRE(combine_jong(Jong::N, Jong::H) == Jong::NH);
    REQUIRE(combine_jong(Jong::R, Jong::G) == Jong::RG);
    REQUIRE(combine_jong(Jong::R, Jong::M) == Jong::RM);
    REQUIRE(combine_jong(Jong::R, Jong::B) == Jong::RB);
    REQUIRE(combine_jong(Jong::R, Jong::S) == Jong::RS);
    REQUIRE(combine_jong(Jong::R, Jong::T) == Jong::RT);
    REQUIRE(combine_jong(Jong::R, Jong::P) == Jong::RP);
    REQUIRE(combine_jong(Jong::R, Jong::H) == Jong::RH);
    REQUIRE(combine_jong(Jong::B, Jong::S) == Jong::BS);
    REQUIRE(combine_jong(Jong::S, Jong::S) == Jong::SS);

    // 클러스터 불가
    REQUIRE_FALSE(combine_jong(Jong::G, Jong::N).has_value());
    REQUIRE_FALSE(combine_jong(Jong::M, Jong::B).has_value());
}

TEST_CASE("split_jung is the inverse of combine_jung", "[jamo][split]") {
    using J = Jung;
    for (auto [a, b, c] : std::initializer_list<std::tuple<J,J,J>>{
            {J::O,  J::A,  J::WA},
            {J::O,  J::AE, J::WAE},
            {J::O,  J::I,  J::OI},
            {J::U,  J::EO, J::UEO},
            {J::U,  J::E,  J::WE},
            {J::U,  J::I,  J::WI},
            {J::EU, J::I,  J::EUI},
        }) {
        auto split = split_jung(c);
        REQUIRE(split.has_value());
        REQUIRE(split->first  == a);
        REQUIRE(split->second == b);
    }
    REQUIRE_FALSE(split_jung(Jung::A).has_value());
    REQUIRE_FALSE(split_jung(Jung::I).has_value());
}

TEST_CASE("split_jong is the inverse of combine_jong", "[jamo][split]") {
    using J = Jong;
    for (auto [a, b, c] : std::initializer_list<std::tuple<J,J,J>>{
            {J::G, J::G, J::GG},
            {J::G, J::S, J::GS},
            {J::N, J::J, J::NJ},
            {J::N, J::H, J::NH},
            {J::R, J::G, J::RG},
            {J::R, J::M, J::RM},
            {J::R, J::B, J::RB},
            {J::R, J::S, J::RS},
            {J::R, J::T, J::RT},
            {J::R, J::P, J::RP},
            {J::R, J::H, J::RH},
            {J::B, J::S, J::BS},
            {J::S, J::S, J::SS},
        }) {
        auto split = split_jong(c);
        REQUIRE(split.has_value());
        REQUIRE(split->first  == a);
        REQUIRE(split->second == b);
    }
    REQUIRE_FALSE(split_jong(Jong::G).has_value());
    REQUIRE_FALSE(split_jong(Jong::None).has_value());
}

TEST_CASE("compatibility jamo conversion", "[jamo][compat]") {
    REQUIRE(cho_to_compat(Cho::G)  == U'ㄱ');
    REQUIRE(cho_to_compat(Cho::SS) == U'ㅆ');
    REQUIRE(cho_to_compat(Cho::H)  == U'ㅎ');

    REQUIRE(jung_to_compat(Jung::A)   == U'ㅏ');
    REQUIRE(jung_to_compat(Jung::WA)  == U'ㅘ');
    REQUIRE(jung_to_compat(Jung::EUI) == U'ㅢ');
    REQUIRE(jung_to_compat(Jung::I)   == U'ㅣ');

    REQUIRE(jong_to_compat(Jong::None) == 0);
    REQUIRE(jong_to_compat(Jong::G)    == U'ㄱ');
    REQUIRE(jong_to_compat(Jong::RG)   == U'ㄺ');
    REQUIRE(jong_to_compat(Jong::H)    == U'ㅎ');
}

TEST_CASE("virtual_to_real casts modern virtual jungs", "[jamo][virtual]") {
    REQUIRE(virtual_to_real(VJung::O)  == Jung::O);
    REQUIRE(virtual_to_real(VJung::U)  == Jung::U);
    REQUIRE(virtual_to_real(VJung::EU) == Jung::EU);
    // 옛한글 ㆍ는 현대 Jung에 대응 없음
    REQUIRE_FALSE(virtual_to_real(VJung::F).has_value());
}
