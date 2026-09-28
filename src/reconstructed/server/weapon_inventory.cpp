#include "server/weapon_inventory.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace cssv34::build4100 {
namespace {

constexpr std::array<const char*, kBuild4100WeaponIdMax + 1> kWeaponAliases = {
    "none", "p228", "glock", "scout", "hegren", "xm1014", "c4", "mac10", "aug",
    "smokegrenade", "elite", "fiveseven", "ump45", "sg550", "galil", "famas", "usp",
    "awp", "mp5navy", "m249", "m3", "m4a1", "tmp", "g3sg1", "flash", "deagle",
    "sg552", "ak47", "knife", "p90", "shield", "kevlar", "assaultsuit", "nightvision",
};

constexpr std::array<const char*, kBuild4100WeaponIdMax + 1> kWeaponClassnames = {
    nullptr,
    "weapon_p228", "weapon_glock", "weapon_scout", "weapon_hegrenade", "weapon_xm1014",
    "weapon_c4", "weapon_mac10", "weapon_aug", "weapon_smokegrenade", "weapon_elite",
    "weapon_fiveseven", "weapon_ump45", "weapon_sg550", "weapon_galil", "weapon_famas",
    "weapon_usp", "weapon_awp", "weapon_mp5navy", "weapon_m249", "weapon_m3", "weapon_m4a1",
    "weapon_tmp", "weapon_g3sg1", "weapon_flashbang", "weapon_deagle", "weapon_sg552",
    "weapon_ak47", "weapon_knife", "weapon_p90", "weapon_shield", "item_kevlar",
    "item_assaultsuit", "item_nvgs",
};

bool CaseEqual(const char* left, const char* right) noexcept {
    if (!left || !right) return false;
    while (*left && *right) {
        const auto a = static_cast<unsigned char>(*left++);
        const auto b = static_cast<unsigned char>(*right++);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return *left == '\0' && *right == '\0';
}

}  // namespace

const char* WeaponIDToAlias(const CSWeaponID id) noexcept {
    if (!IsValidWeaponID(id)) return nullptr;
    return kWeaponAliases[static_cast<std::size_t>(id)];
}

const char* WeaponIDToClassname(const CSWeaponID id) noexcept {
    if (!IsValidWeaponID(id)) return nullptr;
    return kWeaponClassnames[static_cast<std::size_t>(id)];
}

CSWeaponID AliasToWeaponID(const char* const alias) noexcept {
    if (!alias) return CSWeaponID::None;
    for (int i = 0; i <= kBuild4100WeaponIdMax; ++i) {
        if (CaseEqual(alias, kWeaponAliases[static_cast<std::size_t>(i)])) {
            return static_cast<CSWeaponID>(i);
        }
    }
    return CSWeaponID::None;
}

CSWeaponID ClassnameToWeaponID(const char* const classname) noexcept {
    if (!classname) return CSWeaponID::None;
    for (int i = 1; i <= kBuild4100WeaponIdMax; ++i) {
        const char* const known = kWeaponClassnames[static_cast<std::size_t>(i)];
        if (known && CaseEqual(classname, known)) return static_cast<CSWeaponID>(i);
    }
    return CSWeaponID::None;
}

}  // namespace cssv34::build4100

namespace cssv34::reconstructed {

void WeaponInventory::Clear() noexcept {
    counts_.fill(0);
    clips_.fill(0);
    activeWeapon_ = build4100::CSWeaponID::None;
    ammo_.fill(0);
}

int WeaponInventory::MaxOwned(const build4100::CSWeaponID id) noexcept {
    if (id == build4100::CSWeaponID::Flashbang) return 2;
    if (build4100::IsGrenadeWeapon(id)) return 1;
    if (build4100::WeaponSlotForID(id) != build4100::CSWeaponSlot::Invalid) return 1;
    return 0;
}

bool WeaponInventory::SlotOccupiedByOther(const build4100::CSWeaponID id) const noexcept {
    const auto slot = build4100::WeaponSlotForID(id);
    if (slot != build4100::CSWeaponSlot::Rifle && slot != build4100::CSWeaponSlot::Pistol) {
        return false;
    }
    const auto held = WeaponInSlot(slot);
    return held != build4100::CSWeaponID::None && held != id;
}

bool WeaponInventory::Give(const build4100::CSWeaponID id, const bool selectIfEmpty) noexcept {
    if (!build4100::IsValidWeaponID(id) || id == build4100::CSWeaponID::None ||
        build4100::IsEquipmentItem(id) || SlotOccupiedByOther(id)) {
        return false;
    }

    const int limit = MaxOwned(id);
    if (limit <= 0) return false;
    const std::size_t index = static_cast<std::size_t>(id);
    if (counts_[index] >= limit) return false;
    const bool firstOwned = counts_[index] == 0;
    ++counts_[index];
    if (firstOwned && build4100::UsesPrimaryMagazine(id)) {
        clips_[index] = build4100::MaxClipForWeapon(id);
    }
    if (selectIfEmpty && activeWeapon_ == build4100::CSWeaponID::None) activeWeapon_ = id;
    return true;
}

bool WeaponInventory::Remove(const build4100::CSWeaponID id, const int count) noexcept {
    if (!build4100::IsValidWeaponID(id) || id == build4100::CSWeaponID::None || count <= 0) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(id);
    if (counts_[index] == 0) return false;
    const int removed = std::min<int>(count, counts_[index]);
    counts_[index] = static_cast<unsigned char>(counts_[index] - removed);
    if (counts_[index] == 0) {
        clips_[index] = 0;
        if (activeWeapon_ == id) activeWeapon_ = build4100::CSWeaponID::None;
    }
    return true;
}

bool WeaponInventory::Select(const build4100::CSWeaponID id) noexcept {
    if (!Has(id) || build4100::IsEquipmentItem(id)) return false;
    activeWeapon_ = id;
    return true;
}

bool WeaponInventory::Has(const build4100::CSWeaponID id) const noexcept {
    return build4100::IsValidWeaponID(id) && id != build4100::CSWeaponID::None &&
           counts_[static_cast<std::size_t>(id)] != 0;
}

int WeaponInventory::Count(const build4100::CSWeaponID id) const noexcept {
    return Has(id) ? counts_[static_cast<std::size_t>(id)] : 0;
}

bool WeaponInventory::HasPrimaryWeapon() const noexcept {
    return WeaponInSlot(build4100::CSWeaponSlot::Rifle) != build4100::CSWeaponID::None;
}

bool WeaponInventory::HasSecondaryWeapon() const noexcept {
    return WeaponInSlot(build4100::CSWeaponSlot::Pistol) != build4100::CSWeaponID::None;
}

build4100::CSWeaponID WeaponInventory::WeaponInSlot(const build4100::CSWeaponSlot slot) const noexcept {
    for (int i = 1; i <= build4100::kBuild4100WeaponIdMax; ++i) {
        const auto id = static_cast<build4100::CSWeaponID>(i);
        if (build4100::WeaponSlotForID(id) == slot && Has(id)) return id;
    }
    return build4100::CSWeaponID::None;
}

const char* WeaponInventory::ActiveWeaponClassname() const noexcept {
    return build4100::WeaponIDToClassname(activeWeapon_);
}

int WeaponInventory::Clip(const build4100::CSWeaponID id) const noexcept {
    if (!Has(id) || !build4100::UsesPrimaryMagazine(id)) return 0;
    return clips_[static_cast<std::size_t>(id)];
}

int WeaponInventory::MaxClip(const build4100::CSWeaponID id) const noexcept {
    return Has(id) ? build4100::MaxClipForWeapon(id) : 0;
}

bool WeaponInventory::SetClip(const build4100::CSWeaponID id, const int amount) noexcept {
    if (!Has(id) || !build4100::UsesPrimaryMagazine(id)) return false;
    const int maxClip = build4100::MaxClipForWeapon(id);
    clips_[static_cast<std::size_t>(id)] = std::clamp(amount, 0, maxClip);
    return true;
}

bool WeaponInventory::ConsumeRound(const build4100::CSWeaponID id) noexcept {
    if (!Has(id) || !build4100::UsesPrimaryMagazine(id)) return false;
    int& clip = clips_[static_cast<std::size_t>(id)];
    if (clip <= 0) return false;
    --clip;
    return true;
}

int WeaponInventory::Reload(const build4100::CSWeaponID id) noexcept {
    if (!Has(id) || !build4100::UsesPrimaryMagazine(id)) return 0;
    const build4100::CSAmmoType ammoType = build4100::AmmoTypeForWeapon(id);
    if (!build4100::IsValidAmmoType(ammoType)) return 0;

    const std::size_t weaponIndex = static_cast<std::size_t>(id);
    const std::size_t ammoIndex = static_cast<std::size_t>(ammoType);
    const int missing = build4100::MaxClipForWeapon(id) - clips_[weaponIndex];
    if (missing <= 0 || ammo_[ammoIndex] <= 0) return 0;

    const int loaded = std::min(missing, ammo_[ammoIndex]);
    clips_[weaponIndex] += loaded;
    ammo_[ammoIndex] -= loaded;
    return loaded;
}

int WeaponInventory::Ammo(const build4100::CSAmmoType type) const noexcept {
    if (!build4100::IsValidAmmoType(type)) return 0;
    return ammo_[static_cast<std::size_t>(type)];
}

void WeaponInventory::SetAmmo(const build4100::CSAmmoType type, const int amount) noexcept {
    if (!build4100::IsValidAmmoType(type)) return;
    const int maxCarry = build4100::AmmoInfoFor(type).maxCarry;
    ammo_[static_cast<std::size_t>(type)] = std::clamp(amount, 0, maxCarry);
}

void WeaponInventory::GiveAmmo(const build4100::CSAmmoType type, const int amount) noexcept {
    if (!build4100::IsValidAmmoType(type)) return;
    SetAmmo(type, Ammo(type) + amount);
}

}  // namespace cssv34::reconstructed
