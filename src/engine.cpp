#include "engine.h"

#include <utility>
#include <vector>

#include <fcitx-utils/i18n.h>
#include <fcitx-utils/log.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/instance.h>

namespace sinsebeolsik_p2 {

FCITX_DEFINE_LOG_CATEGORY(p2_log, "sinsebeolsik-p2");
#define P2_INFO() FCITX_LOGC(::sinsebeolsik_p2::p2_log, Info)

Engine::Engine(fcitx::Instance *instance) : instance_(instance) {
    P2_INFO() << "Sinsebeolsik P2 engine constructed (M1 skeleton — no Hangul logic yet)";
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
                      fcitx::InputContextEvent &) {
    P2_INFO() << "Sinsebeolsik P2 activated";
}

void Engine::deactivate(const fcitx::InputMethodEntry &,
                        fcitx::InputContextEvent &) {
    P2_INFO() << "Sinsebeolsik P2 deactivated";
}

void Engine::keyEvent(const fcitx::InputMethodEntry &,
                      fcitx::KeyEvent &) {
    // M1: no key handling. Filled in by M2 (automaton) → M3 (fcitx5 wiring).
}

void Engine::reset(const fcitx::InputMethodEntry &,
                   fcitx::InputContextEvent &) {
    P2_INFO() << "Sinsebeolsik P2 reset";
}

fcitx::AddonInstance *EngineFactory::create(fcitx::AddonManager *manager) {
    return new Engine(manager->instance());
}

}  // namespace sinsebeolsik_p2

FCITX_ADDON_FACTORY(::sinsebeolsik_p2::EngineFactory);
