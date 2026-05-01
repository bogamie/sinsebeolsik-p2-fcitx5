#pragma once

// Hangul jamo 자료형 및 합성/분해 표.
//
// 신세벌식 P2 자동기의 토대가 되는 순수 함수 모음.
// fcitx5 의존성 없음 — 단위 테스트에서 그대로 호출 가능.
//
// 인덱스는 Unicode conjoining jamo 순서(U+1100/U+1161/U+11A7)를 따른다.
// 호환 자모(U+3131..U+318F) 변환 테이블은 별도 제공.

#include <cstdint>
#include <optional>
#include <utility>

namespace sin3p2 {

// 초성 19종
enum class Cho : uint8_t {
    G = 0,    // ㄱ
    GG = 1,   // ㄲ
    N = 2,    // ㄴ
    D = 3,    // ㄷ
    DD = 4,   // ㄸ
    R = 5,    // ㄹ
    M = 6,    // ㅁ
    B = 7,    // ㅂ
    BB = 8,   // ㅃ
    S = 9,    // ㅅ
    SS = 10,  // ㅆ
    O = 11,   // ㅇ
    J = 12,   // ㅈ
    JJ = 13,  // ㅉ
    C = 14,   // ㅊ
    K = 15,   // ㅋ
    T = 16,   // ㅌ
    P = 17,   // ㅍ
    H = 18,   // ㅎ
};

// 중성 21종 (현대 한글)
enum class Jung : uint8_t {
    A = 0,    // ㅏ
    AE = 1,   // ㅐ
    YA = 2,   // ㅑ
    YAE = 3,  // ㅒ
    EO = 4,   // ㅓ
    E = 5,    // ㅔ
    YEO = 6,  // ㅕ
    YE = 7,   // ㅖ
    O = 8,    // ㅗ
    WA = 9,   // ㅘ
    WAE = 10, // ㅙ
    OI = 11,  // ㅚ
    YO = 12,  // ㅛ
    U = 13,   // ㅜ
    UEO = 14, // ㅝ
    WE = 15,  // ㅞ
    WI = 16,  // ㅟ
    YU = 17,  // ㅠ
    EU = 18,  // ㅡ
    EUI = 19, // ㅢ
    I = 20,   // ㅣ
};

// 가상 중성 — 신세벌식 P2 갈마들이 단축 입력 전용 중간 상태.
// `/`, `i`, `o`, `p` 키가 초성-only 상태일 때 박는 임시 jung.
// UnitMix가 발동하면 Jung으로 흡수, 발동 못 하면 정착할 때 같은 모양의 Jung으로 캐스트.
//
// .ist 코드 501..504와 1:1 대응.
enum class VJung : uint8_t {
    O = 0,    // virtual ㅗ (501) — / 키
    U = 1,    // virtual ㅜ (502) — o 키
    EU = 2,   // virtual ㅡ (503) — i 키
    F = 3,    // virtual ㆍ (504, 옛한글 아래아) — p 키 (v1에선 미사용)
};

// 종성 27종 + 없음
enum class Jong : uint8_t {
    None = 0,
    G = 1,    // ㄱ
    GG = 2,   // ㄲ
    GS = 3,   // ㄳ
    N = 4,    // ㄴ
    NJ = 5,   // ㄵ
    NH = 6,   // ㄶ
    D = 7,    // ㄷ
    R = 8,    // ㄹ
    RG = 9,   // ㄺ
    RM = 10,  // ㄻ
    RB = 11,  // ㄼ
    RS = 12,  // ㄽ
    RT = 13,  // ㄾ
    RP = 14,  // ㄿ
    RH = 15,  // ㅀ
    M = 16,   // ㅁ
    B = 17,   // ㅂ
    BS = 18,  // ㅄ
    S = 19,   // ㅅ
    SS = 20,  // ㅆ
    O = 21,   // ㅇ
    J = 22,   // ㅈ
    C = 23,   // ㅊ
    K = 24,   // ㅋ
    T = 25,   // ㅌ
    P = 26,   // ㅍ
    H = 27,   // ㅎ
};

// ─── 합성 (.ist UnitMixTable) ────────────────────────────────────────────────

// 일반 중성끼리 합성 (e.g., shift+V → ㅗ, shift+F → ㅏ로 친 뒤 ㅘ)
std::optional<Jung> combine_jung(Jung a, Jung b);

// 가상 중성 + 일반 중성 (P2의 핵심 — 1키 단축 합성)
std::optional<Jung> combine_virtual_jung(VJung a, Jung b);

// 종성 클러스터 (ㄹ + ㄱ → ㄺ 등 13종)
std::optional<Jong> combine_jong(Jong a, Jong b);

// 초성 합성 (ㄱㄱ→ㄲ 등). P2에선 키 단계에서 직접 GG가 들어오는 경우가 많아 거의 안 쓰임.
std::optional<Cho> combine_cho(Cho a, Cho b);

// ─── 분해 (백스페이스용) ──────────────────────────────────────────────────────

// 합성 중성을 두 구성요소로 분해 (ㅘ → ㅗ + ㅏ)
std::optional<std::pair<Jung, Jung>> split_jung(Jung compound);

// 클러스터 종성을 두 구성요소로 분해 (ㄺ → ㄹ + ㄱ)
std::optional<std::pair<Jong, Jong>> split_jong(Jong compound);

// ─── Unicode 변환 ────────────────────────────────────────────────────────────

// 완성형 한글 음절 (U+AC00 ~ U+D7A3). jong이 None이면 받침 없는 음절.
char32_t compose_syllable(Cho c, Jung j, Jong jo);

// 호환 자모 (U+3131 ~ U+318F) — 초성/중성/종성 단독 표시용
char32_t cho_to_compat(Cho c);
char32_t jung_to_compat(Jung j);
char32_t jong_to_compat(Jong j);  // Jong::None이면 0 반환

// 가상 중성 → 일반 중성 (정착 시점에 호출)
// VJung::F (ㆍ)는 현대 한글에 대응이 없어 nullopt.
std::optional<Jung> virtual_to_real(VJung v);

}  // namespace sin3p2
