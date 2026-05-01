#pragma once

#include <cstdint>

namespace sin3p2 {

// 호스트 시스템 XKB 레이아웃이 무엇이든(Canary, Dvorak 등) 들어온 하드웨어
// keycode를 항상 US QWERTY 기준으로 해석해 keysym을 돌려준다.
//
// 우리의 P2 keymap은 QWERTY 위치 기준으로 짜여 있어서, 사용자가 시스템 XKB를
// 비-QWERTY 레이아웃으로 쓰는 경우(예: 영문 입력은 Canary로) 엔진이 받는
// keysym은 이미 그 레이아웃으로 변환된 값이라 매핑이 다 깨진다. keycode는
// 레이아웃-불변(물리 위치 ID)이므로, 자체 xkb_state 위에서 다시 해석한다.
//
// libxkbcommon 의존. 시스템에 xkeyboard-config가 설치돼 있어야 함
// (Linux 데스크탑에선 필수 패키지라 사실상 항상 있음). 초기화 실패 시
// translate()는 fallback_sym을 그대로 반환 — 즉 시스템 XKB가 QWERTY인
// 정상 환경에서는 동작 차이 없음.
class QwertyTranslator {
public:
    QwertyTranslator();
    ~QwertyTranslator();
    QwertyTranslator(const QwertyTranslator&) = delete;
    QwertyTranslator& operator=(const QwertyTranslator&) = delete;
    QwertyTranslator(QwertyTranslator&&) = delete;
    QwertyTranslator& operator=(QwertyTranslator&&) = delete;

    // 초기화(xkb context + US 키맵 + state) 성공 여부.
    bool ok() const noexcept;

    // keycode = X11 컨벤션 (evdev keycode + 8). fcitx5의 Key::code()가 이 값.
    // shift = Shift modifier 눌림.
    // 미지원 keycode 또는 초기화 실패 시 fallback_sym을 그대로 반환.
    std::uint32_t translate(int keycode, bool shift,
                            std::uint32_t fallback_sym) const;

private:
    struct State;
    State* s_;
};

}  // namespace sin3p2
