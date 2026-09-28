#pragma once

#include <cstddef>
#include <cstdint>

#include "public/build4100/vector.h"

namespace cssv34::build4100 {

// build4100 CUserCmd. The original ELF exposes a standalone CUserCmd vtable
// (virtual destructor) and the Source-2006 field ordering yields the same
// 0x40 i386 footprint as the already recovered CBotCmd ABI object.
class UserCmd {
public:
    virtual ~UserCmd() = default;

    std::int32_t commandNumber = 0;      // +0x04
    std::int32_t tickCount = 0;          // +0x08
    Vector viewAngles{};                 // +0x0c
    float forwardMove = 0.0f;            // +0x18
    float sideMove = 0.0f;               // +0x1c
    float upMove = 0.0f;                 // +0x20
    std::int32_t buttons = 0;            // +0x24
    std::uint8_t impulse = 0;            // +0x28
    std::uint8_t pad29[3]{};
    std::int32_t weaponSelect = 0;       // +0x2c
    std::int32_t weaponSubtype = 0;      // +0x30
    std::int32_t randomSeed = 0;         // +0x34
    std::int16_t mouseDx = 0;            // +0x38
    std::int16_t mouseDy = 0;            // +0x3a
    bool hasBeenPredicted = false;       // +0x3c
    std::uint8_t pad3d[3]{};
};

#if UINTPTR_MAX == 0xffffffffu
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(sizeof(UserCmd) == 0x40, "build4100 CUserCmd must be 0x40 bytes on i386");
static_assert(offsetof(UserCmd, commandNumber) == 0x04);
static_assert(offsetof(UserCmd, tickCount) == 0x08);
static_assert(offsetof(UserCmd, viewAngles) == 0x0c);
static_assert(offsetof(UserCmd, forwardMove) == 0x18);
static_assert(offsetof(UserCmd, sideMove) == 0x1c);
static_assert(offsetof(UserCmd, upMove) == 0x20);
static_assert(offsetof(UserCmd, buttons) == 0x24);
static_assert(offsetof(UserCmd, impulse) == 0x28);
static_assert(offsetof(UserCmd, weaponSelect) == 0x2c);
static_assert(offsetof(UserCmd, weaponSubtype) == 0x30);
static_assert(offsetof(UserCmd, randomSeed) == 0x34);
static_assert(offsetof(UserCmd, mouseDx) == 0x38);
static_assert(offsetof(UserCmd, mouseDy) == 0x3a);
static_assert(offsetof(UserCmd, hasBeenPredicted) == 0x3c);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

}  // namespace cssv34::build4100
