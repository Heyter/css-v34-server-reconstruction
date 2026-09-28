#include <cassert>
#include <iostream>

#include "public/build4100/cstrike_defs.h"
#include "server/cs_game_state.h"
#include "server/score_economy.h"

using cssv34::build4100::CSRoundEndReason;
using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;
using cssv34::reconstructed::LossBonusForStreak;
using cssv34::reconstructed::ResolveKillEconomy;
using cssv34::reconstructed::ResolveRoundEconomy;
using cssv34::reconstructed::WinnerRewardForReason;

int main() {
    const int t = static_cast<int>(CSTeam::Terrorist);
    const int ct = static_cast<int>(CSTeam::CounterTerrorist);

    {
        const auto enemy = ResolveKillEconomy(ct, t, true, false);
        assert(enemy.fragDelta == 1 && enemy.accountDelta == 300 && !enemy.teamKill);
        const auto tk = ResolveKillEconomy(ct, ct, true, false);
        assert(tk.fragDelta == -1 && tk.accountDelta == -3300 && tk.teamKill);
        const auto suicide = ResolveKillEconomy(t, t, true, true);
        assert(suicide.fragDelta == -1 && suicide.accountDelta == 0 && !suicide.teamKill);
        assert(ResolveKillEconomy(t, ct, false, false).accountDelta == 0);
    }

    assert(LossBonusForStreak(0) == 0);
    assert(LossBonusForStreak(1) == 1400);
    assert(LossBonusForStreak(2) == 1900);
    assert(LossBonusForStreak(3) == 2400);
    assert(LossBonusForStreak(4) == 2900);
    assert(LossBonusForStreak(5) == 3400);
    assert(LossBonusForStreak(99) == 3400);
    assert(WinnerRewardForReason(CSRoundEndReason::TargetBombed) == 3500);
    assert(WinnerRewardForReason(CSRoundEndReason::BombDefused) == 3250);
    assert(WinnerRewardForReason(CSRoundEndReason::HostagesRescued) == 2500);

    {
        const auto defuse = ResolveRoundEconomy(CSRoundEndReason::BombDefused, true, 0, 0);
        assert(defuse.counterTerroristAward == 3250);
        assert(defuse.terroristAward == 2200);  // $1400 loss + $800 planted-bomb bonus.
        assert(defuse.terroristLossStreak == 1 && defuse.counterTerroristLossStreak == 0);

        const auto secondLoss = ResolveRoundEconomy(
            CSRoundEndReason::BombDefused, false,
            defuse.terroristLossStreak, defuse.counterTerroristLossStreak);
        assert(secondLoss.terroristAward == 1900);
        assert(secondLoss.counterTerroristAward == 3250);
        assert(secondLoss.terroristLossStreak == 2);
    }

    ClientRegistry clients;
    assert(clients.Configure(4));
    CsGameState game(clients);
    game.Configure();

    Edict t1{}, ct1{}, ct2{};
    char reject[64]{};
    assert(clients.Connect(1, &t1, "t1", "10.0.0.1", reject, sizeof(reject)));
    assert(clients.Connect(2, &ct1, "ct1", "10.0.0.2", reject, sizeof(reject)));
    assert(clients.Connect(3, &ct2, "ct2", "10.0.0.3", reject, sizeof(reject)));
    for (std::size_t i = 1; i <= 3; ++i) assert(game.OnClientPutInServer(i));
    assert(game.JoinTeam(1, t));
    assert(game.JoinTeam(2, ct));
    assert(game.JoinTeam(3, ct));
    assert(game.EnterGame(1));
    assert(game.EnterGame(2));
    assert(game.EnterGame(3));

    assert(clients.Get(1)->account == 800);
    assert(game.Kill(2, 1));
    assert(clients.Get(1)->fragCount == 1);
    assert(clients.Get(1)->account == 1100);
    assert(clients.Get(2)->deathCount == 1);

    assert(game.Respawn(2));
    assert(game.Kill(2, 3));
    assert(clients.Get(3)->fragCount == -1);
    assert(clients.Get(3)->teamKillCount == 1);
    assert(clients.Get(3)->account == 0);

    assert(game.Respawn(1));
    assert(game.Kill(1, 1));
    assert(clients.Get(1)->fragCount == 0);

    // Round money is queued on termination and paid on the next RestartRound,
    // matching the original server's m_iAccountTerrorist/CT accumulation shape.
    assert(game.TerminateRound(CSRoundEndReason::CTWin, 10.0f, 5.0f));
    assert(game.PendingTerroristAward() == 1400);
    assert(game.PendingCounterTerroristAward() == 3250);
    assert(game.TerroristLossStreak() == 1);
    assert(game.CounterTerroristLossStreak() == 0);

    const int tMoneyBefore = clients.Get(1)->account;
    const int ctMoneyBefore = clients.Get(2)->account;
    game.RestartRound(20.0f);
    assert(clients.Get(1)->account == tMoneyBefore + 1400);
    assert(clients.Get(2)->account == ctMoneyBefore + 3250);
    assert(game.PendingTerroristAward() == 0 && game.PendingCounterTerroristAward() == 0);

    std::cout << "SCORE_ECONOMY_V0_15_0=PASS\n";
    std::cout << "kill=$300 teamkill=-$3300 loss=$1400+500<=3400 round=$3250 bomb=$3500 defused_loser=+$800 hostage=$2500\n";
    return 0;
}
