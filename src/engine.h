#pragma once

#include <optional>
#include <vector>

#include <fcitx/addonfactory.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodentry.h>

#include "automaton.h"
#include "keymap.h"

namespace fcitx {
class Instance;
class AddonManager;
class InputContext;
class InputContextEvent;
class KeyEvent;
}  // namespace fcitx

namespace sinsebeolsik_p2 {

// Per-InputContext composing state. fcitx5 owns the lifetime via
// InputContextProperty (cleaned up automatically when the IC is destroyed).
struct ComposingState : public fcitx::InputContextProperty {
    sinsebeolsik_p2::State automaton{};
};

class Engine final : public fcitx::InputMethodEngineV2 {
public:
    explicit Engine(fcitx::Instance *instance);

    std::vector<fcitx::InputMethodEntry> listInputMethods() override;

    void activate(const fcitx::InputMethodEntry &entry,
                  fcitx::InputContextEvent &event) override;
    void deactivate(const fcitx::InputMethodEntry &entry,
                    fcitx::InputContextEvent &event) override;
    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &event) override;
    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

private:
    void update_preedit(fcitx::InputContext *ic,
                        const std::string &preedit_utf8);
    void flush(fcitx::InputContext *ic);
    bool load_keymap();

    fcitx::Instance *instance_;
    fcitx::FactoryFor<ComposingState> state_factory_{
        [](fcitx::InputContext &) { return new ComposingState; }};
    std::optional<Keymap> keymap_;
};

class EngineFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override;
};

}  // namespace sinsebeolsik_p2
