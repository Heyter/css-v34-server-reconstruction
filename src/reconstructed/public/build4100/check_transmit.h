#pragma once

#include <cstddef>
#include <cstdint>

namespace cssv34::build4100 {

constexpr std::size_t kMaxEdicts = 2048;
constexpr std::size_t kTransmitWords = kMaxEdicts / 32;

// Exact build4100 CCheckTransmitInfo storage used by
// CServerGameEnts::CheckTransmit.  Pointer fields stay raw 32-bit values so
// the ABI layout is identical even when this source is syntax-checked on a
// 64-bit host.
//
// Recovered from server_i486.so::CServerGameEnts::CheckTransmit:
//   +0x0000 edict_t* m_pClientEnt
//   +0x0004 byte    m_PVS[8192]
//   +0x2004 int     m_nPVSSize
//   +0x2008 uint32* m_pTransmitEdict
//   +0x200c uint32* m_pTransmitAlways
struct CheckTransmitInfoLayout {
    std::uint32_t clientEdict00 = 0;
    std::uint8_t pvs[0x2000]{};
    std::int32_t pvsSize2004 = 0;
    std::uint32_t transmitEdict2008 = 0;
    std::uint32_t transmitAlways200c = 0;
};

static_assert(sizeof(CheckTransmitInfoLayout) == 0x2010,
              "build4100 CCheckTransmitInfo must be 0x2010 bytes");
static_assert(offsetof(CheckTransmitInfoLayout, clientEdict00) == 0x0000);
static_assert(offsetof(CheckTransmitInfoLayout, pvs) == 0x0004);
static_assert(offsetof(CheckTransmitInfoLayout, pvsSize2004) == 0x2004);
static_assert(offsetof(CheckTransmitInfoLayout, transmitEdict2008) == 0x2008);
static_assert(offsetof(CheckTransmitInfoLayout, transmitAlways200c) == 0x200c);

inline void SetTransmitBit(std::uint32_t* const words, const unsigned int index) noexcept {
    if (!words || index >= kMaxEdicts) return;
    words[index >> 5u] |= (1u << (index & 31u));
}

inline void ClearTransmitBit(std::uint32_t* const words, const unsigned int index) noexcept {
    if (!words || index >= kMaxEdicts) return;
    words[index >> 5u] &= ~(1u << (index & 31u));
}

inline bool TestTransmitBit(const std::uint32_t* const words, const unsigned int index) noexcept {
    if (!words || index >= kMaxEdicts) return false;
    return (words[index >> 5u] & (1u << (index & 31u))) != 0;
}

}  // namespace cssv34::build4100
