#include <cassert>
#include <iostream>

#include "public/build4100/cstrike_defs.h"
#include "server/cs_game_state.h"

using cssv34::build4100::CSRoundEndReason;
using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;
using cssv34::reconstructed::CsRoundPhase;
using cssv34::reconstructed::CsRoundTickResult;

int main() {
    ClientRegistry clients;
    assert(clients.Configure(4));
    CsGameState game(clients);
    game.Configure();
    game.ConfigureRoundTiming(2.0f, 10.0f, 3.0f);

    Edict t1{}, t2{}, ct1{}, ct2{};
    Edict* edicts[] = {nullptr, &t1, &t2, &ct1, &ct2};
    const char* names[] = {nullptr, "t1", "t2", "ct1", "ct2"};
    char reject[64]{};
    for (std::size_t i = 1; i <= 4; ++i) {
        assert(clients.Connect(i, edicts[i], names[i], "127.0.0.1", reject, sizeof(reject)));
        assert(game.OnClientPutInServer(i));
    }
    assert(game.JoinTeam(1, static_cast<int>(CSTeam::Terrorist)));
    assert(game.JoinTeam(2, static_cast<int>(CSTeam::Terrorist)));
    assert(game.JoinTeam(3, static_cast<int>(CSTeam::CounterTerrorist)));
    assert(game.JoinTeam(4, static_cast<int>(CSTeam::CounterTerrorist)));
    for (std::size_t i = 1; i <= 4; ++i) assert(game.EnterGame(i));

    game.RestartRound(100.0f);
    assert(game.RoundNumber() == 1);
    assert(game.RoundPhase() == CsRoundPhase::Freeze);
    assert(game.FreezeEndTime() == 102.0f);
    assert(game.RoundDeadline() == 112.0f);

    auto counts = game.CountTeams();
    assert(counts.terrorists == 2 && counts.aliveTerrorists == 2);
    assert(counts.counterTerrorists == 2 && counts.aliveCounterTerrorists == 2);

    assert(game.TickRound(101.9f) == CsRoundTickResult::None);
    assert(game.TickRound(102.0f) == CsRoundTickResult::FreezeEnded);
    assert(game.RoundPhase() == CsRoundPhase::Live);

    assert(game.TickRound(111.9f) == CsRoundTickResult::None);
    assert(game.TickRound(112.0f) == CsRoundTickResult::RoundTimedOut);
    assert(game.RoundPhase() == CsRoundPhase::Ended);
    assert(game.RoundEndReason() == CSRoundEndReason::TargetSaved);
    assert(game.CounterTerroristScore() == 1);
    assert(game.PendingTerroristAward() == 1400);
    assert(game.PendingCounterTerroristAward() == 3250);
    assert(game.ScheduledRestartTime() == 115.0f);

    const int tMoney = clients.Get(1)->account;
    const int ctMoney = clients.Get(3)->account;
    assert(game.TickRound(114.9f) == CsRoundTickResult::None);
    assert(game.TickRound(115.0f) == CsRoundTickResult::Restarted);
    assert(game.RoundNumber() == 2);
    assert(game.RoundPhase() == CsRoundPhase::Freeze);
    assert(game.FreezeEndTime() == 117.0f);
    assert(game.RoundDeadline() == 127.0f);
    assert(clients.Get(1)->account == tMoney + 1400);
    assert(clients.Get(3)->account == ctMoney + 3250);
    assert(game.PendingTerroristAward() == 0);
    assert(game.PendingCounterTerroristAward() == 0);

    std::cout << "ROUND_LIFECYCLE_V0_16_0=PASS\n";
    std::cout << "freeze=2 round=10 restart=3 timeout=TargetSaved auto_restart=PASS team_count_regression=PASS\n";
    return 0;
}
