#pragma once

#include <vector>

#include <fcitx/addonfactory.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodentry.h>

namespace fcitx {
class Instance;
class AddonManager;
class InputContextEvent;
class KeyEvent;
}  // namespace fcitx

namespace sinsebeolsik_p2 {

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
    fcitx::Instance *instance_;
};

class EngineFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override;
};

}  // namespace sinsebeolsik_p2
