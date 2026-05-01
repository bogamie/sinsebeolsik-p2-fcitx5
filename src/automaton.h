#pragma once

// 한글 자동기 (state machine).
//
// 입력은 이미 keymap이 KeySym을 자모 단위로 해석한 뒤 도착한다고 가정한다.
// 자동기는 순수 함수: (state, input) -> (new_state, commit_text, preedit_text).
// fcitx5 의존성 없음 — 단위 테스트로 그대로 검증 가능.
//
// 신세벌식 P2의 핵심 동작 (시뮬레이터로 검증된 것):
//   1. 도깨비불 없음. 종성 → 모음 입력 시 종성 옮기지 않고 commit + 새 standalone jung.
//   2. 자동 ㅇ 보충 없음. 모음만 따로 들어오면 호환 자모로 노출.
//   3. 가상 중성 (VJung) — / i o p 키가 초성-only 상태에서만 박는 임시 jung.
//      다음 입력이 합성 가능한 jung이면 합성 후 실제 jung 1개로 정착.
//      합성 실패면 가상→실제 캐스트 후 입력은 자기 갈마 조건대로 jong이 됨.
//   4. 백스페이스 — preedit 안에서 자모 한 단계씩, 복합 모음/자음은 분해.

#include <optional>
#include <string>
#include <variant>

#include "jamo.h"

namespace sin3p2 {

// 자동기 상태 — 진행 중인 한 음절
struct State {
    std::optional<Cho> cho;
    // jung 슬롯: 비어있거나 / 일반 Jung / 가상 VJung 중 하나
    std::variant<std::monostate, Jung, VJung> jung;
    std::optional<Jong> jong;

    bool empty() const noexcept;
    bool has_jung() const noexcept;
};

bool operator==(const State& a, const State& b) noexcept;

// 자동기 입력 — keymap이 KeySym + 현재 state를 보고 결정한 자모 1개
struct InputCho   { Cho   value; };
struct InputJung  { Jung  value; };
struct InputVJung { VJung value; };
struct InputJong  { Jong  value; };
using Input = std::variant<InputCho, InputJung, InputVJung, InputJong>;

// 한 단계 결과
struct StepResult {
    State state;             // 변환 후 state
    std::u32string commit;   // 이번 단계에서 호스트 앱으로 commit되는 텍스트
    std::u32string preedit;  // 변환 후 state의 preedit 표시 (호스트가 보여줄 진행 중 음절)
};

// 입력 한 개를 적용
StepResult step(const State& s, const Input& in);

// 백스페이스 — preedit 안에서 자모 한 단계 분해
// 복합 모음 (ㅘ → ㅗ), 클러스터 종성 (ㄺ → ㄹ), 가상 중성, 단일 자모를 단계적으로 제거.
// state가 비어있으면 그대로 반환 (호출 측이 호스트 앱 영역의 BS로 위임).
StepResult backspace(const State& s);

// 진행 중 음절을 강제로 commit (포커스 잃음, 비-한글 키 등)
StepResult flush(const State& s);

// state를 표시용 문자열로 렌더 (preedit 영역에 그릴 내용)
std::u32string render_preedit(const State& s);

}  // namespace sin3p2
