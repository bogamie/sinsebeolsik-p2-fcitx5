#include "jamo.h"

#include <array>

namespace sin3p2 {

// ─── 합성 ────────────────────────────────────────────────────────────────────
//
// 출처: 팥(pat.im) 신세벌식 P2 .ist UnitMixTable
//   <UnitMix unit="JUNG" a="O_" b="A_" to="WA"/>     ㅗ + ㅏ → ㅘ
//   <UnitMix unit="JUNG" a="O_" b="AE" to="WAE"/>    ㅗ + ㅐ → ㅙ
//   <UnitMix unit="JUNG" a="O_" b="I_" to="OI"/>     ㅗ + ㅣ → ㅚ
//   <UnitMix unit="JUNG" a="U_" b="EO" to="UEO"/>    ㅜ + ㅓ → ㅝ
//   <UnitMix unit="JUNG" a="U_" b="E_" to="WE"/>     ㅜ + ㅔ → ㅞ
//   <UnitMix unit="JUNG" a="U_" b="I_" to="WI"/>     ㅜ + ㅣ → ㅟ
//   <UnitMix unit="JUNG" a="EU" b="I_" to="EUI"/>    ㅡ + ㅣ → ㅢ
// (.ist의 a="501..504" 가상 중성 항목도 같은 매핑 — combine_virtual_jung에서 처리)

std::optional<Jung> combine_jung(Jung a, Jung b) {
    using J = Jung;
    if (a == J::O) {
        if (b == J::A)  return J::WA;
        if (b == J::AE) return J::WAE;
        if (b == J::I)  return J::OI;
    } else if (a == J::U) {
        if (b == J::EO) return J::UEO;
        if (b == J::E)  return J::WE;
        if (b == J::I)  return J::WI;
    } else if (a == J::EU) {
        if (b == J::I)  return J::EUI;
    } else if (a == J::F) {
        // 옛한글: F + I → ㆎ, F + F → 쌍아래아
        if (b == J::I)  return J::FI;
        if (b == J::F)  return J::FF;
    }
    return std::nullopt;
}

std::optional<Jung> combine_virtual_jung(VJung a, Jung b) {
    using J = Jung;
    switch (a) {
        case VJung::O:
            if (b == J::A)  return J::WA;
            if (b == J::AE) return J::WAE;
            if (b == J::I)  return J::OI;
            break;
        case VJung::U:
            if (b == J::EO) return J::UEO;
            if (b == J::E)  return J::WE;
            if (b == J::I)  return J::WI;
            break;
        case VJung::EU:
            if (b == J::I)  return J::EUI;
            break;
        case VJung::F:
            // .ist UnitMix: 504 + I_ → FI, 504 + F_ → FF
            if (b == J::I) return J::FI;
            if (b == J::F) return J::FF;
            break;
    }
    return std::nullopt;
}

std::optional<Jong> combine_jong(Jong a, Jong b) {
    using J = Jong;
    if (a == J::G && b == J::G) return J::GG;
    if (a == J::G && b == J::S) return J::GS;
    if (a == J::N && b == J::J) return J::NJ;
    if (a == J::N && b == J::H) return J::NH;
    if (a == J::R && b == J::G) return J::RG;
    if (a == J::R && b == J::M) return J::RM;
    if (a == J::R && b == J::B) return J::RB;
    if (a == J::R && b == J::S) return J::RS;
    if (a == J::R && b == J::T) return J::RT;
    if (a == J::R && b == J::P) return J::RP;
    if (a == J::R && b == J::H) return J::RH;
    if (a == J::B && b == J::S) return J::BS;
    if (a == J::S && b == J::S) return J::SS;
    return std::nullopt;
}

std::optional<Cho> combine_cho(Cho a, Cho b) {
    using C = Cho;
    if (a == C::G && b == C::G) return C::GG;
    if (a == C::D && b == C::D) return C::DD;
    if (a == C::B && b == C::B) return C::BB;
    if (a == C::S && b == C::S) return C::SS;
    if (a == C::J && b == C::J) return C::JJ;
    return std::nullopt;
}

// ─── 분해 ────────────────────────────────────────────────────────────────────

std::optional<std::pair<Jung, Jung>> split_jung(Jung compound) {
    using J = Jung;
    switch (compound) {
        case J::WA:  return std::pair{J::O,  J::A};
        case J::WAE: return std::pair{J::O,  J::AE};
        case J::OI:  return std::pair{J::O,  J::I};
        case J::UEO: return std::pair{J::U,  J::EO};
        case J::WE:  return std::pair{J::U,  J::E};
        case J::WI:  return std::pair{J::U,  J::I};
        case J::EUI: return std::pair{J::EU, J::I};
        case J::FI:  return std::pair{J::F,  J::I};
        case J::FF:  return std::pair{J::F,  J::F};
        default:     return std::nullopt;
    }
}

std::optional<std::pair<Jong, Jong>> split_jong(Jong compound) {
    using J = Jong;
    switch (compound) {
        case J::GG: return std::pair{J::G, J::G};
        case J::GS: return std::pair{J::G, J::S};
        case J::NJ: return std::pair{J::N, J::J};
        case J::NH: return std::pair{J::N, J::H};
        case J::RG: return std::pair{J::R, J::G};
        case J::RM: return std::pair{J::R, J::M};
        case J::RB: return std::pair{J::R, J::B};
        case J::RS: return std::pair{J::R, J::S};
        case J::RT: return std::pair{J::R, J::T};
        case J::RP: return std::pair{J::R, J::P};
        case J::RH: return std::pair{J::R, J::H};
        case J::BS: return std::pair{J::B, J::S};
        case J::SS: return std::pair{J::S, J::S};
        default:    return std::nullopt;
    }
}

// ─── Unicode 변환 ────────────────────────────────────────────────────────────

// 한글 음절 합성 공식: U+AC00 + (cho * 21 + jung) * 28 + jong.
// 호출자는 is_modern_jung(j)가 true임을 보장해야 한다 — 옛한글 자모(F/FI/FF)에는
// precomposed 음절이 없으므로 conjoining 시퀀스(_to_conjoining 계열)로 직접 조립할 것.
char32_t compose_syllable(Cho c, Jung j, Jong jo) {
    return 0xAC00u + (static_cast<uint32_t>(c) * 21u
                    + static_cast<uint32_t>(j)) * 28u
                    + static_cast<uint32_t>(jo);
}

char32_t cho_to_compat(Cho c) {
    static constexpr std::array<char32_t, 19> table = {
        0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141,
        0x3142, 0x3143, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149,
        0x314A, 0x314B, 0x314C, 0x314D, 0x314E,
    };
    return table[static_cast<size_t>(c)];
}

char32_t jung_to_compat(Jung j) {
    // 현대 21종은 ㅏ U+314F .. ㅣ U+3163과 인덱스 1:1.
    // 옛한글: ㆍ U+318D, ㆎ U+318E. FF(쌍아래아)는 호환 자모 없음 → 0.
    switch (j) {
        case Jung::F:  return 0x318D;
        case Jung::FI: return 0x318E;
        case Jung::FF: return 0;
        default:       return 0x314Fu + static_cast<uint32_t>(j);
    }
}

char32_t jong_to_compat(Jong j) {
    if (j == Jong::None) return 0;
    static constexpr std::array<char32_t, 28> table = {
        0,      // None
        0x3131, // ㄱ
        0x3132, // ㄲ
        0x3133, // ㄳ
        0x3134, // ㄴ
        0x3135, // ㄵ
        0x3136, // ㄶ
        0x3137, // ㄷ
        0x3139, // ㄹ
        0x313A, // ㄺ
        0x313B, // ㄻ
        0x313C, // ㄼ
        0x313D, // ㄽ
        0x313E, // ㄾ
        0x313F, // ㄿ
        0x3140, // ㅀ
        0x3141, // ㅁ
        0x3142, // ㅂ
        0x3144, // ㅄ
        0x3145, // ㅅ
        0x3146, // ㅆ
        0x3147, // ㅇ
        0x3148, // ㅈ
        0x314A, // ㅊ
        0x314B, // ㅋ
        0x314C, // ㅌ
        0x314D, // ㅍ
        0x314E, // ㅎ
    };
    return table[static_cast<size_t>(j)];
}

// Conjoining jamo — 옛한글 음절(precomposed 없음) 조립용. cho U+1100.., jung U+1161..,
// jong U+11A8.. 가 표준 인덱스. 옛한글 jung은 별도 코드포인트(U+119E/U+11A1/U+11A2).
char32_t cho_to_conjoining(Cho c) {
    return 0x1100u + static_cast<uint32_t>(c);
}

char32_t jung_to_conjoining(Jung j) {
    switch (j) {
        case Jung::F:  return 0x119E;
        case Jung::FI: return 0x11A1;
        case Jung::FF: return 0x11A2;
        default:       return 0x1161u + static_cast<uint32_t>(j);
    }
}

char32_t jong_to_conjoining(Jong j) {
    if (j == Jong::None) return 0;
    // Jong 인덱스 1..27 → U+11A8..U+11C2 (1:1 대응)
    return 0x11A7u + static_cast<uint32_t>(j);
}

std::optional<Jung> virtual_to_real(VJung v) {
    switch (v) {
        case VJung::O:  return Jung::O;
        case VJung::U:  return Jung::U;
        case VJung::EU: return Jung::EU;
        case VJung::F:  return Jung::F;
    }
    return std::nullopt;
}

}  // namespace sin3p2
