#include "keymap.h"

#include <variant>

namespace sin3p2 {

namespace {

// state 술어 (.ist의 D/E/F)
struct Predicates {
    bool D;          // cho 있음
    bool E;          // jung 있음 (real or virtual)
    bool F;          // jong 있음
    bool E_v_O;      // jung == VJung::O
    bool E_v_U;      // jung == VJung::U
    bool E_v_EU;     // jung == VJung::EU
    bool E_v_F;      // jung == VJung::F
    bool E_any_v;    // jung is any VJung
};

Predicates predicates_of(const State& s) {
    Predicates p{};
    p.D = s.cho.has_value();
    p.E = s.has_jung();
    p.F = s.jong.has_value();
    if (auto* vj = std::get_if<VJung>(&s.jung)) {
        p.E_any_v = true;
        p.E_v_O   = (*vj == VJung::O);
        p.E_v_U   = (*vj == VJung::U);
        p.E_v_EU  = (*vj == VJung::EU);
        p.E_v_F   = (*vj == VJung::F);
    }
    return p;
}

}  // namespace

std::optional<Input> translate_p2(char32_t k, const State& s) {
    const auto p = predicates_of(s);

    switch (k) {
        // ─── 오른손 lowercase = 항상 초성 ───────────────────────────────────
        case U'h': return InputCho{Cho::N};   // ㄴ
        case U'j': return InputCho{Cho::O};   // ㅇ ( .ist Q_ )
        case U'k': return InputCho{Cho::G};   // ㄱ
        case U'l': return InputCho{Cho::J};   // ㅈ
        case U'm': return InputCho{Cho::H};   // ㅎ
        case U'n': return InputCho{Cho::S};   // ㅅ
        case U'u': return InputCho{Cho::D};   // ㄷ
        case U'y': return InputCho{Cho::R};   // ㄹ
        case U';': return InputCho{Cho::B};   // ㅂ
        case U'\'':return InputCho{Cho::T};   // ㅌ

        // ─── 오른손 lowercase + 가상 중성 단축 ───────────────────────────────
        // /  : D&&!E&&!F → virtual ㅗ ; else ㅋ choseong
        case U'/':
            if (p.D && !p.E && !p.F) return InputVJung{VJung::O};
            return InputCho{Cho::K};
        // i  : D&&!E&&!F → virtual ㅡ ; else ㅁ choseong
        case U'i':
            if (p.D && !p.E && !p.F) return InputVJung{VJung::EU};
            return InputCho{Cho::M};
        // o  : D&&!E&&!F → virtual ㅜ ; else ㅊ choseong
        case U'o':
            if (p.D && !p.E && !p.F) return InputVJung{VJung::U};
            return InputCho{Cho::C};
        // p  : D&&!E&&!F → virtual ㆍ ; else ㅍ choseong
        // (가상 ㆍ는 옛한글 — v1에선 정상 흐름에 안 쓰지만 충실히 둠)
        case U'p':
            if (p.D && !p.E && !p.F) return InputVJung{VJung::F};
            return InputCho{Cho::P};

        // ─── 왼손 lowercase = 갈마들이 (jung when D&&!E [+조건], else jong) ─
        // q : D&&!E → ㅒ ; else ㅅ jong
        case U'q':
            if (p.D && !p.E) return InputJung{Jung::YAE};
            return InputJong{Jong::S};
        // w : D&&!E → ㅑ ; else ㄹ jong
        case U'w':
            if (p.D && !p.E) return InputJung{Jung::YA};
            return InputJong{Jong::R};
        // e : D&&!E or E==v_O → ㅐ ; else ㅂ jong
        case U'e':
            if ((p.D && !p.E) || p.E_v_O) return InputJung{Jung::AE};
            return InputJong{Jong::B};
        // r : D&&!E or E==v_U → ㅓ ; else ㅌ jong
        case U'r':
            if ((p.D && !p.E) || p.E_v_U) return InputJung{Jung::EO};
            return InputJong{Jong::T};
        // t : D&&!E → ㅕ ; else ㅋ jong
        case U't':
            if (p.D && !p.E) return InputJung{Jung::YEO};
            return InputJong{Jong::K};
        // a : D&&!E → ㅠ ; else ㅇ jong
        case U'a':
            if (p.D && !p.E) return InputJung{Jung::YU};
            return InputJong{Jong::O};
        // s : D&&!E → ㅖ ; else ㄴ jong
        case U's':
            if (p.D && !p.E) return InputJung{Jung::YE};
            return InputJong{Jong::N};
        // d : D&&!E or any virtual jung → ㅣ ; else ㅎ jong
        case U'd':
            if ((p.D && !p.E) || p.E_any_v) return InputJung{Jung::I};
            return InputJong{Jong::H};
        // f : D&&!E or E==v_O → ㅏ ; else ㅍ jong
        case U'f':
            if ((p.D && !p.E) || p.E_v_O) return InputJung{Jung::A};
            return InputJong{Jong::P};
        // g : D&&!E → ㅡ ; else ㄷ jong
        case U'g':
            if (p.D && !p.E) return InputJung{Jung::EU};
            return InputJong{Jong::D};
        // z : D&&!E or E==v_F → ㆍ ; else ㅁ jong
        // (ㆍ jung은 v1 미지원 — 그 분기는 nullopt로 떨어트려 passthrough)
        case U'z':
            if ((p.D && !p.E) || p.E_v_F) return std::nullopt;  // ㆍ 미지원
            return InputJong{Jong::M};
        // x : D&&!E → ㅛ ; else ㅆ jong
        case U'x':
            if (p.D && !p.E) return InputJung{Jung::YO};
            return InputJong{Jong::SS};
        // c : D&&!E or E==v_U → ㅔ ; else ㄱ jong
        case U'c':
            if ((p.D && !p.E) || p.E_v_U) return InputJung{Jung::E};
            return InputJong{Jong::G};
        // v : D&&!E → ㅗ ; else ㅈ jong
        case U'v':
            if (p.D && !p.E) return InputJung{Jung::O};
            return InputJong{Jong::J};
        // b : D&&!E → ㅜ ; else ㅊ jong
        case U'b':
            if (p.D && !p.E) return InputJung{Jung::U};
            return InputJong{Jong::C};

        // ─── 왼손 uppercase = 무조건 jung ────────────────────────────────────
        case U'A': return InputJung{Jung::YU};
        case U'B': return InputJung{Jung::U};
        case U'C': return InputJung{Jung::E};
        case U'D': return InputJung{Jung::I};
        case U'E': return InputJung{Jung::AE};
        case U'F': return InputJung{Jung::A};
        case U'G': return InputJung{Jung::EU};
        case U'Q': return InputJung{Jung::YAE};
        case U'R': return InputJung{Jung::EO};
        case U'S': return InputJung{Jung::YE};
        case U'T': return InputJung{Jung::YEO};
        case U'V': return InputJung{Jung::O};
        case U'W': return InputJung{Jung::YA};
        case U'X': return InputJung{Jung::YO};
        // Z (shift) → ㆍ (옛한글 미지원)
        case U'Z': return std::nullopt;

        // ─── 오른손 uppercase = 기호 치환 (v1: passthrough) ──────────────────
        // H→□, J→', K→", L→·, M→…, N→―, U→○, Y→×, P→;
        // (engine이 nullopt를 받아 그냥 호스트로 보냄)

        default:
            return std::nullopt;
    }
}

}  // namespace sin3p2
