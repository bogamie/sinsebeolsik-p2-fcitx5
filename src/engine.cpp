#include "engine.h"

#include <string>
#include <string_view>
#include <utility>
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

}  // namespace

Engine::Engine(fcitx::Instance *instance)
    : instance_(instance),
      factory_([](fcitx::InputContext &) { return new P2InputState; }) {
    instance_->inputContextManager().registerProperty(
        "sinsebeolsik-p2-state", &factory_);
    P2_INFO() << "Sinsebeolsik P2 engine constructed (M3 wiring)";
}

std::vector<fcitx::InputMethodEntry> Engine::listInputMethods() {
    fcitx::InputMethodEntry entry(
        /*uniqueName=*/"sinsebeolsik-p2",
        /*name=*/_("Sinsebeolsik P2"),
        /*languageCode=*/"ko",
        /*addon=*/"sinsebeolsik-p2");
    entry.setLabel("신P2")
        .setIcon("fcitx-sinsebeolsik-p2")
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

    auto in = sin3p2::translate_p2(static_cast<char32_t>(unicode), state);
    if (!in) {
        // P2에 매핑 안 된 printable (숫자, 공백, 일부 기호) — flush + 통과
        auto r = sin3p2::flush(state);
        commit_text(ic, r.commit);
        state = sin3p2::State{};
        set_preedit(ic, U"");
        return;
    }

    auto r = sin3p2::step(state, *in);
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
