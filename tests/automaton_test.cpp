#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "automaton.h"

using namespace sin3p2;

namespace {

// 입력 시퀀스를 차례로 적용하면서 commit을 모으고 마지막 preedit을 반환
struct RunResult {
    std::u32string committed;
    std::u32string preedit;
    State final_state;
};

RunResult run(const std::vector<Input>& inputs) {
    State s;
    std::u32string committed;
    std::u32string preedit;
    for (const auto& in : inputs) {
        auto r = step(s, in);
        committed += r.commit;
        preedit = r.preedit;
        s = r.state;
    }
    return {committed, preedit, s};
}

// 시뮬레이터에서 본 "전체 화면 표시" = committed + preedit
std::u32string visible(const RunResult& r) {
    return r.committed + r.preedit;
}

// shorthand
InputCho   C(Cho c)   { return {c}; }
InputJung  J(Jung j)  { return {j}; }
InputVJung V(VJung v) { return {v}; }
InputJong  G(Jong g)  { return {g}; }

}  // namespace

// ─── 시뮬레이터 검증 케이스 (Test 1: 도깨비불 없음) ───────────────────────────

TEST_CASE("도깨비불 없음 — kfwR → '갈ㅓ'", "[automaton][dokkaebi]") {
    // k(ㄱ) f(ㅏ) w(ㄹ jong) R(ㅓ)
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R), J(Jung::EO)});
    REQUIRE(r.committed == U"갈");
    REQUIRE(r.preedit == U"ㅓ");
    REQUIRE(visible(r) == U"갈ㅓ");
}

TEST_CASE("도깨비불 없음 — kfsF → '간ㅏ'", "[automaton][dokkaebi]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::N), J(Jung::A)});
    REQUIRE(r.committed == U"간");
    REQUIRE(r.preedit == U"ㅏ");
    REQUIRE(visible(r) == U"간ㅏ");
}

TEST_CASE("도깨비불 없음 — kfeR → '갑ㅓ'", "[automaton][dokkaebi]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::B), J(Jung::EO)});
    REQUIRE(r.committed == U"갑");
    REQUIRE(r.preedit == U"ㅓ");
    REQUIRE(visible(r) == U"갑ㅓ");
}

TEST_CASE("겹받침 분리 없음 — uFwcjD → '닭이'", "[automaton][cluster]") {
    // u(ㄷ) F(ㅏ) w(ㄹ jong) c(ㄱ jong, 클러스터 ㄺ) j(ㅇ) D(ㅣ)
    auto r = run({C(Cho::D), J(Jung::A), G(Jong::R), G(Jong::G),
                  C(Cho::O), J(Jung::I)});
    REQUIRE(r.committed == U"닭");
    REQUIRE(r.preedit == U"이");
    REQUIRE(visible(r) == U"닭이");
}

// ─── 가상 중성 (Test 3) ──────────────────────────────────────────────────────

TEST_CASE("가상 중성 단축 — 의 (jid)", "[automaton][virtual]") {
    auto r = run({C(Cho::O), V(VJung::EU), J(Jung::I)});
    REQUIRE(r.committed.empty());
    REQUIRE(r.preedit == U"의");
}

TEST_CASE("가상 중성 단축 — 위 (jod)", "[automaton][virtual]") {
    auto r = run({C(Cho::O), V(VJung::U), J(Jung::I)});
    REQUIRE(r.preedit == U"위");
}

TEST_CASE("가상 중성 단축 — 과 (k/f)", "[automaton][virtual]") {
    auto r = run({C(Cho::G), V(VJung::O), J(Jung::A)});
    REQUIRE(r.preedit == U"과");
}

TEST_CASE("가상 중성 단축 — 외/왜/워/웨", "[automaton][virtual]") {
    REQUIRE(run({C(Cho::O), V(VJung::O),  J(Jung::I)}).preedit == U"외");
    REQUIRE(run({C(Cho::O), V(VJung::O),  J(Jung::AE)}).preedit == U"왜");
    REQUIRE(run({C(Cho::O), V(VJung::U),  J(Jung::EO)}).preedit == U"워");
    REQUIRE(run({C(Cho::O), V(VJung::U),  J(Jung::E)}).preedit == U"웨");
}

TEST_CASE("정석 vs 단축 입력 동치 — 의/위/과", "[automaton][virtual]") {
    // jGD (정석) vs jid (단축)
    auto a = run({C(Cho::O), J(Jung::EU), J(Jung::I)});
    auto b = run({C(Cho::O), V(VJung::EU), J(Jung::I)});
    REQUIRE(a.preedit == b.preedit);
    REQUIRE(a.preedit == U"의");

    // jBD vs jod
    auto c = run({C(Cho::O), J(Jung::U), J(Jung::I)});
    auto d = run({C(Cho::O), V(VJung::U), J(Jung::I)});
    REQUIRE(c.preedit == d.preedit);
    REQUIRE(c.preedit == U"위");

    // kVF vs k/f
    auto e = run({C(Cho::G), J(Jung::O), J(Jung::A)});
    auto f = run({C(Cho::G), V(VJung::O), J(Jung::A)});
    REQUIRE(e.preedit == f.preedit);
    REQUIRE(e.preedit == U"과");
}

// ─── 가상 중성 정착 (합성 실패 시) ────────────────────────────────────────────

TEST_CASE("가상 중성 + jong 입력 → 가상→실제 정착 + jong 부착", "[automaton][virtual][jong]") {
    // jof: ㅇ + 가상ㅜ + ㅍ jong → 웊
    auto r = run({C(Cho::O), V(VJung::U), G(Jong::P)});
    REQUIRE(r.preedit == U"웊");

    // joe: ㅇ + 가상ㅜ + ㅂ jong → 웁
    auto r2 = run({C(Cho::O), V(VJung::U), G(Jong::B)});
    REQUIRE(r2.preedit == U"웁");

    // j/r: ㅇ + 가상ㅗ + ㅌ jong → 옽
    auto r3 = run({C(Cho::O), V(VJung::O), G(Jong::T)});
    REQUIRE(r3.preedit == U"옽");
}

// ─── 백스페이스 ──────────────────────────────────────────────────────────────

TEST_CASE("BS — kf '가' → 'ㄱ'", "[automaton][backspace]") {
    auto r = run({C(Cho::G), J(Jung::A)});
    REQUIRE(r.preedit == U"가");
    auto bs = backspace(r.final_state);
    REQUIRE(bs.preedit == U"ㄱ");
}

TEST_CASE("BS — kfw '갈' → '가' → 'ㄱ' → 빈", "[automaton][backspace]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R)});
    REQUIRE(r.preedit == U"갈");

    auto b1 = backspace(r.final_state);
    REQUIRE(b1.preedit == U"가");

    auto b2 = backspace(b1.state);
    REQUIRE(b2.preedit == U"ㄱ");

    auto b3 = backspace(b2.state);
    REQUIRE(b3.preedit.empty());
}

TEST_CASE("BS — kfwkf '갈가' → '갈ㄱ'", "[automaton][backspace]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R),
                  C(Cho::G), J(Jung::A)});
    REQUIRE(r.committed == U"갈");
    REQUIRE(r.preedit == U"가");

    auto b1 = backspace(r.final_state);
    REQUIRE(b1.preedit == U"ㄱ");
    // commit은 자동기 영역 밖 — '갈'은 이미 호스트가 갖고 있음
}

TEST_CASE("BS — k/f '과' → '고' (복합 모음 분해)", "[automaton][backspace][virtual]") {
    auto r = run({C(Cho::G), V(VJung::O), J(Jung::A)});
    REQUIRE(r.preedit == U"과");

    auto b1 = backspace(r.final_state);
    REQUIRE(b1.preedit == U"고");

    auto b2 = backspace(b1.state);
    REQUIRE(b2.preedit == U"ㄱ");

    auto b3 = backspace(b2.state);
    REQUIRE(b3.preedit.empty());
}

TEST_CASE("BS — 클러스터 종성 분해 (닭 → 달)", "[automaton][backspace][cluster]") {
    auto r = run({C(Cho::D), J(Jung::A), G(Jong::R), G(Jong::G)});
    REQUIRE(r.preedit == U"닭");

    auto b1 = backspace(r.final_state);
    REQUIRE(b1.preedit == U"달");

    auto b2 = backspace(b1.state);
    REQUIRE(b2.preedit == U"다");
}

TEST_CASE("BS — 가상 중성 단독 (jo 후 BS → ㅇ)", "[automaton][backspace][virtual]") {
    auto r = run({C(Cho::O), V(VJung::U)});
    REQUIRE(r.preedit == U"우");  // 가상 ㅜ도 표시는 ㅜ로

    auto b1 = backspace(r.final_state);
    REQUIRE(b1.preedit == U"ㅇ");
}

// ─── 합성 모음 (UnitMix 정석 입력 — 둘 다 shift) ──────────────────────────────

TEST_CASE("UnitMix 모음 — kVF '과', kVE '괘', kVD '괴'", "[automaton][unitmix]") {
    REQUIRE(run({C(Cho::G), J(Jung::O), J(Jung::A)}).preedit  == U"과");
    REQUIRE(run({C(Cho::G), J(Jung::O), J(Jung::AE)}).preedit == U"괘");
    REQUIRE(run({C(Cho::G), J(Jung::O), J(Jung::I)}).preedit  == U"괴");
}

TEST_CASE("UnitMix 모음 — jBR '워', jBC '웨', jBD '위', jGD '의'",
          "[automaton][unitmix]") {
    REQUIRE(run({C(Cho::O), J(Jung::U),  J(Jung::EO)}).preedit == U"워");
    REQUIRE(run({C(Cho::O), J(Jung::U),  J(Jung::E)}).preedit  == U"웨");
    REQUIRE(run({C(Cho::O), J(Jung::U),  J(Jung::I)}).preedit  == U"위");
    REQUIRE(run({C(Cho::O), J(Jung::EU), J(Jung::I)}).preedit  == U"의");
}

// ─── flush (포커스 잃음 등) ──────────────────────────────────────────────────

TEST_CASE("flush commits in-progress syllable", "[automaton][flush]") {
    auto r = run({C(Cho::G), J(Jung::A)});  // "가" 진행 중
    auto f = flush(r.final_state);
    REQUIRE(f.commit == U"가");
    REQUIRE(f.state.empty());
}

TEST_CASE("flush on empty state is no-op", "[automaton][flush]") {
    auto f = flush(State{});
    REQUIRE(f.commit.empty());
    REQUIRE(f.state.empty());
}

// ─── 기본 단음절 ─────────────────────────────────────────────────────────────

TEST_CASE("기본 단음절 — 가/한/닭/힣", "[automaton][basic]") {
    REQUIRE(run({C(Cho::G), J(Jung::A)}).preedit == U"가");
    REQUIRE(run({C(Cho::H), J(Jung::A), G(Jong::N)}).preedit == U"한");
    REQUIRE(run({C(Cho::D), J(Jung::A), G(Jong::R), G(Jong::G)}).preedit == U"닭");
    REQUIRE(run({C(Cho::H), J(Jung::I), G(Jong::H)}).preedit == U"힣");
}

TEST_CASE("연속 음절 commit — 가나 (kFhF)", "[automaton][sequence]") {
    // k(ㄱ) F(ㅏ) h(ㄴ cho) F(ㅏ) — 두 음절
    auto r = run({C(Cho::G), J(Jung::A), C(Cho::N), J(Jung::A)});
    REQUIRE(r.committed == U"가");
    REQUIRE(r.preedit == U"나");
    REQUIRE(visible(r) == U"가나");
}

// ─── 모든 클러스터 종성 합성 ────────────────────────────────────────────────

TEST_CASE("cluster ㄳ — 가 + ㄱ + ㅅ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::G), G(Jong::S)});
    REQUIRE(r.preedit == U"갃");
}
TEST_CASE("cluster ㄵ — 가 + ㄴ + ㅈ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::N), G(Jong::J)});
    REQUIRE(r.preedit == U"갅");
}
TEST_CASE("cluster ㄶ — 가 + ㄴ + ㅎ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::N), G(Jong::H)});
    REQUIRE(r.preedit == U"갆");
}
TEST_CASE("cluster ㄻ — 갈 + ㅁ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R), G(Jong::M)});
    REQUIRE(r.preedit == U"갊");
}
TEST_CASE("cluster ㄽ — 갈 + ㅅ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R), G(Jong::S)});
    REQUIRE(r.preedit == U"갌");
}
TEST_CASE("cluster ㄾ — 갈 + ㅌ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R), G(Jong::T)});
    REQUIRE(r.preedit == U"갍");
}
TEST_CASE("cluster ㄿ — 갈 + ㅍ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R), G(Jong::P)});
    REQUIRE(r.preedit == U"갎");
}
TEST_CASE("cluster ㅀ — 갈 + ㅎ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R), G(Jong::H)});
    REQUIRE(r.preedit == U"갏");
}
TEST_CASE("cluster ㅄ — 갑 + ㅅ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::B), G(Jong::S)});
    REQUIRE(r.preedit == U"값");
}
TEST_CASE("cluster ㄲ — 가 + ㄱ + ㄱ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::G), G(Jong::G)});
    REQUIRE(r.preedit == U"갂");
}
TEST_CASE("cluster ㅆ — 가 + ㅅ + ㅅ", "[automaton][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::S), G(Jong::S)});
    REQUIRE(r.preedit == U"갔");
}

// ─── 클러스터 합성 실패 → commit + 새 음절 ──────────────────────────────────

TEST_CASE("cluster 실패 — 갓 + ㄴ → 갓 + ㄴ standalone", "[automaton][cluster]") {
    // ㅅ + ㄴ 클러스터는 .ist에 없음
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::S), G(Jong::N)});
    REQUIRE(r.committed == U"갓");
    REQUIRE(r.preedit == U"ㄴ");
}

TEST_CASE("cluster 안 함 — cho 없이 ㅅ+ㅅ는 standalone 두 개", "[automaton][cluster]") {
    // 시뮬: qq=ㅅㅅ. 진행 중 음절이 없는 상태에서 jong이 누적되면 안 됨.
    auto r = run({G(Jong::S), G(Jong::S)});
    REQUIRE(r.committed == U"ㅅ");
    REQUIRE(r.preedit == U"ㅅ");
}

// ─── BS — 모든 클러스터 분해 ────────────────────────────────────────────────

TEST_CASE("BS — ㄳ → ㄱ", "[automaton][backspace][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::G), G(Jong::S)});
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"각");
}
TEST_CASE("BS — ㄵ → ㄴ", "[automaton][backspace][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::N), G(Jong::J)});
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"간");
}
TEST_CASE("BS — ㄶ → ㄴ", "[automaton][backspace][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::N), G(Jong::H)});
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"간");
}
TEST_CASE("BS — ㄻ → ㄹ", "[automaton][backspace][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::R), G(Jong::M)});
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"갈");
}
TEST_CASE("BS — ㅄ → ㅂ", "[automaton][backspace][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::B), G(Jong::S)});
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"갑");
}

// ─── BS — 복합 모음 분해 ────────────────────────────────────────────────────

TEST_CASE("BS — ㅘ → ㅗ (실제 jung UnitMix 분해)", "[automaton][backspace]") {
    auto r = run({C(Cho::G), J(Jung::O), J(Jung::A)});
    REQUIRE(r.preedit == U"과");
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"고");
}
TEST_CASE("BS — ㅙ → ㅗ", "[automaton][backspace]") {
    auto r = run({C(Cho::G), J(Jung::O), J(Jung::AE)});
    REQUIRE(r.preedit == U"괘");
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"고");
}
TEST_CASE("BS — ㅢ → ㅡ", "[automaton][backspace]") {
    auto r = run({C(Cho::O), J(Jung::EU), J(Jung::I)});
    REQUIRE(r.preedit == U"의");
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"으");
}
TEST_CASE("BS — ㅟ → ㅜ", "[automaton][backspace]") {
    auto r = run({C(Cho::O), J(Jung::U), J(Jung::I)});
    REQUIRE(r.preedit == U"위");
    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"우");
}

// ─── 빈 state BS → no-op (호스트 위임) ──────────────────────────────────────

TEST_CASE("BS — 빈 state → 빈 결과", "[automaton][backspace]") {
    auto b = backspace(State{});
    REQUIRE(b.preedit.empty());
    REQUIRE(b.state.empty());
}

// ─── BS — 단독 jong (음절 컨텍스트 없음) → 한 번에 클리어 ──────────────────────
// 빈 state에서 x 키를 누르면 keymap이 jong=SS를 박는다 (단일 키 입력).
// 사용자 멘탈 모델: 한 키 = 한 BS. 클러스터 분해하지 않고 통째로 제거되어야 함.

TEST_CASE("BS — 단독 jong=SS (x 빈 state) → 1 BS로 클리어",
          "[automaton][backspace][standalone]") {
    auto r = run({G(Jong::SS)});
    REQUIRE(r.preedit == U"ㅆ");
    REQUIRE(r.committed.empty());

    auto b = backspace(r.final_state);
    REQUIRE(b.preedit.empty());
    REQUIRE(b.state.empty());
}

TEST_CASE("BS — 단독 jong=GG → 1 BS로 클리어",
          "[automaton][backspace][standalone]") {
    State s;
    s.jong = Jong::GG;
    auto b = backspace(s);
    REQUIRE(b.preedit.empty());
    REQUIRE(b.state.empty());
}

// ─── BS — 단일 키로 박힌 jong=SS는 음절 컨텍스트가 있어도 통째로 제거 ──────
// keymap이 jong=SS를 한 키 입력으로 직접 박는 경우 (x 키 fallback) BS 한 번에 ㅆ 통째 사라짐.
// (kfqq처럼 두 키로 합성된 SS와 구분해야 함.)

TEST_CASE("BS — kfx → 갔 → BS → 가 (단일 키 SS 통째 제거)",
          "[automaton][backspace]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::SS)});
    REQUIRE(r.preedit == U"갔");

    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"가");
}

TEST_CASE("BS — jfx → 았 → BS → 아 (단일 키 SS 통째 제거)",
          "[automaton][backspace]") {
    auto r = run({C(Cho::O), J(Jung::A), G(Jong::SS)});
    REQUIRE(r.preedit == U"았");

    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"아");
}

TEST_CASE("BS — 두 키로 합성된 SS (kfqq → 갔)는 분해 (갔 → 갓)",
          "[automaton][backspace][cluster]") {
    auto r = run({C(Cho::G), J(Jung::A), G(Jong::S), G(Jong::S)});
    REQUIRE(r.preedit == U"갔");

    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"갓");
    auto b2 = backspace(b.state);
    REQUIRE(b2.preedit == U"가");
}

// ─── BS — 합성 모음 분해 후 가상 복귀 ───────────────────────────────────────
// joc(가상ㅜ+ㅔ→웨) → BS → '우' (cho=O+가상ㅜ) → c → 웨 복원.
// 분해 결과의 첫 부분이 ㅗ/ㅜ/ㅡ면 가상 중성으로 환원해야 keymap의 갈마들이가
// 다음 키에서 재합성을 트리거할 수 있다.

TEST_CASE("BS — joc 후 BS+ㅔ 재합성 → 웨", "[automaton][backspace][virtual]") {
    auto r = run({C(Cho::O), V(VJung::U), J(Jung::E)});
    REQUIRE(r.preedit == U"웨");

    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"우");

    // 자동기에 같은 jung을 다시 흘리면 가상+ㅔ → 웨 합성이 일어나야 한다.
    auto re = step(b.state, J(Jung::E));
    REQUIRE(re.preedit == U"웨");
}

TEST_CASE("BS — kVF 후 BS+ㅏ 재합성 → 과", "[automaton][backspace][virtual]") {
    // 실제+실제 합성으로 만들어진 과도 BS 후 가상으로 복귀해 일관된 재합성 가능.
    auto r = run({C(Cho::G), J(Jung::O), J(Jung::A)});
    REQUIRE(r.preedit == U"과");

    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"고");

    auto re = step(b.state, J(Jung::A));
    REQUIRE(re.preedit == U"과");
}

TEST_CASE("BS — jGD(의) 후 BS+ㅣ 재합성 → 의", "[automaton][backspace][virtual]") {
    auto r = run({C(Cho::O), J(Jung::EU), J(Jung::I)});
    REQUIRE(r.preedit == U"의");

    auto b = backspace(r.final_state);
    REQUIRE(b.preedit == U"으");

    auto re = step(b.state, J(Jung::I));
    REQUIRE(re.preedit == U"의");
}

TEST_CASE("BS — jong 제거가 freeze된 가상을 복귀 (joc→f→BS→c → 웨)",
          "[automaton][backspace][virtual]") {
    // joc → 웨 → BS → 우(vjung U) → f(jong=P, vjung freeze) → 웊
    auto r1 = run({C(Cho::O), V(VJung::U), J(Jung::E)});
    REQUIRE(r1.preedit == U"웨");

    auto b1 = backspace(r1.final_state);
    REQUIRE(b1.preedit == U"우");

    auto r2 = step(b1.state, G(Jong::P));
    REQUIRE(r2.preedit == U"웊");

    // 이 BS는 freeze된 ㅜ를 가상으로 복귀시켜야 한다.
    auto b2 = backspace(r2.state);
    REQUIRE(b2.preedit == U"우");

    // c가 매핑하는 jung=E 입력이 가상ㅜ + ㅔ → ㅞ 합성을 다시 일으켜야 함.
    auto r3 = step(b2.state, J(Jung::E));
    REQUIRE(r3.preedit == U"웨");
}

// ─── 쌍자음 (UnitMix CHO) ───────────────────────────────────────────────────

TEST_CASE("쌍자음 — ㄱ ㄱ → ㄲ", "[automaton][double-cho]") {
    auto r = run({C(Cho::G), C(Cho::G)});
    REQUIRE(r.preedit == U"ㄲ");
}
TEST_CASE("쌍자음 — ㄷ ㄷ → ㄸ", "[automaton][double-cho]") {
    auto r = run({C(Cho::D), C(Cho::D)});
    REQUIRE(r.preedit == U"ㄸ");
}
TEST_CASE("쌍자음 + jung → 음절합성 — ㄸㅏ → 따", "[automaton][double-cho]") {
    auto r = run({C(Cho::D), C(Cho::D), J(Jung::A)});
    REQUIRE(r.preedit == U"따");
}
TEST_CASE("쌍자음 + jung + jong — ㅉㅏㄼ → 짧", "[automaton][double-cho][cluster]") {
    auto r = run({C(Cho::J), C(Cho::J), J(Jung::A), G(Jong::R), G(Jong::B)});
    REQUIRE(r.preedit == U"짧");
}

// ─── 다른 cho 두 번 → 쌍자음 아님, 새 음절 ──────────────────────────────────

TEST_CASE("다른 cho — ㄱ ㄴ → ㄱ commit, ㄴ preedit", "[automaton][boundary]") {
    auto r = run({C(Cho::G), C(Cho::N)});
    REQUIRE(r.committed == U"ㄱ");
    REQUIRE(r.preedit == U"ㄴ");
}

// ─── 가상 중성 + 합성 실패 jung → 정착 + standalone jung ─────────────────────

TEST_CASE("vjung U + ㅏ → 합성 안 됨, 우 commit + ㅏ standalone", "[automaton][virtual]") {
    // combine_virtual_jung(U, A)는 nullopt — 합성 실패 → vjung→real cast 후 commit, ㅏ standalone
    auto r = run({C(Cho::O), V(VJung::U), J(Jung::A)});
    REQUIRE(r.committed == U"우");
    REQUIRE(r.preedit == U"ㅏ");
}

// ─── 옛한글 ㆍ (아래아) — 시뮬레이터 검증 ───────────────────────────────────
// 시뮬 결과 (2026-05-02):
//   kz=ᄀᆞ  jzd=ᄋᆞᇂ  jzz=ᄋᆞᆷ  jzq=ᄋᆞᆺ
//   jzD=ᄋᆡ (FI)  jzF=ᄋᆞㅏ (break)  jpz=ᄋᆢ (FF)
//   jz→BS=ㅇ
// 옛한글 음절은 precomposed 없음 → conjoining 시퀀스로 출력.

TEST_CASE("아래아 — kz → ᄀᆞ", "[automaton][araea]") {
    // C(G) + J(F) → cho ㄱ + jung ㆍ
    auto r = run({C(Cho::G), J(Jung::F)});
    REQUIRE(r.committed.empty());
    REQUIRE(r.preedit == U"ᄀᆞ");  // ᄀᆞ
}

TEST_CASE("아래아 + jong — jzd → ᄋᆞᇂ", "[automaton][araea]") {
    auto r = run({C(Cho::O), J(Jung::F), G(Jong::H)});
    REQUIRE(r.committed.empty());
    REQUIRE(r.preedit == U"ᄋᆞᇂ");  // ᄋᆞᇂ
}

TEST_CASE("아래아 후 z = jong ㅁ — jzz → ᄋᆞᆷ (keymap path)", "[automaton][araea]") {
    // 두 번째 z는 keymap이 jong M으로 떨어트림 (jung 채워졌으니).
    // 자동기 직접 테스트: J(F) 후 G(M).
    auto r = run({C(Cho::O), J(Jung::F), G(Jong::M)});
    REQUIRE(r.committed.empty());
    REQUIRE(r.preedit == U"ᄋᆞᆷ");  // ᄋᆞᆷ
}

TEST_CASE("아래아 + jong ㅅ — jzq → ᄋᆞᆺ", "[automaton][araea]") {
    auto r = run({C(Cho::O), J(Jung::F), G(Jong::S)});
    REQUIRE(r.committed.empty());
    REQUIRE(r.preedit == U"ᄋᆞᆺ");  // ᄋᆞᆺ
}

TEST_CASE("FI compound — jzD → ᄋᆡ", "[automaton][araea][unitmix]") {
    // F + I → FI (ㆎ). combine_jung(F, I)
    auto r = run({C(Cho::O), J(Jung::F), J(Jung::I)});
    REQUIRE(r.committed.empty());
    REQUIRE(r.preedit == U"ᄋᆡ");  // ᄋᆡ
}

TEST_CASE("F + 합성 안 되는 jung → 음절 break — jzF → ᄋᆞ + ㅏ", "[automaton][araea]") {
    // combine_jung(F, A)는 nullopt → ᄋᆞ commit + 새 state {jung=A}
    auto r = run({C(Cho::O), J(Jung::F), J(Jung::A)});
    REQUIRE(r.committed == U"ᄋᆞ");  // ᄋᆞ
    REQUIRE(r.preedit == U"ㅏ");                // 호환 자모 standalone
}

TEST_CASE("FF compound via virtual — jpz → ᄋᆢ", "[automaton][araea][unitmix][virtual]") {
    // p에서 vF를 박고 z(real F)가 들어오면 combine_virtual_jung(vF, F) → FF
    auto r = run({C(Cho::O), V(VJung::F), J(Jung::F)});
    REQUIRE(r.committed.empty());
    REQUIRE(r.preedit == U"ᄋᆢ");  // ᄋᆢ (쌍아래아)
}

TEST_CASE("BS — jz → ᄋᆞ → ㅇ", "[automaton][araea][backspace]") {
    auto r = run({C(Cho::O), J(Jung::F)});
    REQUIRE(r.preedit == U"ᄋᆞ");
    auto bs = backspace(r.final_state);
    REQUIRE(bs.preedit == U"ㅇ");  // 호환 자모 cho-only
}

TEST_CASE("BS — FI 분해 ᄋᆡ → ᄋᆞ + 가상", "[automaton][araea][backspace]") {
    // FI → split_jung 분해. split.first=F → VJung::F로 복귀.
    // 재입력 시 D(=jung I) 다시 받으면 combine_virtual_jung(vF, I) → FI 재합성.
    auto r = run({C(Cho::O), J(Jung::F), J(Jung::I)});
    REQUIRE(r.preedit == U"ᄋᆡ");
    auto bs = backspace(r.final_state);
    REQUIRE(bs.preedit == U"ᄋᆞ");  // ᄋᆞ — F는 jung_to_compat이 ㆍ로 표시
    // 가상으로 복귀했는지 확인 — 다시 jung I 입력 시 FI로 합성
    auto r2 = step(bs.state, J(Jung::I));
    REQUIRE(r2.preedit == U"ᄋᆡ");  // ᄋᆡ
}

TEST_CASE("BS — FF 분해 ᄋᆢ → ᄋᆞ + 가상", "[automaton][araea][backspace]") {
    auto r = run({C(Cho::O), V(VJung::F), J(Jung::F)});
    REQUIRE(r.preedit == U"ᄋᆢ");
    auto bs = backspace(r.final_state);
    REQUIRE(bs.preedit == U"ᄋᆞ");  // ᄋᆞ
    // 다시 z(real F) 입력 시 FF로 합성
    auto r2 = step(bs.state, J(Jung::F));
    REQUIRE(r2.preedit == U"ᄋᆢ");
}
