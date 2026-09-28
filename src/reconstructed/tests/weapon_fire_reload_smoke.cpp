#include <cassert>
#include <iostream>

#include "public/build4100/cs_weapon_runtime.h"
#include "server/cs_game_state.h"

using cssv34::build4100::CSAmmoType;
using cssv34::build4100::CSTeam;
using cssv34::build4100::CSWeaponID;
using cssv34::build4100::Edict;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;
using cssv34::reconstructed::WeaponActionResult;

int main() {
    static_assert(cssv34::build4100::MaxClipForWeapon(CSWeaponID::P228) == 13);
    static_assert(cssv34::build4100::MaxClipForWeapon(CSWeaponID::Glock) == 20);
    static_assert(cssv34::build4100::MaxClipForWeapon(CSWeaponID::Ak47) == 30);
    static_assert(cssv34::build4100::MaxClipForWeapon(CSWeaponID::Galil) == 35);
    static_assert(cssv34::build4100::MaxClipForWeapon(CSWeaponID::P90) == 50);
    static_assert(cssv34::build4100::MaxClipForWeapon(CSWeaponID::M249) == 100);

    ClientRegistry clients;
    assert(clients.Configure(2));
    CsGameState game(clients);
    game.Configure();

    Edict terrorist{};
    char reject[64]{};
    assert(clients.Connect(1, &terrorist, "t", "10.0.0.1", reject, sizeof(reject)));
    assert(game.OnClientPutInServer(1));
    assert(game.JoinTeam(1, static_cast<int>(CSTeam::Terrorist)));
    assert(game.EnterGame(1));

    auto* player = clients.Get(1);
    assert(player);
    assert(player->weapons.ActiveWeapon() == CSWeaponID::Glock);
    assert(player->weapons.Clip(CSWeaponID::Glock) == 20);
    assert(player->weapons.Ammo(CSAmmoType::Ammo9Mm) == 40);

    // Freeze period blocks primary fire without consuming the magazine.
    game.RestartRound(10.0f);
    assert(player->weapons.ActiveWeapon() == CSWeaponID::Glock);
    assert(game.FireActiveWeapon(1) == WeaponActionResult::FreezePeriod);
    assert(player->weapons.Clip(CSWeaponID::Glock) == 20);
    game.EndFreezePeriod();

    for (int i = 0; i < 20; ++i) {
        assert(game.FireActiveWeapon(1) == WeaponActionResult::Fired);
    }
    assert(player->weapons.Clip(CSWeaponID::Glock) == 0);
    assert(game.FireActiveWeapon(1) == WeaponActionResult::EmptyClip);

    // Reload moves rounds from the source-owned reserve pool into the magazine.
    assert(game.ReloadActiveWeapon(1) == WeaponActionResult::Reloaded);
    assert(player->weapons.Clip(CSWeaponID::Glock) == 20);
    assert(player->weapons.Ammo(CSAmmoType::Ammo9Mm) == 20);
    assert(game.ReloadActiveWeapon(1) == WeaponActionResult::ClipFull);

    // A purchased rifle starts with its classic full magazine and reloads from
    // the caliber-specific reserve pool without crossing magazine/max-carry bounds.
    assert(game.SetInBuyZone(1, true));
    assert(game.AddAccount(1, 5000));
    assert(game.BuyWeapon(1, CSWeaponID::Ak47, 11.0f) == cssv34::reconstructed::BuyResult::Purchased);
    assert(game.SelectWeapon(1, CSWeaponID::Ak47));
    assert(player->weapons.Clip(CSWeaponID::Ak47) == 30);
    assert(game.BuyAmmo(1, CSWeaponID::Ak47, 11.0f) == cssv34::reconstructed::BuyResult::Purchased);
    assert(player->weapons.Ammo(CSAmmoType::Ammo762Mm) == 30);

    for (int i = 0; i < 30; ++i) {
        assert(game.FireActiveWeapon(1) == WeaponActionResult::Fired);
    }
    assert(game.FireActiveWeapon(1) == WeaponActionResult::EmptyClip);
    assert(game.ReloadActiveWeapon(1) == WeaponActionResult::Reloaded);
    assert(player->weapons.Clip(CSWeaponID::Ak47) == 30);
    assert(player->weapons.Ammo(CSAmmoType::Ammo762Mm) == 0);

    assert(game.FireActiveWeapon(1) == WeaponActionResult::Fired);
    assert(player->weapons.Clip(CSWeaponID::Ak47) == 29);
    assert(game.ReloadActiveWeapon(1) == WeaponActionResult::NoReserveAmmo);

    // Non-firearm active slots are not silently treated as guns.
    assert(game.SelectWeapon(1, CSWeaponID::Knife));
    assert(game.FireActiveWeapon(1) == WeaponActionResult::NotFirearm);
    assert(game.ReloadActiveWeapon(1) == WeaponActionResult::NotFirearm);

    std::cout << "WEAPON_FIRE_RELOAD_V0_13_0=PASS\n";
    std::cout << "magazines=classic_css fire=clip_consume reload=reserve_transfer freeze_gate=PASS\n";
    return 0;
}
