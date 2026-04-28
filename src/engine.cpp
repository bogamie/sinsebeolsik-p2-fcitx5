#include "engine.h"

#include <utility>
#include <vector>

#include <fcitx-utils/i18n.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpath.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>

namespace sinsebeolsik_p2 {

FCITX_DEFINE_LOG_CATEGORY(p2_log, "sinsebeolsik-p2");
#define P2_INFO()  FCITX_LOGC(::sinsebeolsik_p2::p2_log, Info)
#define P2_WARN()  FCITX_LOGC(::sinsebeolsik_p2::p2_log, Warn)

Engine::Engine(fcitx::Instance *instance) : instance_(instance) {
    instance_->inputContextManager().registerProperty(
        "sinsebeolsik-p2", &state_factory_);

    if (load_keymap()) {
        P2_INFO() << "Sinsebeolsik P2 engine ready (keymap loaded: "
                  << keymap_->name << " "
                  << keymap_->layout_revision << "/"
                  << keymap_->symbol_revision << ")";
    } else {
        P2_WARN() << "Sinsebeolsik P2 engine started WITHOUT a keymap; "
                  << "all keystrokes will pass through untouched.";
    }
}

bool Engine::load_keymap() {
    auto path = fcitx::StandardPath::global().locate(
        fcitx::StandardPath::Type::PkgData,
        "sinsebeolsik-p2/sinsebeolsik_p2.toml");
    if (path.empty()) {
        P2_WARN() << "keymap file 'sinsebeolsik-p2/sinsebeolsik_p2.toml' "
                  << "not found in any XDG_DATA_DIRS path";
        return false;
    }
    auto result = load_keymap_from_file(path);
    if (!result.ok()) {
        P2_WARN() << "failed to parse keymap " << path << ": " << result.error;
        return false;
    }
    keymap_ = std::move(*result.keymap);
    return true;
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
    P2_INFO() << "Sinsebeolsik P2 activated";
    auto *ic = event.inputContext();
    auto *st = ic->propertyFor(&state_factory_);
    st->automaton = State{};
    update_preedit(ic, {});
}

void Engine::deactivate(const fcitx::InputMethodEntry &,
                        fcitx::InputContextEvent &event) {
    P2_INFO() << "Sinsebeolsik P2 deactivated";
    flush(event.inputContext());
}

void Engine::reset(const fcitx::InputMethodEntry &,
                   fcitx::InputContextEvent &event) {
    P2_INFO() << "Sinsebeolsik P2 reset";
    auto *ic = event.inputContext();
    auto *st = ic->propertyFor(&state_factory_);
    st->automaton = State{};
    update_preedit(ic, {});
}

void Engine::keyEvent(const fcitx::InputMethodEntry &,
                      fcitx::KeyEvent &event) {
    if (event.isRelease()) return;
    if (!keymap_) return;  // safe no-op

    auto *ic = event.inputContext();
    auto *st = ic->propertyFor(&state_factory_);

    auto keysym  = event.key().sym();
    auto states  = event.key().states();

    // Backspace: dedicated entry point.
    if (keysym == FcitxKey_BackSpace &&
        !states.test(fcitx::KeyState::Ctrl) &&
        !states.test(fcitx::KeyState::Alt)) {
        StepResult r = backspace(st->automaton);
        st->automaton = r.next;
        if (r.consumed) {
            update_preedit(ic, r.preedit);
            event.filterAndAccept();
        }
        return;
    }

    // Build automaton-facing event.
    KeyInput k;
    k.keysym = static_cast<char32_t>(keysym);
    k.ctrl   = states.test(fcitx::KeyState::Ctrl);
    k.alt    = states.test(fcitx::KeyState::Alt);
    k.super  = states.test(fcitx::KeyState::Super);

    StepResult r = step(st->automaton, *keymap_, k);
    st->automaton = r.next;

    if (!r.commit.empty()) {
        ic->commitString(r.commit);
    }
    update_preedit(ic, r.preedit);

    if (r.consumed) {
        event.filterAndAccept();
    }
}

void Engine::flush(fcitx::InputContext *ic) {
    auto *st = ic->propertyFor(&state_factory_);
    StepResult r = commit_and_reset(st->automaton);
    st->automaton = r.next;
    if (!r.commit.empty()) {
        ic->commitString(r.commit);
    }
    update_preedit(ic, {});
}

void Engine::update_preedit(fcitx::InputContext *ic,
                            const std::string &preedit_utf8) {
    fcitx::Text text;
    if (!preedit_utf8.empty()) {
        text.append(preedit_utf8, fcitx::TextFormatFlag::Underline);
    }

    if (ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
        ic->inputPanel().setClientPreedit(text);
        ic->updatePreedit();
    } else {
        ic->inputPanel().setPreedit(text);
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }
}

fcitx::AddonInstance *EngineFactory::create(fcitx::AddonManager *manager) {
    return new Engine(manager->instance());
}

}  // namespace sinsebeolsik_p2

FCITX_ADDON_FACTORY(::sinsebeolsik_p2::EngineFactory);
