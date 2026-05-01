#pragma once

// 신세벌식 P2 키맵 — QWERTY printable → 자동기 입력(Input) 변환.
//
// 입력은 ASCII printable code point (대소문자가 shift 여부를 가름).
// 자동기 state는 갈마들이 조건 평가에 필요해서 받는다.
//
// 반환값:
//   std::optional<Input> = 자모 입력으로 해석된 결과
//   std::nullopt          = Hangul-mapped이 아님 → 호스트로 passthrough
//
// 호출 측(engine)이 nullopt를 받으면 자동기 state를 flush하고 원래 키를 호스트에 넘긴다.
//
// 미지원 (현 단계):
//   - 옛한글 ㆍ (z lowercase 갈마, Z uppercase) — 현대 한글 음절 합성에 못 끼어듦
//   - 기호 치환 (shift+J→', shift+L→· 등) — v1에선 그냥 passthrough
//
// 참고: 키 조건식의 진리값은 모두 .ist 파일의 ternary 그대로.
//   D = state.cho.has_value()
//   E = state.has_jung()
//   F = state.jong.has_value()
//   E==0x1F5 = state.jung == VJung::O
//   E==0x1F6 = state.jung == VJung::U
//   E in [0x1F5..0x1F8] = any virtual jung

#include <optional>

#include "automaton.h"

namespace sin3p2 {

// QWERTY printable 한 문자(대소문자 구분) + 현재 자동기 state → 자모 입력
std::optional<Input> translate_p2(char32_t qwerty_key, const State& state);

}  // namespace sin3p2
