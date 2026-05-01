#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "automaton.h"
#include "keymap.h"

using namespace sin3p2;

namespace {

// QWERTY 문자열을 그대로 키 입력으로 흘려넣고 (committed, preedit) 반환.
// 자모 매핑이 안 되는 키(nullopt)를 만나면 자동기 flush + 그 키를 그대로 commit한다.
struct RunQwerty {
    std::u32string committed;
    std::u32string preedit;
    State final_state;
};

RunQwerty run_qwerty(std::u32string_view keys) {
    State s;
    std::u32string committed;
    std::u32string preedit;
    for (char32_t k : keys) {
        auto in = translate_p2(k, s);
        if (in) {
            auto r = step(s, *in);
            committed += r.commit;
            preedit = r.preedit;
            s = r.state;
        } else {
            // 자모가 아님 — flush + 키 그대로 commit
            auto f = flush(s);
            committed += f.commit;
            committed.push_back(k);
            preedit.clear();
            s = State{};
        }
    }
    return {committed, preedit, s};
}

std::u32string visible(const RunQwerty& r) {
    return r.committed + r.preedit;
}

}  // namespace

// ─── 시뮬레이터 검증 — 사용자가 직접 돌려본 케이스 그대로 ────────────────────

TEST_CASE("keymap: 시뮬레이터 정합 — 기본 음절", "[keymap][corpus]") {
    REQUIRE(visible(run_qwerty(U"kf"))   == U"가");
    REQUIRE(visible(run_qwerty(U"kfw"))  == U"갈");
    REQUIRE(visible(run_qwerty(U"kfwR")) == U"갈ㅓ");
    REQUIRE(visible(run_qwerty(U"kfsF")) == U"간ㅏ");
    REQUIRE(visible(run_qwerty(U"kfeR")) == U"갑ㅓ");
}

TEST_CASE("keymap: 겹받침 닭이 (uFwcjD)", "[keymap][corpus]") {
    REQUIRE(visible(run_qwerty(U"uFwcjD")) == U"닭이");
}

TEST_CASE("keymap: 가상 중성 — 의/위/과 (jid/jod/k/f)", "[keymap][corpus][virtual]") {
    REQUIRE(visible(run_qwerty(U"jid")) == U"의");
    REQUIRE(visible(run_qwerty(U"jod")) == U"위");
    REQUIRE(visible(run_qwerty(U"k/f")) == U"과");
}

TEST_CASE("keymap: 가상 중성 정착 — jof/joe/j/r → 웊/웁/옽", "[keymap][corpus][virtual]") {
    REQUIRE(visible(run_qwerty(U"jof"))  == U"웊");
    REQUIRE(visible(run_qwerty(U"joe"))  == U"웁");
    REQUIRE(visible(run_qwerty(U"j/r"))  == U"옽");
}

TEST_CASE("keymap: 정석 vs 단축 동치 — 의/위/외/왜/워/웨", "[keymap][corpus][virtual]") {
    // 정석 (둘 다 shift)
    REQUIRE(visible(run_qwerty(U"jGD")) == U"의");
    REQUIRE(visible(run_qwerty(U"jBD")) == U"위");
    REQUIRE(visible(run_qwerty(U"kVD")) == U"괴");
    REQUIRE(visible(run_qwerty(U"kVE")) == U"괘");
    REQUIRE(visible(run_qwerty(U"jBR")) == U"워");
    REQUIRE(visible(run_qwerty(U"jBC")) == U"웨");

    // 단축 (오른손 가상 중성 키 사용)
    REQUIRE(visible(run_qwerty(U"jid")) == U"의");
    REQUIRE(visible(run_qwerty(U"jod")) == U"위");
    REQUIRE(visible(run_qwerty(U"j/d")) == U"외");
    REQUIRE(visible(run_qwerty(U"j/e")) == U"왜");
    REQUIRE(visible(run_qwerty(U"jor")) == U"워");
    REQUIRE(visible(run_qwerty(U"joc")) == U"웨");
}

TEST_CASE("keymap: real ㅜ + lowercase d → ㅎ jong (jbd → 웋, jBd → 웋)",
          "[keymap][corpus][virtual]") {
    // 실제 ㅜ가 박힌 상태에서 lowercase d는 갈마조건 못 만족 → ㅎ jong
    REQUIRE(visible(run_qwerty(U"jbd")) == U"웋");
    REQUIRE(visible(run_qwerty(U"jBd")) == U"웋");
    // shift+D는 무조건 ㅣ jung → UnitMix → ㅟ
    REQUIRE(visible(run_qwerty(U"jbD")) == U"위");
    REQUIRE(visible(run_qwerty(U"jBD")) == U"위");
}

// ─── 짧은 단어 ───────────────────────────────────────────────────────────────

TEST_CASE("keymap: 짧은 단어들", "[keymap][corpus]") {
    // 안녕 = ㅇ ㅏ ㄴ + ㄴ ㅕ ㅇ
    REQUIRE(visible(run_qwerty(U"jfshta")) == U"안녕");
    // 한글 = ㅎ ㅏ ㄴ + ㄱ ㅡ ㄹ
    REQUIRE(visible(run_qwerty(U"mfskgw")) == U"한글");
    // 가자 = ㄱ ㅏ + ㅈ ㅏ
    REQUIRE(visible(run_qwerty(U"kflf")) == U"가자");
}

// ─── passthrough (Hangul-mapped이 아닌 키) ───────────────────────────────────

TEST_CASE("keymap: 숫자/공백/문장부호 passthrough", "[keymap][passthrough]") {
    // 숫자
    REQUIRE(visible(run_qwerty(U"123")) == U"123");
    // 한글 + 숫자 (음절 commit 후 숫자)
    REQUIRE(visible(run_qwerty(U"kf1")) == U"가1");
    // 공백
    REQUIRE(visible(run_qwerty(U"kf ")) == U"가 ");
    // 문장부호
    REQUIRE(visible(run_qwerty(U"kf.")) == U"가.");
    REQUIRE(visible(run_qwerty(U"kf,")) == U"가,");
    REQUIRE(visible(run_qwerty(U"kf?")) == U"가?");
}
