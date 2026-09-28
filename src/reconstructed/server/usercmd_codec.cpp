#include "server/usercmd_codec.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace cssv34::reconstructed {
namespace {

inline std::uint32_t RotateLeft(const std::uint32_t value, const std::uint32_t bits) noexcept {
    return (value << bits) | (value >> (32U - bits));
}

std::uint32_t ReadUnsignedBits(build4100::BitReadBuffer& buffer, const int bits) noexcept {
    if (bits <= 0 || bits > 32 || !buffer.data || buffer.curBit < 0 || buffer.dataBits < 0 ||
        buffer.curBit > buffer.dataBits || bits > buffer.dataBits - buffer.curBit) {
        buffer.overflow = true;
        if (buffer.dataBits >= 0) buffer.curBit = buffer.dataBits;
        return 0;
    }

    std::uint32_t value = 0;
    for (int bit = 0; bit < bits; ++bit) {
        const int sourceBit = buffer.curBit + bit;
        const std::uint8_t byte = buffer.data[sourceBit >> 3];
        value |= static_cast<std::uint32_t>((byte >> (sourceBit & 7)) & 1U) << bit;
    }
    buffer.curBit += bits;
    return value;
}

bool ReadOneBit(build4100::BitReadBuffer& buffer) noexcept {
    return ReadUnsignedBits(buffer, 1) != 0;
}

std::int32_t ReadSignedBits(build4100::BitReadBuffer& buffer, const int bits) noexcept {
    const std::uint32_t value = ReadUnsignedBits(buffer, bits);
    if (buffer.overflow) return 0;
    if (bits == 32) return static_cast<std::int32_t>(value);
    const std::uint32_t sign = 1U << (bits - 1);
    return static_cast<std::int32_t>((value ^ sign) - sign);
}

float ReadBitAngle(build4100::BitReadBuffer& buffer, const int bits) noexcept {
    const std::uint32_t value = ReadUnsignedBits(buffer, bits);
    if (buffer.overflow) return 0.0f;
    return static_cast<float>(value) * (360.0f / static_cast<float>(std::uint32_t{1} << bits));
}

// MD5 of exactly four little-endian seed bytes. Valve MD5_PseudoRandom returns
// the unaligned dword beginning at digest byte 6.
std::array<std::uint8_t, 16> Md5FourBytes(const std::uint32_t seed) noexcept {
    static constexpr std::uint32_t shifts[64] = {
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21,
    };
    static constexpr std::uint32_t constants[64] = {
        0xd76aa478U,0xe8c7b756U,0x242070dbU,0xc1bdceeeU,0xf57c0fafU,0x4787c62aU,0xa8304613U,0xfd469501U,
        0x698098d8U,0x8b44f7afU,0xffff5bb1U,0x895cd7beU,0x6b901122U,0xfd987193U,0xa679438eU,0x49b40821U,
        0xf61e2562U,0xc040b340U,0x265e5a51U,0xe9b6c7aaU,0xd62f105dU,0x02441453U,0xd8a1e681U,0xe7d3fbc8U,
        0x21e1cde6U,0xc33707d6U,0xf4d50d87U,0x455a14edU,0xa9e3e905U,0xfcefa3f8U,0x676f02d9U,0x8d2a4c8aU,
        0xfffa3942U,0x8771f681U,0x6d9d6122U,0xfde5380cU,0xa4beea44U,0x4bdecfa9U,0xf6bb4b60U,0xbebfbc70U,
        0x289b7ec6U,0xeaa127faU,0xd4ef3085U,0x04881d05U,0xd9d4d039U,0xe6db99e5U,0x1fa27cf8U,0xc4ac5665U,
        0xf4292244U,0x432aff97U,0xab9423a7U,0xfc93a039U,0x655b59c3U,0x8f0ccc92U,0xffeff47dU,0x85845dd1U,
        0x6fa87e4fU,0xfe2ce6e0U,0xa3014314U,0x4e0811a1U,0xf7537e82U,0xbd3af235U,0x2ad7d2bbU,0xeb86d391U,
    };

    std::array<std::uint8_t, 64> block{};
    block[0] = static_cast<std::uint8_t>(seed);
    block[1] = static_cast<std::uint8_t>(seed >> 8);
    block[2] = static_cast<std::uint8_t>(seed >> 16);
    block[3] = static_cast<std::uint8_t>(seed >> 24);
    block[4] = 0x80;
    block[56] = 32;  // message length in bits, little endian

    std::uint32_t words[16]{};
    for (int i = 0; i < 16; ++i) {
        const int o = i * 4;
        words[i] = static_cast<std::uint32_t>(block[o]) |
                   (static_cast<std::uint32_t>(block[o + 1]) << 8) |
                   (static_cast<std::uint32_t>(block[o + 2]) << 16) |
                   (static_cast<std::uint32_t>(block[o + 3]) << 24);
    }

    std::uint32_t a0 = 0x67452301U;
    std::uint32_t b0 = 0xefcdab89U;
    std::uint32_t c0 = 0x98badcfeU;
    std::uint32_t d0 = 0x10325476U;
    std::uint32_t a = a0, b = b0, c = c0, d = d0;

    for (std::uint32_t i = 0; i < 64; ++i) {
        std::uint32_t f = 0;
        std::uint32_t g = 0;
        if (i < 16) {
            f = (b & c) | ((~b) & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | ((~d) & c);
            g = (5U * i + 1U) & 15U;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3U * i + 5U) & 15U;
        } else {
            f = c ^ (b | (~d));
            g = (7U * i) & 15U;
        }
        const std::uint32_t nextD = c;
        c = b;
        b += RotateLeft(a + f + constants[i] + words[g], shifts[i]);
        a = d;
        d = nextD;
    }

    a0 += a;
    b0 += b;
    c0 += c;
    d0 += d;

    std::array<std::uint8_t, 16> digest{};
    const std::uint32_t state[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 4; ++i) {
        digest[i * 4] = static_cast<std::uint8_t>(state[i]);
        digest[i * 4 + 1] = static_cast<std::uint8_t>(state[i] >> 8);
        digest[i * 4 + 2] = static_cast<std::uint8_t>(state[i] >> 16);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state[i] >> 24);
    }
    return digest;
}

}  // namespace

std::uint32_t Md5PseudoRandom(const std::uint32_t seed) noexcept {
    const auto digest = Md5FourBytes(seed);
    return static_cast<std::uint32_t>(digest[6]) |
           (static_cast<std::uint32_t>(digest[7]) << 8) |
           (static_cast<std::uint32_t>(digest[8]) << 16) |
           (static_cast<std::uint32_t>(digest[9]) << 24);
}

bool ReadDeltaUserCmd(build4100::BitReadBuffer& buffer,
                      build4100::UserCmd& move,
                      const build4100::UserCmd& from) noexcept {
    move = from;

    if (ReadOneBit(buffer)) move.commandNumber = static_cast<std::int32_t>(ReadUnsignedBits(buffer, 32));
    else move.commandNumber = from.commandNumber + 1;

    if (ReadOneBit(buffer)) move.tickCount = static_cast<std::int32_t>(ReadUnsignedBits(buffer, 32));
    else move.tickCount = from.tickCount + 1;

    if (ReadOneBit(buffer)) move.viewAngles.x = ReadBitAngle(buffer, 16);
    if (ReadOneBit(buffer)) move.viewAngles.y = ReadBitAngle(buffer, 16);
    if (ReadOneBit(buffer)) move.viewAngles.z = ReadBitAngle(buffer, 8);

    if (ReadOneBit(buffer)) move.forwardMove = static_cast<float>(ReadSignedBits(buffer, 16));
    if (ReadOneBit(buffer)) move.sideMove = static_cast<float>(ReadSignedBits(buffer, 16));
    if (ReadOneBit(buffer)) move.upMove = static_cast<float>(ReadSignedBits(buffer, 16));

    if (ReadOneBit(buffer)) move.buttons = static_cast<std::int32_t>(ReadUnsignedBits(buffer, 32));
    if (ReadOneBit(buffer)) move.impulse = static_cast<std::uint8_t>(ReadUnsignedBits(buffer, 8));

    if (ReadOneBit(buffer)) {
        move.weaponSelect = static_cast<std::int32_t>(ReadUnsignedBits(buffer, kBuild4100MaxEdictBits));
        if (ReadOneBit(buffer)) {
            move.weaponSubtype = static_cast<std::int32_t>(ReadUnsignedBits(buffer, kBuild4100WeaponSubtypeBits));
        }
    }

    move.randomSeed = static_cast<std::int32_t>(Md5PseudoRandom(static_cast<std::uint32_t>(move.commandNumber)) & 0x7fffffffU);

    if (ReadOneBit(buffer)) move.mouseDx = static_cast<std::int16_t>(ReadUnsignedBits(buffer, 16));
    if (ReadOneBit(buffer)) move.mouseDy = static_cast<std::int16_t>(ReadUnsignedBits(buffer, 16));

    // build4100 CS:S CUserCmd is exactly 0x40 and has no HL2 ground-contact tail.
    move.hasBeenPredicted = false;
    return !buffer.overflow;
}

bool DecodeUserCmdBatch(build4100::BitReadBuffer& buffer,
                        const int totalCmds,
                        std::vector<build4100::UserCmd>& chronological) noexcept {
    chronological.clear();
    if (totalCmds < 0 || totalCmds > kBuild4100MaxWireUsercmds) {
        buffer.overflow = true;
        return false;
    }

    try {
        chronological.reserve(static_cast<std::size_t>(totalCmds));
        build4100::UserCmd from;
        for (int i = 0; i < totalCmds; ++i) {
            build4100::UserCmd move;
            if (!ReadDeltaUserCmd(buffer, move, from)) {
                chronological.clear();
                return false;
            }
            chronological.push_back(move);
            from = move;
        }
    } catch (...) {
        chronological.clear();
        buffer.overflow = true;
        return false;
    }
    return true;
}

}  // namespace cssv34::reconstructed
