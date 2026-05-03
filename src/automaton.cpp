#include "automaton.h"

#include <type_traits>
#include <utility>

namespace sin3p2 {

// ─── State ───────────────────────────────────────────────────────────────────

bool State::empty() const noexcept {
    return !cho && !has_jung() && !jong;
}

bool State::has_jung() const noexcept {
    return !std::holds_alternative<std::monostate>(jung);
}

bool operator==(const State& a, const State& b) noexcept {
    return a.cho == b.cho && a.jung == b.jung && a.jong == b.jong;
}

// ─── 내부 헬퍼 ───────────────────────────────────────────────────────────────

namespace {

// State를 화면용 문자열로 렌더 (preedit/commit 공용).
//   cho + (real/virtual) jung [+ jong] 모두 있으면:
//     - 현대 Jung → 완성형 음절 한 글자 (U+AC00..)
//     - 옛한글 Jung (F/FI/FF) → conjoining 시퀀스 (precomposed 없음)
//   부족하면 호환 자모를 이어 붙인 분리 표시.
//   가상 중성은 표시 시점에 실제로 캐스트.
std::u32string render(const State& s) {
    if (s.empty()) return U"";

    std::optional<Jung> jung_view;
    if (auto* rj = std::get_if<Jung>(&s.jung)) {
        jung_view = *rj;
    } else if (auto* vj = std::get_if<VJung>(&s.jung)) {
        jung_view = virtual_to_real(*vj);
    }

    if (s.cho && jung_view) {
        if (is_modern_jung(*jung_view)) {
            const Jong jo = s.jong.value_or(Jong::None);
            return std::u32string(1, compose_syllable(*s.cho, *jung_view, jo));
        }
        // 옛한글: conjoining 시퀀스로 조립
        std::u32string out;
        out.push_back(cho_to_conjoining(*s.cho));
        out.push_back(jung_to_conjoining(*jung_view));
        if (s.jong) out.push_back(jong_to_conjoining(*s.jong));
        return out;
    }

    // 부분 음절 — 호환 자모 분리 표시
    std::u32string out;
    if (s.cho)      out.push_back(cho_to_compat(*s.cho));
    if (jung_view) {
        // 호환 자모가 있으면(현대/F/FI) 그대로, 없으면(FF) cho-filler+conjoining으로 폴백
        const char32_t cp = jung_to_compat(*jung_view);
        if (cp != 0) {
            out.push_back(cp);
        } else {
            if (!s.cho) out.push_back(0x115F);  // cho filler
            out.push_back(jung_to_conjoining(*jung_view));
        }
    }
    if (s.jong)     out.push_back(jong_to_compat(*s.jong));
    return out;
}

// 가상 중성이면 실제로 in-place 캐스트 (jong 추가 시점, 클러스터 시점 등에 호출).
// 실제로 변환됐으면 true 반환 — 호출자가 jung_was_virtual 플래그를 켤 수 있도록.
bool freeze_virtual_jung(State& s) {
    if (auto* vj = std::get_if<VJung>(&s.jung)) {
        if (auto rj = virtual_to_real(*vj)) {
            s.jung = *rj;
            return true;
        }
    }
    return false;
}

StepResult apply_cho(const State& s, Cho c) {
    StepResult r;

    // 초성-only 상태에서 같은 초성 다시 → 쌍자음 (UnitMix CHO)
    //   ㄱㄱ→ㄲ, ㄷㄷ→ㄸ, ㅂㅂ→ㅃ, ㅅㅅ→ㅆ, ㅈㅈ→ㅉ
    if (s.cho && !s.has_jung() && !s.jong) {
        if (auto doubled = combine_cho(*s.cho, c)) {
            r.state = s;
            r.state.cho = *doubled;
            r.preedit = render(r.state);
            return r;
        }
    }

    if (!s.empty()) {
        r.commit = render(s);
    }
    r.state.cho = c;
    r.preedit = render(r.state);
    return r;
}

StepResult apply_jung(const State& s, Jung j) {
    StepResult r;

    // apply_jung이 만드는 jung은 어떤 분기든 fresh freeze가 아니다 — 플래그는 항상 false.
    // (단순 입력 / real+real 합성 / virtual+real 합성 모두 BS는 split_jung 경로로 처리.)

    // 가상 중성 위에 실제 중성이 오면 합성 시도
    if (auto* vj = std::get_if<VJung>(&s.jung)) {
        if (auto compound = combine_virtual_jung(*vj, j)) {
            r.state = s;
            r.state.jung = *compound;
            r.state.jung_was_virtual = false;
            r.preedit = render(r.state);
            return r;
        }
        // 정상 흐름이라면 keymap이 막아줘서 여기 안 와야 함.
        // 안전장치: 가상→실제 캐스트로 commit, 새 state는 cho 없는 standalone jung.
        State commit_state = s;
        freeze_virtual_jung(commit_state);
        r.commit = render(commit_state);
        r.state.jung = j;
        r.preedit = render(r.state);
        return r;
    }

    // 실제 중성 위에 실제 중성이 오면 UnitMix 시도
    if (auto* rj = std::get_if<Jung>(&s.jung)) {
        if (auto compound = combine_jung(*rj, j)) {
            r.state = s;
            r.state.jung = *compound;
            r.state.jung_was_virtual = false;
            r.preedit = render(r.state);
            return r;
        }
    }

    // jung 없는 상태(state 0/1): 그냥 추가
    if (!s.has_jung() && !s.jong) {
        r.state = s;
        r.state.jung = j;
        r.state.jung_was_virtual = false;
        r.preedit = render(r.state);
        return r;
    }

    // state 2 (합성 실패) 또는 state 3 — 기존 음절 commit, standalone jung으로 새 음절 시작
    r.commit = render(s);
    r.state.jung = j;
    r.preedit = render(r.state);
    return r;
}

StepResult apply_vjung(const State& s, VJung vj) {
    StepResult r;

    // 정상 흐름: 초성-only 상태에서만 가상 중성이 들어옴 (keymap의 D&&!E&&!F 조건).
    // 그 외 상태면 안전하게 commit하고 새로 시작.
    if (s.has_jung() || s.jong) {
        r.commit = render(s);
    } else {
        r.state.cho = s.cho;
    }
    r.state.jung = vj;
    r.preedit = render(r.state);
    return r;
}

StepResult apply_jong(const State& s, Jong jo) {
    StepResult r;
    r.state = s;

    // 클러스터 합성은 진행 중인 음절(cho 있음) 위에서만 — cho 없이 떠 있는
    // standalone jong은 시뮬상 누적되지 않는다 (qq=ㅅㅅ, ㅆ 아님).
    if (s.jong) {
        if (s.cho) {
            if (auto cluster = combine_jong(*s.jong, jo)) {
                r.state.jong = *cluster;
                r.state.jong_combined = true;  // 두 키스트로크가 합쳐짐 → BS는 분해
                // 첫 jong 시점에 이미 freeze됐으므로 여기선 보통 no-op.
                // jung_was_virtual 플래그는 그대로 유지 (BS 통째 제거 시 복귀 신호).
                freeze_virtual_jung(r.state);
                r.preedit = render(r.state);
                return r;
            }
        }
        // 클러스터 불가 (실패 또는 cho 없음) — commit + standalone jong
        r.commit = render(s);
        r.state = State{};
        r.state.jong = jo;
        r.state.jong_combined = false;
        r.preedit = render(r.state);
        return r;
    }

    // 첫 jong: 가상 중성이 있다면 이 시점에 실제로 정착.
    // 실제로 freeze가 일어났을 때만 jung_was_virtual=true — BS로 jong 제거 시
    // real로 박힌 jung(예: jvf의 ㅗ)은 그대로 두고, freeze된 것(예: jof의 ㅜ)만 가상 복귀.
    if (freeze_virtual_jung(r.state)) {
        r.state.jung_was_virtual = true;
    }
    r.state.jong = jo;
    r.state.jong_combined = false;  // 단일 키 입력 → BS는 통째로 제거
    r.preedit = render(r.state);
    return r;
}

}  // namespace

// ─── 공개 API ────────────────────────────────────────────────────────────────

StepResult step(const State& s, const Input& in) {
    return std::visit([&](auto&& v) -> StepResult {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, InputCho>)        return apply_cho  (s, v.value);
        else if constexpr (std::is_same_v<T, InputJung>)  return apply_jung (s, v.value);
        else if constexpr (std::is_same_v<T, InputVJung>) return apply_vjung(s, v.value);
        else                                              return apply_jong (s, v.value);
    }, in);
}

StepResult backspace(const State& s) {
    StepResult r;
    r.state = s;

    if (s.jong) {
        // 두 키스트로크가 combine_jong으로 합쳐진 cluster jong만 BS로 분해.
        // 한 키 입력으로 박힌 jong (단독 jong, 또는 keymap이 jong=SS 같이 직접 cluster
        // 값을 박은 경우)은 통째로 제거한다 — 한 키 = 한 BS.
        if (s.jong_combined) {
            if (auto split = split_jong(*s.jong)) {
                r.state.jong = split->first;
                r.state.jong_combined = false;  // 분해 후 남은 jong은 단일 단위
            } else {
                // jong_combined=true면 split이 항상 성공하지만 안전장치.
                r.state.jong = std::nullopt;
                r.state.jong_combined = false;
            }
        } else {
            r.state.jong = std::nullopt;
            r.state.jong_combined = false;
            // jong 부착 시점에 freeze된 가상 중성만 BS로 복귀시킨다.
            // freeze 이력은 jung_was_virtual로 추적 — true면 가상으로 환원해 다음 키
            // 입력이 합성을 다시 트리거할 수 있게 한다.
            // (jof → 웊 → BS → 우(가상ㅜ) → c → 웨; jvf → 옾 → BS → 오(real) → f → 옾)
            if (s.jung_was_virtual) {
                if (auto* rj = std::get_if<Jung>(&r.state.jung)) {
                    switch (*rj) {
                        case Jung::O:  r.state.jung = VJung::O;  break;
                        case Jung::U:  r.state.jung = VJung::U;  break;
                        case Jung::EU: r.state.jung = VJung::EU; break;
                        case Jung::F:  r.state.jung = VJung::F;  break;
                        default: break;
                    }
                }
                r.state.jung_was_virtual = false;
            }
        }
    } else if (auto* rj = std::get_if<Jung>(&s.jung)) {
        if (auto split = split_jung(*rj)) {
            // 합성 모음 분해 — 첫 부분(ㅗ/ㅜ/ㅡ/옛한글ㆍ)을 가상 중성으로 복귀시켜
            // 동일 키 재입력 시 합성이 다시 일어나도록 한다.
            // (joc → 웨 → BS → cho=O+가상ㅜ → c → 웨)
            // (jpz → ᄋᆢ(FF) → BS → cho=ㅇ+가상ㆍ → z → ᄋᆢ)
            switch (split->first) {
                case Jung::O:  r.state.jung = VJung::O;  break;
                case Jung::U:  r.state.jung = VJung::U;  break;
                case Jung::EU: r.state.jung = VJung::EU; break;
                case Jung::F:  r.state.jung = VJung::F;  break;
                default:       r.state.jung = split->first; break;
            }
        } else {
            r.state.jung = std::monostate{};
        }
    } else if (std::holds_alternative<VJung>(s.jung)) {
        r.state.jung = std::monostate{};
    } else if (s.cho) {
        r.state.cho = std::nullopt;
    }
    // s.empty() — 호스트 영역 BS로 위임, state 그대로

    r.preedit = render(r.state);
    return r;
}

StepResult flush(const State& s) {
    StepResult r;
    r.commit = render(s);
    return r;
}

std::u32string render_preedit(const State& s) {
    return render(s);
}

}  // namespace sin3p2
