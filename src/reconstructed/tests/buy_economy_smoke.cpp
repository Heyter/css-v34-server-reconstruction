#include <cassert>
#include <iostream>

#include "public/build4100/cs_buy.h"
#include "server/cs_game_state.h"

using cssv34::build4100::CSAmmoType;
using cssv34::build4100::CSEquipment;
using cssv34::build4100::CSTeam;
using cssv34::build4100::CSWeaponID;
using cssv34::build4100::Edict;
using cssv34::reconstructed::BuyResult;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;

int main() {
    static_assert(cssv34::build4100::WeaponBuyInfo(CSWeaponID::Ak47).price == 2500);
    static_assert(cssv34::build4100::WeaponBuyInfo(CSWeaponID::M4A1).price == 3100);
    static_assert(cssv34::build4100::WeaponBuyInfo(CSWeaponID::Awp).price == 4750);
    static_assert(cssv34::build4100::AmmoInfoFor(CSAmmoType::Ammo762Mm).price == 80);
    static_assert(cssv34::build4100::AmmoInfoFor(CSAmmoType::Ammo762Mm).buySize == 30);
    static_assert(cssv34::build4100::AmmoInfoFor(CSAmmoType::Ammo762Mm).maxCarry == 90);

    ClientRegistry clients;
    assert(clients.Configure(4));
    CsGameState game(clients);
    game.Configure(800, 16000, 45.0f, 90.0f);

    Edict terrorist{}, counterTerrorist{};
    char reject[64]{};
    assert(clients.Connect(1, &terrorist, "t", "10.0.0.1", reject, sizeof(reject)));
    assert(clients.Connect(2, &counterTerrorist, "ct", "10.0.0.2", reject, sizeof(reject)));
    assert(game.OnClientPutInServer(1));
    assert(game.OnClientPutInServer(2));
    assert(game.JoinTeam(1, static_cast<int>(CSTeam::Terrorist)));
    assert(game.JoinTeam(2, static_cast<int>(CSTeam::CounterTerrorist)));
    assert(game.EnterGame(1));
    assert(game.EnterGame(2));
    assert(game.AddAccount(1, 5000));
    assert(game.AddAccount(2, 8000));

    game.RestartRound(10.0f);
    assert(game.Economy().BuyTimeEnd() == 100.0f);

    assert(game.BuyWeapon(1, CSWeaponID::Ak47, 11.0f) == BuyResult::NotInBuyZone);
    assert(game.SetInBuyZone(1, true));
    assert(game.SetInBuyZone(2, true));

    // Team-specific rifle availability and exact classic CSS prices.
    assert(game.BuyWeapon(1, CSWeaponID::M4A1, 11.0f) == BuyResult::WrongTeam);
    assert(game.BuyWeapon(2, CSWeaponID::Ak47, 11.0f) == BuyResult::WrongTeam);
    const int tBeforeAk = clients.Get(1)->account;
    assert(game.BuyWeapon(1, CSWeaponID::Ak47, 11.0f) == BuyResult::Purchased);
    assert(clients.Get(1)->account == tBeforeAk - 2500);
    assert(clients.Get(1)->weapons.Has(CSWeaponID::Ak47));

    const int ctBeforeM4 = clients.Get(2)->account;
    assert(game.BuyWeapon(2, CSWeaponID::M4A1, 11.0f) == BuyResult::Purchased);
    assert(clients.Get(2)->account == ctBeforeM4 - 3100);
    assert(clients.Get(2)->weapons.Has(CSWeaponID::M4A1));

    // Buying a new weapon in the same slot replaces the old owned weapon.
    assert(game.BuyWeapon(1, CSWeaponID::Deagle, 12.0f) == BuyResult::Purchased);
    assert(!clients.Get(1)->weapons.Has(CSWeaponID::Glock));
    assert(clients.Get(1)->weapons.Has(CSWeaponID::Deagle));

    // Reserve ammo is bought by caliber, in fixed boxes, capped at max carry.
    assert(game.BuyAmmo(1, CSWeaponID::Ak47, 12.0f) == BuyResult::Purchased);
    assert(clients.Get(1)->weapons.Ammo(CSAmmoType::Ammo762Mm) == 30);
    assert(game.BuyAmmo(1, CSWeaponID::Ak47, 12.0f) == BuyResult::Purchased);
    assert(game.BuyAmmo(1, CSWeaponID::Ak47, 12.0f) == BuyResult::Purchased);
    assert(clients.Get(1)->weapons.Ammo(CSAmmoType::Ammo762Mm) == 90);
    assert(game.BuyAmmo(1, CSWeaponID::Ak47, 12.0f) == BuyResult::AmmoFull);

    // Equipment is account-backed and preserves the classic vest/helmet split.
    const int ctBeforeArmor = clients.Get(2)->account;
    assert(game.BuyEquipment(2, CSEquipment::Kevlar, 13.0f) == BuyResult::Purchased);
    assert(clients.Get(2)->armorValue == 100);
    assert(clients.Get(2)->account == ctBeforeArmor - 650);
    const int ctBeforeHelmet = clients.Get(2)->account;
    assert(game.BuyEquipment(2, CSEquipment::AssaultSuit, 13.0f) == BuyResult::Purchased);
    assert(clients.Get(2)->hasHelmet);
    assert(clients.Get(2)->account == ctBeforeHelmet - 350);
    assert(game.BuyEquipment(2, CSEquipment::Defuser, 13.0f) == BuyResult::Purchased);
    assert(clients.Get(2)->hasDefuser);
    assert(game.BuyEquipment(1, CSEquipment::Defuser, 13.0f) == BuyResult::WrongTeam);
    assert(game.BuyEquipment(1, CSEquipment::NightVision, 13.0f) == BuyResult::Purchased);
    assert(clients.Get(1)->hasNightVision);

    // Buy period closes even while the player remains physically in a buy zone.
    assert(game.BuyWeapon(1, CSWeaponID::Ump45, 100.01f) == BuyResult::BuyTimeExpired);

    std::cout << "BUY_ECONOMY_V0_12_0=PASS\n";
    std::cout << "prices=classic_css team_restrictions=PASS ammo_boxes=9 maxcarry=PASS equipment=kevlar,helmet,defuser,nvg buytime=90\n";
    return 0;
}
