#pragma once

#include <cstddef>

#include "public/build4100/cs_buy.h"
#include "server/client_registry.h"

namespace cssv34::reconstructed {

enum class BuyResult {
    Purchased,
    InvalidPlayer,
    Dead,
    NotInBuyZone,
    BuyTimeExpired,
    WrongTeam,
    NotPurchasable,
    AlreadyOwned,
    InsufficientFunds,
    AmmoFull,
    InventoryRejected,
};

class BuyEconomy final {
public:
    explicit BuyEconomy(ClientRegistry& clients) noexcept : clients_(clients) {}

    void Configure(float buyTimeSeconds = build4100::kCsDefaultBuyTimeSeconds) noexcept;
    void BeginRound(float roundStartTime) noexcept;

    BuyResult BuyWeapon(std::size_t clientIndex, build4100::CSWeaponID weapon,
                        float now) noexcept;
    BuyResult BuyAmmo(std::size_t clientIndex, build4100::CSWeaponID weapon,
                      float now) noexcept;
    BuyResult BuyEquipment(std::size_t clientIndex, build4100::CSEquipment equipment,
                           float now) noexcept;

    bool CanBuy(std::size_t clientIndex, float now) const noexcept;
    float BuyTimeSeconds() const noexcept { return buyTimeSeconds_; }
    float BuyTimeEnd() const noexcept { return buyTimeEnd_; }

private:
    ClientRecord* Player(std::size_t clientIndex) noexcept;
    const ClientRecord* Player(std::size_t clientIndex) const noexcept;
    BuyResult CheckBuyAccess(const ClientRecord* player, float now) const noexcept;
    bool Spend(ClientRecord& player, int amount) noexcept;

    ClientRegistry& clients_;
    float buyTimeSeconds_ = build4100::kCsDefaultBuyTimeSeconds;
    float buyTimeEnd_ = 0.0f;
};

}  // namespace cssv34::reconstructed
