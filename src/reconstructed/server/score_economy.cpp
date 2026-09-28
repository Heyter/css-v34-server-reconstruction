#include "server/score_economy.h"

#include <algorithm>

namespace cssv34::reconstructed {

KillEconomyResult ResolveKillEconomy(const int victimTeam, const int attackerTeam,
                                     const bool attackerPresent, const bool suicide) noexcept {
    KillEconomyResult result;
    if (!attackerPresent) return result;

    if (suicide) {
        result.fragDelta = -1;
        return result;
    }

    if (!build4100::IsPlayableTeam(victimTeam) || !build4100::IsPlayableTeam(attackerTeam)) {
        return result;
    }

    if (victimTeam == attackerTeam) {
        result.fragDelta = -1;
        result.accountDelta = -kCsTeamKillPenalty;
        result.teamKill = true;
        return result;
    }

    result.fragDelta = 1;
    result.accountDelta = kCsEnemyKillReward;
    return result;
}

int LossBonusForStreak(const int consecutiveLosses) noexcept {
    if (consecutiveLosses <= 0) return 0;
    const int steps = std::clamp(consecutiveLosses - 1, 0, 4);
    return std::min(kCsLossBonusBase + steps * kCsLossBonusStep, kCsLossBonusMax);
}

int WinnerRewardForReason(const build4100::CSRoundEndReason reason) noexcept {
    switch (reason) {
        case build4100::CSRoundEndReason::TargetBombed:
            return kCsBombExplodedReward;
        case build4100::CSRoundEndReason::HostagesRescued:
            return kCsHostageRescueReward;
        case build4100::CSRoundEndReason::BombDefused:
        case build4100::CSRoundEndReason::CTWin:
        case build4100::CSRoundEndReason::TerroristWin:
        case build4100::CSRoundEndReason::TargetSaved:
        case build4100::CSRoundEndReason::HostagesNotRescued:
        case build4100::CSRoundEndReason::VIPEscaped:
        case build4100::CSRoundEndReason::VIPKilled:
        case build4100::CSRoundEndReason::TerroristsEscaped:
        case build4100::CSRoundEndReason::CTStoppedEscape:
        case build4100::CSRoundEndReason::TerroristsStopped:
        case build4100::CSRoundEndReason::TerroristsNotEscaped:
        case build4100::CSRoundEndReason::VIPNotEscaped:
            return kCsRoundWinReward;
        default:
            return 0;
    }
}

RoundEconomyResult ResolveRoundEconomy(const build4100::CSRoundEndReason reason,
                                       const bool bombWasPlanted,
                                       const int terroristLossStreak,
                                       const int counterTerroristLossStreak) noexcept {
    RoundEconomyResult result;
    result.terroristLossStreak = std::max(0, terroristLossStreak);
    result.counterTerroristLossStreak = std::max(0, counterTerroristLossStreak);

    const auto winningTeam = [reason]() noexcept {
        switch (reason) {
            case build4100::CSRoundEndReason::TargetBombed:
            case build4100::CSRoundEndReason::VIPKilled:
            case build4100::CSRoundEndReason::TerroristsEscaped:
            case build4100::CSRoundEndReason::TerroristWin:
            case build4100::CSRoundEndReason::HostagesNotRescued:
                return static_cast<int>(build4100::CSTeam::Terrorist);
            case build4100::CSRoundEndReason::VIPEscaped:
            case build4100::CSRoundEndReason::CTStoppedEscape:
            case build4100::CSRoundEndReason::TerroristsStopped:
            case build4100::CSRoundEndReason::BombDefused:
            case build4100::CSRoundEndReason::CTWin:
            case build4100::CSRoundEndReason::HostagesRescued:
            case build4100::CSRoundEndReason::TargetSaved:
            case build4100::CSRoundEndReason::TerroristsNotEscaped:
            case build4100::CSRoundEndReason::VIPNotEscaped:
                return static_cast<int>(build4100::CSTeam::CounterTerrorist);
            default:
                return static_cast<int>(build4100::CSTeam::Unassigned);
        }
    }();

    const int winnerReward = WinnerRewardForReason(reason);
    if (winningTeam == static_cast<int>(build4100::CSTeam::Terrorist)) {
        result.terroristAward += winnerReward;
        result.terroristLossStreak = 0;
        result.counterTerroristLossStreak = std::min(result.counterTerroristLossStreak + 1, 5);
        result.counterTerroristAward += LossBonusForStreak(result.counterTerroristLossStreak);
    } else if (winningTeam == static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
        result.counterTerroristAward += winnerReward;
        result.counterTerroristLossStreak = 0;
        result.terroristLossStreak = std::min(result.terroristLossStreak + 1, 5);
        result.terroristAward += LossBonusForStreak(result.terroristLossStreak);
        if (reason == build4100::CSRoundEndReason::BombDefused && bombWasPlanted) {
            result.terroristAward += kCsBombPlantedButDefusedReward;
        }
    }

    return result;
}

}  // namespace cssv34::reconstructed
