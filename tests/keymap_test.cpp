#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <variant>

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
        auto act = translate_p2(k, s);
        if (!act) {
            // 자모가 아님 — flush + 키 그대로 commit
            auto f = flush(s);
            committed += f.commit;
            committed.push_back(k);
            preedit.clear();
            s = State{};
        } else if (auto* in = std::get_if<Input>(&*act)) {
            auto r = step(s, *in);
            committed += r.commit;
            preedit = r.preedit;
            s = r.state;
        } else {
            // LiteralText — flush + 리터럴 commit (빈 문자열이면 흡수)
            const auto& lit = std::get<LiteralText>(*act);
            auto f = flush(s);
            committed += f.commit;
            committed += lit.text;
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

TEST_CASE("keymap: 쌍자음 — 같은 초성 두 번", "[keymap][double-cho]") {
    REQUIRE(visible(run_qwerty(U"kk")) == U"ㄲ");
    REQUIRE(visible(run_qwerty(U"uu")) == U"ㄸ");
    REQUIRE(visible(run_qwerty(U";;")) == U"ㅃ");
    REQUIRE(visible(run_qwerty(U"nn")) == U"ㅆ");
    REQUIRE(visible(run_qwerty(U"ll")) == U"ㅉ");
    // 음절 합성
    REQUIRE(visible(run_qwerty(U"kkf"))  == U"까");
    REQUIRE(visible(run_qwerty(U";;f"))  == U"빠");
    REQUIRE(visible(run_qwerty(U"nnfs")) == U"싼");  // ㅆ + ㅏ + ㄴ jong
    // 다른 초성 두 번 → commit + 새 음절
    REQUIRE(visible(run_qwerty(U"kh"))  == U"ㄱㄴ");
}

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

// ─── 모든 cho 키 × shift+F (ㅏ) ──────────────────────────────────────────────
// 오른손 lowercase 초성 + i/o/p/' (cho-only state에서 vjung으로 빠지지 않는 경우는 항상 cho)

TEST_CASE("keymap: cho ㄱ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"kF")) == U"가");
}
TEST_CASE("keymap: cho ㄴ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"hF")) == U"나");
}
TEST_CASE("keymap: cho ㄷ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"uF")) == U"다");
}
TEST_CASE("keymap: cho ㄹ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"yF")) == U"라");
}
TEST_CASE("keymap: cho ㅁ + ㅏ (i 키, 빈 state에서)", "[keymap][cho]") {
    // i: D&&!E&&!F → vjung. 빈 state에서는 D=false → cho M.
    REQUIRE(visible(run_qwerty(U"iF")) == U"마");
}
TEST_CASE("keymap: cho ㅂ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U";F")) == U"바");
}
TEST_CASE("keymap: cho ㅅ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"nF")) == U"사");
}
TEST_CASE("keymap: cho ㅇ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"jF")) == U"아");
}
TEST_CASE("keymap: cho ㅈ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"lF")) == U"자");
}
TEST_CASE("keymap: cho ㅊ + ㅏ (o 키, 빈 state)", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"oF")) == U"차");
}
TEST_CASE("keymap: cho ㅋ + ㅏ (/ 키, 빈 state)", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"/F")) == U"카");
}
TEST_CASE("keymap: cho ㅌ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"'F")) == U"타");
}
TEST_CASE("keymap: cho ㅍ + ㅏ (p 키, 빈 state)", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"pF")) == U"파");
}
TEST_CASE("keymap: cho ㅎ + ㅏ", "[keymap][cho]") {
    REQUIRE(visible(run_qwerty(U"mF")) == U"하");
}

// ─── ㄱ + 모든 shift jung 키 (왼손 uppercase 14개) ───────────────────────────

TEST_CASE("keymap: ㄱ + F → 가", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kF")) == U"가");
}
TEST_CASE("keymap: ㄱ + E → 개", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kE")) == U"개");
}
TEST_CASE("keymap: ㄱ + W → 갸", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kW")) == U"갸");
}
TEST_CASE("keymap: ㄱ + Q → 걔", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kQ")) == U"걔");
}
TEST_CASE("keymap: ㄱ + R → 거", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kR")) == U"거");
}
TEST_CASE("keymap: ㄱ + C → 게", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kC")) == U"게");
}
TEST_CASE("keymap: ㄱ + T → 겨", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kT")) == U"겨");
}
TEST_CASE("keymap: ㄱ + S → 계", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kS")) == U"계");
}
TEST_CASE("keymap: ㄱ + V → 고", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kV")) == U"고");
}
TEST_CASE("keymap: ㄱ + X → 교", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kX")) == U"교");
}
TEST_CASE("keymap: ㄱ + B → 구", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kB")) == U"구");
}
TEST_CASE("keymap: ㄱ + A → 규", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kA")) == U"규");
}
TEST_CASE("keymap: ㄱ + G → 그", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kG")) == U"그");
}
TEST_CASE("keymap: ㄱ + D → 기", "[keymap][shift-jung]") {
    REQUIRE(visible(run_qwerty(U"kD")) == U"기");
}

// ─── ㄱ ㅏ + 모든 lowercase jong 키 (왼손 lowercase 15개) ────────────────────
// 음절 cho+jung 상태에서는 lowercase 키의 갈마들이 첫 분기가 깨진다 (D&&!E false) → jong 분기.

TEST_CASE("keymap: 갓 (kFq, q→ㅅ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFq")) == U"갓");
}
TEST_CASE("keymap: 갈 (kFw, w→ㄹ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFw")) == U"갈");
}
TEST_CASE("keymap: 갑 (kFe, e→ㅂ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFe")) == U"갑");
}
TEST_CASE("keymap: 같 (kFr, r→ㅌ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFr")) == U"같");
}
TEST_CASE("keymap: 갘 (kFt, t→ㅋ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFt")) == U"갘");
}
TEST_CASE("keymap: 강 (kFa, a→ㅇ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFa")) == U"강");
}
TEST_CASE("keymap: 간 (kFs, s→ㄴ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFs")) == U"간");
}
TEST_CASE("keymap: 갛 (kFd, d→ㅎ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFd")) == U"갛");
}
TEST_CASE("keymap: 갚 (kFf, f→ㅍ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFf")) == U"갚");
}
TEST_CASE("keymap: 갇 (kFg, g→ㄷ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFg")) == U"갇");
}
TEST_CASE("keymap: 감 (kFz, z→ㅁ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFz")) == U"감");
}
TEST_CASE("keymap: 갔 (kFx, x→ㅆ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFx")) == U"갔");
}
TEST_CASE("keymap: 각 (kFc, c→ㄱ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFc")) == U"각");
}
TEST_CASE("keymap: 갖 (kFv, v→ㅈ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFv")) == U"갖");
}
TEST_CASE("keymap: 갗 (kFb, b→ㅊ jong)", "[keymap][jong]") {
    REQUIRE(visible(run_qwerty(U"kFb")) == U"갗");
}

// ─── 한국어 단어 시퀀스 (다음절 commit 흐름) ─────────────────────────────────

TEST_CASE("keymap: 안녕 (jFshTa)", "[keymap][word]") {
    REQUIRE(visible(run_qwerty(U"jFshTa")) == U"안녕");
}
TEST_CASE("keymap: 한국 (mFskbc)", "[keymap][word]") {
    // m=ㅎ F=ㅏ s=ㄴjong / k=ㄱ b=ㅜ(D&&!E true) c=ㄱjong
    REQUIRE(visible(run_qwerty(U"mFskbc")) == U"한국");
}
TEST_CASE("keymap: 사랑 (nFyFa)", "[keymap][word]") {
    REQUIRE(visible(run_qwerty(U"nFyFa")) == U"사랑");
}
TEST_CASE("keymap: 학교 (mFckx)", "[keymap][word]") {
    // m=ㅎ F=ㅏ c=ㄱjong / k=ㄱ x=ㅛ
    REQUIRE(visible(run_qwerty(U"mFckx")) == U"학교");
}
TEST_CASE("keymap: 안녕하세요 (jFshTamFnCjx)", "[keymap][word]") {
    REQUIRE(visible(run_qwerty(U"jFshTamFnCjx")) == U"안녕하세요");
}
TEST_CASE("keymap: 사람들 (nFyFzuGw)", "[keymap][word]") {
    // 사 ㅅㅏ / 람 ㄹㅏㅁ / 들 ㄷㅡㄹ
    REQUIRE(visible(run_qwerty(U"nFyFzuGw")) == U"사람들");
}
TEST_CASE("keymap: 신세벌식 (nDsnc;rwnDc)", "[keymap][word]") {
    // 신 ㅅㅣㄴ / 세 ㅅㅔ(c=ㅔ jung at cho-only) / 벌 ㅂㅓㄹ / 식 ㅅㅣㄱ
    REQUIRE(visible(run_qwerty(U"nDsnc;rwnDc")) == U"신세벌식");
}
TEST_CASE("keymap: 키보드 (/D;VuG)", "[keymap][word]") {
    // 키 ㅋㅣ / 보 ㅂㅗ / 드 ㄷㅡ
    REQUIRE(visible(run_qwerty(U"/D;VuG")) == U"키보드");
}
TEST_CASE("keymap: 가나다라마 (kfhfufyfif)", "[keymap][word]") {
    // 모두 lowercase f의 D&&!E true → ㅏ jung
    REQUIRE(visible(run_qwerty(U"kfhfufyfif")) == U"가나다라마");
}
TEST_CASE("keymap: 컴퓨터 (/RzpA'R)", "[keymap][word]") {
    // 컴 ㅋㅓㅁ / 퓨 ㅍㅠ / 터 ㅌㅓ
    REQUIRE(visible(run_qwerty(U"/RzpA'R")) == U"컴퓨터");
}

// ─── 클러스터 jong (자동기 combine_jong 흐름) ────────────────────────────────

TEST_CASE("keymap: 흙 (mGwc, ㄺ 클러스터)", "[keymap][cluster]") {
    REQUIRE(visible(run_qwerty(U"mGwc")) == U"흙");
}
TEST_CASE("keymap: 삶 (nFwz, ㄻ 클러스터)", "[keymap][cluster]") {
    REQUIRE(visible(run_qwerty(U"nFwz")) == U"삶");
}
TEST_CASE("keymap: 짧 (llFwe, ㄼ 클러스터 + ㅉ 쌍자음)", "[keymap][cluster][double-cho]") {
    REQUIRE(visible(run_qwerty(U"llFwe")) == U"짧");
}
TEST_CASE("keymap: 핥 (mFwr, ㄾ 클러스터)", "[keymap][cluster]") {
    REQUIRE(visible(run_qwerty(U"mFwr")) == U"핥");
}
TEST_CASE("keymap: 옳 (jVwd, ㅀ 클러스터)", "[keymap][cluster]") {
    REQUIRE(visible(run_qwerty(U"jVwd")) == U"옳");
}
TEST_CASE("keymap: 값 (kFeq, ㅄ 클러스터)", "[keymap][cluster]") {
    REQUIRE(visible(run_qwerty(U"kFeq")) == U"값");
}
TEST_CASE("keymap: 앉 (jFsv, ㄵ 클러스터)", "[keymap][cluster]") {
    REQUIRE(visible(run_qwerty(U"jFsv")) == U"앉");
}
TEST_CASE("keymap: 많 (iFsd, ㄶ 클러스터)", "[keymap][cluster]") {
    // i 키: 빈 state에서 D=false → cho M
    REQUIRE(visible(run_qwerty(U"iFsd")) == U"많");
}
TEST_CASE("keymap: 몫 (iVcq, ㄳ 클러스터)", "[keymap][cluster]") {
    // i=ㅁ V=ㅗ c=ㄱjong q=ㅅjong → ㄳ 클러스터
    REQUIRE(visible(run_qwerty(U"iVcq")) == U"몫");
}

// ─── 가상 중성 추가 — 단축 입력 다양화 ───────────────────────────────────────

TEST_CASE("keymap: 괘 (k/E, 가상ㅗ + ㅐ)", "[keymap][virtual]") {
    REQUIRE(visible(run_qwerty(U"k/E")) == U"괘");
}
TEST_CASE("keymap: 괴 (k/D, 가상ㅗ + ㅣ)", "[keymap][virtual]") {
    REQUIRE(visible(run_qwerty(U"k/D")) == U"괴");
}
TEST_CASE("keymap: 솨 (n/f, 가상ㅗ + ㅏ via 갈마)", "[keymap][virtual]") {
    // f 키: (D&&!E)||E_v_O → E_v_O true → ㅏ jung
    REQUIRE(visible(run_qwerty(U"n/f")) == U"솨");
}
TEST_CASE("keymap: 좌 (l/f, 가상ㅗ + ㅏ via 갈마)", "[keymap][virtual]") {
    REQUIRE(visible(run_qwerty(U"l/f")) == U"좌");
}
TEST_CASE("keymap: 줘 (lor, 가상ㅜ + ㅓ via 갈마)", "[keymap][virtual]") {
    // r 키: (D&&!E)||E_v_U → E_v_U true → ㅓ jung
    REQUIRE(visible(run_qwerty(U"lor")) == U"줘");
}
TEST_CASE("keymap: 둬 (uor, 가상ㅜ + ㅓ via 갈마)", "[keymap][virtual]") {
    REQUIRE(visible(run_qwerty(U"uor")) == U"둬");
}
TEST_CASE("keymap: 쥐 (lod, 가상ㅜ + ㅣ via 갈마)", "[keymap][virtual]") {
    REQUIRE(visible(run_qwerty(U"lod")) == U"쥐");
}
TEST_CASE("keymap: 쇠 (n/d, 가상ㅗ + ㅣ via 갈마)", "[keymap][virtual]") {
    REQUIRE(visible(run_qwerty(U"n/d")) == U"쇠");
}

// ─── BS — 키맵 경로 통합 테스트 ──────────────────────────────────────────────
// 자동기 BS 자체는 automaton_test에서 다루고, 여기선 keymap → automaton → BS 흐름.
// run_qwerty 헬퍼는 BS를 안 다루므로 여기 케이스들은 자동기 내부 시그널 검증.

// ─── 음절간 commit 경계 ──────────────────────────────────────────────────────

TEST_CASE("keymap: cho 두 번 다른 → commit + 새 시작", "[keymap][boundary]") {
    REQUIRE(visible(run_qwerty(U"kh")) == U"ㄱㄴ");   // 가완성 안 된 ㄱ + ㄴ cho
    REQUIRE(visible(run_qwerty(U"kFh")) == U"가ㄴ");  // 가 commit + ㄴ
}

TEST_CASE("keymap: 같은 cho 두 번 → 쌍자음", "[keymap][double-cho]") {
    REQUIRE(visible(run_qwerty(U"kk")) == U"ㄲ");
    REQUIRE(visible(run_qwerty(U"kkF")) == U"까");
}

TEST_CASE("keymap: 음절간 multi-key — 가가가 (kFkFkF)", "[keymap][boundary]") {
    REQUIRE(visible(run_qwerty(U"kFkFkF")) == U"가가가");
}

// ─── 빈 상태에서 jung 단독 ──────────────────────────────────────────────────

TEST_CASE("keymap: shift jung 단독 (cho 없음) → 호환 자모", "[keymap][standalone]") {
    // P2 정책: 자동 ㅇ 보충 안 함. 모음 단독은 호환 자모 노출.
    REQUIRE(visible(run_qwerty(U"F")) == U"ㅏ");
    REQUIRE(visible(run_qwerty(U"D")) == U"ㅣ");
    REQUIRE(visible(run_qwerty(U"V")) == U"ㅗ");
}

// ─── 오른손 uppercase 기호 layer (.ist 0x48..0x5A 매핑) ──────────────────────

TEST_CASE("keymap: 오른손 uppercase 기호 — 빈 state", "[keymap][symbol]") {
    REQUIRE(visible(run_qwerty(U"H")) == U"□");
    REQUIRE(visible(run_qwerty(U"J")) == U"'");
    REQUIRE(visible(run_qwerty(U"K")) == U"\"");
    REQUIRE(visible(run_qwerty(U"L")) == U"·");
    REQUIRE(visible(run_qwerty(U"M")) == U"…");
    REQUIRE(visible(run_qwerty(U"N")) == U"―");
    REQUIRE(visible(run_qwerty(U"P")) == U";");
    REQUIRE(visible(run_qwerty(U"U")) == U"○");
    REQUIRE(visible(run_qwerty(U"Y")) == U"×");
    REQUIRE(visible(run_qwerty(U"Z")) == U"ㆍ");
    REQUIRE(visible(run_qwerty(U"\"")) == U"/");  // Shift+'
}

TEST_CASE("keymap: 기호 — 음절 commit 후 기호 출력", "[keymap][symbol]") {
    // 진행 중 음절은 commit, 그 뒤 기호.
    REQUIRE(visible(run_qwerty(U"kFH")) == U"가□");
    REQUIRE(visible(run_qwerty(U"kFP")) == U"가;");
    REQUIRE(visible(run_qwerty(U"kFY")) == U"가×");
}

TEST_CASE("keymap: shift+I/O — 흡수 (빈 텍스트)", "[keymap][symbol]") {
    // .ist 0x49, 0x4F 미정의 → 빈 텍스트로 흡수. 음절은 flush됨.
    REQUIRE(visible(run_qwerty(U"I")) == U"");
    REQUIRE(visible(run_qwerty(U"O")) == U"");
    REQUIRE(visible(run_qwerty(U"kFI")) == U"가");
    REQUIRE(visible(run_qwerty(U"kFO")) == U"가");
}

// ─── 옛한글 ㆍ (아래아) — 시뮬레이터 검증 (기본 배열) ──────────────────────
// 시뮬 결과 (2026-05-02): kz=ᄀᆞ jzd=ᄋᆞᇂ jzz=ᄋᆞᆷ jzq=ᄋᆞᆺ jzD=ᄋᆡ jzF=ᄋᆞㅏ jpz=ᄋᆢ
// .ist 0x7A: D&&!E || E==0x1F8 ? F_ : _M

TEST_CASE("keymap: 아래아 — kz/jzd/jzq", "[keymap][corpus][araea]") {
    REQUIRE(visible(run_qwerty(U"kz"))  == U"ᄀᆞ");
    REQUIRE(visible(run_qwerty(U"jzd")) == U"ᄋᆞᇂ");
    REQUIRE(visible(run_qwerty(U"jzq")) == U"ᄋᆞᆺ");
}

TEST_CASE("keymap: 아래아 후 z = jong ㅁ — jzz", "[keymap][corpus][araea]") {
    // jung 채워진 후 두 번째 z는 갈마분기에서 _M으로 떨어짐
    REQUIRE(visible(run_qwerty(U"jzz")) == U"ᄋᆞᆷ");
}

TEST_CASE("keymap: FI compound — jzD", "[keymap][corpus][araea][unitmix]") {
    REQUIRE(visible(run_qwerty(U"jzD")) == U"ᄋᆡ");
}

TEST_CASE("keymap: F + 합성 안 되는 jung → 음절 break — jzF", "[keymap][corpus][araea]") {
    REQUIRE(visible(run_qwerty(U"jzF")) == U"ᄋᆞㅏ");
}

TEST_CASE("keymap: FF compound via 가상 — jpz", "[keymap][corpus][araea][unitmix][virtual]") {
    REQUIRE(visible(run_qwerty(U"jpz")) == U"ᄋᆢ");
}

// 시뮬: Z=ㆍ (standalone), jZ=ᄋᆞ (cho 위에 합성). jung=F 한 줄로 두 케이스 모두 커버.
TEST_CASE("keymap: shift+Z standalone — Z = ㆍ", "[keymap][corpus][araea]") {
    REQUIRE(visible(run_qwerty(U"Z")) == U"ㆍ");
}

TEST_CASE("keymap: shift+Z after cho — jZ = ᄋᆞ", "[keymap][corpus][araea]") {
    REQUIRE(visible(run_qwerty(U"jZ")) == U"ᄋᆞ");
}

// 시뮬: 빈 상태에서 jong+jong은 cluster 합성 안 됨. qq=ㅅㅅ, ㅆ 아님.
TEST_CASE("keymap: no-cho 상태 jong+jong은 standalone 누적 — qq = ㅅㅅ", "[keymap][corpus][cluster]") {
    REQUIRE(visible(run_qwerty(U"qq")) == U"ㅅㅅ");
}
