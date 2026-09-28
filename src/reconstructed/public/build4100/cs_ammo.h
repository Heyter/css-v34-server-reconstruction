#pragma once

#include <cstddef>
#include <cstdint>

#include "public/build4100/cs_weapon.h"

namespace cssv34::build4100 {

enum class CSAmmoType : std::int32_t {
    None = -1,
    Ammo338Mag = 0,
    Ammo357Sig,
    Ammo45Acp,
    Ammo50Ae,
    Ammo556Mm,
    Ammo57Mm,
    Ammo762Mm,
    Ammo9Mm,
    Buckshot,
    Count,
};

struct CSAmmoInfo {
    int price;
    int buySize;
    int maxCarry;
};

constexpr std::size_t kCsAmmoTypeCount = static_cast<std::size_t>(CSAmmoType::Count);

constexpr bool IsValidAmmoType(const CSAmmoType type) noexcept {
    const int value = static_cast<int>(type);
    return value >= 0 && value < static_cast<int>(CSAmmoType::Count);
}

constexpr CSAmmoInfo AmmoInfoFor(const CSAmmoType type) noexcept {
    switch (type) {
        case CSAmmoType::Ammo338Mag: return {125, 10, 30};
        case CSAmmoType::Ammo357Sig: return {50, 13, 52};
        case CSAmmoType::Ammo45Acp: return {25, 12, 100};
        case CSAmmoType::Ammo50Ae: return {40, 7, 35};
        case CSAmmoType::Ammo556Mm: return {60, 30, 90};
        case CSAmmoType::Ammo57Mm: return {50, 50, 100};
        case CSAmmoType::Ammo762Mm: return {80, 30, 90};
        case CSAmmoType::Ammo9Mm: return {20, 30, 120};
        case CSAmmoType::Buckshot: return {65, 8, 32};
        default: return {0, 0, 0};
    }
}

constexpr CSAmmoType AmmoTypeForWeapon(const CSWeaponID id) noexcept {
    switch (id) {
        case CSWeaponID::P228: return CSAmmoType::Ammo357Sig;
        case CSWeaponID::Glock:
        case CSWeaponID::Elite:
        case CSWeaponID::Mp5Navy:
            return CSAmmoType::Ammo9Mm;
        case CSWeaponID::Usp:
        case CSWeaponID::Mac10:
        case CSWeaponID::Ump45:
            return CSAmmoType::Ammo45Acp;
        case CSWeaponID::Deagle: return CSAmmoType::Ammo50Ae;
        case CSWeaponID::FiveSeven:
        case CSWeaponID::P90:
            return CSAmmoType::Ammo57Mm;
        case CSWeaponID::Galil:
        case CSWeaponID::Famas:
        case CSWeaponID::M4A1:
        case CSWeaponID::Aug:
        case CSWeaponID::Sg550:
            return CSAmmoType::Ammo556Mm;
        case CSWeaponID::Scout:
        case CSWeaponID::Ak47:
        case CSWeaponID::Sg552:
        case CSWeaponID::G3Sg1:
            return CSAmmoType::Ammo762Mm;
        case CSWeaponID::Awp: return CSAmmoType::Ammo338Mag;
        case CSWeaponID::M3:
        case CSWeaponID::XM1014:
            return CSAmmoType::Buckshot;
        default: return CSAmmoType::None;
    }
}

static_assert(AmmoInfoFor(CSAmmoType::Ammo9Mm).price == 20);
static_assert(AmmoInfoFor(CSAmmoType::Ammo338Mag).maxCarry == 30);
static_assert(AmmoTypeForWeapon(CSWeaponID::Ak47) == CSAmmoType::Ammo762Mm);
static_assert(AmmoTypeForWeapon(CSWeaponID::M4A1) == CSAmmoType::Ammo556Mm);

}  // namespace cssv34::build4100
