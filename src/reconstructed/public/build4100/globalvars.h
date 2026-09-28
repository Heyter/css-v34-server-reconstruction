#pragma once

#include <cstddef>
#include <cstdint>

namespace cssv34::build4100 {

// Runtime-proven leading CGlobalVarsBase layout for the build4100 i386 target.
// Only the fields proven by the v51.30 timed-frame work are represented here.
struct GlobalVarsBase {
    float realtime = 0.0f;              // +0x00
    std::int32_t framecount = 0;        // +0x04
    float absoluteframetime = 0.0f;     // +0x08
    float curtime = 0.0f;               // +0x0c
    float frametime = 0.0f;             // +0x10
    std::int32_t maxClients = 0;        // +0x14
    std::int32_t tickcount = 0;         // +0x18
    float interval_per_tick = 0.0f;     // +0x1c
};

static_assert(sizeof(GlobalVarsBase) == 0x20, "build4100 GlobalVarsBase prefix must be 0x20 bytes");
static_assert(offsetof(GlobalVarsBase, realtime) == 0x00);
static_assert(offsetof(GlobalVarsBase, framecount) == 0x04);
static_assert(offsetof(GlobalVarsBase, absoluteframetime) == 0x08);
static_assert(offsetof(GlobalVarsBase, curtime) == 0x0c);
static_assert(offsetof(GlobalVarsBase, frametime) == 0x10);
static_assert(offsetof(GlobalVarsBase, maxClients) == 0x14);
static_assert(offsetof(GlobalVarsBase, tickcount) == 0x18);
static_assert(offsetof(GlobalVarsBase, interval_per_tick) == 0x1c);

}  // namespace cssv34::build4100
