# INT5 Passthrough Modifier

신세벌식 P2가 자모로 가로채는 글자 (`;`, `'`, `/`, `"`)를 한글 모드에서도 호스트에 그대로 박을 수 있게 해주는 우회 메커니즘.

## 왜 필요한가

P2 keymap에서 `;` → ㅂ cho, `'` → ㅌ cho, `/` → ㅋ cho/가상 ㅗ로 매핑되어 있다. 이는 신세벌식 P2의 핵심 자모 입력 경로이므로 keymap 수정으로는 풀 수 없다 (수정하면 ㅂ/ㅌ/ㅋ 입력 자체가 망가짐).

영문 모드 토글은 작동하지만 토글 누적 어긋남 문제가 있다 (사용자가 수동으로 IME 상태를 바꾼 경우 ZMK의 토글 시퀀스와 어긋남). INT5 passthrough는 **상태 의존 없이** 작동한다.

## 작동 방식

ZMK가 sim_layer 심볼 키를 INT5 (`Muhenkan`, USB HID 0x8B / KC_INT5) 키로 감싸 송신하면, 엔진이 INT5를 모디파이어처럼 인식해 그 동안 도착하는 모든 키를 P2 자동기를 거치지 않고 호스트로 통과시킨다.

```
ZMK 매크로 시퀀스:
  INT5 down → ; down → (release 대기) → ; up → INT5 up

엔진 처리:
  INT5 down  → int5_held = true, 자동기 flush, event 흡수
  ;     down → int5_held set이므로 passthrough (호스트 XKB가 ; 출력)
  ;     up   → release는 원래도 passthrough
  INT5 up    → int5_held = false, event 흡수
```

엔진은 INT5 press/release를 모두 흡수해 호스트에 전달하지 않는다. 이는 호스트 측 GUI에서 Muhenkan keysym에 단축키가 묶여 있을 가능성을 차단하기 위한 것이다.

## 동작 보장

- **상태 비의존**: 토글이 아니므로 누적 어긋남이 없다. ZMK가 항상 down/up 짝을 보내는 한 사용자가 한영 토글을 별도로 눌러도 영향 없음.
- **양방향 동작**: 한글 모드뿐 아니라 영문 모드 (엔진 비활성)에서도 동일하게 작동. 영문 모드에선 fcitx5가 엔진을 거치지 않으므로 INT5와 심볼 키 모두 호스트로 직접 통과.
- **자동기 보호**: 한글 합성 중간에 INT5가 도착하면 그 시점에 진행 중인 음절을 flush 후 commit한다. passthrough 글자가 자모와 섞여 박히지 않는다.
- **상태 누수 방지**: activate / deactivate / reset 모두 `int5_held = false`로 초기화. ZMK 떼는 이벤트가 IME 토글이나 포커스 전환으로 누락돼도 다음 IME 활성화에선 깨끗한 상태로 시작.

## 구현 위치

- `src/engine.h`: `P2InputState`에 `bool int5_held` 추가
- `src/engine.cpp` `keyEvent`:
  - `FcitxKey_Muhenkan` 분기 (release 검사 전): press/release에 따라 플래그 토글, event 흡수
  - 기존 modifier 검사 직후: `if (prop->int5_held) return;`로 호스트 passthrough
- `src/engine.cpp` `activate` / `deactivate` / `reset`: 플래그 false로 초기화

## ZMK 키맵 권장 패턴

토템 38키 + ZMK 환경에서 sim_layer의 문제 글자 4개 (`;`, `'`, `/`, `"`)를 INT5 매크로로 감싼다. 다른 글자 (`:`, `?`, `[`, `]`, `(`, `)` 등)는 P2 keymap에 매핑이 없어 자연 통과되므로 매크로 불필요.

매크로 예 (정확한 ZMK 문법은 펌웨어 버전에 따라 약간 다를 수 있음):

```c
behaviors {
    sym_pt: sym_passthrough {
        compatible = "zmk,behavior-macro-one-param";
        #binding-cells = <1>;
        bindings
            = <&macro_press &kp INT5>
            , <&macro_param_1to1>, <&macro_press &kp MACRO_PLACEHOLDER>
            , <&macro_pause_for_release>
            , <&macro_param_1to1>, <&macro_release &kp MACRO_PLACEHOLDER>
            , <&macro_release &kp INT5>
            ;
    };
};

// sim_layer 사용:
//   &sym_pt SEMICOLON   대신 &kp SEMICOLON
//   &sym_pt SQT         대신 &kp SQT
//   &sym_pt FSLH        대신 &kp FSLH
//   &sym_pt DQT         대신 &kp DQT  (Shift+'로 보내는 경우)
```

## 통합 테스트 절차

엔진 단위 테스트로는 fcitx5 KeyEvent를 모킹해야 해서 부담이 크므로 수동/통합 테스트로 검증.

1. **빌드 및 설치**:
   ```bash
   cmake -S . -B build -G Ninja && ninja -C build
   sudo ninja -C build install
   fcitx5 -r   # 재시작
   ```

2. **gedit (또는 임의 GTK 텍스트 에디터)에서**:
   - 한글 모드 진입 (한영 토글)
   - INT5 매크로로 감싼 sim_layer `;` 키 누름 → `;` 박힘 (ㅂ 자모 X) ✓
   - 동일하게 `'`, `/`, `"` 검증
   - INT5 없이 `;` 누름 → ㅂ preedit (정상 동작 유지) ✓

3. **자동기 보호 검증**:
   - 한글 모드에서 `ㄱ` (`k`) 누른 직후 INT5+`;` 시퀀스
   - 결과: `ㄱ` commit 후 `;` 박힘. `ㄱ`이 ㅂ과 합성되어 깨지지 않음.

4. **상태 비의존 검증**:
   - 한글 모드에서 INT5+`;` → 영문 모드 토글 → 영문 모드에서 INT5+`;` → 한글 모드 복귀
   - 매번 `;`이 박혀야 함. 토글 어긋남 없음.

5. **wev 관찰**:
   - INT5 단독 송신 (매크로 없이): wev에 Muhenkan 도착하나, 한글 모드면 엔진이 흡수해 wev에는 안 보일 수 있음. 영문 모드면 wev에 keysym `Muhenkan` 표시.
   - INT5+`;`: wev는 IME 경로 외부라 항상 keysym 받음. 한글 모드에선 다른 앱(gedit 등)에서 정상 박히는지가 핵심 검증.

## 참고

- INT5 (Muhenkan)은 일본어 IME에서 변환 취소 등의 용도로 쓰이지만, 본 프로젝트는 한국어 IME이고 한국어 키보드에서 사용하지 않으므로 충돌 없음.
- 다른 modifier 후보로 RAlt(AltGr)가 있으나 GUI 단축키나 XKB level 3 shift와 충돌 가능성이 있어 INT5 채택.
- 본 기능은 fcitx5-sinsebeolsik-p2 0.1.5+ 부터 제공.
