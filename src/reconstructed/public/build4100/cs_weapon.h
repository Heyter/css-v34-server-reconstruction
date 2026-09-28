#pragma once

#include <cstdint>

namespace cssv34::build4100 {

// Exact CS:S-era weapon ID ordering recovered from build4100's
// s_WeaponAliasInfo / WeaponIDToAlias table (IDs 0..33).
enum class CSWeaponID : std::int32_t {
    None = 0,
    P228 = 1,
    Glock = 2,
    Scout = 3,
    HEGrenade = 4,
    XM1014 = 5,
    C4 = 6,
    Mac10 = 7,
    Aug = 8,
    SmokeGrenade = 9,
    Elite = 10,
    FiveSeven = 11,
    Ump45 = 12,
    Sg550 = 13,
    Galil = 14,
    Famas = 15,
    Usp = 16,
    Awp = 17,
    Mp5Navy = 18,
    M249 = 19,
    M3 = 20,
    M4A1 = 21,
    Tmp = 22,
    G3Sg1 = 23,
    Flashbang = 24,
    Deagle = 25,
    Sg552 = 26,
    Ak47 = 27,
    Knife = 28,
    P90 = 29,
    Shield = 30,
    Kevlar = 31,
    AssaultSuit = 32,
    NightVision = 33,
};

// CBaseCombatCharacter weapon slots used by build4100. Slots 0/1 are proven
// directly by CCSPlayer::HasPrimaryWeapon/HasSecondaryWeapon disassembly.
enum class CSWeaponSlot : std::int32_t {
    Rifle = 0,
    Pistol = 1,
    Knife = 2,
    Grenades = 3,
    C4 = 4,
    Invalid = -1,
};

constexpr std::uint32_t kBuild4100PrimaryWeaponMask = 0x6cfef1a8u;
constexpr std::uint32_t kBuild4100SecondaryWeaponMask = 0x02010c06u;
constexpr int kBuild4100WeaponIdMax = 33;

constexpr bool IsValidWeaponID(const CSWeaponID id) noexcept {
    const int value = static_cast<int>(id);
    return value >= 0 && value <= kBuild4100WeaponIdMax;
}

constexpr bool IsPrimaryWeapon(const CSWeaponID id) noexcept {
    const int value = static_cast<int>(id);
    return value >= 0 && value <= 30 &&
           (kBuild4100PrimaryWeaponMask & (std::uint32_t{1} << value)) != 0;
}

constexpr bool IsSecondaryWeapon(const CSWeaponID id) noexcept {
    const int value = static_cast<int>(id);
    return value >= 0 && value <= 25 &&
           (kBuild4100SecondaryWeaponMask & (std::uint32_t{1} << value)) != 0;
}

constexpr bool IsGrenadeWeapon(const CSWeaponID id) noexcept {
    return id == CSWeaponID::HEGrenade || id == CSWeaponID::SmokeGrenade ||
           id == CSWeaponID::Flashbang;
}

constexpr bool IsEquipmentItem(const CSWeaponID id) noexcept {
    return id == CSWeaponID::Kevlar || id == CSWeaponID::AssaultSuit ||
           id == CSWeaponID::NightVision;
}

constexpr CSWeaponSlot WeaponSlotForID(const CSWeaponID id) noexcept {
    if (IsPrimaryWeapon(id)) return CSWeaponSlot::Rifle;
    if (IsSecondaryWeapon(id)) return CSWeaponSlot::Pistol;
    if (id == CSWeaponID::Knife) return CSWeaponSlot::Knife;
    if (IsGrenadeWeapon(id)) return CSWeaponSlot::Grenades;
    if (id == CSWeaponID::C4) return CSWeaponSlot::C4;
    return CSWeaponSlot::Invalid;
}

const char* WeaponIDToAlias(CSWeaponID id) noexcept;
const char* WeaponIDToClassname(CSWeaponID id) noexcept;
CSWeaponID AliasToWeaponID(const char* alias) noexcept;
CSWeaponID ClassnameToWeaponID(const char* classname) noexcept;

static_assert(static_cast<int>(CSWeaponID::C4) == 6);
static_assert(static_cast<int>(CSWeaponID::Knife) == 28);
static_assert(static_cast<int>(CSWeaponID::NightVision) == 33);
static_assert(IsPrimaryWeapon(CSWeaponID::Ak47));
static_assert(IsPrimaryWeapon(CSWeaponID::Shield));
static_assert(IsSecondaryWeapon(CSWeaponID::Usp));
static_assert(!IsSecondaryWeapon(CSWeaponID::Knife));

}  // namespace cssv34::build4100
