#include "keymap.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include <toml++/toml.hpp>

namespace sin3p2 {

// ─── state predicates ────────────────────────────────────────────────────────

namespace {

struct Predicates {
    bool D = false;
    bool E = false;
    bool F = false;
    bool E_v_O = false;
    bool E_v_U = false;
    bool E_v_EU = false;
    bool E_v_F = false;
    bool E_any_v = false;
};

Predicates predicates_of(const State& s) {
    Predicates p;
    p.D = s.cho.has_value();
    p.E = s.has_jung();
    p.F = s.jong.has_value();
    if (auto* vj = std::get_if<VJung>(&s.jung)) {
        p.E_any_v = true;
        p.E_v_O   = (*vj == VJung::O);
        p.E_v_U   = (*vj == VJung::U);
        p.E_v_EU  = (*vj == VJung::EU);
        p.E_v_F   = (*vj == VJung::F);
    }
    return p;
}

}  // namespace

// ─── Predicate AST ───────────────────────────────────────────────────────────

struct Keymap::Predicate {
    enum class AtomKind { D, E, F, E_v_O, E_v_U, E_v_EU, E_v_F, E_any_v };
    enum class Kind { Atom, Not, And, Or };

    Kind kind = Kind::Atom;
    AtomKind atom = AtomKind::D;
    std::shared_ptr<const Predicate> a;
    std::shared_ptr<const Predicate> b;

    bool eval(const Predicates& p) const noexcept {
        switch (kind) {
            case Kind::Atom:
                switch (atom) {
                    case AtomKind::D:       return p.D;
                    case AtomKind::E:       return p.E;
                    case AtomKind::F:       return p.F;
                    case AtomKind::E_v_O:   return p.E_v_O;
                    case AtomKind::E_v_U:   return p.E_v_U;
                    case AtomKind::E_v_EU:  return p.E_v_EU;
                    case AtomKind::E_v_F:   return p.E_v_F;
                    case AtomKind::E_any_v: return p.E_any_v;
                }
                return false;
            case Kind::Not: return !a->eval(p);
            case Kind::And: return a->eval(p) && b->eval(p);
            case Kind::Or:  return a->eval(p) || b->eval(p);
        }
        return false;
    }
};

// ─── predicate parser (recursive descent) ────────────────────────────────────

namespace {

using PredPtr = std::shared_ptr<const Keymap::Predicate>;

struct Lexer {
    std::string_view src;
    std::size_t pos = 0;

    void skip_ws() noexcept {
        while (pos < src.size() &&
               std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
    }
    bool eof() noexcept { skip_ws(); return pos >= src.size(); }

    bool peek_str(std::string_view s) noexcept {
        skip_ws();
        if (pos + s.size() > src.size()) return false;
        return src.compare(pos, s.size(), s) == 0;
    }
    bool eat_str(std::string_view s) noexcept {
        if (peek_str(s)) { pos += s.size(); return true; }
        return false;
    }
    bool eat_char(char c) noexcept {
        skip_ws();
        if (pos < src.size() && src[pos] == c) { ++pos; return true; }
        return false;
    }
    std::string_view ident() noexcept {
        skip_ws();
        std::size_t start = pos;
        while (pos < src.size() &&
               (std::isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_'))
            ++pos;
        return src.substr(start, pos - start);
    }
};

std::optional<Keymap::Predicate::AtomKind> name_to_atom(std::string_view s) {
    using A = Keymap::Predicate::AtomKind;
    if (s == "D") return A::D;
    if (s == "E") return A::E;
    if (s == "F") return A::F;
    if (s == "E_v_O") return A::E_v_O;
    if (s == "E_v_U") return A::E_v_U;
    if (s == "E_v_EU") return A::E_v_EU;
    if (s == "E_v_F") return A::E_v_F;
    if (s == "E_any_v") return A::E_any_v;
    return std::nullopt;
}

PredPtr parse_or_expr(Lexer& lx);

PredPtr parse_unary(Lexer& lx) {
    if (lx.eat_char('!')) {
        auto inner = parse_unary(lx);
        if (!inner) return nullptr;
        auto p = std::make_shared<Keymap::Predicate>();
        p->kind = Keymap::Predicate::Kind::Not;
        p->a = inner;
        return p;
    }
    if (lx.eat_char('(')) {
        auto inner = parse_or_expr(lx);
        if (!inner) return nullptr;
        if (!lx.eat_char(')')) return nullptr;
        return inner;
    }
    auto name = lx.ident();
    if (name.empty()) return nullptr;
    auto atom = name_to_atom(name);
    if (!atom) return nullptr;
    auto p = std::make_shared<Keymap::Predicate>();
    p->kind = Keymap::Predicate::Kind::Atom;
    p->atom = *atom;
    return p;
}

PredPtr parse_and_expr(Lexer& lx) {
    auto lhs = parse_unary(lx);
    if (!lhs) return nullptr;
    while (lx.eat_str("&&")) {
        auto rhs = parse_unary(lx);
        if (!rhs) return nullptr;
        auto p = std::make_shared<Keymap::Predicate>();
        p->kind = Keymap::Predicate::Kind::And;
        p->a = lhs;
        p->b = rhs;
        lhs = p;
    }
    return lhs;
}

PredPtr parse_or_expr(Lexer& lx) {
    auto lhs = parse_and_expr(lx);
    if (!lhs) return nullptr;
    while (lx.eat_str("||")) {
        auto rhs = parse_and_expr(lx);
        if (!rhs) return nullptr;
        auto p = std::make_shared<Keymap::Predicate>();
        p->kind = Keymap::Predicate::Kind::Or;
        p->a = lhs;
        p->b = rhs;
        lhs = p;
    }
    return lhs;
}

PredPtr parse_predicate(std::string_view s) {
    Lexer lx{s, 0};
    auto p = parse_or_expr(lx);
    if (!p || !lx.eof()) return nullptr;
    return p;
}

// ─── jamo enum parsers ───────────────────────────────────────────────────────

std::optional<Cho> name_to_cho(std::string_view s) {
    if (s == "G")  return Cho::G;
    if (s == "GG") return Cho::GG;
    if (s == "N")  return Cho::N;
    if (s == "D")  return Cho::D;
    if (s == "DD") return Cho::DD;
    if (s == "R")  return Cho::R;
    if (s == "M")  return Cho::M;
    if (s == "B")  return Cho::B;
    if (s == "BB") return Cho::BB;
    if (s == "S")  return Cho::S;
    if (s == "SS") return Cho::SS;
    if (s == "O")  return Cho::O;
    if (s == "J")  return Cho::J;
    if (s == "JJ") return Cho::JJ;
    if (s == "C")  return Cho::C;
    if (s == "K")  return Cho::K;
    if (s == "T")  return Cho::T;
    if (s == "P")  return Cho::P;
    if (s == "H")  return Cho::H;
    return std::nullopt;
}

std::optional<Jung> name_to_jung(std::string_view s) {
    if (s == "A")   return Jung::A;
    if (s == "AE")  return Jung::AE;
    if (s == "YA")  return Jung::YA;
    if (s == "YAE") return Jung::YAE;
    if (s == "EO")  return Jung::EO;
    if (s == "E")   return Jung::E;
    if (s == "YEO") return Jung::YEO;
    if (s == "YE")  return Jung::YE;
    if (s == "O")   return Jung::O;
    if (s == "WA")  return Jung::WA;
    if (s == "WAE") return Jung::WAE;
    if (s == "OI")  return Jung::OI;
    if (s == "YO")  return Jung::YO;
    if (s == "U")   return Jung::U;
    if (s == "UEO") return Jung::UEO;
    if (s == "WE")  return Jung::WE;
    if (s == "WI")  return Jung::WI;
    if (s == "YU")  return Jung::YU;
    if (s == "EU")  return Jung::EU;
    if (s == "EUI") return Jung::EUI;
    if (s == "I")   return Jung::I;
    return std::nullopt;
}

std::optional<VJung> name_to_vjung(std::string_view s) {
    if (s == "O")  return VJung::O;
    if (s == "U")  return VJung::U;
    if (s == "EU") return VJung::EU;
    if (s == "F")  return VJung::F;
    return std::nullopt;
}

std::optional<Jong> name_to_jong(std::string_view s) {
    if (s == "G")  return Jong::G;
    if (s == "GG") return Jong::GG;
    if (s == "GS") return Jong::GS;
    if (s == "N")  return Jong::N;
    if (s == "NJ") return Jong::NJ;
    if (s == "NH") return Jong::NH;
    if (s == "D")  return Jong::D;
    if (s == "R")  return Jong::R;
    if (s == "RG") return Jong::RG;
    if (s == "RM") return Jong::RM;
    if (s == "RB") return Jong::RB;
    if (s == "RS") return Jong::RS;
    if (s == "RT") return Jong::RT;
    if (s == "RP") return Jong::RP;
    if (s == "RH") return Jong::RH;
    if (s == "M")  return Jong::M;
    if (s == "B")  return Jong::B;
    if (s == "BS") return Jong::BS;
    if (s == "S")  return Jong::S;
    if (s == "SS") return Jong::SS;
    if (s == "O")  return Jong::O;
    if (s == "J")  return Jong::J;
    if (s == "C")  return Jong::C;
    if (s == "K")  return Jong::K;
    if (s == "T")  return Jong::T;
    if (s == "P")  return Jong::P;
    if (s == "H")  return Jong::H;
    return std::nullopt;
}

// rule.output 테이블 → KeyOutput. 정확히 1개의 슬롯만 채워져야 함.
std::optional<KeyOutput> parse_output(const toml::table& tbl) {
    int filled = 0;
    KeyOutput out{PassThrough{}};

    if (auto v = tbl["cho"].value<std::string>()) {
        auto c = name_to_cho(*v);
        if (!c) return std::nullopt;
        out = InputCho{*c};
        ++filled;
    }
    if (auto v = tbl["jung"].value<std::string>()) {
        auto j = name_to_jung(*v);
        if (!j) return std::nullopt;
        out = InputJung{*j};
        ++filled;
    }
    if (auto v = tbl["vjung"].value<std::string>()) {
        auto j = name_to_vjung(*v);
        if (!j) return std::nullopt;
        out = InputVJung{*j};
        ++filled;
    }
    if (auto v = tbl["jong"].value<std::string>()) {
        auto j = name_to_jong(*v);
        if (!j) return std::nullopt;
        out = InputJong{*j};
        ++filled;
    }
    if (auto v = tbl["passthrough"].value<bool>()) {
        if (*v) {
            out = PassThrough{};
            ++filled;
        }
    }
    if (filled != 1) return std::nullopt;
    return out;
}

}  // namespace

// ─── Keymap public impl ──────────────────────────────────────────────────────

void Keymap::set_rules(char32_t key, std::vector<Rule> rules) {
    rules_[key] = std::move(rules);
}

std::optional<Input> Keymap::translate(char32_t k, const State& s) const {
    auto it = rules_.find(k);
    if (it == rules_.end()) return std::nullopt;
    auto preds = predicates_of(s);
    for (const auto& rule : it->second) {
        if (rule.when && !rule.when->eval(preds)) continue;
        return std::visit(
            [](auto&& v) -> std::optional<Input> {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, PassThrough>) {
                    return std::nullopt;
                } else {
                    return Input{v};
                }
            },
            rule.output);
    }
    return std::nullopt;
}

// ─── load_keymap_from_string / from_file ─────────────────────────────────────

std::optional<Keymap> load_keymap_from_string(std::string_view toml_src) {
    toml::table tbl;
    try {
        tbl = toml::parse(toml_src);
    } catch (const toml::parse_error&) {
        return std::nullopt;
    }

    Keymap km;

    if (auto* meta = tbl["meta"].as_table()) {
        if (auto name = (*meta)["name"].value<std::string>())
            km.set_meta_name(*name);
    }

    auto* keys = tbl["key"].as_array();
    if (!keys) return std::nullopt;

    for (auto& node : *keys) {
        auto* entry = node.as_table();
        if (!entry) return std::nullopt;

        auto key_str = (*entry)["key"].value<std::string>();
        if (!key_str || key_str->size() != 1) return std::nullopt;
        // key는 ASCII printable 1바이트 — 그대로 unsigned char로 캐스트.
        char32_t qkey =
            static_cast<char32_t>(static_cast<unsigned char>((*key_str)[0]));

        auto* rules_arr = (*entry)["rules"].as_array();
        if (!rules_arr) return std::nullopt;

        std::vector<Keymap::Rule> rules;
        rules.reserve(rules_arr->size());
        for (auto& rnode : *rules_arr) {
            auto* rtbl = rnode.as_table();
            if (!rtbl) return std::nullopt;

            Keymap::Rule rule;
            if (auto when = (*rtbl)["when"].value<std::string>()) {
                rule.when = parse_predicate(*when);
                if (!rule.when) return std::nullopt;
            }

            auto* out_tbl = (*rtbl)["output"].as_table();
            if (!out_tbl) return std::nullopt;
            auto out = parse_output(*out_tbl);
            if (!out) return std::nullopt;
            rule.output = std::move(*out);

            rules.push_back(std::move(rule));
        }

        km.set_rules(qkey, std::move(rules));
    }

    return km;
}

std::optional<Keymap> load_keymap_from_file(const std::filesystem::path& path) {
    std::ifstream is(path);
    if (!is) return std::nullopt;
    std::ostringstream os;
    os << is.rdbuf();
    return load_keymap_from_string(os.str());
}

// ─── default keymap singleton ────────────────────────────────────────────────

namespace {

std::mutex g_default_mu;
std::unique_ptr<Keymap> g_default;

const Keymap& ensure_default_locked() {
    if (!g_default) {
        auto km = load_keymap_from_string(EMBEDDED_P2_KEYMAP_TOML);
        // 임베드된 TOML이 깨졌다면 빌드 자체가 깨진 것 — 호출 측이 살아남을 수 없음.
        if (!km) std::abort();
        g_default = std::make_unique<Keymap>(std::move(*km));
    }
    return *g_default;
}

}  // namespace

const Keymap& default_keymap() {
    std::lock_guard<std::mutex> lk(g_default_mu);
    return ensure_default_locked();
}

void install_default_keymap(Keymap km) {
    std::lock_guard<std::mutex> lk(g_default_mu);
    g_default = std::make_unique<Keymap>(std::move(km));
}

std::optional<Input> translate_p2(char32_t k, const State& s) {
    return default_keymap().translate(k, s);
}

}  // namespace sin3p2
