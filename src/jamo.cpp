#include "jamo.h"

#include <array>

namespace sinsebeolsik_p2 {

namespace {

// Hangul Syllables Block composition constants (Unicode 3.12 / KS X 1026-1).
constexpr char32_t SBASE  = 0xAC00;
constexpr char32_t LBASE  = 0x1100;
constexpr char32_t VBASE  = 0x1161;
constexpr char32_t TBASE  = 0x11A7;     // (jong_index - 1); 0x11A8 → t_index 1
constexpr int     LCOUNT  = 19;
constexpr int     VCOUNT  = 21;
constexpr int     TCOUNT  = 28;          // includes t_index 0 = "no jong"
constexpr int     NCOUNT  = VCOUNT * TCOUNT;
constexpr char32_t TLAST  = 0x11C2;     // last valid jong (ㅎ받침)
constexpr char32_t LLAST  = LBASE + LCOUNT - 1;  // 0x1112
constexpr char32_t VLAST  = VBASE + VCOUNT - 1;  // 0x1175

// 14 simple 종성 ↔ 초성 pairs. Compound 종성 (ㄲ ㄳ ㄺ ...) and the
// compound 초성 (ㄲ ㄸ ㅃ ㅆ ㅉ) are intentionally excluded.
struct JongChoMap {
    char32_t jong;
    char32_t cho;
};

constexpr std::array<JongChoMap, 14> kJongCho = {{
    {0x11A8, 0x1100},  // ㄱ
    {0x11AB, 0x1102},  // ㄴ
    {0x11AE, 0x1103},  // ㄷ
    {0x11AF, 0x1105},  // ㄹ
    {0x11B7, 0x1106},  // ㅁ
    {0x11B8, 0x1107},  // ㅂ
    {0x11BA, 0x1109},  // ㅅ
    {0x11BC, 0x110B},  // ㅇ
    {0x11BD, 0x110C},  // ㅈ
    {0x11BE, 0x110E},  // ㅊ
    {0x11BF, 0x110F},  // ㅋ
    {0x11C0, 0x1110},  // ㅌ
    {0x11C1, 0x1111},  // ㅍ
    {0x11C2, 0x1112},  // ㅎ
}};

}  // namespace

JamoSlot classify(char32_t code) noexcept {
    if (code >= LBASE && code <= LLAST)              return JamoSlot::Cho;
    if (code >= VBASE && code <= VLAST)              return JamoSlot::Jung;
    if (code == 0x119E)                              return JamoSlot::Jung;  // ㆍ
    if (code >= 0x11A1 && code <= 0x11A2)            return JamoSlot::Jung;  // ㆎ ᆢ
    if (code >= 0x11A8 && code <= TLAST)             return JamoSlot::Jong;
    return JamoSlot::None;
}

bool is_modern_jung(char32_t code) noexcept {
    return code >= VBASE && code <= VLAST;
}

char32_t jong_to_cho(char32_t code) noexcept {
    for (const auto& m : kJongCho) {
        if (m.jong == code) return m.cho;
    }
    return 0;
}

char32_t cho_to_jong(char32_t code) noexcept {
    for (const auto& m : kJongCho) {
        if (m.cho == code) return m.jong;
    }
    return 0;
}

std::optional<char32_t> compose(char32_t cho, char32_t jung, char32_t jong) noexcept {
    if (cho < LBASE || cho > LLAST)      return std::nullopt;
    if (!is_modern_jung(jung))           return std::nullopt;

    int t_index = 0;
    if (jong != 0) {
        if (jong < 0x11A8 || jong > TLAST) return std::nullopt;
        t_index = static_cast<int>(jong - TBASE);
    }
    int l_index = static_cast<int>(cho - LBASE);
    int v_index = static_cast<int>(jung - VBASE);

    return static_cast<char32_t>(SBASE + l_index * NCOUNT + v_index * TCOUNT + t_index);
}

std::string utf8_encode(char32_t cp) {
    std::string out;
    if (cp == 0) return out;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string render(const Syllable& s) {
    if (s.empty()) return {};

    if (s.cho && s.jung) {
        if (auto syll = compose(s.cho, s.jung, s.jong)) {
            return utf8_encode(*syll);
        }
    }

    std::string out;
    if (s.cho)  out += utf8_encode(s.cho);
    if (s.jung) out += utf8_encode(s.jung);
    if (s.jong) out += utf8_encode(s.jong);
    return out;
}

}  // namespace sinsebeolsik_p2
