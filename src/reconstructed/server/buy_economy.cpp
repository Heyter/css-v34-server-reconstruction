#include "server/buy_economy.h"

#include <algorithm>

namespace cssv34::reconstructed {

void BuyEconomy::Configure(const float buyTimeSeconds) noexcept {
    buyTimeSeconds_ = std::max(0.0f, buyTimeSeconds);
    buyTimeEnd_ = 0.0f;
}

void BuyEconomy::BeginRound(const float roundStartTime) noexcept {
    buyTimeEnd_ = roundStartTime + buyTimeSeconds_;
}

ClientRecord* BuyEconomy::Player(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = clients_.Get(clientIndex);
    return player && player->state != ClientLifecycleState::Empty ? player : nullptr;
}

const ClientRecord* BuyEconomy::Player(const std::size_t clientIndex) const noexcept {
    const ClientRecord* const player = clients_.Get(clientIndex);
    return player && player->state != ClientLifecycleState::Empty ? player : nullptr;
}

BuyResult BuyEconomy::CheckBuyAccess(const ClientRecord* const player, const float now) const noexcept {
    if (!player) return BuyResult::InvalidPlayer;
    if (player->isDead) return BuyResult::Dead;
    if (!build4100::IsPlayableTeam(player->teamIndex)) return BuyResult::WrongTeam;
    if (!player->inBuyZone) return BuyResult::NotInBuyZone;
    if (now > buyTimeEnd_) return BuyResult::BuyTimeExpired;
    return BuyResult::Purchased;
}

bool BuyEconomy::CanBuy(const std::size_t clientIndex, const float now) const noexcept {
    return CheckBuyAccess(Player(clientIndex), now) == BuyResult::Purchased;
}

bool BuyEconomy::Spend(ClientRecord& player, const int amount) noexcept {
    if (amount < 0 || player.account < amount) return false;
    player.account -= amount;
    return true;
}

BuyResult BuyEconomy::BuyWeapon(const std::size_t clientIndex,
                                const build4100::CSWeaponID weapon,
                                const float now) noexcept {
    ClientRecord* const player = Player(clientIndex);
    const BuyResult access = CheckBuyAccess(player, now);
    if (access != BuyResult::Purchased) return access;

    const auto info = build4100::WeaponBuyInfo(weapon);
    if (!info.purchasable) return BuyResult::NotPurchasable;
    if (!build4100::CanTeamBuyWeapon(weapon, player->teamIndex)) return BuyResult::WrongTeam;
    if (player->weapons.Has(weapon) && !build4100::IsGrenadeWeapon(weapon)) {
        return BuyResult::AlreadyOwned;
    }
    if (build4100::IsGrenadeWeapon(weapon) &&
        player->weapons.Count(weapon) >= (weapon == build4100::CSWeaponID::Flashbang ? 2 : 1)) {
        return BuyResult::AlreadyOwned;
    }
    if (player->account < info.price) return BuyResult::InsufficientFunds;

    const auto slot = build4100::WeaponSlotForID(weapon);
    build4100::CSWeaponID replaced = build4100::CSWeaponID::None;
    if (slot == build4100::CSWeaponSlot::Rifle || slot == build4100::CSWeaponSlot::Pistol) {
        replaced = player->weapons.WeaponInSlot(slot);
        if (replaced != build4100::CSWeaponID::None && replaced != weapon) {
            (void)player->weapons.Remove(replaced);
        }
    }

    if (!player->weapons.Give(weapon, true)) {
        if (replaced != build4100::CSWeaponID::None) (void)player->weapons.Give(replaced, false);
        return BuyResult::InventoryRejected;
    }
    (void)Spend(*player, info.price);
    return BuyResult::Purchased;
}

BuyResult BuyEconomy::BuyAmmo(const std::size_t clientIndex,
                              const build4100::CSWeaponID weapon,
                              const float now) noexcept {
    ClientRecord* const player = Player(clientIndex);
    const BuyResult access = CheckBuyAccess(player, now);
    if (access != BuyResult::Purchased) return access;
    if (!player->weapons.Has(weapon)) return BuyResult::NotPurchasable;

    const auto ammoType = build4100::AmmoTypeForWeapon(weapon);
    if (!build4100::IsValidAmmoType(ammoType)) return BuyResult::NotPurchasable;
    const auto ammoInfo = build4100::AmmoInfoFor(ammoType);
    if (player->weapons.Ammo(ammoType) >= ammoInfo.maxCarry) return BuyResult::AmmoFull;
    if (player->account < ammoInfo.price) return BuyResult::InsufficientFunds;

    player->weapons.GiveAmmo(ammoType, ammoInfo.buySize);
    (void)Spend(*player, ammoInfo.price);
    return BuyResult::Purchased;
}

BuyResult BuyEconomy::BuyEquipment(const std::size_t clientIndex,
                                   const build4100::CSEquipment equipment,
                                   const float now) noexcept {
    ClientRecord* const player = Player(clientIndex);
    const BuyResult access = CheckBuyAccess(player, now);
    if (access != BuyResult::Purchased) return access;

    int price = 0;
    switch (equipment) {
        case build4100::CSEquipment::Kevlar:
            if (player->armorValue >= 100) return BuyResult::AlreadyOwned;
            price = build4100::kCsKevlarPrice;
            break;
        case build4100::CSEquipment::AssaultSuit:
            if (player->armorValue >= 100 && player->hasHelmet) return BuyResult::AlreadyOwned;
            if (player->armorValue < 100) price += build4100::kCsKevlarPrice;
            if (!player->hasHelmet) price += build4100::kCsHelmetPrice;
            break;
        case build4100::CSEquipment::Defuser:
            if (player->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
                return BuyResult::WrongTeam;
            }
            if (player->hasDefuser) return BuyResult::AlreadyOwned;
            price = build4100::kCsDefuserPrice;
            break;
        case build4100::CSEquipment::NightVision:
            if (player->hasNightVision) return BuyResult::AlreadyOwned;
            price = build4100::kCsNightVisionPrice;
            break;
    }

    if (player->account < price) return BuyResult::InsufficientFunds;
    (void)Spend(*player, price);
    switch (equipment) {
        case build4100::CSEquipment::Kevlar:
            player->armorValue = 100;
            break;
        case build4100::CSEquipment::AssaultSuit:
            player->armorValue = 100;
            player->hasHelmet = true;
            break;
        case build4100::CSEquipment::Defuser:
            player->hasDefuser = true;
            break;
        case build4100::CSEquipment::NightVision:
            player->hasNightVision = true;
            break;
    }
    return BuyResult::Purchased;
}

}  // namespace cssv34::reconstructed
