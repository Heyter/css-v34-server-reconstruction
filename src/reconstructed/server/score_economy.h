#pragma once

#include "public/build4100/cstrike_defs.h"

namespace cssv34::reconstructed {

// Constants recovered from the original build4100 Linux server_i486.so:
// - CCSGameRules::PlayerKilled uses +0x12c ($300) for an enemy kill.
// - the team-kill path passes -0xce4 (-$3300) to CCSPlayer::AddAccount.
// - BombRoundEndCheck uses 0xcb2 ($3250), 0xdac ($3500), and 0x320 ($800).
// - HostageRescueRoundEndCheck uses 0x9c4 ($2500).
// - RestartRound initializes the losing bonus to 0x578 ($1400) and advances it by 0x1f4 ($500).
constexpr int kCsEnemyKillReward = 300;
constexpr int kCsTeamKillPenalty = 3300;
constexpr int kCsRoundWinReward = 3250;
constexpr int kCsBombExplodedReward = 3500;
constexpr int kCsBombPlantedButDefusedReward = 800;
constexpr int kCsHostageRescueReward = 2500;
constexpr int kCsLossBonusBase = 1400;
constexpr int kCsLossBonusStep = 500;
constexpr int kCsLossBonusMax = 3400;

struct KillEconomyResult {
    int fragDelta = 0;
    int accountDelta = 0;
    bool teamKill = false;
};

struct RoundEconomyResult {
    int terroristAward = 0;
    int counterTerroristAward = 0;
    int terroristLossStreak = 0;
    int counterTerroristLossStreak = 0;
};

KillEconomyResult ResolveKillEconomy(int victimTeam, int attackerTeam,
                                     bool attackerPresent, bool suicide) noexcept;

int LossBonusForStreak(int consecutiveLosses) noexcept;
int WinnerRewardForReason(build4100::CSRoundEndReason reason) noexcept;

RoundEconomyResult ResolveRoundEconomy(build4100::CSRoundEndReason reason,
                                       bool bombWasPlanted,
                                       int terroristLossStreak,
                                       int counterTerroristLossStreak) noexcept;

}  // namespace cssv34::reconstructed
