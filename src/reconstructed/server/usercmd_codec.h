#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "public/build4100/bitbuf.h"
#include "public/build4100/usercmd.h"

namespace cssv34::reconstructed {

constexpr int kBuild4100MaxEdictBits = 11;
constexpr int kBuild4100WeaponSubtypeBits = 6;
// Original build4100 CServerGameClients::ProcessUsercmds accepts totalcmds <= 28.
constexpr int kBuild4100MaxWireUsercmds = 28;

std::uint32_t Md5PseudoRandom(std::uint32_t seed) noexcept;

bool ReadDeltaUserCmd(build4100::BitReadBuffer& buffer,
                      build4100::UserCmd& move,
                      const build4100::UserCmd& from) noexcept;

bool DecodeUserCmdBatch(build4100::BitReadBuffer& buffer,
                        int totalCmds,
                        std::vector<build4100::UserCmd>& chronological) noexcept;

}  // namespace cssv34::reconstructed
