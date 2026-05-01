#include "qwerty_translator.h"

#include <xkbcommon/xkbcommon.h>

namespace sin3p2 {

struct QwertyTranslator::State {
    xkb_context* ctx = nullptr;
    xkb_keymap* keymap = nullptr;
    // Shift OFF / ON 두 state를 미리 만들어 두고 lookup 시 골라 쓴다.
    // (매 키 입력마다 update_mask 호출하지 않아도 되도록.)
    xkb_state* base = nullptr;
    xkb_state* shifted = nullptr;
};

QwertyTranslator::QwertyTranslator() : s_(new State) {
    s_->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!s_->ctx) return;

    // RMLVO: rules=evdev, layout=us. 시스템 xkeyboard-config에서 us 정의를
    // 찾아 컴파일. 사용자의 활성 XKB 레이아웃과 무관하게 동작.
    xkb_rule_names names{};
    names.rules = "evdev";
    names.model = "pc105";
    names.layout = "us";
    names.variant = "";
    names.options = "";
    s_->keymap = xkb_keymap_new_from_names(s_->ctx, &names,
                                           XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!s_->keymap) return;

    s_->base = xkb_state_new(s_->keymap);
    s_->shifted = xkb_state_new(s_->keymap);
    if (s_->shifted) {
        const auto shift_idx =
            xkb_keymap_mod_get_index(s_->keymap, XKB_MOD_NAME_SHIFT);
        if (shift_idx != XKB_MOD_INVALID) {
            xkb_state_update_mask(s_->shifted,
                                  /*depressed=*/1u << shift_idx,
                                  /*latched=*/0, /*locked=*/0,
                                  /*depressed_layout=*/0,
                                  /*latched_layout=*/0,
                                  /*locked_layout=*/0);
        }
    }
}

QwertyTranslator::~QwertyTranslator() {
    if (!s_) return;
    if (s_->base) xkb_state_unref(s_->base);
    if (s_->shifted) xkb_state_unref(s_->shifted);
    if (s_->keymap) xkb_keymap_unref(s_->keymap);
    if (s_->ctx) xkb_context_unref(s_->ctx);
    delete s_;
}

bool QwertyTranslator::ok() const noexcept {
    return s_ && s_->keymap && s_->base && s_->shifted;
}

std::uint32_t QwertyTranslator::translate(int keycode, bool shift,
                                          std::uint32_t fallback_sym) const {
    if (!ok() || keycode <= 0) return fallback_sym;
    auto* st = shift ? s_->shifted : s_->base;
    const auto sym =
        xkb_state_key_get_one_sym(st, static_cast<xkb_keycode_t>(keycode));
    if (sym == XKB_KEY_NoSymbol) return fallback_sym;
    return static_cast<std::uint32_t>(sym);
}

}  // namespace sin3p2
