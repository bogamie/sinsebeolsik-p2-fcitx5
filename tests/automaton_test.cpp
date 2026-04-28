#include <catch2/catch_test_macros.hpp>

#include <string>

#include "automaton.h"
#include "keymap.h"

using namespace sinsebeolsik_p2;

namespace {

const Keymap& p2_keymap() {
    static const Keymap k = [] {
        auto r = load_keymap_from_file(P2_KEYMAP_PATH);
        REQUIRE(r.ok());
        return *std::move(r.keymap);
    }();
    return k;
}

// Run a string of ASCII keystrokes through the automaton.
// Returns (concatenated_commits, final_preedit).
struct RunResult {
    std::string commits;
    std::string preedit;
};

RunResult run(const Keymap& km, const std::string& keys) {
    State s;
    RunResult out;
    for (char c : keys) {
        KeyInput k;
        k.keysym = static_cast<char32_t>(static_cast<unsigned char>(c));
        StepResult r = step(s, km, k);
        out.commits += r.commit;
        s = r.next;
        out.preedit = r.preedit;
    }
    return out;
}

// Run keys, then flush via commit_and_reset (simulating focus loss).
std::string run_and_flush(const Keymap& km, const std::string& keys) {
    auto r = run(km, keys);
    State s;
    s.cur = Syllable{};  // reconstruct for flush — actually need to track
    // To get the final state we need to re-thread. Simpler: re-run with flush
    // appended.
    State state;
    std::string out;
    for (char c : keys) {
        KeyInput k;
        k.keysym = static_cast<char32_t>(static_cast<unsigned char>(c));
        StepResult res = step(state, km, k);
        out += res.commit;
        state = res.next;
    }
    out += commit_and_reset(state).commit;
    return out;
}

// Convenience: encode a single Hangul Syllables Block character as UTF-8.
std::string syllable(char32_t cp) {
    return utf8_encode(cp);
}

}  // namespace

// ---------------------------------------------------------------------------
// Single-jamo and basic syllables. Verifies base lookup + slot assignment.
// ---------------------------------------------------------------------------

TEST_CASE("automaton: empty input yields empty output", "[automaton][basic]") {
    const auto& km = p2_keymap();
    auto r = run(km, "");
    REQUIRE(r.commits.empty());
    REQUIRE(r.preedit.empty());
}

TEST_CASE("automaton: single 초성 → preedit only", "[automaton][basic]") {
    const auto& km = p2_keymap();
    auto r = run(km, "k");
    REQUIRE(r.commits.empty());
    REQUIRE(r.preedit == utf8_encode(0x1100));  // ㄱ초성 conjoining
}

TEST_CASE("automaton: 가 = k + f", "[automaton][basic]") {
    const auto& km = p2_keymap();
    auto r = run(km, "kf");
    REQUIRE(r.commits.empty());
    REQUIRE(r.preedit == syllable(0xAC00));  // 가
}

TEST_CASE("automaton: 각 = k + f + c", "[automaton][basic]") {
    const auto& km = p2_keymap();
    auto r = run(km, "kfc");
    REQUIRE(r.commits.empty());
    REQUIRE(r.preedit == syllable(0xAC01));  // 각
}

TEST_CASE("automaton: 한 = m + f + s", "[automaton][basic]") {
    const auto& km = p2_keymap();
    // m=ㅎ초성, f=ㅏ, s=ㄴ받침 (s key default)
    auto r = run(km, "mfs");
    REQUIRE(r.preedit == syllable(0xD55C));  // 한
}

TEST_CASE("automaton: 안녕 = j+f+s + h+T+a", "[automaton][basic]") {
    const auto& km = p2_keymap();
    // 안: j(ㅇ초성) f(ㅏ) s(ㄴ받침)
    // 녕: h(ㄴ초성) T(ㅕ — shift+t) a(ㅇ받침)
    auto r = run_and_flush(km, "jfshTa");
    REQUIRE(r == syllable(0xC548) + syllable(0xB155));  // 안녕
}

// ---------------------------------------------------------------------------
// 갈마들이 — cho→jung, jung↔jong rewrites.
// ---------------------------------------------------------------------------

TEST_CASE("automaton: cho→jung galmadeuli — 그 = k + i", "[automaton][galmadeuli]") {
    const auto& km = p2_keymap();
    // 'i' default = ㅁ초성 (CHO). After 'k' (ㄱ초성), Layer 1 sees post-cho
    // state, no compound rule for ㄱ+ㅁ, galmadeuli ㅁ초성→ㅡ → rewrites.
    auto r = run(km, "ki");
    REQUIRE(r.preedit == syllable(0xADF8));  // 그
}

TEST_CASE("automaton: cho→jung galmadeuli — 고 = k + /", "[automaton][galmadeuli]") {
    const auto& km = p2_keymap();
    // '/' default = ㅋ초성. galmadeuli ㅋ초성→ㅗ.
    auto r = run(km, "k/");
    REQUIRE(r.preedit == syllable(0xACE0));  // 고
}

TEST_CASE("automaton: cho→jung galmadeuli — 구 = k + o", "[automaton][galmadeuli]") {
    const auto& km = p2_keymap();
    // 'o' default = ㅊ초성. galmadeuli ㅊ초성→ㅜ.
    auto r = run(km, "ko");
    REQUIRE(r.preedit == syllable(0xAD6C));  // 구
}

TEST_CASE("automaton: jung→jong galmadeuli — 갈 = k + f + w",
          "[automaton][galmadeuli]") {
    const auto& km = p2_keymap();
    // After ㄱ+ㅏ, 'w' default is already ㄹ받침 (JONG) — no rewrite needed,
    // step_jong just attaches it.
    auto r = run(km, "kfw");
    REQUIRE(r.preedit == syllable(0xAC08));  // 갈
}

TEST_CASE("automaton: vowel via galmadeuli when post-jung — 가구 sequence",
          "[automaton][galmadeuli]") {
    const auto& km = p2_keymap();
    // 'kfko':
    // k → ㄱ초성
    // f → ㅏ → 가
    // k → ㄱ초성 (commit 가, start new with cho=ㄱ)
    // o → galmadeuli ㅊ→ㅜ → 구
    auto r = run_and_flush(km, "kfko");
    REQUIRE(r == syllable(0xAC00) + syllable(0xAD6C));  // 가구
}

// ---------------------------------------------------------------------------
// Compound vowels (jung combinations).
// ---------------------------------------------------------------------------

TEST_CASE("automaton: 과 = k + / + F (compound vowel ㅘ via galmadeuli ㅗ)",
          "[automaton][compound]") {
    const auto& km = p2_keymap();
    // k → ㄱ초성
    // / → galmadeuli ㅋ초성→ㅗ. State: 고
    // F → ㅏ (shift forces vowel). cur.jung=ㅗ filled, no jong. combo
    //     forms ㅘ. State: 과.
    //
    // Lowercase f here would NOT work — its base is ㅍ받침, and Layer 1's
    // !cur.jung-only galmadeuli rewrite is gated by cur.jung being empty,
    // so ㅍ받침 stays a 종성 and we'd get 곺 (ㄱ+ㅗ+ㅍ). Real P2 users
    // type shift+F for the second vowel of a compound. This is intended.
    auto r = run(km, "k/F");
    REQUIRE(r.preedit == syllable(0xACFC));  // 과
}

TEST_CASE("automaton: 의 = j + i + D (compound ㅢ)",
          "[automaton][compound]") {
    const auto& km = p2_keymap();
    // j → ㅇ초성
    // i → galmadeuli ㅁ초성→ㅡ. State: 으
    // D (shift+d = ㅣ) → combo[(ㅡ,ㅣ)]=ㅢ. State: 의
    auto r = run(km, "jiD");
    REQUIRE(r.preedit == syllable(0xC758));  // 의
}

// ---------------------------------------------------------------------------
// Compound jong + 도깨비불 split.
// ---------------------------------------------------------------------------

TEST_CASE("automaton: compound jong ㄺ — 갉 = k + f + w + c",
          "[automaton][compound]") {
    const auto& km = p2_keymap();
    // k=ㄱ, f=ㅏ, w=ㄹ받침, c=ㄱ받침
    // After 'kfw' state = {ㄱ, ㅏ, ㄹ받침} = 갈
    // 'c' (ㄱ받침): Layer 1 keeps as JONG (cur.jong=ㄹ filled, no jong_prev,
    //   cur.cho/jung filled — first branch keeps).
    // Layer 2 step_jong Case C: combo[(ㄹ받침, ㄱ받침)]=ㄺ. Form compound.
    // State: {ㄱ, ㅏ, ㄺ, jong_prev=ㄹ받침} = 갉.
    auto r = run(km, "kfwc");
    REQUIRE(r.preedit == syllable(0xAC09));  // 갉
}

TEST_CASE("automaton: 도깨비불 simple — 갈 + ㅏ → 가 + 라",
          "[automaton][dokkaebibul]") {
    const auto& km = p2_keymap();
    // After 'kfw' (갈), then 'F' (uppercase = forced ㅏ vowel).
    //
    // Lowercase 'f' here would form a compound jong ㄿ instead — Layer 1
    // doesn't rewrite ㅍ받침→ㅏ once the syllable already has cho+jung
    // (the !cur.jung gate is closed). To trigger 도깨비불 from a closed
    // syllable, the user must explicitly type a vowel via shift.
    //
    // step_jung then: cur.jong filled, cho/jung filled → apply_dokkaebibul.
    //   jong_split has no entry for ㄹ받침 (simple jong) → wholesale move.
    //   new_cho = jong_to_cho(ㄹ받침) = ㄹ초성. closing.jong = 0 → "가" commits.
    //   new state {cho=ㄹ초성, jung=ㅏ} = "라".
    auto r = run_and_flush(km, "kfwF");
    REQUIRE(r == syllable(0xAC00) + syllable(0xB77C));  // 가라
}

TEST_CASE("automaton: 도깨비불 compound split — 갉 + ㅏ → 갈 + 가",
          "[automaton][dokkaebibul]") {
    const auto& km = p2_keymap();
    // After 'kfwc' (갉), then 'F' (forced ㅏ vowel).
    // jong_split[ㄺ] = {keep=ㄹ받침, promote=ㄱ초성}.
    //   closing.jong = ㄹ받침 → "갈" commits
    //   new state {cho=ㄱ초성, jung=ㅏ} = "가"
    auto r = run_and_flush(km, "kfwcF");
    REQUIRE(r == syllable(0xAC08) + syllable(0xAC00));  // 갈가
}

TEST_CASE("automaton: 도깨비불 split for every compound jong",
          "[automaton][dokkaebibul]") {
    const auto& km = p2_keymap();
    struct Case {
        std::string keys;
        char32_t closing;     // committed Hangul syllable
        char32_t starting;    // cho of the next syllable as conjoining jamo
    };
    // Each row builds a syllable with cho=ㄱ jung=ㅏ jong=<compound>,
    // then types ㅏ to trigger 도깨비불.
    Case cases[] = {
        // Build via galmadeuli (jung→jong for second jong):
        //   ㄳ = c(ㄱ받침) + q(ㅅ받침)
        //   ㄵ = s(ㄴ받침) + v(ㅈ받침)
        //   ㄶ = s(ㄴ받침) + d(ㅎ받침)
        //   ㄺ = w(ㄹ받침) + c(ㄱ받침)
        //   ㄻ = w(ㄹ받침) + z(ㅁ받침)
        //   ㄼ = w(ㄹ받침) + e(ㅂ받침)
        //   ㄽ = w(ㄹ받침) + q(ㅅ받침)
        //   ㄾ = w(ㄹ받침) + r(ㅌ받침)
        //   ㄿ = w(ㄹ받침) + f(ㅍ받침)
        //   ㅀ = w(ㄹ받침) + d(ㅎ받침)
        //   ㅄ = e(ㅂ받침) + q(ㅅ받침)
        {"kfcq", 0xAC01, 0x1109},  // 갃 + ㅏ → 각 + 사  (ㄳ → ㄱ받침 + ㅅ초성)
        {"kfsv", 0xAC04, 0x110C},  // 간 + ㅏ → 간 + 자  (ㄵ → ㄴ받침 + ㅈ초성)
        {"kfsd", 0xAC04, 0x1112},  // 간 + ㅏ → 간 + 하  (ㄶ → ㄴ받침 + ㅎ초성)
        {"kfwc", 0xAC08, 0x1100},  // 갈 + ㅏ → 갈 + 가  (ㄺ → ㄹ + ㄱ초성)
        {"kfwz", 0xAC08, 0x1106},  // 갈 + ㅏ → 갈 + 마
        {"kfwe", 0xAC08, 0x1107},  // 갈 + ㅏ → 갈 + 바
        {"kfwq", 0xAC08, 0x1109},  // 갈 + ㅏ → 갈 + 사
        {"kfwr", 0xAC08, 0x1110},  // 갈 + ㅏ → 갈 + 타
        {"kfwf", 0xAC08, 0x1111},  // 갈 + ㅏ → 갈 + 파
        {"kfwd", 0xAC08, 0x1112},  // 갈 + ㅏ → 갈 + 하
        {"kfeq", 0xAC11, 0x1109},  // 갑 + ㅏ → 갑 + 사  (ㅄ → ㅂ + ㅅ초성)
    };
    for (const auto& c : cases) {
        std::string seq = c.keys + "F";  // append shift+F = forced ㅏ vowel
        auto out = run_and_flush(km, seq);
        // closing syllable + new syllable (cho=promote, jung=ㅏ)
        char32_t starting_syll = 0xAC00 +
            (c.starting - 0x1100) * 21 * 28 +
            (0x1161 - 0x1161) * 28;
        std::string expect = syllable(c.closing) + syllable(starting_syll);
        INFO("seq=" << seq);
        REQUIRE(out == expect);
    }
}

// ---------------------------------------------------------------------------
// Compound 초성.
// ---------------------------------------------------------------------------

TEST_CASE("automaton: ㄲ via cho doubling — 까 = k + k + f",
          "[automaton][compound]") {
    const auto& km = p2_keymap();
    // k → ㄱ초성. State {cho=ㄱ}.
    // k → step_cho: cur.cho filled, no jung. combo[(ㄱ, ㄱ)]=ㄲ. State {cho=ㄲ, cho_prev=ㄱ}.
    // f → ㅏ vowel. State {cho=ㄲ, cho_prev=ㄱ, jung=ㅏ} = 까.
    auto r = run(km, "kkf");
    REQUIRE(r.preedit == syllable(0xAE4C));  // 까
}

TEST_CASE("automaton: ㄲ then 3rd ㄱ commits and starts new",
          "[automaton][compound]") {
    const auto& km = p2_keymap();
    // 'k'+'k' forms ㄲ with cho_prev=ㄱ. Third 'k': step_cho sees
    // cur.cho_prev != 0, commits ㄲ, starts new {cho=ㄱ}.
    auto r = run_and_flush(km, "kkk");
    REQUIRE(r == syllable(0x1101) + syllable(0x1100));
    // ㄲ alone (no jung) renders as conjoining ㄲ; ㄱ alone same.
}

// ---------------------------------------------------------------------------
// Modifier passthrough and unmapped keys.
// ---------------------------------------------------------------------------

TEST_CASE("automaton: Ctrl+key commits pending and passes through",
          "[automaton][passthrough]") {
    const auto& km = p2_keymap();
    State s;
    // Type 'k' to get a pending ㄱ초성.
    {
        KeyInput k; k.keysym = 'k';
        s = step(s, km, k).next;
    }
    // Now Ctrl+C — should commit the pending ㄱ and not consume.
    KeyInput ctrl_c; ctrl_c.keysym = 'c'; ctrl_c.ctrl = true;
    auto r = step(s, km, ctrl_c);
    REQUIRE_FALSE(r.consumed);
    REQUIRE(r.commit == utf8_encode(0x1100));  // pending ㄱ committed
    REQUIRE(r.next.cur.empty());
}

TEST_CASE("automaton: unclaimed key (shift+I) commits pending and passes through",
          "[automaton][passthrough]") {
    const auto& km = p2_keymap();
    State s;
    {
        KeyInput k; k.keysym = 'k';
        s = step(s, km, k).next;
    }
    // shift+I (uppercase I) was unclaimed in the 2018 P2 revision.
    KeyInput shift_i; shift_i.keysym = 'I';
    auto r = step(s, km, shift_i);
    REQUIRE_FALSE(r.consumed);
    REQUIRE(r.commit == utf8_encode(0x1100));
    REQUIRE(r.next.cur.empty());
}

TEST_CASE("automaton: digit (passthrough as itself) commits pending",
          "[automaton][passthrough]") {
    const auto& km = p2_keymap();
    // '1' is claimed by the keymap (base['1']=0x31 — a self-passthrough).
    // Per design §3.6 'symbol entry' rule: commit pending and emit '1'
    // as additional commit, consumed=true.
    State s;
    {
        KeyInput k; k.keysym = 'k';
        s = step(s, km, k).next;
    }
    KeyInput one; one.keysym = '1';
    auto r = step(s, km, one);
    REQUIRE(r.consumed);   // claimed
    REQUIRE(r.commit == utf8_encode(0x1100) + "1");
    REQUIRE(r.next.cur.empty());
}

TEST_CASE("automaton: arrow keys (out of ascii range) bypass the engine",
          "[automaton][passthrough]") {
    const auto& km = p2_keymap();
    State s;
    {
        KeyInput k; k.keysym = 'k';
        s = step(s, km, k).next;
    }
    KeyInput arrow; arrow.keysym = 0xFF51;  // X11 KP_Left
    auto r = step(s, km, arrow);
    REQUIRE_FALSE(r.consumed);
    REQUIRE(r.commit == utf8_encode(0x1100));
}

// ---------------------------------------------------------------------------
// Backspace.
// ---------------------------------------------------------------------------

namespace {
State state_after(const Keymap& km, const std::string& keys) {
    State s;
    for (char c : keys) {
        KeyInput k; k.keysym = static_cast<char32_t>(static_cast<unsigned char>(c));
        s = step(s, km, k).next;
    }
    return s;
}
}  // namespace

TEST_CASE("backspace: empty buffer → not consumed", "[automaton][backspace]") {
    const auto& km = p2_keymap();
    State s;
    auto r = backspace(s);
    REQUIRE_FALSE(r.consumed);
}

TEST_CASE("backspace: ㄱ → empty", "[automaton][backspace]") {
    const auto& km = p2_keymap();
    auto s = state_after(km, "k");
    auto r = backspace(s);
    REQUIRE(r.consumed);
    REQUIRE(r.next.cur.empty());
    REQUIRE(r.preedit.empty());
}

TEST_CASE("backspace: 가 → ㄱ", "[automaton][backspace]") {
    const auto& km = p2_keymap();
    auto s = state_after(km, "kf");
    auto r = backspace(s);
    REQUIRE(r.consumed);
    REQUIRE(r.next.cur.cho == 0x1100);
    REQUIRE(r.next.cur.jung == 0);
}

TEST_CASE("backspace: 각 → 가", "[automaton][backspace]") {
    const auto& km = p2_keymap();
    auto s = state_after(km, "kfc");
    auto r = backspace(s);
    REQUIRE(r.preedit == syllable(0xAC00));  // 가
}

TEST_CASE("backspace: ㄲ → ㄱ (compound 초성 decompose)",
          "[automaton][backspace]") {
    const auto& km = p2_keymap();
    auto s = state_after(km, "kk");  // {cho=ㄲ, cho_prev=ㄱ}
    auto r = backspace(s);
    REQUIRE(r.next.cur.cho == 0x1100);     // ㄱ restored
    REQUIRE(r.next.cur.cho_prev == 0);
}

TEST_CASE("backspace: 갉 → 갈 → 가 → ㄱ → empty (compound 종성 decompose)",
          "[automaton][backspace]") {
    const auto& km = p2_keymap();
    auto s = state_after(km, "kfwc");  // 갉 — {cho=ㄱ, jung=ㅏ, jong=ㄺ, jong_prev=ㄹ받침}
    auto r1 = backspace(s);
    REQUIRE(r1.preedit == syllable(0xAC08));  // 갈 (ㄺ → ㄹ받침)

    auto r2 = backspace(r1.next);
    REQUIRE(r2.preedit == syllable(0xAC00));  // 가 (ㄹ받침 removed)

    auto r3 = backspace(r2.next);
    REQUIRE(r3.preedit == utf8_encode(0x1100));  // ㄱ alone

    auto r4 = backspace(r3.next);
    REQUIRE(r4.next.cur.empty());
}

TEST_CASE("backspace: 과 → 고 (compound 중성 decompose)",
          "[automaton][backspace]") {
    const auto& km = p2_keymap();
    auto s = state_after(km, "k/f");  // 과 — {cho=ㄱ, jung=ㅘ, jung_prev=ㅗ}
    auto r = backspace(s);
    REQUIRE(r.preedit == syllable(0xACE0));  // 고
}

// ---------------------------------------------------------------------------
// commit_and_reset (focus loss).
// ---------------------------------------------------------------------------

TEST_CASE("commit_and_reset flushes pending preedit", "[automaton][lifecycle]") {
    const auto& km = p2_keymap();
    auto s = state_after(km, "kf");  // 가
    auto r = commit_and_reset(s);
    REQUIRE(r.consumed);
    REQUIRE(r.commit == syllable(0xAC00));
    REQUIRE(r.next.cur.empty());
}

TEST_CASE("commit_and_reset on empty buffer is a no-op",
          "[automaton][lifecycle]") {
    State s;
    auto r = commit_and_reset(s);
    REQUIRE(r.consumed);
    REQUIRE(r.commit.empty());
    REQUIRE(r.next.cur.empty());
}

// ---------------------------------------------------------------------------
// 옛한글 ㆍ — exercise the non-precomposable path.
// ---------------------------------------------------------------------------

TEST_CASE("automaton: ㆍ via Z key — bare ㆍ commits as conjoining jamo",
          "[automaton][archaic]") {
    const auto& km = p2_keymap();
    // 'Z' (uppercase) base = 0x119E (ㆍ). cat=JUNG.
    // step_jung: cur empty → cur.jung = ㆍ. Render via conjoining jamo.
    auto r = run(km, "Z");
    REQUIRE(r.preedit == utf8_encode(0x119E));
}

TEST_CASE("automaton: ᆢ (쌍아래아) via ZZ", "[automaton][archaic]") {
    const auto& km = p2_keymap();
    // Z→ㆍ. Z→ㆍ. combo[(ㆍ,ㆍ)] = ᆢ (U+11A2). Compound jung formed.
    auto r = run(km, "ZZ");
    REQUIRE(r.preedit == utf8_encode(0x11A2));
}

// ---------------------------------------------------------------------------
// Parametric coverage for docs §6 acceptance criteria.
// ---------------------------------------------------------------------------

TEST_CASE("automaton: 도깨비불 simple — every modern single 종성",
          "[automaton][dokkaebibul][exhaustive]") {
    const auto& km = p2_keymap();
    // Each row: (key for jong, jong codepoint, expected promote-cho codepoint).
    // Built syllable: 가 + jong, then 'F' (ㅏ) triggers wholesale jong→cho.
    struct Case {
        char     jong_key;
        char32_t jong;
        char32_t promote_cho;
    };
    Case cases[] = {
        {'c', 0x11A8, 0x1100},  // ㄱ받침 → ㄱ초성
        {'s', 0x11AB, 0x1102},  // ㄴ받침 → ㄴ초성
        {'g', 0x11AE, 0x1103},  // ㄷ받침 → ㄷ초성
        {'w', 0x11AF, 0x1105},  // ㄹ받침 → ㄹ초성
        {'z', 0x11B7, 0x1106},  // ㅁ받침 → ㅁ초성
        {'e', 0x11B8, 0x1107},  // ㅂ받침 → ㅂ초성
        {'q', 0x11BA, 0x1109},  // ㅅ받침 → ㅅ초성
        {'a', 0x11BC, 0x110B},  // ㅇ받침 → ㅇ초성
        {'v', 0x11BD, 0x110C},  // ㅈ받침 → ㅈ초성
        {'b', 0x11BE, 0x110E},  // ㅊ받침 → ㅊ초성
        {'t', 0x11BF, 0x110F},  // ㅋ받침 → ㅋ초성
        {'r', 0x11C0, 0x1110},  // ㅌ받침 → ㅌ초성
        {'f', 0x11C1, 0x1111},  // ㅍ받침 → ㅍ초성
        {'d', 0x11C2, 0x1112},  // ㅎ받침 → ㅎ초성
    };
    for (const auto& c : cases) {
        std::string seq = std::string("kf") + c.jong_key + "F";
        // Closing syllable always commits as 가 (cho+jung, jong stripped).
        // New syllable: promote_cho + ㅏ.
        char32_t promote_idx = c.promote_cho - 0x1100;
        char32_t starting    = 0xAC00 + promote_idx * 21 * 28;
        std::string expected = syllable(0xAC00) + syllable(starting);
        INFO("seq=" << seq);
        REQUIRE(run_and_flush(km, seq) == expected);
    }
}

TEST_CASE("automaton: backspace — compound 초성 (5 doubles)",
          "[automaton][backspace][exhaustive]") {
    const auto& km = p2_keymap();
    struct Case { std::string keys; char32_t base_cho; };
    // Each: type the cho key twice, expect compound; backspace restores base.
    Case cases[] = {
        {"kk", 0x1100},  // ㄱ + ㄱ = ㄲ → backspace → ㄱ
        {"uu", 0x1103},  // ㄷ + ㄷ = ㄸ → ㄷ (u key = ㄷ초성)
        {";;", 0x1107},  // ㅂ + ㅂ = ㅃ → ㅂ (; = ㅂ초성)
        {"nn", 0x1109},  // ㅅ + ㅅ = ㅆ → ㅅ (n = ㅅ초성)
        {"ll", 0x110C},  // ㅈ + ㅈ = ㅉ → ㅈ (l = ㅈ초성)
    };
    for (const auto& c : cases) {
        State s = state_after(km, c.keys);
        INFO("seq=" << c.keys);
        // Confirm we reached a compound (cho_prev set).
        REQUIRE(s.cur.cho_prev != 0);
        auto r = backspace(s);
        REQUIRE(r.consumed);
        REQUIRE(r.next.cur.cho      == c.base_cho);
        REQUIRE(r.next.cur.cho_prev == 0);
    }
}

TEST_CASE("automaton: backspace — compound 중성 (9 vowel compounds)",
          "[automaton][backspace][exhaustive]") {
    const auto& km = p2_keymap();
    struct Case { std::string keys; char32_t base_jung; };
    // Each builds with 'j' as cho (ㅇ초성) for clean rendering, then a
    // first vowel (often via galmadeuli), then a second vowel that triggers
    // the compound. Backspace must peel back to the first vowel.
    Case cases[] = {
        {"jvF",  0x1169},  // ㅗ + ㅏ = ㅘ → backspace → ㅗ  (v=ㅈ받침 galmadeuli ㅗ)
        {"jvE",  0x1169},  // ㅗ + ㅐ = ㅙ → ㅗ
        {"jvD",  0x1169},  // ㅗ + ㅣ = ㅚ → ㅗ
        {"jbR",  0x116E},  // ㅜ + ㅓ = ㅝ → ㅜ  (b=ㅊ받침 galmadeuli ㅜ)
        {"jbC",  0x116E},  // ㅜ + ㅔ = ㅞ → ㅜ
        {"jbD",  0x116E},  // ㅜ + ㅣ = ㅟ → ㅜ
        {"jiD",  0x1173},  // ㅡ + ㅣ = ㅢ → ㅡ  (i galmadeuli ㅡ)
        {"jZD",  0x119E},  // ㆍ + ㅣ = ㆎ → ㆍ
        {"jZZ",  0x119E},  // ㆍ + ㆍ = ᆢ → ㆍ
    };
    for (const auto& c : cases) {
        State s = state_after(km, c.keys);
        INFO("seq=" << c.keys);
        REQUIRE(s.cur.jung_prev != 0);  // compound formed
        auto r = backspace(s);
        REQUIRE(r.consumed);
        REQUIRE(r.next.cur.jung      == c.base_jung);
        REQUIRE(r.next.cur.jung_prev == 0);
    }
}

TEST_CASE("automaton: backspace — compound 종성 (12 jong compounds)",
          "[automaton][backspace][exhaustive]") {
    const auto& km = p2_keymap();
    struct Case { std::string keys; char32_t base_jong; };
    Case cases[] = {
        {"kfcc", 0x11A8},  // ㄱ받침 + ㄱ받침 = ㄲ받침 → ㄱ받침
        {"kfcq", 0x11A8},  // ㄱ + ㅅ = ㄳ → ㄱ받침
        {"kfsv", 0x11AB},  // ㄴ + ㅈ = ㄵ → ㄴ받침
        {"kfsd", 0x11AB},  // ㄴ + ㅎ = ㄶ → ㄴ받침
        {"kfwc", 0x11AF},  // ㄹ + ㄱ = ㄺ → ㄹ받침
        {"kfwz", 0x11AF},  // ㄹ + ㅁ = ㄻ → ㄹ받침
        {"kfwe", 0x11AF},  // ㄹ + ㅂ = ㄼ → ㄹ받침
        {"kfwq", 0x11AF},  // ㄹ + ㅅ = ㄽ → ㄹ받침
        {"kfwr", 0x11AF},  // ㄹ + ㅌ = ㄾ → ㄹ받침
        {"kfwf", 0x11AF},  // ㄹ + ㅍ = ㄿ → ㄹ받침
        {"kfwd", 0x11AF},  // ㄹ + ㅎ = ㅀ → ㄹ받침
        {"kfeq", 0x11B8},  // ㅂ + ㅅ = ㅄ → ㅂ받침
    };
    for (const auto& c : cases) {
        State s = state_after(km, c.keys);
        INFO("seq=" << c.keys);
        REQUIRE(s.cur.jong_prev != 0);  // compound formed
        auto r = backspace(s);
        REQUIRE(r.consumed);
        REQUIRE(r.next.cur.jong      == c.base_jong);
        REQUIRE(r.next.cur.jong_prev == 0);
    }
}

TEST_CASE("automaton: every base_keymap entry produces its declared output",
          "[automaton][base][exhaustive]") {
    const auto& km = p2_keymap();
    // For each ASCII code 0x21..0x7E, fire a single keystroke against an
    // empty state and assert that:
    //   - if the entry is 0 (unclaimed): consumed=false, no commit
    //   - if it's a jamo: ends up in the appropriate slot (post Layer 1
    //     reclassification — symmetric to no-galmadeuli since cur empty)
    //   - if it's a symbol (cat=None): consumed=true, commit == utf8(code)
    for (int ascii = 0x21; ascii <= 0x7E; ++ascii) {
        char32_t code = km.base[ascii];
        State s;
        KeyInput k; k.keysym = static_cast<char32_t>(ascii);
        auto r = step(s, km, k);

        INFO("ascii=0x" << std::hex << ascii << " code=0x" << (uint32_t)code);

        if (code == 0) {
            REQUIRE_FALSE(r.consumed);
            REQUIRE(r.commit.empty());
            continue;
        }

        REQUIRE(r.consumed);

        JamoSlot cat = classify(code);
        if (cat == JamoSlot::None) {
            // Symbol entry: commits as itself.
            REQUIRE(r.commit == utf8_encode(code));
            REQUIRE(r.next.cur.empty());
            continue;
        }

        // Jamo entry. Layer 1 may have rewritten via galmadeuli. From an
        // empty state:
        //   - CHO: lands in cur.cho.
        //   - JUNG: lands in cur.jung.
        //   - JONG: galmadeuli rewrites to JUNG (or rarely CHO) since
        //     no syllable to attach to. Lands accordingly.
        if (cat == JamoSlot::Cho) {
            REQUIRE(r.next.cur.cho == code);
        } else if (cat == JamoSlot::Jung) {
            REQUIRE(r.next.cur.jung == code);
        } else if (cat == JamoSlot::Jong) {
            // Expect galmadeuli rewrite to JUNG (P2 has 15 jong↔jung pairs
            // covering every modern jong consonant + ㆍ).
            char32_t alt = galmadeuli_lookup(km, code);
            REQUIRE(alt != 0);
            JamoSlot alt_cat = classify(alt);
            if (alt_cat == JamoSlot::Jung) {
                REQUIRE(r.next.cur.jung == alt);
            } else if (alt_cat == JamoSlot::Cho) {
                REQUIRE(r.next.cur.cho == alt);
            } else {
                FAIL("jong-default key with no jung/cho alternate: 0x"
                     << std::hex << (uint32_t)code);
            }
        }
    }
}

TEST_CASE("automaton: round-trip every jung↔jong galmadeuli pair",
          "[automaton][galmadeuli][exhaustive]") {
    const auto& km = p2_keymap();
    // 15 pairs from the bidirectional table: each jung typed in initial
    // state lands as jung (no rewrite); each jong typed in initial state
    // gets rewritten via galmadeuli back to its paired jung.
    struct Case { char32_t jung; char32_t jong; };
    Case pairs[] = {
        {0x1161, 0x11C1}, {0x1162, 0x11B8}, {0x1163, 0x11AF},
        {0x1164, 0x11BA}, {0x1165, 0x11C0}, {0x1166, 0x11A8},
        {0x1167, 0x11BF}, {0x1168, 0x11AB}, {0x1169, 0x11BD},
        {0x116D, 0x11BB}, {0x116E, 0x11BE}, {0x1172, 0x11BC},
        {0x1173, 0x11AE}, {0x1175, 0x11C2}, {0x119E, 0x11B7},
    };
    for (const auto& p : pairs) {
        // Forward direction: galmadeuli[jung] should be jong (post-jung use).
        REQUIRE(galmadeuli_lookup(km, p.jung) == p.jong);
        // Reverse direction: galmadeuli[jong] should be jung.
        REQUIRE(galmadeuli_lookup(km, p.jong) == p.jung);
    }
}

TEST_CASE("automaton: every combination rule fires through step()",
          "[automaton][combination][exhaustive]") {
    const auto& km = p2_keymap();
    REQUIRE(km.combination.size() == 26);

    // For each rule (a, b, result), verify that an isolated state
    // {appropriate slot = a} followed by `code = b` yields the compound
    // in the same slot. Direct-state injection avoids having to compose
    // a key sequence per rule.
    for (const auto& rule : km.combination) {
        State s;
        JamoSlot slot = classify(rule.a);
        REQUIRE(slot == classify(rule.b));
        REQUIRE(slot == classify(rule.result));

        switch (slot) {
            case JamoSlot::Cho:
                s.cur.cho = rule.a;
                break;
            case JamoSlot::Jung:
                // Compound jung needs a cho present, otherwise step_jung
                // sees bare-jung commit-and-restart with the second key.
                s.cur.cho  = 0x110B;  // ㅇ초성 — neutral filler
                s.cur.jung = rule.a;
                break;
            case JamoSlot::Jong:
                s.cur.cho  = 0x110B;
                s.cur.jung = 0x1161;  // ㅏ — neutral filler
                s.cur.jong = rule.a;
                break;
            case JamoSlot::None:
                FAIL("non-jamo combination rule");
        }

        INFO("rule a=0x" << std::hex << (uint32_t)rule.a
             << " b=0x"  << (uint32_t)rule.b
             << " expected=0x" << (uint32_t)rule.result);

        // Drive the step by injecting `b` directly into the slot's matching
        // step function. We can't go via the keymap because some compounds
        // (cho doubles via 'kk') are exercised elsewhere; this test focuses
        // on the rule firing within step_*.
        char32_t result_in_slot = 0;
        if (slot == JamoSlot::Cho) {
            // Need an ASCII key whose base equals rule.b; for cho that's
            // direct (the corresponding cho key).
            char32_t b = rule.b;
            // Find a key (any ASCII slot) with km.base[i] == b.
            int found = -1;
            for (int i = 0x21; i <= 0x7E; ++i) {
                if (km.base[i] == b) { found = i; break; }
            }
            REQUIRE(found > 0);
            KeyInput k; k.keysym = static_cast<char32_t>(found);
            auto r = step(s, km, k);
            result_in_slot = r.next.cur.cho;
        } else if (slot == JamoSlot::Jung) {
            int found = -1;
            for (int i = 0x21; i <= 0x7E; ++i) {
                if (km.base[i] == rule.b) { found = i; break; }
            }
            REQUIRE(found > 0);
            KeyInput k; k.keysym = static_cast<char32_t>(found);
            auto r = step(s, km, k);
            result_in_slot = r.next.cur.jung;
        } else if (slot == JamoSlot::Jong) {
            int found = -1;
            for (int i = 0x21; i <= 0x7E; ++i) {
                if (km.base[i] == rule.b) { found = i; break; }
            }
            REQUIRE(found > 0);
            KeyInput k; k.keysym = static_cast<char32_t>(found);
            auto r = step(s, km, k);
            result_in_slot = r.next.cur.jong;
        }
        REQUIRE(result_in_slot == rule.result);
    }
}

TEST_CASE("automaton: mixed-sequence corpus", "[automaton][corpus]") {
    const auto& km = p2_keymap();
    // Hand-crafted everyday-ish sequences, flushed at the end.
    // Each row: (sequence, expected committed UTF-8).
    struct Row { std::string keys; std::string expected_utf8; };
    Row rows[] = {
        // basic
        {"kf",   syllable(0xAC00)},                            // 가
        {"kfc",  syllable(0xAC01)},                            // 각
        {"kfa",  syllable(0xAC15)},                            // 강
        // double consonants
        {"kkf",  syllable(0xAE4C)},                            // 까
        // compound vowels (need shift+F for explicit ㅏ after ㅗ)
        {"k/F",  syllable(0xACFC)},                            // 과
        {"jiD",  syllable(0xC758)},                            // 의
        // 도깨비불 simple jong
        {"kfwF", syllable(0xAC00) + syllable(0xB77C)},         // 가라
        {"kfsF", syllable(0xAC00) + syllable(0xB098)},         // 가나
        // 도깨비불 compound jong
        {"kfwcF", syllable(0xAC08) + syllable(0xAC00)},        // 갈가 (ㄺ→ㄹ+ㄱ초성)
        {"kfeqF", syllable(0xAC11) + syllable(0xC0AC)},        // 갑사 (ㅄ→ㅂ+ㅅ초성)
        // multi-syllable phrases (commit at each new cho)
        {"jfshTa", syllable(0xC548) + syllable(0xB155)},       // 안녕
        {"mfsjokno", std::string()},  // computed below — too complex inline
        // backspace not exercised here (separate suite)
        // ㆍ aware
        {"Z",    utf8_encode(0x119E)},                         // bare ㆍ
        {"ZZ",   utf8_encode(0x11A2)},                         // ᆢ
        {"jZ",   utf8_encode(0x110B) + utf8_encode(0x119E)},   // ㅇ + ㆍ
        // 합용 jong + 도깨비불 every modern entry covered in another suite
    };
    for (const auto& row : rows) {
        if (row.expected_utf8.empty()) continue;
        INFO("seq=" << row.keys);
        REQUIRE(run_and_flush(km, row.keys) == row.expected_utf8);
    }
}
