#include "server/weapon_script_profile.h"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace cssv34::reconstructed {
namespace {

struct IceSubkey {
    std::uint32_t val[3]{};
};

constexpr std::array<std::array<std::uint16_t, 4>, 4> kIceSmod{{
    {{333, 313, 505, 369}},
    {{379, 375, 319, 391}},
    {{361, 445, 451, 397}},
    {{397, 425, 395, 505}},
}};
constexpr std::array<std::array<std::uint8_t, 4>, 4> kIceSxor{{
    {{0x83, 0x85, 0x9b, 0xcd}},
    {{0xcc, 0xa7, 0xad, 0x41}},
    {{0x4b, 0x2e, 0xd4, 0x33}},
    {{0xea, 0xcb, 0x2e, 0x04}},
}};
constexpr std::array<std::uint32_t, 32> kIcePbox{{
    0x00000001u, 0x00000080u, 0x00000400u, 0x00002000u,
    0x00080000u, 0x00200000u, 0x01000000u, 0x40000000u,
    0x00000008u, 0x00000020u, 0x00000100u, 0x00004000u,
    0x00010000u, 0x00800000u, 0x04000000u, 0x20000000u,
    0x00000004u, 0x00000010u, 0x00000200u, 0x00008000u,
    0x00020000u, 0x00400000u, 0x08000000u, 0x10000000u,
    0x00000002u, 0x00000040u, 0x00000800u, 0x00001000u,
    0x00040000u, 0x00100000u, 0x02000000u, 0x80000000u,
}};
constexpr std::array<int, 16> kIceKeyrot{{
    0, 1, 2, 3, 2, 1, 3, 0,
    1, 3, 2, 0, 3, 1, 0, 2,
}};
constexpr std::array<unsigned char, 8> kCssIceKey{{
    'd', '7', 'N', 'S', 'u', 'L', 'q', '2'
}};

std::uint32_t GfMult(std::uint32_t a, std::uint32_t b, const std::uint32_t m) noexcept {
    std::uint32_t result = 0;
    while (b != 0) {
        if ((b & 1u) != 0) result ^= a;
        a <<= 1u;
        b >>= 1u;
        if (a >= 256u) a ^= m;
    }
    return result;
}

std::uint32_t GfExp7(const std::uint32_t b, const std::uint32_t m) noexcept {
    if (b == 0) return 0;
    std::uint32_t x = GfMult(b, b, m);
    x = GfMult(b, x, m);
    x = GfMult(x, x, m);
    return GfMult(b, x, m);
}

std::uint32_t Perm32(std::uint32_t x) noexcept {
    std::uint32_t result = 0;
    std::size_t bit = 0;
    while (x != 0 && bit < kIcePbox.size()) {
        if ((x & 1u) != 0) result |= kIcePbox[bit];
        ++bit;
        x >>= 1u;
    }
    return result;
}

const std::array<std::array<std::uint32_t, 1024>, 4>& IceSboxes() noexcept {
    static const auto boxes = [] {
        std::array<std::array<std::uint32_t, 1024>, 4> result{};
        for (std::size_t i = 0; i < 1024; ++i) {
            const int col = static_cast<int>((i >> 1u) & 0xffu);
            const int row = static_cast<int>((i & 1u) | ((i & 0x200u) >> 8u));
            for (std::size_t box = 0; box < 4; ++box) {
                const auto value = GfExp7(
                    static_cast<std::uint32_t>(col ^ kIceSxor[box][row]),
                    kIceSmod[box][row]);
                const unsigned shift = static_cast<unsigned>((3u - box) * 8u);
                result[box][i] = Perm32(value << shift);
            }
        }
        return result;
    }();
    return boxes;
}

std::uint32_t IceF(const std::uint32_t p, const IceSubkey& sk) noexcept {
    const auto& sbox = IceSboxes();
    const std::uint32_t tl = ((p >> 16u) & 0x3ffu) |
                             (((p >> 14u) | (p << 18u)) & 0xffc00u);
    const std::uint32_t tr = (p & 0x3ffu) | ((p << 2u) & 0xffc00u);
    std::uint32_t al = sk.val[2] & (tl ^ tr);
    std::uint32_t ar = al ^ tr;
    al ^= tl;
    al ^= sk.val[0];
    ar ^= sk.val[1];
    return sbox[0][al >> 10u] |
           sbox[1][al & 0x3ffu] |
           sbox[2][ar >> 10u] |
           sbox[3][ar & 0x3ffu];
}

void BuildSchedule(std::array<IceSubkey, 8>& schedule,
                   std::array<std::uint16_t, 4>& keyBits) noexcept {
    for (int i = 0; i < 8; ++i) {
        const int kr = kIceKeyrot[static_cast<std::size_t>(i)];
        auto& subkey = schedule[static_cast<std::size_t>(i)];
        for (int j = 0; j < 15; ++j) {
            std::uint32_t& current = subkey.val[static_cast<std::size_t>(j % 3)];
            for (int k = 0; k < 4; ++k) {
                auto& bits = keyBits[static_cast<std::size_t>((kr + k) & 3)];
                const std::uint16_t bit = static_cast<std::uint16_t>(bits & 1u);
                current = (current << 1u) | bit;
                bits = static_cast<std::uint16_t>((bits >> 1u) | ((bit ^ 1u) << 15u));
            }
        }
    }
}

const std::array<IceSubkey, 8>& Build4100Schedule() noexcept {
    static const auto schedule = [] {
        std::array<IceSubkey, 8> result{};
        std::array<std::uint16_t, 4> kb{};
        for (int i = 0; i < 4; ++i) {
            kb[static_cast<std::size_t>(3 - i)] = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(kCssIceKey[static_cast<std::size_t>(i * 2)]) << 8u) |
                kCssIceKey[static_cast<std::size_t>(i * 2 + 1)]);
        }
        BuildSchedule(result, kb);
        return result;
    }();
    return schedule;
}

void DecryptBlock(const unsigned char* const input, unsigned char* const output) noexcept {
    const auto& schedule = Build4100Schedule();
    std::uint32_t left = (static_cast<std::uint32_t>(input[0]) << 24u) |
                         (static_cast<std::uint32_t>(input[1]) << 16u) |
                         (static_cast<std::uint32_t>(input[2]) << 8u) |
                         static_cast<std::uint32_t>(input[3]);
    std::uint32_t right = (static_cast<std::uint32_t>(input[4]) << 24u) |
                          (static_cast<std::uint32_t>(input[5]) << 16u) |
                          (static_cast<std::uint32_t>(input[6]) << 8u) |
                          static_cast<std::uint32_t>(input[7]);
    for (int i = 7; i > 0; i -= 2) {
        left ^= IceF(right, schedule[static_cast<std::size_t>(i)]);
        right ^= IceF(left, schedule[static_cast<std::size_t>(i - 1)]);
    }
    for (int i = 0; i < 4; ++i) {
        output[3 - i] = static_cast<unsigned char>(right & 0xffu);
        output[7 - i] = static_cast<unsigned char>(left & 0xffu);
        right >>= 8u;
        left >>= 8u;
    }
}

bool IsSpace(const char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::vector<std::string> TokenizeKeyValues(const std::string_view input) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < input.size()) {
        if (IsSpace(input[i])) {
            ++i;
            continue;
        }
        if (input[i] == '/' && i + 1 < input.size() && input[i + 1] == '/') {
            i += 2;
            while (i < input.size() && input[i] != '\n') ++i;
            continue;
        }
        if (input[i] == '{' || input[i] == '}') {
            tokens.emplace_back(1, input[i++]);
            continue;
        }
        if (input[i] == '"') {
            ++i;
            std::string token;
            while (i < input.size() && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < input.size()) {
                    ++i;
                }
                token.push_back(input[i++]);
            }
            if (i < input.size() && input[i] == '"') ++i;
            tokens.push_back(std::move(token));
            continue;
        }
        const std::size_t start = i;
        while (i < input.size() && !IsSpace(input[i]) && input[i] != '{' && input[i] != '}' &&
               input[i] != '"') {
            ++i;
        }
        if (i > start) tokens.emplace_back(input.substr(start, i - start));
    }
    return tokens;
}

bool ParseFloat(const std::string& text, float& out) noexcept {
    errno = 0;
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || !end || *end != '\0' || !std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

bool ParseInt(const std::string& text, int& out) noexcept {
    errno = 0;
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || !end || *end != '\0' || value < 0 || value > 128) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool ParseBool(const std::string& text, bool& out) noexcept {
    if (text == "1" || text == "true" || text == "TRUE" || text == "yes") {
        out = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "FALSE" || text == "no") {
        out = false;
        return true;
    }
    return false;
}

}  // namespace

bool DecodeBuild4100Ice(std::vector<unsigned char>& bytes) noexcept {
    if (bytes.empty()) return false;
    std::array<unsigned char, 8> block{};
    for (std::size_t offset = 0; offset + 8 <= bytes.size(); offset += 8) {
        DecryptBlock(bytes.data() + offset, block.data());
        for (std::size_t i = 0; i < block.size(); ++i) bytes[offset + i] = block[i];
    }
    return true;
}

bool ParseBuild4100WeaponScript(const std::string_view text,
                                WeaponRuntimeProfile& profile) noexcept {
    const auto tokens = TokenizeKeyValues(text);
    if (tokens.size() < 3 || tokens[0] != "WeaponData" || tokens[1] != "{") return false;

    bool sawDamage = false;
    bool sawRange = false;
    bool sawRangeModifier = false;
    bool sawArmor = false;
    bool sawCycle = false;
    int depth = 1;
    for (std::size_t i = 2; i < tokens.size() && depth > 0;) {
        if (tokens[i] == "{") {
            ++depth;
            ++i;
            continue;
        }
        if (tokens[i] == "}") {
            --depth;
            ++i;
            continue;
        }
        if (depth != 1 || i + 1 >= tokens.size() || tokens[i + 1] == "{" || tokens[i + 1] == "}") {
            ++i;
            continue;
        }

        const std::string& key = tokens[i];
        const std::string& value = tokens[i + 1];
        float parsed = 0.0f;
        int parsedInt = 0;
        if (key == "Damage" && ParseFloat(value, parsed) && parsed > 0.0f) {
            profile.baseDamage = parsed;
            sawDamage = true;
        } else if (key == "Range" && ParseFloat(value, parsed) && parsed > 0.0f) {
            profile.range = parsed;
            sawRange = true;
        } else if (key == "RangeModifier" && ParseFloat(value, parsed) && parsed > 0.0f && parsed <= 1.0f) {
            profile.rangeModifier = parsed;
            sawRangeModifier = true;
        } else if (key == "WeaponArmorRatio" && ParseFloat(value, parsed) && parsed > 0.0f) {
            profile.armorRatio = parsed;
            sawArmor = true;
        } else if (key == "CycleTime" && ParseFloat(value, parsed) && parsed > 0.0f) {
            profile.cycleTime = parsed;
            sawCycle = true;
        } else if (key == "Bullets" && ParseInt(value, parsedInt) && parsedInt > 0) {
            profile.bulletsPerShot = parsedInt;
        } else if (key == "Penetration" && ParseInt(value, parsedInt)) {
            profile.penetration = parsedInt;
        } else if (key == "AccuracyDivisor" && ParseFloat(value, parsed) && parsed >= -1.0f) {
            profile.accuracyDivisor = parsed;
        } else if (key == "AccuracyQuadratic") {
            bool quadratic = false;
            if (ParseBool(value, quadratic)) {
                profile.accuracyQuadratic = quadratic ? std::uint8_t{1} : std::uint8_t{0};
            }
        } else if (key == "AccuracyOffset" && ParseFloat(value, parsed) && parsed >= 0.0f) {
            profile.accuracyOffset = parsed;
        } else if (key == "MaxInaccuracy" && ParseFloat(value, parsed) && parsed >= 0.0f) {
            profile.maxInaccuracy = parsed;
        }
        i += 2;
    }
    return sawDamage && sawRange && sawRangeModifier && sawArmor && sawCycle;
}

bool LoadBuild4100WeaponScript(const char* const path, const bool encrypted,
                               WeaponRuntimeProfile& profile) noexcept {
    if (!path || !*path) return false;
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
        if (bytes.empty()) return false;
        if (encrypted && !DecodeBuild4100Ice(bytes)) return false;
        const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return ParseBuild4100WeaponScript(text, profile);
    } catch (...) {
        return false;
    }
}

}  // namespace cssv34::reconstructed
