#include <cassert>
#include <cmath>
#include <iostream>

#include "public/build4100/cstrike_defs.h"
#include "server/cs_game_state.h"

using cssv34::build4100::CSRoundEndReason;
using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::build4100::Vector;
using cssv34::reconstructed::BombObjectiveState;
using cssv34::reconstructed::BombObjectiveTransition;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;
using cssv34::reconstructed::CsRoundPhase;

namespace {

void ConnectAndJoin(ClientRegistry& clients, CsGameState& game,
                    const std::size_t index, Edict& edict,
                    const char* name, const int team) {
    char reject[64]{};
    assert(clients.Connect(index, &edict, name, "127.0.0.1", reject, sizeof(reject)));
    assert(game.OnClientPutInServer(index));
    assert(game.JoinTeam(index, team));
    assert(game.EnterGame(index));
}

}  // namespace

int main() {
    ClientRegistry clients;
    assert(clients.Configure(8));
    CsGameState game(clients);
    game.Configure(800, 16000, 45.0f);

    Edict t1{}, t2{}, ct1{}, ct2{};
    ConnectAndJoin(clients, game, 1, t1, "t1", static_cast<int>(CSTeam::Terrorist));
    ConnectAndJoin(clients, game, 2, t2, "t2", static_cast<int>(CSTeam::Terrorist));
    ConnectAndJoin(clients, game, 3, ct1, "ct1", static_cast<int>(CSTeam::CounterTerrorist));
    ConnectAndJoin(clients, game, 4, ct2, "ct2", static_cast<int>(CSTeam::CounterTerrorist));

    game.RestartRound(100.0f);
    assert(game.RoundPhase() == CsRoundPhase::Freeze);
    assert(game.Bomb().State() == BombObjectiveState::Carried);
    assert(game.BombCarrierIndex() == 1 && clients.Get(1)->hasC4);
    assert(!game.PlantBomb(1, 101.0f));  // freeze period

    game.EndFreezePeriod();
    assert(game.RoundPhase() == CsRoundPhase::Live);
    assert(game.DropBomb(1));
    assert(game.Bomb().State() == BombObjectiveState::Dropped);
    assert(!clients.Get(1)->hasC4);
    assert(!game.PickupBomb(3));  // CT cannot pick up C4
    assert(game.PickupBomb(2));
    assert(game.BombCarrierIndex() == 2 && clients.Get(2)->hasC4);

    const Vector siteA{128.0f, -64.0f, 16.0f};
    assert(game.PlantBomb(2, 110.0f, 1, siteA));
    assert(game.Bomb().State() == BombObjectiveState::Planted);
    assert(game.BombCarrierIndex() == -1 && !clients.Get(2)->hasC4);
    assert(std::fabs(game.Bomb().ExplodeTime() - 155.0f) < 0.001f);
    assert(game.Bomb().SiteIndex() == 1);
    assert(std::fabs(game.Bomb().PlantOrigin().x - siteA.x) < 0.001f);
    assert(std::fabs(game.Bomb().PlantOrigin().y - siteA.y) < 0.001f);
    assert(std::fabs(game.Bomb().PlantOrigin().z - siteA.z) < 0.001f);
    assert(!game.ExpireRoundTimer(130.0f));  // planted bomb overrides round timer

    assert(game.BeginDefuse(3, 120.0f));
    assert(std::fabs(game.Bomb().DefuseCompleteTime() - 130.0f) < 0.001f);
    assert(std::fabs(game.Bomb().DefuseLength() - 10.0f) < 0.001f);
    // Held +use arrives in repeated usercmd batches. The same owner is
    // idempotent and must not push the deadline forward; another CT cannot
    // steal the active C4 defuse.
    assert(game.BeginDefuse(3, 124.0f));
    assert(std::fabs(game.Bomb().DefuseCompleteTime() - 130.0f) < 0.001f);
    assert(!game.BeginDefuse(4, 124.0f));
    assert(game.CancelDefuse(3));
    assert(game.GiveDefuser(4));
    assert(game.BeginDefuse(4, 124.0f));
    assert(std::fabs(game.Bomb().DefuseCompleteTime() - 129.0f) < 0.001f);
    assert(std::fabs(game.Bomb().DefuseLength() - 5.0f) < 0.001f);
    assert(game.TickObjective(128.99f) == BombObjectiveTransition::None);
    assert(game.TickObjective(129.0f, 6.0f) == BombObjectiveTransition::Defused);
    assert(game.Bomb().State() == BombObjectiveState::Defused);
    assert(game.RoundPhase() == CsRoundPhase::Ended);
    assert(game.RoundEndReason() == CSRoundEndReason::BombDefused);
    assert(game.CounterTerroristScore() == 1);
    assert(std::fabs(game.ScheduledRestartTime() - 135.0f) < 0.001f);

    // A planted bomb explodes on the C4 timer and scores for T.
    game.RestartRound(200.0f);
    game.EndFreezePeriod();
    assert(game.BombCarrierIndex() == 1);
    assert(game.PlantBomb(1, 205.0f));
    assert(game.TickObjective(249.99f) == BombObjectiveTransition::None);
    assert(game.TickObjective(250.0f) == BombObjectiveTransition::Exploded);
    assert(game.RoundEndReason() == CSRoundEndReason::TargetBombed);
    assert(game.TerroristScore() == 1);

    // Round timeout is a CT target-save only while no bomb is planted.
    game.RestartRound(300.0f);
    game.EndFreezePeriod();
    assert(game.ExpireRoundTimer(420.0f, 3.0f));
    assert(game.RoundEndReason() == CSRoundEndReason::TargetSaved);
    assert(game.CounterTerroristScore() == 2);
    assert(std::fabs(game.ScheduledRestartTime() - 423.0f) < 0.001f);

    // Elimination: dead terrorists lose unless the bomb is already planted.
    game.RestartRound(500.0f);
    game.EndFreezePeriod();
    assert(game.Kill(1, 3));
    assert(game.Kill(2, 3));
    assert(game.CheckWinConditions(505.0f));
    assert(game.RoundEndReason() == CSRoundEndReason::CTWin);
    assert(game.CounterTerroristScore() == 3);

    game.RestartRound(600.0f);
    game.EndFreezePeriod();
    assert(game.PlantBomb(1, 602.0f));
    assert(game.Kill(1, 3));
    assert(game.Kill(2, 3));
    assert(!game.CheckWinConditions(605.0f));
    assert(game.RoundPhase() == CsRoundPhase::Live);
    assert(game.BeginDefuse(3, 606.0f));
    assert(game.Kill(3, 1));
    assert(game.Bomb().DefuserIndex() == -1);
    assert(game.TickObjective(616.0f) == BombObjectiveTransition::None);
    assert(game.GiveDefuser(4));
    assert(game.BeginDefuse(4, 616.0f));
    assert(game.TickObjective(621.0f) == BombObjectiveTransition::Defused);
    assert(game.RoundEndReason() == CSRoundEndReason::BombDefused);

    // No living CTs means immediate terrorist round win, even with a planted bomb.
    game.RestartRound(700.0f);
    game.EndFreezePeriod();
    assert(game.PlantBomb(1, 702.0f));
    assert(game.Kill(3, 1));
    assert(game.Kill(4, 1));
    assert(game.CheckWinConditions(704.0f));
    assert(game.RoundEndReason() == CSRoundEndReason::TerroristWin);
    assert(game.TerroristScore() == 2);

    static_assert(static_cast<int>(CSRoundEndReason::TargetBombed) == 0);
    static_assert(static_cast<int>(CSRoundEndReason::BombDefused) == 6);
    static_assert(static_cast<int>(CSRoundEndReason::CTWin) == 7);
    static_assert(static_cast<int>(CSRoundEndReason::TerroristWin) == 8);
    static_assert(static_cast<int>(CSRoundEndReason::TargetSaved) == 11);
    static_assert(static_cast<int>(CSRoundEndReason::GameStart) == 15);

    std::cout << "BOMB_ROUND_OBJECTIVE_V0_9_1=PASS\n";
    std::cout << "c4=carry,drop,pickup,plant_site_origin,defuse_5_10,idempotent_owner,no_defuse_steal,death_cancel,explode_45 round=terminate,timeout,elimination\n";
    return 0;
}
