#pragma once

#include <cstddef>
#include <cstdint>

#include "public/build4100/vector.h"

namespace cssv34::build4100 {

// Exact server-side build4100 CPlayerState layout recovered from the original
// cstrike/bin/server_i486.so. CServerGameClients::GetPlayerState returns
// CBasePlayer + 0x938; direct field accesses prove deadflag at +0x04,
// v_angle at +0x08, netname/fixangle at +0x14/+0x18, anglechange at +0x1c,
// hltv at +0x28 and frags/deaths at +0x2c/+0x30.
class PlayerState {
public:
    virtual const char* _GetClassName() { return "CPlayerState"; }       // slot 0
    virtual void NetworkStateChanged() {}                               // slot 1
    virtual void NetworkStateChanged(void*) {}                          // slot 2
    virtual ~PlayerState() = default;                                   // slots 3/4

    bool deadflag = false;                                              // +0x04
    std::uint8_t pad05[3]{};
    Vector vAngle{};                                                    // +0x08
    std::uint32_t netname = 0;                                         // +0x14 string_t
    std::int32_t fixangle = 0;                                         // +0x18
    Vector anglechange{};                                               // +0x1c
    bool hltv = false;                                                  // +0x28
    std::uint8_t pad29[3]{};
    std::int32_t frags = 0;                                            // +0x2c
    std::int32_t deaths = 0;                                           // +0x30
};

#if UINTPTR_MAX == 0xffffffffu
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(sizeof(PlayerState) == 0x34, "build4100 CPlayerState must be 0x34 bytes on i386");
static_assert(offsetof(PlayerState, deadflag) == 0x04);
static_assert(offsetof(PlayerState, vAngle) == 0x08);
static_assert(offsetof(PlayerState, netname) == 0x14);
static_assert(offsetof(PlayerState, fixangle) == 0x18);
static_assert(offsetof(PlayerState, anglechange) == 0x1c);
static_assert(offsetof(PlayerState, hltv) == 0x28);
static_assert(offsetof(PlayerState, frags) == 0x2c);
static_assert(offsetof(PlayerState, deaths) == 0x30);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

}  // namespace cssv34::build4100
