#pragma once

#include <cstddef>

#include "public/build4100/cstrike_defs.h"
#include "server/bomb_objective.h"
#include "server/client_registry.h"
#include "server/buy_economy.h"
#include "server/hostage_objective.h"
#include "server/player_damage.h"
#include "server/score_economy.h"
#include "server/weapon_action.h"

namespace cssv34::reconstructed {

struct CsTeamCounts {
    int terrorists = 0;
    int counterTerrorists = 0;
    int aliveTerrorists = 0;
    int aliveCounterTerrorists = 0;
};

enum class CsRoundPhase {
    Waiting,
    Freeze,
    Live,
    Ended,
};

enum class DamageApplyResult {
    Applied,
    Killed,
    InvalidPlayer,
    AlreadyDead,
    FriendlyFireBlocked,
    NoDamage,
};

enum class CsMapObjective {
    None,
    Bomb,
    Hostage,
};

enum class CsRoundTickResult {
    None,
    FreezeEnded,
    RoundTimedOut,
    ObjectiveResolved,
    Restarted,
};

// Source-level reconstruction of deterministic CS:S player/round/objective
// state. Entity and sendtable exposure consumes this state in later layers.
class CsGameState final {
public:
    explicit CsGameState(ClientRegistry& clients) noexcept : clients_(clients), economy_(clients) {}

    void Configure(int startMoney = build4100::kCsDefaultStartMoney,
                   int maxMoney = build4100::kCsDefaultMaxMoney,
                   float bombTimerSeconds = 45.0f,
                   float buyTimeSeconds = build4100::kCsDefaultBuyTimeSeconds) noexcept;
    void ConfigureRoundTiming(float freezeTimeSeconds, float roundTimeSeconds,
                              float restartDelaySeconds = 5.0f) noexcept;

    bool OnClientPutInServer(std::size_t clientIndex) noexcept;
    bool JoinTeam(std::size_t clientIndex, int team) noexcept;
    bool EnterGame(std::size_t clientIndex) noexcept;
    bool Kill(std::size_t victimIndex, std::size_t attackerIndex = 0) noexcept;
    DamageApplyResult DamagePlayer(std::size_t victimIndex, std::size_t attackerIndex,
                                   float baseDamage, build4100::HitGroup hitGroup,
                                   float weaponArmorRatio = 1.0f,
                                   bool bypassArmor = false) noexcept;
    void SetFriendlyFire(bool enabled) noexcept { friendlyFire_ = enabled; }
    bool FriendlyFire() const noexcept { return friendlyFire_; }
    bool Respawn(std::size_t clientIndex) noexcept;
    bool AddAccount(std::size_t clientIndex, int amount) noexcept;
    bool GiveDefuser(std::size_t clientIndex, bool value = true) noexcept;
    bool SetInBuyZone(std::size_t clientIndex, bool value) noexcept;
    BuyResult BuyWeapon(std::size_t clientIndex, build4100::CSWeaponID weapon, float now) noexcept;
    BuyResult BuyAmmo(std::size_t clientIndex, build4100::CSWeaponID weapon, float now) noexcept;
    BuyResult BuyEquipment(std::size_t clientIndex, build4100::CSEquipment equipment, float now) noexcept;

    bool GiveDefaultItems(std::size_t clientIndex) noexcept;
    bool GiveWeapon(std::size_t clientIndex, build4100::CSWeaponID weapon,
                    bool selectIfEmpty = true) noexcept;
    bool SelectWeapon(std::size_t clientIndex, build4100::CSWeaponID weapon) noexcept;
    bool DropPrimaryWeapon(std::size_t clientIndex) noexcept;
    bool DropSecondaryWeapon(std::size_t clientIndex) noexcept;
    bool HasPrimaryWeapon(std::size_t clientIndex) const noexcept;
    bool HasSecondaryWeapon(std::size_t clientIndex) const noexcept;
    WeaponActionResult FireActiveWeapon(std::size_t clientIndex) noexcept;
    WeaponActionResult ReloadActiveWeapon(std::size_t clientIndex) noexcept;

    void ConfigureMapObjective(CsMapObjective objective,
                               std::size_t hostageCount = 0,
                               int hostageHealth = 100);
    bool BeginHostageFollow(std::size_t hostageIndex, std::size_t clientIndex) noexcept;
    bool DropHostage(std::size_t hostageIndex, std::size_t clientIndex) noexcept;
    bool RescueHostage(std::size_t hostageIndex, std::size_t clientIndex, float now,
                       float restartDelay = 5.0f) noexcept;
    bool DamageHostage(std::size_t hostageIndex, int damage, float now,
                       float restartDelay = 5.0f) noexcept;
    bool CheckHostageWinConditions(float now, float restartDelay = 5.0f) noexcept;

    void RestartRound(float roundStartTime) noexcept;
    void EndFreezePeriod() noexcept;
    CsRoundTickResult TickRound(float now) noexcept;
    bool AssignBombCarrier(std::size_t preferredClientIndex = 0) noexcept;
    bool DropBomb(std::size_t clientIndex) noexcept;
    bool PickupBomb(std::size_t clientIndex) noexcept;
    bool PlantBomb(std::size_t clientIndex, float now, int siteIndex = -1,
                   const build4100::Vector& origin = {}) noexcept;
    bool BeginDefuse(std::size_t clientIndex, float now) noexcept;
    bool CancelDefuse(std::size_t clientIndex) noexcept;
    BombObjectiveTransition TickObjective(float now, float restartDelay = 5.0f) noexcept;

    bool CheckWinConditions(float now, float restartDelay = 5.0f) noexcept;
    bool ExpireRoundTimer(float now, float restartDelay = 5.0f) noexcept;
    bool TerminateRound(build4100::CSRoundEndReason reason, float now,
                        float restartDelay = 5.0f) noexcept;
    void AwardRound(int winningTeam) noexcept;
    static int WinningTeam(build4100::CSRoundEndReason reason) noexcept;

    CsTeamCounts CountTeams() const noexcept;

    int StartMoney() const noexcept { return startMoney_; }
    int MaxMoney() const noexcept { return maxMoney_; }
    int RoundNumber() const noexcept { return roundNumber_; }
    float RoundStartTime() const noexcept { return roundStartTime_; }
    bool IsFreezePeriod() const noexcept { return freezePeriod_; }
    CsRoundPhase RoundPhase() const noexcept { return roundPhase_; }
    build4100::CSRoundEndReason RoundEndReason() const noexcept { return roundEndReason_; }
    float RoundEndTime() const noexcept { return roundEndTime_; }
    float ScheduledRestartTime() const noexcept { return scheduledRestartTime_; }
    float FreezeEndTime() const noexcept { return freezeEndTime_; }
    float RoundDeadline() const noexcept { return roundDeadline_; }
    float FreezeTimeSeconds() const noexcept { return freezeTimeSeconds_; }
    float RoundTimeSeconds() const noexcept { return roundTimeSeconds_; }
    float RestartDelaySeconds() const noexcept { return restartDelaySeconds_; }
    int TerroristScore() const noexcept { return terroristScore_; }
    int CounterTerroristScore() const noexcept { return counterTerroristScore_; }
    int PendingTerroristAward() const noexcept { return pendingTerroristAward_; }
    int PendingCounterTerroristAward() const noexcept { return pendingCounterTerroristAward_; }
    int TerroristLossStreak() const noexcept { return terroristLossStreak_; }
    int CounterTerroristLossStreak() const noexcept { return counterTerroristLossStreak_; }
    CsMapObjective MapObjective() const noexcept { return mapObjective_; }
    int BombCarrierIndex() const noexcept { return bomb_.CarrierIndex(); }
    const BombObjective& Bomb() const noexcept { return bomb_; }
    const HostageObjective& Hostages() const noexcept { return hostages_; }
    const BuyEconomy& Economy() const noexcept { return economy_; }

private:
    ClientRecord* Player(std::size_t clientIndex) noexcept;
    const ClientRecord* Player(std::size_t clientIndex) const noexcept;
    void ClearBombOwnership() noexcept;
    void QueueRoundEconomy(build4100::CSRoundEndReason reason) noexcept;
    void PayPendingRoundAwards() noexcept;
    ClientRegistry& clients_;
    BuyEconomy economy_;
    BombObjective bomb_;
    HostageObjective hostages_;
    CsMapObjective mapObjective_ = CsMapObjective::Bomb;
    int startMoney_ = build4100::kCsDefaultStartMoney;
    int maxMoney_ = build4100::kCsDefaultMaxMoney;
    int roundNumber_ = 0;
    float roundStartTime_ = 0.0f;
    bool freezePeriod_ = false;
    CsRoundPhase roundPhase_ = CsRoundPhase::Waiting;
    build4100::CSRoundEndReason roundEndReason_ = build4100::CSRoundEndReason::Invalid;
    float roundEndTime_ = 0.0f;
    float scheduledRestartTime_ = 0.0f;
    float freezeTimeSeconds_ = 6.0f;
    float roundTimeSeconds_ = 180.0f;
    float restartDelaySeconds_ = 5.0f;
    float freezeEndTime_ = 0.0f;
    float roundDeadline_ = 0.0f;
    int terroristScore_ = 0;
    int counterTerroristScore_ = 0;
    int pendingTerroristAward_ = 0;
    int pendingCounterTerroristAward_ = 0;
    int terroristLossStreak_ = 0;
    int counterTerroristLossStreak_ = 0;
    bool friendlyFire_ = false;
};

}  // namespace cssv34::reconstructed
