#pragma once

#include <vector>

#include <fcitx/addonfactory.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodentry.h>

#include "automaton.h"

namespace fcitx {
class Instance;
class AddonManager;
class InputContextEvent;
class KeyEvent;
}  // namespace fcitx

namespace sinsebeolsik_p2 {

// 입력 컨텍스트별 자동기 상태 (창/필드마다 독립)
class P2InputState final : public fcitx::InputContextProperty {
public:
    sin3p2::State state{};

    // INT5 (Muhenkan) passthrough modifier — set while INT5 is held by the
    // host. While true, every other key is forwarded to the host without P2
    // automaton processing. Keyboards (e.g. ZMK on Totem) wrap a symbol-layer
    // key with INT5 down/up to bypass P2 jamo capture for `;`, `'`, `/`, `"`.
    // Reset on activate/deactivate/reset to prevent stuck state across IME
    // toggles or focus changes that may swallow the release event.
    bool int5_held = false;
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

    fcitx::FactoryFor<P2InputState>& factory() { return factory_; }

private:
    fcitx::Instance *instance_;
    fcitx::FactoryFor<P2InputState> factory_;
};

class EngineFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override;
};

}  // namespace sinsebeolsik_p2
