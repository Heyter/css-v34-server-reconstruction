#include <cassert>
#include <iostream>

#include "public/build4100/damage.h"
#include "server/cs_game_state.h"
#include "server/player_damage.h"

using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::build4100::HitGroup;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;
using cssv34::reconstructed::DamageApplyResult;
using cssv34::reconstructed::DamageModelInput;
using cssv34::reconstructed::ResolvePlayerDamage;

int main() {
    {
        DamageModelInput in;
        in.health = 100;
        in.armor = 100;
        in.hasHelmet = true;
        in.baseDamage = 20.0f;
        in.hitGroup = HitGroup::Head;
        in.weaponArmorRatio = 1.0f;
        const auto out = ResolvePlayerDamage(in);
        assert(out.scaledDamage == 80.0f);
        assert(out.armorProtected && out.headshot);
        assert(out.healthDamage == 40);
        assert(out.healthAfter == 60);
        assert(out.armorDamage == 20);
        assert(out.armorAfter == 80);
    }

    {
        DamageModelInput in;
        in.health = 100;
        in.armor = 100;
        in.hasHelmet = false;
        in.baseDamage = 20.0f;
        in.hitGroup = HitGroup::Head;
        const auto out = ResolvePlayerDamage(in);
        assert(!out.armorProtected);
        assert(out.healthDamage == 80);
        assert(out.armorAfter == 100);
    }

    {
        DamageModelInput in;
        in.health = 100;
        in.armor = 5;
        in.baseDamage = 40.0f;
        in.hitGroup = HitGroup::Chest;
        in.weaponArmorRatio = 1.0f;
        const auto out = ResolvePlayerDamage(in);
        assert(out.armorProtected);
        assert(out.armorAfter == 0);
        assert(out.healthDamage == 30);
        assert(out.healthAfter == 70);
    }

    {
        DamageModelInput in;
        in.health = 100;
        in.armor = 100;
        in.baseDamage = 40.0f;
        in.hitGroup = HitGroup::LeftLeg;
        const auto out = ResolvePlayerDamage(in);
        assert(!out.armorProtected);
        assert(out.healthDamage == 30);
        assert(out.armorAfter == 100);
    }

    ClientRegistry clients;
    assert(clients.Configure(4));
    CsGameState game(clients);
    game.Configure();

    Edict t{}, ct{}, ctMate{};
    char reject[64]{};
    assert(clients.Connect(1, &t, "t", "10.0.0.1", reject, sizeof(reject)));
    assert(clients.Connect(2, &ct, "ct", "10.0.0.2", reject, sizeof(reject)));
    assert(clients.Connect(3, &ctMate, "ct2", "10.0.0.3", reject, sizeof(reject)));
    for (std::size_t i = 1; i <= 3; ++i) assert(game.OnClientPutInServer(i));
    assert(game.JoinTeam(1, static_cast<int>(CSTeam::Terrorist)));
    assert(game.JoinTeam(2, static_cast<int>(CSTeam::CounterTerrorist)));
    assert(game.JoinTeam(3, static_cast<int>(CSTeam::CounterTerrorist)));
    assert(game.EnterGame(1));
    assert(game.EnterGame(2));
    assert(game.EnterGame(3));

    clients.Get(2)->armorValue = 100;
    clients.Get(2)->hasHelmet = true;
    auto applied = game.DamagePlayer(2, 1, 20.0f, HitGroup::Head, 1.0f);
    assert(applied == DamageApplyResult::Applied);
    assert(clients.Get(2)->health == 60);
    assert(clients.Get(2)->armorValue == 80);
    assert(clients.Get(2)->lastHitGroup == HitGroup::Head);
    assert(clients.Get(2)->lastAttackerIndex == 1);
    assert(clients.Get(2)->lastDamageTaken == 40);
    assert(!clients.Get(2)->lastDeathWasHeadshot);

    assert(game.DamagePlayer(3, 2, 120.0f, HitGroup::Chest, 2.0f) ==
           DamageApplyResult::FriendlyFireBlocked);
    assert(clients.Get(3)->health == 100);

    game.SetFriendlyFire(true);
    assert(game.DamagePlayer(3, 2, 20.0f, HitGroup::Chest, 2.0f) == DamageApplyResult::Applied);
    assert(clients.Get(3)->health == 80);

    clients.Get(2)->armorValue = 0;
    assert(game.DamagePlayer(2, 1, 20.0f, HitGroup::Head, 1.0f) == DamageApplyResult::Killed);
    assert(clients.Get(2)->isDead);
    assert(clients.Get(2)->lastDeathWasHeadshot);
    assert(clients.Get(1)->fragCount == 1);
    assert(clients.Get(2)->deathCount == 1);

    assert(game.DamagePlayer(2, 1, 10.0f, HitGroup::Chest, 1.0f) ==
           DamageApplyResult::AlreadyDead);

    std::cout << "PLAYER_DAMAGE_V0_14_0=PASS\n";
    std::cout << "hitgroups=head4,stomach1.25,legs0.75 armor=helmet/body friendlyfire=gate death=headshot\n";
    return 0;
}
