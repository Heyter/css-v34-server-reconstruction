#pragma once

#include <array>
#include <cstddef>

#include "public/build4100/cs_ammo.h"
#include "public/build4100/cs_weapon_runtime.h"

namespace cssv34::reconstructed {

// Source-owned inventory model for the build4100 CCSPlayer weapon slots. This
// deliberately models ownership/selection only; entity handles/sendtables are
// a later ABI layer.
class WeaponInventory final {
public:
    void Clear() noexcept;

    bool Give(build4100::CSWeaponID id, bool selectIfEmpty = true) noexcept;
    bool Remove(build4100::CSWeaponID id, int count = 1) noexcept;
    bool Select(build4100::CSWeaponID id) noexcept;

    bool Has(build4100::CSWeaponID id) const noexcept;
    int Count(build4100::CSWeaponID id) const noexcept;
    bool HasPrimaryWeapon() const noexcept;
    bool HasSecondaryWeapon() const noexcept;
    build4100::CSWeaponID WeaponInSlot(build4100::CSWeaponSlot slot) const noexcept;

    build4100::CSWeaponID ActiveWeapon() const noexcept { return activeWeapon_; }
    const char* ActiveWeaponClassname() const noexcept;

    int Clip(build4100::CSWeaponID id) const noexcept;
    int MaxClip(build4100::CSWeaponID id) const noexcept;
    bool SetClip(build4100::CSWeaponID id, int amount) noexcept;
    bool ConsumeRound(build4100::CSWeaponID id) noexcept;
    int Reload(build4100::CSWeaponID id) noexcept;

    int Ammo(build4100::CSAmmoType type) const noexcept;
    void SetAmmo(build4100::CSAmmoType type, int amount) noexcept;
    void GiveAmmo(build4100::CSAmmoType type, int amount) noexcept;

    int Ammo9mm() const noexcept { return Ammo(build4100::CSAmmoType::Ammo9Mm); }
    int Ammo45Acp() const noexcept { return Ammo(build4100::CSAmmoType::Ammo45Acp); }
    void SetAmmo9mm(int amount) noexcept { SetAmmo(build4100::CSAmmoType::Ammo9Mm, amount); }
    void SetAmmo45Acp(int amount) noexcept { SetAmmo(build4100::CSAmmoType::Ammo45Acp, amount); }
    void GiveAmmo9mm(int amount) noexcept { GiveAmmo(build4100::CSAmmoType::Ammo9Mm, amount); }
    void GiveAmmo45Acp(int amount) noexcept { GiveAmmo(build4100::CSAmmoType::Ammo45Acp, amount); }

private:
    static int MaxOwned(build4100::CSWeaponID id) noexcept;
    bool SlotOccupiedByOther(build4100::CSWeaponID id) const noexcept;

    std::array<unsigned char, build4100::kBuild4100WeaponIdMax + 1> counts_{};
    std::array<int, build4100::kBuild4100WeaponIdMax + 1> clips_{};
    build4100::CSWeaponID activeWeapon_ = build4100::CSWeaponID::None;
    std::array<int, build4100::kCsAmmoTypeCount> ammo_{};
};

}  // namespace cssv34::reconstructed
