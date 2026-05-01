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
// 데이터는 TOML로 외화. 기본 keymap은 빌드 시 `keymaps/sinsebeolsik_p2.toml`을
// 임베드해 만들고, 엔진 시작 시 사용자 dir의 override TOML이 있으면 그걸로 교체한다.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "automaton.h"

namespace sin3p2 {

// 빌드 시 임베드된 기본 P2 keymap TOML 본문 (keymaps/sinsebeolsik_p2.toml).
// 단위 테스트와 엔진 fallback이 동일한 내용을 공유한다.
extern const char* const EMBEDDED_P2_KEYMAP_TOML;

// QWERTY 한 키에 대해 결정된 출력 1개.
// rule이 매치되면 자동기에 흘러갈 Input이거나, "이 키는 자모가 아님" passthrough 마커.
struct PassThrough {};
using KeyOutput = std::variant<InputCho, InputJung, InputVJung, InputJong, PassThrough>;

class Keymap {
public:
    // 한 글자 + 현재 state → 자모 입력 (없으면 nullopt = passthrough)
    std::optional<Input> translate(char32_t qwerty_key, const State& state) const;

    // 디버깅/테스트용
    std::size_t key_count() const noexcept { return rules_.size(); }
    const std::string& meta_name() const noexcept { return meta_name_; }

    // 내부 표현 — 빌더가 채움
    struct Predicate;
    struct Rule {
        std::shared_ptr<const Predicate> when;  // null = 무조건 매치
        KeyOutput output;
    };

    // 파서가 직접 채우기 위한 setter (구현 디테일이지만 단순화)
    void set_rules(char32_t key, std::vector<Rule> rules);
    void set_meta_name(std::string name) { meta_name_ = std::move(name); }

private:
    std::unordered_map<char32_t, std::vector<Rule>> rules_;
    std::string meta_name_;
};

// TOML 본문 → Keymap 파싱. 실패하면 nullopt.
std::optional<Keymap> load_keymap_from_string(std::string_view toml);

// 파일 경로 → Keymap. 파일 못 읽거나 파싱 실패 시 nullopt.
std::optional<Keymap> load_keymap_from_file(const std::filesystem::path& path);

// ─── process-global 기본 keymap (호환 API) ────────────────────────────────────
//
// 첫 호출 시 EMBEDDED_P2_KEYMAP_TOML을 lazy-parse한다.
// 엔진은 시작 시 install_default_keymap()으로 사용자 override를 박을 수 있다.

const Keymap& default_keymap();
void install_default_keymap(Keymap km);

// 기존 시그니처 유지 — 내부적으로 default_keymap().translate()를 호출.
std::optional<Input> translate_p2(char32_t qwerty_key, const State& state);

}  // namespace sin3p2
