#pragma once

#include <cstdint>

#include "public/build4100/cs_ammo.h"
#include "public/build4100/cstrike_defs.h"

namespace cssv34::build4100 {

enum class CSEquipment : std::int32_t {
    Kevlar,
    AssaultSuit,
    Defuser,
    NightVision,
};

struct CSWeaponBuyInfo {
    int price = 0;
    bool terrorist = false;
    bool counterTerrorist = false;
    bool purchasable = false;
};

constexpr CSWeaponBuyInfo WeaponBuyInfo(const CSWeaponID id) noexcept {
    switch (id) {
        case CSWeaponID::P228: return {600, true, true, true};
        case CSWeaponID::Glock: return {400, true, true, true};
        case CSWeaponID::Scout: return {2750, true, true, true};
        case CSWeaponID::HEGrenade: return {300, true, true, true};
        case CSWeaponID::XM1014: return {3000, true, true, true};
        case CSWeaponID::Mac10: return {1400, true, false, true};
        case CSWeaponID::Aug: return {3500, false, true, true};
        case CSWeaponID::SmokeGrenade: return {300, true, true, true};
        case CSWeaponID::Elite: return {800, true, false, true};
        case CSWeaponID::FiveSeven: return {750, false, true, true};
        case CSWeaponID::Ump45: return {1700, true, true, true};
        case CSWeaponID::Sg550: return {4200, false, true, true};
        case CSWeaponID::Galil: return {2000, true, false, true};
        case CSWeaponID::Famas: return {2250, false, true, true};
        case CSWeaponID::Usp: return {500, true, true, true};
        case CSWeaponID::Awp: return {4750, true, true, true};
        case CSWeaponID::Mp5Navy: return {1500, true, true, true};
        case CSWeaponID::M249: return {5750, true, true, true};
        case CSWeaponID::M3: return {1700, true, true, true};
        case CSWeaponID::M4A1: return {3100, false, true, true};
        case CSWeaponID::Tmp: return {1250, false, true, true};
        case CSWeaponID::G3Sg1: return {5000, true, false, true};
        case CSWeaponID::Flashbang: return {200, true, true, true};
        case CSWeaponID::Deagle: return {650, true, true, true};
        case CSWeaponID::Sg552: return {3500, true, false, true};
        case CSWeaponID::Ak47: return {2500, true, false, true};
        case CSWeaponID::P90: return {2350, true, true, true};
        default: return {};
    }
}

constexpr bool CanTeamBuyWeapon(const CSWeaponID id, const int team) noexcept {
    const auto info = WeaponBuyInfo(id);
    if (!info.purchasable) return false;
    if (team == static_cast<int>(CSTeam::Terrorist)) return info.terrorist;
    if (team == static_cast<int>(CSTeam::CounterTerrorist)) return info.counterTerrorist;
    return false;
}

constexpr int kCsKevlarPrice = 650;
constexpr int kCsHelmetPrice = 350;
constexpr int kCsAssaultSuitPrice = kCsKevlarPrice + kCsHelmetPrice;
constexpr int kCsDefuserPrice = 200;
constexpr int kCsNightVisionPrice = 1250;
constexpr float kCsDefaultBuyTimeSeconds = 90.0f;

static_assert(WeaponBuyInfo(CSWeaponID::Ak47).price == 2500);
static_assert(WeaponBuyInfo(CSWeaponID::M4A1).price == 3100);
static_assert(!CanTeamBuyWeapon(CSWeaponID::Ak47, static_cast<int>(CSTeam::CounterTerrorist)));
static_assert(CanTeamBuyWeapon(CSWeaponID::M4A1, static_cast<int>(CSTeam::CounterTerrorist)));

}  // namespace cssv34::build4100
