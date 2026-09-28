#pragma once

#include <cstddef>
#include <cstdint>

namespace cssv34::build4100 {

constexpr std::uint32_t kEdictChanged = 0x1u;
constexpr std::uint32_t kEdictFree = 0x2u;
constexpr std::uint32_t kEdictFull = 0x4u;
constexpr std::uint32_t kEdictFullChanged = 0x100u;

// Exact build4100 FL_EDICT_* transmit-state field recovered from
// CBaseEntity::SetTransmitState and CServerGameEnts::CheckTransmit.  These bits
// are independent from kEdictChanged/kEdictFree/kEdictFull above.
constexpr std::uint32_t kEdictTransmitMask = 0x38u;
constexpr std::uint32_t kEdictTransmitFullCheck = 0x00u;
constexpr std::uint32_t kEdictTransmitAlways = 0x08u;
constexpr std::uint32_t kEdictTransmitDontSend = 0x10u;
constexpr std::uint32_t kEdictTransmitPvsCheck = 0x20u;

inline std::uint32_t WithEdictTransmitState(const std::uint32_t flags,
                                            const std::uint32_t transmitState) noexcept {
    return (flags & ~kEdictTransmitMask) | (transmitState & kEdictTransmitMask);
}

// Runtime/static proof from build4100 engine_i486.so: edict_t stride is 0x14.
// Pointer-bearing fields stay raw 32-bit storage so the layout remains exact
// even when this source is syntax-checked on a 64-bit host.
// Exact 4-byte IChangeInfoAccessor returned by VEngineServer021::
// GetChangeAccessor(slot98). CBaseEdict::StateChanged() invalidates the
// accessor serial when all SendProps must be repacked.
struct ChangeInfoAccessor {
    std::uint16_t changeInfo = 0;
    std::uint16_t changeInfoSerialNumber = 0;
};

static_assert(sizeof(ChangeInfoAccessor) == 0x04,
              "build4100 IChangeInfoAccessor must be 4 bytes");
static_assert(offsetof(ChangeInfoAccessor, changeInfo) == 0x00);
static_assert(offsetof(ChangeInfoAccessor, changeInfoSerialNumber) == 0x02);

struct Edict {
    std::uint32_t stateFlags = kEdictFree;
    std::int32_t networkSerial = -1;
    union {
        std::uint32_t field08 = 0;
        std::uint32_t networkable08;    // build4100 edict_t::m_pNetworkable
    };
    union {
        std::uint32_t unknown0c = 0;      // build4100 edict_t::m_pUnk / IServerUnknown*
        std::uint32_t networkable0c;      // legacy reconstructed alias only
    };
    std::uint32_t field10 = 0;
};

static_assert(sizeof(Edict) == 0x14, "build4100 edict_t must be 20 bytes");
static_assert(offsetof(Edict, stateFlags) == 0x00);
static_assert(offsetof(Edict, networkSerial) == 0x04);
static_assert(offsetof(Edict, networkable08) == 0x08);
static_assert(offsetof(Edict, unknown0c) == 0x0c);
static_assert(offsetof(Edict, field10) == 0x10);

inline bool HasEdictTransmitState(const Edict& edict, const std::uint32_t transmitState) noexcept {
    return (edict.stateFlags & kEdictTransmitMask) == (transmitState & kEdictTransmitMask);
}

inline bool IsFree(const Edict& edict) noexcept {
    return (edict.stateFlags & kEdictFree) != 0;
}

}  // namespace cssv34::build4100
