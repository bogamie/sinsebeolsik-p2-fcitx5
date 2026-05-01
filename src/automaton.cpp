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
//   cho + (real/virtual) jung [+ jong] 모두 있으면 완성형 음절 한 글자.
//   부족하면 호환 자모를 이어 붙인 분리 표시.
//   가상 중성은 표시 시점에 실제로 캐스트.
std::u32string render(const State& s) {
    if (s.empty()) return U"";

    std::optional<Jung> jung_view;
    if (auto* rj = std::get_if<Jung>(&s.jung)) {
        jung_view = *rj;
    } else if (auto* vj = std::get_if<VJung>(&s.jung)) {
        jung_view = virtual_to_real(*vj);
        // VJung::F (옛한글 ㆍ)는 현대 Jung에 대응 없음 → 분리 표시로 떨어짐
    }

    if (s.cho && jung_view) {
        const Jong jo = s.jong.value_or(Jong::None);
        return std::u32string(1, compose_syllable(*s.cho, *jung_view, jo));
    }

    std::u32string out;
    if (s.cho)      out.push_back(cho_to_compat(*s.cho));
    if (jung_view)  out.push_back(jung_to_compat(*jung_view));
    if (s.jong)     out.push_back(jong_to_compat(*s.jong));
    return out;
}

// 가상 중성이면 실제로 in-place 캐스트 (jong 추가 시점, 클러스터 시점 등에 호출)
void freeze_virtual_jung(State& s) {
    if (auto* vj = std::get_if<VJung>(&s.jung)) {
        if (auto rj = virtual_to_real(*vj)) {
            s.jung = *rj;
        }
    }
}

StepResult apply_cho(const State& s, Cho c) {
    StepResult r;
    if (!s.empty()) {
        r.commit = render(s);
    }
    r.state.cho = c;
    r.preedit = render(r.state);
    return r;
}

StepResult apply_jung(const State& s, Jung j) {
    StepResult r;

    // 가상 중성 위에 실제 중성이 오면 합성 시도
    if (auto* vj = std::get_if<VJung>(&s.jung)) {
        if (auto compound = combine_virtual_jung(*vj, j)) {
            r.state = s;
            r.state.jung = *compound;
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
            r.preedit = render(r.state);
            return r;
        }
    }

    // jung 없는 상태(state 0/1): 그냥 추가
    if (!s.has_jung() && !s.jong) {
        r.state = s;
        r.state.jung = j;
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

    // 클러스터 합성
    if (s.jong) {
        if (auto cluster = combine_jong(*s.jong, jo)) {
            r.state.jong = *cluster;
            freeze_virtual_jung(r.state);
            r.preedit = render(r.state);
            return r;
        }
        // 클러스터 실패 — commit + standalone jong
        r.commit = render(s);
        r.state = State{};
        r.state.jong = jo;
        r.preedit = render(r.state);
        return r;
    }

    // 첫 jong: 가상 중성이 있다면 이 시점에 실제로 정착
    freeze_virtual_jung(r.state);
    r.state.jong = jo;
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
        if (auto split = split_jong(*s.jong)) {
            r.state.jong = split->first;
        } else {
            r.state.jong = std::nullopt;
        }
    } else if (auto* rj = std::get_if<Jung>(&s.jung)) {
        if (auto split = split_jung(*rj)) {
            r.state.jung = split->first;
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
