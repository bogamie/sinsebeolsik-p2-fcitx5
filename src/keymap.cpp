#include "keymap.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <toml++/toml.hpp>

#include "jamo.h"

namespace sinsebeolsik_p2 {

namespace {

constexpr uint64_t pack_pair(char32_t a, char32_t b) noexcept {
    return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
}

std::optional<std::pair<int64_t, int64_t>>
parse_pair_array(const toml::array* arr) {
    if (!arr || arr->size() != 2) return std::nullopt;
    auto a = (*arr)[0].value<int64_t>();
    auto b = (*arr)[1].value<int64_t>();
    if (!a || !b) return std::nullopt;
    return std::make_pair(*a, *b);
}

std::optional<std::tuple<int64_t, int64_t, int64_t>>
parse_triple_array(const toml::array* arr) {
    if (!arr || arr->size() != 3) return std::nullopt;
    auto a = (*arr)[0].value<int64_t>();
    auto b = (*arr)[1].value<int64_t>();
    auto c = (*arr)[2].value<int64_t>();
    if (!a || !b || !c) return std::nullopt;
    return std::make_tuple(*a, *b, *c);
}

void derive_jong_split(Keymap& km) {
    for (const auto& rule : km.combination) {
        if (classify(rule.result) != JamoSlot::Jong) continue;
        if (classify(rule.a)      != JamoSlot::Jong) continue;
        if (classify(rule.b)      != JamoSlot::Jong) continue;
        char32_t promote = jong_to_cho(rule.b);
        if (promote == 0) continue;  // e.g., ㅆ받침 has no simple cho-form
        km.jong_split[rule.result] = JongSplit{rule.a, promote};
    }
}

LoadResult build_from_table(const toml::table& tbl) {
    Keymap km;

    if (auto meta = tbl["meta"].as_table()) {
        if (auto v = (*meta)["name"].value<std::string>())             km.name = *v;
        if (auto v = (*meta)["layout_revision"].value<std::string>())  km.layout_revision = *v;
        if (auto v = (*meta)["symbol_revision"].value<std::string>())  km.symbol_revision = *v;
        if (auto v = (*meta)["scope"].value<std::string>())            km.scope = *v;
    }

    if (auto opts = tbl["options"].as_table()) {
        if (auto v = (*opts)["ancient_hangul"].value<bool>())     km.ancient_hangul = *v;
        if (auto v = (*opts)["extended_symbols"].value<bool>())   km.extended_symbols = *v;
    }

    // base_keymap: array of [ascii, output] pairs
    auto base = tbl["base_keymap"].as_array();
    if (!base) {
        return {std::nullopt, "missing 'base_keymap' array"};
    }
    for (const auto& elem : *base) {
        auto pair = parse_pair_array(elem.as_array());
        if (!pair) {
            return {std::nullopt, "base_keymap entry must be [ascii, output]"};
        }
        if (pair->first < 0x21 || pair->first > 0x7E) {
            std::ostringstream oss;
            oss << "base_keymap ascii out of range 0x21..0x7E: 0x"
                << std::hex << pair->first;
            return {std::nullopt, oss.str()};
        }
        km.base[pair->first] = static_cast<char32_t>(pair->second);
    }

    // [galmadeuli] one_way + bidirectional
    if (auto galma = tbl["galmadeuli"].as_table()) {
        if (auto one_way = (*galma)["one_way"].as_array()) {
            for (const auto& e : *one_way) {
                auto p = parse_pair_array(e.as_array());
                if (!p) return {std::nullopt, "galmadeuli.one_way entry malformed"};
                km.galmadeuli.push_back({
                    static_cast<char32_t>(p->first),
                    static_cast<char32_t>(p->second),
                });
            }
        }
        if (auto bid = (*galma)["bidirectional"].as_array()) {
            for (const auto& e : *bid) {
                auto p = parse_pair_array(e.as_array());
                if (!p) return {std::nullopt, "galmadeuli.bidirectional entry malformed"};
                char32_t a = static_cast<char32_t>(p->first);
                char32_t b = static_cast<char32_t>(p->second);
                km.galmadeuli.push_back({a, b});
                km.galmadeuli.push_back({b, a});  // auto inverse
            }
        }
    }

    std::sort(km.galmadeuli.begin(), km.galmadeuli.end(),
              [](const GalmadeuliEntry& l, const GalmadeuliEntry& r) {
                  return l.from < r.from;
              });

    // Detect duplicate `from` entries (would shadow each other on lookup).
    for (size_t i = 1; i < km.galmadeuli.size(); ++i) {
        if (km.galmadeuli[i].from == km.galmadeuli[i - 1].from) {
            std::ostringstream oss;
            oss << "galmadeuli has duplicate 'from' codepoint: 0x"
                << std::hex << static_cast<uint32_t>(km.galmadeuli[i].from)
                << " (would the auto-inverse of a one_way entry collide "
                << "with a bidirectional pair?)";
            return {std::nullopt, oss.str()};
        }
    }

    // [combination] rules
    if (auto combo = tbl["combination"].as_table()) {
        if (auto rules = (*combo)["rules"].as_array()) {
            for (const auto& e : *rules) {
                auto t = parse_triple_array(e.as_array());
                if (!t) return {std::nullopt, "combination.rules entry malformed"};
                km.combination.push_back({
                    static_cast<char32_t>(std::get<0>(*t)),
                    static_cast<char32_t>(std::get<1>(*t)),
                    static_cast<char32_t>(std::get<2>(*t)),
                });
            }
        }
    }

    std::sort(km.combination.begin(), km.combination.end(),
              [](const CombinationRule& l, const CombinationRule& r) {
                  return pack_pair(l.a, l.b) < pack_pair(r.a, r.b);
              });

    derive_jong_split(km);

    return {std::move(km), {}};
}

}  // namespace

char32_t galmadeuli_lookup(const Keymap& km, char32_t code) noexcept {
    auto it = std::lower_bound(
        km.galmadeuli.begin(), km.galmadeuli.end(), code,
        [](const GalmadeuliEntry& e, char32_t c) { return e.from < c; });
    if (it != km.galmadeuli.end() && it->from == code) return it->to;
    return 0;
}

char32_t combination_lookup(const Keymap& km, char32_t a, char32_t b) noexcept {
    uint64_t key = pack_pair(a, b);
    auto it = std::lower_bound(
        km.combination.begin(), km.combination.end(), key,
        [](const CombinationRule& r, uint64_t k) {
            return pack_pair(r.a, r.b) < k;
        });
    if (it != km.combination.end() && pack_pair(it->a, it->b) == key) {
        return it->result;
    }
    return 0;
}

LoadResult load_keymap_from_string(std::string_view toml_text) {
    try {
        auto tbl = toml::parse(toml_text);
        return build_from_table(tbl);
    } catch (const toml::parse_error& e) {
        std::ostringstream oss;
        oss << "TOML parse error: " << e.description()
            << " at line " << e.source().begin.line
            << ":" << e.source().begin.column;
        return {std::nullopt, oss.str()};
    }
}

LoadResult load_keymap_from_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        return {std::nullopt, "cannot open keymap file: " + path};
    }
    std::ostringstream buf;
    buf << f.rdbuf();
    return load_keymap_from_string(buf.str());
}

}  // namespace sinsebeolsik_p2
