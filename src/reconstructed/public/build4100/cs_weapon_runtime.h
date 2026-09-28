#pragma once

#include "public/build4100/cs_weapon.h"

namespace cssv34::build4100 {

// Classic CS:S magazine capacities. These are the script-level max-clip values
// consumed by CBaseCombatWeapon/CWeaponCSBase in the build4100 era. Grenades,
// knife, C4 and equipment do not use a primary firearm magazine.
constexpr int MaxClipForWeapon(const CSWeaponID id) noexcept {
    switch (id) {
        case CSWeaponID::P228: return 13;
        case CSWeaponID::Glock: return 20;
        case CSWeaponID::Scout: return 10;
        case CSWeaponID::XM1014: return 7;
        case CSWeaponID::Mac10: return 30;
        case CSWeaponID::Aug: return 30;
        case CSWeaponID::Elite: return 30;
        case CSWeaponID::FiveSeven: return 20;
        case CSWeaponID::Ump45: return 25;
        case CSWeaponID::Sg550: return 30;
        case CSWeaponID::Galil: return 35;
        case CSWeaponID::Famas: return 25;
        case CSWeaponID::Usp: return 12;
        case CSWeaponID::Awp: return 10;
        case CSWeaponID::Mp5Navy: return 30;
        case CSWeaponID::M249: return 100;
        case CSWeaponID::M3: return 8;
        case CSWeaponID::M4A1: return 30;
        case CSWeaponID::Tmp: return 30;
        case CSWeaponID::G3Sg1: return 20;
        case CSWeaponID::Deagle: return 7;
        case CSWeaponID::Sg552: return 30;
        case CSWeaponID::Ak47: return 30;
        case CSWeaponID::P90: return 50;
        default: return 0;
    }
}

constexpr bool UsesPrimaryMagazine(const CSWeaponID id) noexcept {
    return MaxClipForWeapon(id) > 0;
}

static_assert(MaxClipForWeapon(CSWeaponID::Ak47) == 30);
static_assert(MaxClipForWeapon(CSWeaponID::M4A1) == 30);
static_assert(MaxClipForWeapon(CSWeaponID::Glock) == 20);
static_assert(MaxClipForWeapon(CSWeaponID::Deagle) == 7);
static_assert(MaxClipForWeapon(CSWeaponID::P90) == 50);
static_assert(MaxClipForWeapon(CSWeaponID::M249) == 100);
static_assert(!UsesPrimaryMagazine(CSWeaponID::Knife));
static_assert(!UsesPrimaryMagazine(CSWeaponID::HEGrenade));

}  // namespace cssv34::build4100
