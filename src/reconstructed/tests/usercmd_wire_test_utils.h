#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "public/build4100/bitbuf.h"
#include "public/build4100/usercmd.h"
#include "server/usercmd_codec.h"

namespace cssv34::reconstructed::test {

class BitWriter final {
public:
    void WriteUnsigned(std::uint32_t value, int bits) {
        for (int i = 0; i < bits; ++i) {
            if ((bitCount_ & 7) == 0) data_.push_back(0);
            if ((value >> i) & 1U) data_.back() |= static_cast<std::uint8_t>(1U << (bitCount_ & 7));
            ++bitCount_;
        }
    }

    void WriteOneBit(bool value) { WriteUnsigned(value ? 1U : 0U, 1); }

    void WriteSigned(std::int32_t value, int bits) {
        const std::uint32_t mask = bits == 32 ? 0xffffffffU : ((1U << bits) - 1U);
        WriteUnsigned(static_cast<std::uint32_t>(value) & mask, bits);
    }

    void WriteBitAngle(float angle, int bits) {
        const std::uint32_t levels = 1U << bits;
        const auto encoded = static_cast<std::uint32_t>(angle * (static_cast<float>(levels) / 360.0f)) & (levels - 1U);
        WriteUnsigned(encoded, bits);
    }

    const std::vector<std::uint8_t>& Data() const noexcept { return data_; }
    int BitCount() const noexcept { return bitCount_; }

private:
    std::vector<std::uint8_t> data_;
    int bitCount_ = 0;
};

inline void WriteDeltaUserCmd(BitWriter& writer,
                              const build4100::UserCmd& to,
                              const build4100::UserCmd& from) {
    const bool commandChanged = to.commandNumber != from.commandNumber + 1;
    writer.WriteOneBit(commandChanged);
    if (commandChanged) writer.WriteUnsigned(static_cast<std::uint32_t>(to.commandNumber), 32);

    const bool tickChanged = to.tickCount != from.tickCount + 1;
    writer.WriteOneBit(tickChanged);
    if (tickChanged) writer.WriteUnsigned(static_cast<std::uint32_t>(to.tickCount), 32);

    writer.WriteOneBit(to.viewAngles.x != from.viewAngles.x);
    if (to.viewAngles.x != from.viewAngles.x) writer.WriteBitAngle(to.viewAngles.x, 16);
    writer.WriteOneBit(to.viewAngles.y != from.viewAngles.y);
    if (to.viewAngles.y != from.viewAngles.y) writer.WriteBitAngle(to.viewAngles.y, 16);
    writer.WriteOneBit(to.viewAngles.z != from.viewAngles.z);
    if (to.viewAngles.z != from.viewAngles.z) writer.WriteBitAngle(to.viewAngles.z, 8);

    writer.WriteOneBit(to.forwardMove != from.forwardMove);
    if (to.forwardMove != from.forwardMove) writer.WriteSigned(static_cast<std::int32_t>(to.forwardMove), 16);
    writer.WriteOneBit(to.sideMove != from.sideMove);
    if (to.sideMove != from.sideMove) writer.WriteSigned(static_cast<std::int32_t>(to.sideMove), 16);
    writer.WriteOneBit(to.upMove != from.upMove);
    if (to.upMove != from.upMove) writer.WriteSigned(static_cast<std::int32_t>(to.upMove), 16);

    writer.WriteOneBit(to.buttons != from.buttons);
    if (to.buttons != from.buttons) writer.WriteUnsigned(static_cast<std::uint32_t>(to.buttons), 32);
    writer.WriteOneBit(to.impulse != from.impulse);
    if (to.impulse != from.impulse) writer.WriteUnsigned(to.impulse, 8);

    const bool weaponChanged = to.weaponSelect != from.weaponSelect;
    writer.WriteOneBit(weaponChanged);
    if (weaponChanged) {
        writer.WriteUnsigned(static_cast<std::uint32_t>(to.weaponSelect), kBuild4100MaxEdictBits);
        const bool subtypeChanged = to.weaponSubtype != from.weaponSubtype;
        writer.WriteOneBit(subtypeChanged);
        if (subtypeChanged) writer.WriteUnsigned(static_cast<std::uint32_t>(to.weaponSubtype), kBuild4100WeaponSubtypeBits);
    }

    writer.WriteOneBit(to.mouseDx != from.mouseDx);
    if (to.mouseDx != from.mouseDx) writer.WriteUnsigned(static_cast<std::uint16_t>(to.mouseDx), 16);
    writer.WriteOneBit(to.mouseDy != from.mouseDy);
    if (to.mouseDy != from.mouseDy) writer.WriteUnsigned(static_cast<std::uint16_t>(to.mouseDy), 16);
}

inline build4100::BitReadBuffer MakeBitReader(const BitWriter& writer) noexcept {
    build4100::BitReadBuffer buffer{};
    build4100::InitBitReadBuffer(buffer, writer.Data().data(),
                                 static_cast<std::int32_t>(writer.Data().size()), writer.BitCount());
    return buffer;
}

}  // namespace cssv34::reconstructed::test
