#include <array>
#include <cassert>
#include <cstring>
#include <iostream>

#include "public/build4100/cs_weapon.h"
#include "public/build4100/cstrike_defs.h"
#include "server/cs_game_state.h"
#include "server/weapon_inventory.h"

using cssv34::build4100::CSWeaponID;
using cssv34::build4100::CSWeaponSlot;
using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;
using cssv34::reconstructed::WeaponInventory;

int main() {
    static_assert(static_cast<int>(CSWeaponID::P228) == 1);
    static_assert(static_cast<int>(CSWeaponID::C4) == 6);
    static_assert(static_cast<int>(CSWeaponID::Ak47) == 27);
    static_assert(static_cast<int>(CSWeaponID::Knife) == 28);
    static_assert(static_cast<int>(CSWeaponID::NightVision) == 33);

    assert(std::strcmp(cssv34::build4100::WeaponIDToAlias(CSWeaponID::None), "none") == 0);
    assert(std::strcmp(cssv34::build4100::WeaponIDToAlias(CSWeaponID::HEGrenade), "hegren") == 0);
    assert(std::strcmp(cssv34::build4100::WeaponIDToAlias(CSWeaponID::Flashbang), "flash") == 0);
    assert(std::strcmp(cssv34::build4100::WeaponIDToClassname(CSWeaponID::Usp), "weapon_usp") == 0);
    assert(cssv34::build4100::AliasToWeaponID("AK47") == CSWeaponID::Ak47);
    assert(cssv34::build4100::ClassnameToWeaponID("weapon_flashbang") == CSWeaponID::Flashbang);

    constexpr std::array<int, 19> kPrimary = {3,5,7,8,12,13,14,15,17,18,19,20,21,22,23,26,27,29,30};
    constexpr std::array<int, 6> kSecondary = {1,2,10,11,16,25};
    int primaryCount = 0;
    int secondaryCount = 0;
    for (int i = 0; i <= 33; ++i) {
        const auto id = static_cast<CSWeaponID>(i);
        if (cssv34::build4100::IsPrimaryWeapon(id)) ++primaryCount;
        if (cssv34::build4100::IsSecondaryWeapon(id)) ++secondaryCount;
    }
    assert(primaryCount == static_cast<int>(kPrimary.size()));
    assert(secondaryCount == static_cast<int>(kSecondary.size()));
    for (const int i : kPrimary) assert(cssv34::build4100::IsPrimaryWeapon(static_cast<CSWeaponID>(i)));
    for (const int i : kSecondary) assert(cssv34::build4100::IsSecondaryWeapon(static_cast<CSWeaponID>(i)));

    WeaponInventory inventory;
    assert(inventory.Give(CSWeaponID::Ak47));
    assert(inventory.ActiveWeapon() == CSWeaponID::Ak47);
    assert(inventory.HasPrimaryWeapon());
    assert(!inventory.Give(CSWeaponID::M4A1));
    assert(inventory.Give(CSWeaponID::Glock, false));
    assert(inventory.HasSecondaryWeapon());
    assert(inventory.Select(CSWeaponID::Glock));
    assert(std::strcmp(inventory.ActiveWeaponClassname(), "weapon_glock") == 0);
    assert(inventory.Remove(CSWeaponID::Ak47));
    assert(!inventory.HasPrimaryWeapon());
    assert(inventory.Give(CSWeaponID::Shield, false));
    assert(inventory.WeaponInSlot(CSWeaponSlot::Rifle) == CSWeaponID::Shield);

    assert(inventory.Give(CSWeaponID::Flashbang, false));
    assert(inventory.Give(CSWeaponID::Flashbang, false));
    assert(!inventory.Give(CSWeaponID::Flashbang, false));
    assert(inventory.Count(CSWeaponID::Flashbang) == 2);
    assert(inventory.Give(CSWeaponID::HEGrenade, false));
    assert(!inventory.Give(CSWeaponID::HEGrenade, false));
    assert(!inventory.Give(CSWeaponID::Kevlar, false));

    ClientRegistry clients;
    assert(clients.Configure(4));
    CsGameState game(clients);
    game.Configure();

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

    const auto* t = clients.Get(1);
    const auto* ct = clients.Get(2);
    assert(t && ct);
    assert(t->weapons.Has(CSWeaponID::Knife) && t->weapons.Has(CSWeaponID::Glock));
    assert(t->weapons.Ammo9mm() == 40 && t->weapons.Ammo45Acp() == 0);
    assert(t->weapons.ActiveWeapon() == CSWeaponID::Glock);
    assert(ct->weapons.Has(CSWeaponID::Knife) && ct->weapons.Has(CSWeaponID::Usp));
    assert(ct->weapons.Ammo45Acp() == 24 && ct->weapons.Ammo9mm() == 0);
    assert(ct->weapons.ActiveWeapon() == CSWeaponID::Usp);

    assert(game.GiveWeapon(1, CSWeaponID::Ak47, false));
    assert(game.HasPrimaryWeapon(1));
    assert(!game.GiveWeapon(1, CSWeaponID::M4A1, false));
    assert(game.SelectWeapon(1, CSWeaponID::Ak47));
    assert(std::strcmp(clients.Get(1)->weapons.ActiveWeaponClassname(), "weapon_ak47") == 0);
    assert(game.DropPrimaryWeapon(1));
    assert(!game.HasPrimaryWeapon(1));
    assert(game.GiveWeapon(1, CSWeaponID::M4A1));
    assert(game.SelectWeapon(1, CSWeaponID::M4A1));

    game.RestartRound(5.0f);
    assert(clients.Get(1)->hasC4);
    assert(clients.Get(1)->weapons.Has(CSWeaponID::C4));
    assert(game.DropBomb(1));
    assert(!clients.Get(1)->hasC4 && !clients.Get(1)->weapons.Has(CSWeaponID::C4));
    assert(game.PickupBomb(1));
    assert(clients.Get(1)->hasC4 && clients.Get(1)->weapons.Has(CSWeaponID::C4));

    std::cout << "WEAPON_INVENTORY_V0_11_0=PASS\n";
    std::cout << "weapon_ids=0..33 primary_mask=0x6cfef1a8 secondary_mask=0x02010c06 default=T:glock40,CT:usp24 C4_sync=PASS\n";
    return 0;
}
