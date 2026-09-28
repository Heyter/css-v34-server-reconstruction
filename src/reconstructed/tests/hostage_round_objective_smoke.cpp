#include <cassert>
#include <cmath>
#include <iostream>

#include "public/build4100/cstrike_defs.h"
#include "server/cs_game_state.h"

using cssv34::build4100::CSRoundEndReason;
using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;
using cssv34::reconstructed::CsMapObjective;
using cssv34::reconstructed::CsRoundPhase;
using cssv34::reconstructed::HostageState;

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

    game.ConfigureMapObjective(CsMapObjective::Hostage, 4, 100);
    game.RestartRound(10.0f);
    assert(game.MapObjective() == CsMapObjective::Hostage);
    assert(game.BombCarrierIndex() == -1);
    assert(game.Hostages().Total() == 4);
    assert(game.Hostages().AliveUnrescuedCount() == 4);
    assert(game.Hostages().RequiredRescues() == 2);
    game.EndFreezePeriod();

    // Only living CTs can lead/rescue hostages during a live round.
    assert(!game.BeginHostageFollow(0, 1));
    assert(game.BeginHostageFollow(0, 3));
    assert(game.Hostages().Get(0)->leaderClientIndex == 3);
    assert(game.DropHostage(0, 3));
    assert(game.BeginHostageFollow(0, 3));
    assert(game.RescueHostage(0, 3, 20.0f));
    assert(game.Hostages().Get(0)->state == HostageState::Rescued);
    assert(game.Hostages().RescuedCount() == 1);
    assert(game.RoundPhase() == CsRoundPhase::Live);

    // A dead leader releases every hostage following that CT.
    assert(game.BeginHostageFollow(1, 3));
    assert(game.Kill(3, 1));
    assert(game.Hostages().Get(1)->leaderClientIndex == -1);
    assert(game.Respawn(3));

    // The original rules count only living, unrescued hostages as remaining.
    assert(game.DamageHostage(1, 100, 21.0f));
    assert(game.Hostages().Get(1)->state == HostageState::Dead);
    assert(game.Hostages().DeadCount() == 1);
    assert(game.Hostages().AliveUnrescuedCount() == 2);

    assert(game.BeginHostageFollow(2, 4));
    assert(game.RescueHostage(2, 4, 22.0f));
    assert(game.Hostages().RescuedCount() == 2);
    assert(game.Hostages().AliveUnrescuedCount() == 1);
    assert(game.RoundPhase() == CsRoundPhase::Live);

    // 2/4 rescued meets the 50% quota once no living unrescued hostage remains.
    assert(game.DamageHostage(3, 100, 23.0f, 6.0f));
    assert(game.Hostages().AliveUnrescuedCount() == 0);
    assert(game.RoundPhase() == CsRoundPhase::Ended);
    assert(game.RoundEndReason() == CSRoundEndReason::HostagesRescued);
    assert(std::fabs(game.ScheduledRestartTime() - 29.0f) < 0.001f);

    // Odd totals require ceil(total / 2): 1/3 rescued is not enough.
    game.ConfigureMapObjective(CsMapObjective::Hostage, 3, 100);
    game.RestartRound(100.0f);
    game.EndFreezePeriod();
    assert(game.Hostages().RequiredRescues() == 2);
    assert(game.BeginHostageFollow(0, 3));
    assert(game.RescueHostage(0, 3, 101.0f));
    assert(game.DamageHostage(1, 100, 102.0f));
    assert(game.DamageHostage(2, 100, 103.0f));
    assert(game.Hostages().AliveUnrescuedCount() == 0);
    assert(game.Hostages().RescuedCount() == 1);
    assert(game.RoundPhase() == CsRoundPhase::Live);
    assert(game.ExpireRoundTimer(120.0f, 4.0f));
    assert(game.RoundEndReason() == CSRoundEndReason::HostagesNotRescued);
    assert(std::fabs(game.ScheduledRestartTime() - 124.0f) < 0.001f);

    // Team extermination is checked before hostage rescue in CCSGameRules.
    game.ConfigureMapObjective(CsMapObjective::Hostage, 2, 100);
    game.RestartRound(200.0f);
    game.EndFreezePeriod();
    assert(game.Kill(1, 3));
    assert(game.Kill(2, 3));
    assert(game.CheckWinConditions(202.0f));
    assert(game.RoundEndReason() == CSRoundEndReason::CTWin);

    static_assert(static_cast<int>(CSRoundEndReason::HostagesRescued) == 10);
    static_assert(static_cast<int>(CSRoundEndReason::HostagesNotRescued) == 12);

    std::cout << "HOSTAGE_ROUND_OBJECTIVE_V0_10_0=PASS\n";
    std::cout << "hostages=follow,drop_on_leader_death,damage,rescue,50pct_quota,timeout,elimination\n";
    return 0;
}
