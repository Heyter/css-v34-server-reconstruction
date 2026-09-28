#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "server/weapon_runtime.h"

namespace cssv34::reconstructed {

// Build4100 CSS encrypts weapon_*.ctx with Thin-ICE (level 0, 8 byte key).
// These helpers are source-owned so the rebuilt DLL can consume the original
// game content instead of hard-coding values from a different CSS revision.
bool DecodeBuild4100Ice(std::vector<unsigned char>& bytes) noexcept;
bool ParseBuild4100WeaponScript(std::string_view text, WeaponRuntimeProfile& profile) noexcept;
bool LoadBuild4100WeaponScript(const char* path, bool encrypted,
                               WeaponRuntimeProfile& profile) noexcept;

}  // namespace cssv34::reconstructed
