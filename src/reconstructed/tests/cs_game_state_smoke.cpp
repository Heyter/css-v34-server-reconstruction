#include <cassert>
#include <iostream>

#include "public/build4100/cstrike_defs.h"
#include "server/cs_game_state.h"

using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::build4100::CSPlayerState;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;

int main() {
    ClientRegistry clients;
    assert(clients.Configure(8));
    CsGameState game(clients);
    game.Configure(800, 16000);

    Edict t1{}, t2{}, ct1{}, spec{};
    char reject[64]{};
    assert(clients.Connect(1, &t1, "t1", "10.0.0.1", reject, sizeof(reject)));
    assert(clients.Connect(2, &t2, "t2", "10.0.0.2", reject, sizeof(reject)));
    assert(clients.Connect(3, &ct1, "ct1", "10.0.0.3", reject, sizeof(reject)));
    assert(clients.Connect(4, &spec, "spec", "10.0.0.4", reject, sizeof(reject)));
    for (std::size_t i = 1; i <= 4; ++i) assert(game.OnClientPutInServer(i));

    assert(game.JoinTeam(1, static_cast<int>(CSTeam::Terrorist)));
    assert(game.JoinTeam(2, static_cast<int>(CSTeam::Terrorist)));
    assert(game.JoinTeam(3, static_cast<int>(CSTeam::CounterTerrorist)));
    assert(game.JoinTeam(4, static_cast<int>(CSTeam::Spectator)));
    assert(clients.Get(4)->csState == CSPlayerState::ObserverMode && clients.Get(4)->isObserver);

    assert(game.EnterGame(1));
    assert(game.EnterGame(2));
    assert(game.EnterGame(3));
    assert(!game.EnterGame(4));
    assert(clients.Get(1)->health == 100 && clients.Get(1)->csState == CSPlayerState::Active);

    assert(game.AddAccount(1, 20000));
    assert(clients.Get(1)->account == 16000);
    assert(game.AddAccount(1, -20000));
    assert(clients.Get(1)->account == 0);
    assert(game.AddAccount(1, 1500));
    assert(clients.Get(1)->account == 1500);

    assert(!game.GiveDefuser(1));
    assert(game.GiveDefuser(3));
    assert(clients.Get(3)->hasDefuser);
    assert(game.SetInBuyZone(1, true) && clients.Get(1)->inBuyZone);

    game.RestartRound(12.5f);
    assert(game.RoundNumber() == 1 && game.RoundStartTime() == 12.5f && game.IsFreezePeriod());
    assert(game.BombCarrierIndex() == 1 && clients.Get(1)->hasC4);
    assert(!clients.Get(3)->hasDefuser);
    game.EndFreezePeriod();
    assert(!game.IsFreezePeriod());

    auto counts = game.CountTeams();
    assert(counts.terrorists == 2 && counts.counterTerrorists == 1);
    assert(counts.aliveTerrorists == 2 && counts.aliveCounterTerrorists == 1);

    assert(game.Kill(1, 3));
    assert(clients.Get(1)->isDead && clients.Get(1)->health == 0);
    assert(clients.Get(1)->deathCount == 1);
    assert(clients.Get(3)->fragCount == 1);
    assert(game.BombCarrierIndex() == -1);
    assert(game.AssignBombCarrier());
    assert(game.BombCarrierIndex() == 2 && clients.Get(2)->hasC4);

    counts = game.CountTeams();
    assert(counts.aliveTerrorists == 1 && counts.aliveCounterTerrorists == 1);
    assert(game.Respawn(1));
    assert(!clients.Get(1)->isDead && clients.Get(1)->health == 100);

    game.AwardRound(static_cast<int>(CSTeam::CounterTerrorist));
    game.AwardRound(static_cast<int>(CSTeam::Terrorist));
    game.AwardRound(static_cast<int>(CSTeam::CounterTerrorist));
    assert(game.TerroristScore() == 1 && game.CounterTerroristScore() == 2);

    std::cout << "CS_GAME_STATE_V0_6_0=PASS\n";
    std::cout << "teams=T2,CT1,SPEC1 money=0..16000 round=1 bomb=handoff kill_respawn=PASS\n";
    return 0;
}
