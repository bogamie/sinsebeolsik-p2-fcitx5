#include "engine.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

#include "automaton.h"
#include "keymap.h"

namespace sinsebeolsik_p2 {

FCITX_DEFINE_LOG_CATEGORY(p2_log, "sinsebeolsik-p2");
#define P2_INFO()  FCITX_LOGC(::sinsebeolsik_p2::p2_log, Info)
#define P2_DEBUG() FCITX_LOGC(::sinsebeolsik_p2::p2_log, Debug)

namespace {

// UTF-32 → UTF-8 (한글 음절 + 호환 자모 모두 BMP라 최대 3바이트)
std::string to_utf8(std::u32string_view u32) {
    std::string out;
    out.reserve(u32.size() * 3);
    for (char32_t c : u32) {
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

void set_preedit(fcitx::InputContext *ic, std::u32string_view text) {
    fcitx::Text preedit;
    if (!text.empty()) {
        preedit.append(to_utf8(text), fcitx::TextFormatFlag::Underline);
        preedit.setCursor(static_cast<int>(to_utf8(text).size()));
    }
    ic->inputPanel().setClientPreedit(preedit);
    ic->updatePreedit();
}

void commit_text(fcitx::InputContext *ic, std::u32string_view text) {
    if (text.empty()) return;
    ic->commitString(to_utf8(text));
}

// 사용자 override 또는 시스템 설치본 keymap을 찾아 로드. 못 찾거나
// 파싱 실패면 임베드된 기본값(=빌드 시점의 keymaps/sinsebeolsik_p2.toml)이
// lazy-load되어 그대로 쓰인다.
//
// 검색 순서 (윗줄이 우선):
//   1. $SIN3P2_KEYMAP                                          (개발자 override)
//   2. $XDG_CONFIG_HOME/sinsebeolsik-p2/sinsebeolsik_p2.toml
//   3. ~/.config/sinsebeolsik-p2/sinsebeolsik_p2.toml          (사용자 커스텀)
//   4. ~/.local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml
//   5. /usr/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml
//   6. /usr/local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml
void try_load_user_keymap() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    if (const char* env = std::getenv("SIN3P2_KEYMAP"); env && *env)
        candidates.emplace_back(env);

    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        candidates.emplace_back(fs::path(xdg) / "sinsebeolsik-p2/sinsebeolsik_p2.toml");

    if (const char* home = std::getenv("HOME"); home && *home) {
        fs::path h(home);
        candidates.emplace_back(h / ".config/sinsebeolsik-p2/sinsebeolsik_p2.toml");
        candidates.emplace_back(h / ".local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml");
    }
    candidates.emplace_back("/usr/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml");
    candidates.emplace_back("/usr/local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml");

    for (const auto& p : candidates) {
        std::error_code ec;
        if (!fs::exists(p, ec)) continue;
        if (auto km = sin3p2::load_keymap_from_file(p)) {
            sin3p2::install_default_keymap(std::move(*km));
            FCITX_LOGC(::sinsebeolsik_p2::p2_log, Info)
                << "Loaded P2 keymap from: " << p.string();
            return;
        }
        FCITX_LOGC(::sinsebeolsik_p2::p2_log, Warn)
            << "Failed to parse keymap at " << p.string()
            << " — continuing search";
    }
    FCITX_LOGC(::sinsebeolsik_p2::p2_log, Info) << "Using embedded P2 keymap";
}

}  // namespace

Engine::Engine(fcitx::Instance *instance)
    : instance_(instance),
      factory_([](fcitx::InputContext &) { return new P2InputState; }) {
    instance_->inputContextManager().registerProperty(
        "sinsebeolsik-p2-state", &factory_);
    try_load_user_keymap();
    P2_INFO() << "Sinsebeolsik P2 engine constructed (M3 wiring)";
}

std::vector<fcitx::InputMethodEntry> Engine::listInputMethods() {
    fcitx::InputMethodEntry entry(
        /*uniqueName=*/"sinsebeolsik-p2",
        /*name=*/_("Sinsebeolsik P2"),
        /*languageCode=*/"ko",
        /*addon=*/"sinsebeolsik-p2");
    entry.setLabel("한")
        .setIcon("fcitx-sinsebeolsik-p2-symbolic")
        .setNativeName("신세벌식 P2")
        .setConfigurable(false);
    std::vector<fcitx::InputMethodEntry> entries;
    entries.push_back(std::move(entry));
    return entries;
}

void Engine::activate(const fcitx::InputMethodEntry &,
                      fcitx::InputContextEvent &event) {
    auto *prop = event.inputContext()->propertyFor(&factory_);
    prop->state = sin3p2::State{};
    set_preedit(event.inputContext(), U"");
    P2_DEBUG() << "Sinsebeolsik P2 activated";
}

void Engine::deactivate(const fcitx::InputMethodEntry &,
                        fcitx::InputContextEvent &event) {
    auto *ic = event.inputContext();
    auto *prop = ic->propertyFor(&factory_);
    auto r = sin3p2::flush(prop->state);
    commit_text(ic, r.commit);
    prop->state = sin3p2::State{};
    set_preedit(ic, U"");
    P2_DEBUG() << "Sinsebeolsik P2 deactivated";
}

void Engine::reset(const fcitx::InputMethodEntry &,
                   fcitx::InputContextEvent &event) {
    auto *ic = event.inputContext();
    auto *prop = ic->propertyFor(&factory_);
    auto r = sin3p2::flush(prop->state);
    commit_text(ic, r.commit);
    prop->state = sin3p2::State{};
    set_preedit(ic, U"");
}

void Engine::keyEvent(const fcitx::InputMethodEntry &,
                      fcitx::KeyEvent &event) {
    if (event.isRelease()) return;

    auto *ic = event.inputContext();
    auto *prop = ic->propertyFor(&factory_);
    auto &state = prop->state;
    const auto &key = event.key();

    // Modifier 단독 키 (Shift/Ctrl/Alt/Super 누름 자체) — 무시.
    // 이 KeyEvent를 비-printable로 처리해 flush해버리면 다음에 올
    // shift+letter 조합이 깨진다 (현재 음절을 미리 commit해버림).
    if (key.isModifier()) return;

    // Ctrl/Alt/Super 조합은 자동기에서 처리하지 않고 호스트로 보냄.
    // (단축키 — Ctrl+C, Ctrl+Z 등 — 충돌 방지)
    const auto modifiers = key.states();
    const bool blocking_mod =
        modifiers.test(fcitx::KeyState::Ctrl) ||
        modifiers.test(fcitx::KeyState::Alt) ||
        modifiers.test(fcitx::KeyState::Super);
    if (blocking_mod) {
        // 진행 중 음절 commit 후 키 그대로 통과
        auto r = sin3p2::flush(state);
        commit_text(ic, r.commit);
        state = sin3p2::State{};
        set_preedit(ic, U"");
        return;  // event 미수락 → 호스트가 받음
    }

    // Backspace
    if (key.sym() == FcitxKey_BackSpace) {
        if (state.empty()) {
            // 자동기에 진행 중인 음절 없음 — 호스트가 BS 처리
            return;
        }
        auto r = sin3p2::backspace(state);
        state = r.state;
        set_preedit(ic, r.preedit);
        event.filterAndAccept();
        return;
    }

    // Printable로 변환
    const auto unicode = fcitx::Key::keySymToUnicode(key.sym());
    if (unicode == 0) {
        // 비문자 키 (방향키, F-키 등) — 진행 중 음절 commit 후 통과
        auto r = sin3p2::flush(state);
        commit_text(ic, r.commit);
        state = sin3p2::State{};
        set_preedit(ic, U"");
        return;
    }

    auto act = sin3p2::translate_p2(static_cast<char32_t>(unicode), state);
    if (!act) {
        // P2에 매핑 안 된 printable (숫자, 공백, 일부 기호) — flush + 통과
        auto r = sin3p2::flush(state);
        commit_text(ic, r.commit);
        state = sin3p2::State{};
        set_preedit(ic, U"");
        return;
    }

    if (auto* lit = std::get_if<sin3p2::LiteralText>(&*act)) {
        // 기호 layer (.ist의 단순 코드포인트 매핑) — flush + 텍스트 commit.
        // 빈 문자열이면 키 흡수만 (시뮬레이터 빈 슬롯, ex. shift+I/O).
        auto r = sin3p2::flush(state);
        commit_text(ic, r.commit);
        state = sin3p2::State{};
        commit_text(ic, lit->text);
        set_preedit(ic, U"");
        event.filterAndAccept();
        return;
    }

    const auto& in = std::get<sin3p2::Input>(*act);
    auto r = sin3p2::step(state, in);
    state = r.state;
    commit_text(ic, r.commit);
    set_preedit(ic, r.preedit);
    event.filterAndAccept();
}

fcitx::AddonInstance *EngineFactory::create(fcitx::AddonManager *manager) {
    return new Engine(manager->instance());
}

}  // namespace sinsebeolsik_p2

FCITX_ADDON_FACTORY(::sinsebeolsik_p2::EngineFactory);
