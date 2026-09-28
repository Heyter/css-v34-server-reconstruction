#pragma once

#include <cstdint>

namespace cssv34::build4100 {

enum class CSTeam : std::int32_t {
    Unassigned = 0,
    Spectator = 1,
    Terrorist = 2,
    CounterTerrorist = 3,
};

// CS:S build4100 player-state numbering. The states and ordering are preserved
// by the original CCSPlayer state machine (State_Enter_* / State_PreThink_*).

// Counter-Strike: Source round termination reasons used by CCSGameRules.
// Values 0..15 match the pre-CS:GO CSS ordering.
enum class CSRoundEndReason : std::int32_t {
    Invalid = -1,
    TargetBombed = 0,
    VIPEscaped = 1,
    VIPKilled = 2,
    TerroristsEscaped = 3,
    CTStoppedEscape = 4,
    TerroristsStopped = 5,
    BombDefused = 6,
    CTWin = 7,
    TerroristWin = 8,
    Draw = 9,
    HostagesRescued = 10,
    TargetSaved = 11,
    HostagesNotRescued = 12,
    TerroristsNotEscaped = 13,
    VIPNotEscaped = 14,
    GameStart = 15,
};

enum class CSPlayerState : std::int32_t {
    Active = 0,
    Welcome = 1,
    PickingTeam = 2,
    PickingClass = 3,
    DeathAnim = 4,
    DeathWaitForKey = 5,
    ObserverMode = 6,
};

constexpr int kCsDefaultHealth = 100;
constexpr int kCsDefaultStartMoney = 800;
constexpr int kCsDefaultMaxMoney = 16000;

constexpr bool IsPlayableTeam(const int team) noexcept {
    return team == static_cast<int>(CSTeam::Terrorist) ||
           team == static_cast<int>(CSTeam::CounterTerrorist);
}

}  // namespace cssv34::build4100
