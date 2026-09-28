#include "server/cs_game_state.h"

#include <algorithm>

namespace cssv34::reconstructed {

void CsGameState::Configure(const int startMoney, const int maxMoney,
                            const float bombTimerSeconds, const float buyTimeSeconds) noexcept {
    maxMoney_ = std::max(0, maxMoney);
    startMoney_ = std::clamp(startMoney, 0, maxMoney_);
    roundNumber_ = 0;
    roundStartTime_ = 0.0f;
    freezePeriod_ = false;
    roundPhase_ = CsRoundPhase::Waiting;
    roundEndReason_ = build4100::CSRoundEndReason::Invalid;
    roundEndTime_ = 0.0f;
    scheduledRestartTime_ = 0.0f;
    freezeEndTime_ = 0.0f;
    roundDeadline_ = 0.0f;
    terroristScore_ = 0;
    counterTerroristScore_ = 0;
    pendingTerroristAward_ = 0;
    pendingCounterTerroristAward_ = 0;
    terroristLossStreak_ = 0;
    counterTerroristLossStreak_ = 0;
    friendlyFire_ = false;
    mapObjective_ = CsMapObjective::Bomb;
    bomb_.Configure(bombTimerSeconds);
    hostages_.Configure(0);
    economy_.Configure(buyTimeSeconds);
}

void CsGameState::ConfigureRoundTiming(const float freezeTimeSeconds,
                                       const float roundTimeSeconds,
                                       const float restartDelaySeconds) noexcept {
    freezeTimeSeconds_ = std::max(0.0f, freezeTimeSeconds);
    roundTimeSeconds_ = std::max(0.0f, roundTimeSeconds);
    restartDelaySeconds_ = std::max(0.0f, restartDelaySeconds);
}

ClientRecord* CsGameState::Player(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = clients_.Get(clientIndex);
    return player && player->state != ClientLifecycleState::Empty ? player : nullptr;
}

const ClientRecord* CsGameState::Player(const std::size_t clientIndex) const noexcept {
    const ClientRecord* const player = clients_.Get(clientIndex);
    return player && player->state != ClientLifecycleState::Empty ? player : nullptr;
}

bool CsGameState::OnClientPutInServer(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player) return false;
    player->csState = build4100::CSPlayerState::Welcome;
    player->account = startMoney_;
    player->maxHealth = build4100::kCsDefaultHealth;
    player->health = build4100::kCsDefaultHealth;
    player->lastHitGroup = build4100::HitGroup::Invalid;
    player->lastAttackerIndex = 0;
    player->lastDamageTaken = 0;
    player->lastDeathWasHeadshot = false;
    player->isDead = false;
    player->isObserver = false;
    player->hasDefuser = false;
    player->hasHelmet = false;
    player->hasNightVision = false;
    player->armorValue = 0;
    player->hasC4 = false;
    player->weapons.Clear();
    player->inBuyZone = false;
    player->receivesRoundStartMoney = true;
    (void)clients_.SyncPlayerState(clientIndex);
    return true;
}

bool CsGameState::JoinTeam(const std::size_t clientIndex, const int team) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player || team < static_cast<int>(build4100::CSTeam::Unassigned) ||
        team > static_cast<int>(build4100::CSTeam::CounterTerrorist)) return false;

    if (player->hasC4) (void)DropBomb(clientIndex);
    if (player->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist) &&
        team != static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
        (void)hostages_.DropLeader(clientIndex);
        if (bomb_.DefuserIndex() == static_cast<int>(clientIndex)) {
            (void)bomb_.CancelDefuse(clientIndex);
        }
    }
    player->teamIndex = team;

    if (team == static_cast<int>(build4100::CSTeam::Spectator)) {
        player->csState = build4100::CSPlayerState::ObserverMode;
        player->isObserver = true;
        player->isDead = true;
        player->health = 0;
        player->weapons.Clear();
    } else if (build4100::IsPlayableTeam(team)) {
        player->csState = build4100::CSPlayerState::PickingClass;
        player->isObserver = false;
    } else {
        player->csState = build4100::CSPlayerState::PickingTeam;
        player->isObserver = false;
    }
    if (team == static_cast<int>(build4100::CSTeam::Terrorist) &&
        mapObjective_ == CsMapObjective::Bomb &&
        bomb_.State() == BombObjectiveState::Unavailable) {
        (void)AssignBombCarrier(clientIndex);
    }
    (void)clients_.SyncPlayerState(clientIndex);
    return true;
}

bool CsGameState::EnterGame(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player || !build4100::IsPlayableTeam(player->teamIndex)) return false;
    return Respawn(clientIndex);
}

bool CsGameState::Kill(const std::size_t victimIndex, const std::size_t attackerIndex) noexcept {
    ClientRecord* const victim = Player(victimIndex);
    if (!victim || !build4100::IsPlayableTeam(victim->teamIndex) || victim->isDead) return false;

    victim->isDead = true;
    victim->health = 0;
    victim->csState = build4100::CSPlayerState::DeathAnim;
    ++victim->deathCount;
    if (victim->hasC4) (void)DropBomb(victimIndex);
    if (victim->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
        (void)hostages_.DropLeader(victimIndex);
        if (bomb_.DefuserIndex() == static_cast<int>(victimIndex)) {
            (void)bomb_.CancelDefuse(victimIndex);
        }
    }

    if (attackerIndex != 0) {
        ClientRecord* const attacker = Player(attackerIndex);
        if (attacker) {
            const KillEconomyResult economy = ResolveKillEconomy(
                victim->teamIndex, attacker->teamIndex, true, attackerIndex == victimIndex);
            attacker->fragCount += economy.fragDelta;
            if (economy.teamKill) ++attacker->teamKillCount;
            if (economy.accountDelta != 0) (void)AddAccount(attackerIndex, economy.accountDelta);
            (void)clients_.SyncPlayerState(attackerIndex);
        }
    }
    (void)clients_.SyncPlayerState(victimIndex);
    return true;
}

DamageApplyResult CsGameState::DamagePlayer(const std::size_t victimIndex,
                                                const std::size_t attackerIndex,
                                                const float baseDamage,
                                                const build4100::HitGroup hitGroup,
                                                const float weaponArmorRatio,
                                                const bool bypassArmor) noexcept {
    ClientRecord* const victim = Player(victimIndex);
    if (!victim || !build4100::IsPlayableTeam(victim->teamIndex)) {
        return DamageApplyResult::InvalidPlayer;
    }
    if (victim->isDead || victim->health <= 0) return DamageApplyResult::AlreadyDead;

    ClientRecord* attacker = nullptr;
    if (attackerIndex != 0) attacker = Player(attackerIndex);
    const bool teamDamage = attacker && attackerIndex != victimIndex &&
                            build4100::IsPlayableTeam(attacker->teamIndex) &&
                            attacker->teamIndex == victim->teamIndex;
    if (teamDamage && !friendlyFire_) return DamageApplyResult::FriendlyFireBlocked;

    DamageModelInput input;
    input.health = victim->health;
    input.armor = victim->armorValue;
    input.hasHelmet = victim->hasHelmet;
    input.baseDamage = baseDamage;
    input.hitGroup = hitGroup;
    input.weaponArmorRatio = weaponArmorRatio;
    input.bypassArmor = bypassArmor;
    const DamageModelResult resolved = ResolvePlayerDamage(input);
    if (resolved.healthDamage <= 0 && resolved.armorDamage <= 0) {
        return DamageApplyResult::NoDamage;
    }

    victim->health = resolved.healthAfter;
    victim->armorValue = resolved.armorAfter;
    victim->lastHitGroup = hitGroup;
    victim->lastAttackerIndex = attacker ? attackerIndex : 0;
    victim->lastDamageTaken = resolved.healthDamage;
    victim->lastDeathWasHeadshot = false;

    if (resolved.lethal) {
        victim->lastDeathWasHeadshot = resolved.headshot;
        if (!Kill(victimIndex, attacker ? attackerIndex : 0)) return DamageApplyResult::InvalidPlayer;
        return DamageApplyResult::Killed;
    }

    (void)clients_.SyncPlayerState(victimIndex);
    return DamageApplyResult::Applied;
}

bool CsGameState::Respawn(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player || !build4100::IsPlayableTeam(player->teamIndex)) return false;
    player->csState = build4100::CSPlayerState::Active;
    player->isDead = false;
    player->isObserver = false;
    if (player->maxHealth <= 0) player->maxHealth = build4100::kCsDefaultHealth;
    player->health = player->maxHealth;
    player->lastHitGroup = build4100::HitGroup::Invalid;
    player->lastAttackerIndex = 0;
    player->lastDamageTaken = 0;
    player->lastDeathWasHeadshot = false;
    player->inBuyZone = false;
    (void)GiveDefaultItems(clientIndex);
    (void)clients_.SyncPlayerState(clientIndex);
    return true;
}

bool CsGameState::AddAccount(const std::size_t clientIndex, const int amount) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player) return false;
    const long long next = static_cast<long long>(player->account) + amount;
    player->account = static_cast<int>(std::clamp<long long>(next, 0, maxMoney_));
    return true;
}

bool CsGameState::GiveDefuser(const std::size_t clientIndex, const bool value) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player || player->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist)) return false;
    player->hasDefuser = value;
    return true;
}

bool CsGameState::SetInBuyZone(const std::size_t clientIndex, const bool value) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player || !build4100::IsPlayableTeam(player->teamIndex)) return false;
    player->inBuyZone = value;
    return true;
}

BuyResult CsGameState::BuyWeapon(const std::size_t clientIndex,
                                 const build4100::CSWeaponID weapon,
                                 const float now) noexcept {
    return economy_.BuyWeapon(clientIndex, weapon, now);
}

BuyResult CsGameState::BuyAmmo(const std::size_t clientIndex,
                               const build4100::CSWeaponID weapon,
                               const float now) noexcept {
    return economy_.BuyAmmo(clientIndex, weapon, now);
}

BuyResult CsGameState::BuyEquipment(const std::size_t clientIndex,
                                    const build4100::CSEquipment equipment,
                                    const float now) noexcept {
    return economy_.BuyEquipment(clientIndex, equipment, now);
}

bool CsGameState::GiveDefaultItems(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player || !build4100::IsPlayableTeam(player->teamIndex) || player->isDead) return false;

    // Exact build4100 behavior: GiveDefaultItems exits immediately when slot 1
    // already contains a pistol. Otherwise it gives knife + team pistol and
    // 40 rounds of 9mm (T) or 24 rounds of .45 ACP (CT).
    if (player->weapons.HasSecondaryWeapon()) return true;
    (void)player->weapons.Give(build4100::CSWeaponID::Knife, false);

    build4100::CSWeaponID pistol = build4100::CSWeaponID::None;
    if (player->teamIndex == static_cast<int>(build4100::CSTeam::Terrorist)) {
        pistol = build4100::CSWeaponID::Glock;
        if (!player->weapons.Give(pistol, false)) return false;
        player->weapons.GiveAmmo9mm(40);
    } else if (player->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
        pistol = build4100::CSWeaponID::Usp;
        if (!player->weapons.Give(pistol, false)) return false;
        player->weapons.GiveAmmo45Acp(24);
    } else {
        return false;
    }
    return player->weapons.Select(pistol);
}

bool CsGameState::GiveWeapon(const std::size_t clientIndex,
                             const build4100::CSWeaponID weapon,
                             const bool selectIfEmpty) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player || player->isDead || !build4100::IsPlayableTeam(player->teamIndex) ||
        weapon == build4100::CSWeaponID::C4 || build4100::IsEquipmentItem(weapon)) {
        return false;
    }
    return player->weapons.Give(weapon, selectIfEmpty);
}

bool CsGameState::SelectWeapon(const std::size_t clientIndex,
                               const build4100::CSWeaponID weapon) noexcept {
    ClientRecord* const player = Player(clientIndex);
    return player && !player->isDead && player->weapons.Select(weapon);
}

bool CsGameState::DropPrimaryWeapon(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player) return false;
    const auto weapon = player->weapons.WeaponInSlot(build4100::CSWeaponSlot::Rifle);
    return weapon != build4100::CSWeaponID::None && player->weapons.Remove(weapon);
}

bool CsGameState::DropSecondaryWeapon(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player) return false;
    const auto weapon = player->weapons.WeaponInSlot(build4100::CSWeaponSlot::Pistol);
    return weapon != build4100::CSWeaponID::None && player->weapons.Remove(weapon);
}

bool CsGameState::HasPrimaryWeapon(const std::size_t clientIndex) const noexcept {
    const ClientRecord* const player = Player(clientIndex);
    return player && player->weapons.HasPrimaryWeapon();
}

bool CsGameState::HasSecondaryWeapon(const std::size_t clientIndex) const noexcept {
    const ClientRecord* const player = Player(clientIndex);
    return player && player->weapons.HasSecondaryWeapon();
}

WeaponActionResult CsGameState::FireActiveWeapon(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player) return WeaponActionResult::InvalidPlayer;
    if (player->isDead) return WeaponActionResult::Dead;
    if (player->csState != build4100::CSPlayerState::Active) return WeaponActionResult::NotActive;
    if (freezePeriod_) return WeaponActionResult::FreezePeriod;

    const build4100::CSWeaponID weapon = player->weapons.ActiveWeapon();
    if (weapon == build4100::CSWeaponID::None) return WeaponActionResult::NoActiveWeapon;
    if (!build4100::UsesPrimaryMagazine(weapon)) return WeaponActionResult::NotFirearm;
    if (!player->weapons.ConsumeRound(weapon)) return WeaponActionResult::EmptyClip;
    return WeaponActionResult::Fired;
}

WeaponActionResult CsGameState::ReloadActiveWeapon(const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (!player) return WeaponActionResult::InvalidPlayer;
    if (player->isDead) return WeaponActionResult::Dead;
    if (player->csState != build4100::CSPlayerState::Active) return WeaponActionResult::NotActive;

    const build4100::CSWeaponID weapon = player->weapons.ActiveWeapon();
    if (weapon == build4100::CSWeaponID::None) return WeaponActionResult::NoActiveWeapon;
    if (!build4100::UsesPrimaryMagazine(weapon)) return WeaponActionResult::NotFirearm;
    if (player->weapons.Clip(weapon) >= player->weapons.MaxClip(weapon)) {
        return WeaponActionResult::ClipFull;
    }
    const build4100::CSAmmoType ammoType = build4100::AmmoTypeForWeapon(weapon);
    if (!build4100::IsValidAmmoType(ammoType) || player->weapons.Ammo(ammoType) <= 0) {
        return WeaponActionResult::NoReserveAmmo;
    }
    return player->weapons.Reload(weapon) > 0
               ? WeaponActionResult::Reloaded
               : WeaponActionResult::NoReserveAmmo;
}

void CsGameState::ConfigureMapObjective(const CsMapObjective objective,
                                        const std::size_t hostageCount,
                                        const int hostageHealth) {
    mapObjective_ = objective;
    ClearBombOwnership();
    bomb_.ResetRound();
    if (mapObjective_ == CsMapObjective::Hostage) {
        hostages_.Configure(hostageCount, hostageHealth);
    } else {
        hostages_.Configure(0);
    }
}

bool CsGameState::BeginHostageFollow(const std::size_t hostageIndex,
                                     const std::size_t clientIndex) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (mapObjective_ != CsMapObjective::Hostage || !player || player->isDead ||
        player->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist) ||
        roundPhase_ != CsRoundPhase::Live) return false;
    return hostages_.BeginFollow(hostageIndex, clientIndex);
}

bool CsGameState::DropHostage(const std::size_t hostageIndex,
                              const std::size_t clientIndex) noexcept {
    if (mapObjective_ != CsMapObjective::Hostage) return false;
    return hostages_.DropFollow(hostageIndex, clientIndex);
}

bool CsGameState::RescueHostage(const std::size_t hostageIndex,
                                const std::size_t clientIndex, const float now,
                                const float restartDelay) noexcept {
    ClientRecord* const player = Player(clientIndex);
    if (mapObjective_ != CsMapObjective::Hostage || !player || player->isDead ||
        player->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist) ||
        roundPhase_ != CsRoundPhase::Live ||
        !hostages_.Rescue(hostageIndex, clientIndex)) return false;
    (void)CheckHostageWinConditions(now, restartDelay);
    return true;
}

bool CsGameState::DamageHostage(const std::size_t hostageIndex, const int damage,
                                const float now, const float restartDelay) noexcept {
    if (mapObjective_ != CsMapObjective::Hostage || roundPhase_ != CsRoundPhase::Live ||
        !hostages_.Damage(hostageIndex, damage)) return false;
    (void)CheckHostageWinConditions(now, restartDelay);
    return true;
}

bool CsGameState::CheckHostageWinConditions(const float now,
                                            const float restartDelay) noexcept {
    if (mapObjective_ != CsMapObjective::Hostage || roundPhase_ == CsRoundPhase::Ended ||
        !hostages_.RescueWinReady()) return false;
    return TerminateRound(build4100::CSRoundEndReason::HostagesRescued, now, restartDelay);
}

void CsGameState::ClearBombOwnership() noexcept {
    for (std::size_t index = 1; index <= clients_.MaxClients(); ++index) {
        if (ClientRecord* const player = Player(index)) {
            player->hasC4 = false;
            (void)player->weapons.Remove(build4100::CSWeaponID::C4);
        }
    }
}

void CsGameState::QueueRoundEconomy(const build4100::CSRoundEndReason reason) noexcept {
    const BombObjectiveState bombState = bomb_.State();
    const bool bombWasPlanted = bombState == BombObjectiveState::Planted ||
                                bombState == BombObjectiveState::Defused ||
                                bombState == BombObjectiveState::Exploded;
    const RoundEconomyResult result = ResolveRoundEconomy(
        reason, bombWasPlanted, terroristLossStreak_, counterTerroristLossStreak_);
    pendingTerroristAward_ += result.terroristAward;
    pendingCounterTerroristAward_ += result.counterTerroristAward;
    terroristLossStreak_ = result.terroristLossStreak;
    counterTerroristLossStreak_ = result.counterTerroristLossStreak;
}

void CsGameState::PayPendingRoundAwards() noexcept {
    for (std::size_t index = 1; index <= clients_.MaxClients(); ++index) {
        ClientRecord* const player = Player(index);
        if (!player || !build4100::IsPlayableTeam(player->teamIndex)) continue;
        if (!player->receivesRoundStartMoney) {
            player->receivesRoundStartMoney = true;
            continue;
        }
        if (player->teamIndex == static_cast<int>(build4100::CSTeam::Terrorist)) {
            (void)AddAccount(index, pendingTerroristAward_);
        } else if (player->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
            (void)AddAccount(index, pendingCounterTerroristAward_);
        }
    }
    pendingTerroristAward_ = 0;
    pendingCounterTerroristAward_ = 0;
}

void CsGameState::RestartRound(const float roundStartTime) noexcept {
    PayPendingRoundAwards();
    ++roundNumber_;
    roundStartTime_ = roundStartTime;
    freezePeriod_ = true;
    roundPhase_ = CsRoundPhase::Freeze;
    roundEndReason_ = build4100::CSRoundEndReason::Invalid;
    roundEndTime_ = 0.0f;
    scheduledRestartTime_ = 0.0f;
    freezeEndTime_ = roundStartTime_ + freezeTimeSeconds_;
    roundDeadline_ = freezeEndTime_ + roundTimeSeconds_;
    ClearBombOwnership();
    bomb_.ResetRound();
    hostages_.ResetRound();
    economy_.BeginRound(roundStartTime);

    for (std::size_t index = 1; index <= clients_.MaxClients(); ++index) {
        ClientRecord* const player = Player(index);
        if (!player) continue;
        player->hasDefuser = false;
        player->inBuyZone = false;
        if (build4100::IsPlayableTeam(player->teamIndex)) (void)Respawn(index);
    }
    if (mapObjective_ == CsMapObjective::Bomb) (void)AssignBombCarrier();
}

void CsGameState::EndFreezePeriod() noexcept {
    freezePeriod_ = false;
    if (roundPhase_ == CsRoundPhase::Freeze) roundPhase_ = CsRoundPhase::Live;
}

CsRoundTickResult CsGameState::TickRound(const float now) noexcept {
    if (roundPhase_ == CsRoundPhase::Ended) {
        if (scheduledRestartTime_ > 0.0f && now >= scheduledRestartTime_) {
            RestartRound(now);
            return CsRoundTickResult::Restarted;
        }
        return CsRoundTickResult::None;
    }

    if (roundPhase_ == CsRoundPhase::Freeze) {
        if (now >= freezeEndTime_) {
            EndFreezePeriod();
            return CsRoundTickResult::FreezeEnded;
        }
        return CsRoundTickResult::None;
    }

    if (roundPhase_ != CsRoundPhase::Live) return CsRoundTickResult::None;

    if (mapObjective_ == CsMapObjective::Bomb && bomb_.State() == BombObjectiveState::Planted) {
        const BombObjectiveTransition transition = TickObjective(now, restartDelaySeconds_);
        if (transition == BombObjectiveTransition::Defused ||
            transition == BombObjectiveTransition::Exploded) {
            return CsRoundTickResult::ObjectiveResolved;
        }
    }

    if (roundDeadline_ > 0.0f && now >= roundDeadline_ &&
        ExpireRoundTimer(now, restartDelaySeconds_)) {
        return CsRoundTickResult::RoundTimedOut;
    }
    return CsRoundTickResult::None;
}

bool CsGameState::AssignBombCarrier(const std::size_t preferredClientIndex) noexcept {
    if (mapObjective_ != CsMapObjective::Bomb) return false;
    if (bomb_.State() == BombObjectiveState::Planted ||
        bomb_.State() == BombObjectiveState::Defused ||
        bomb_.State() == BombObjectiveState::Exploded) return false;

    ClearBombOwnership();
    auto give = [this](const std::size_t index) noexcept -> bool {
        ClientRecord* const player = Player(index);
        if (!player || player->teamIndex != static_cast<int>(build4100::CSTeam::Terrorist) ||
            player->isDead) return false;
        if (!bomb_.GiveTo(index)) return false;
        if (!player->weapons.Give(build4100::CSWeaponID::C4, false)) return false;
        player->hasC4 = true;
        return true;
    };
    if (preferredClientIndex > 0 && give(preferredClientIndex)) return true;
    for (std::size_t index = 1; index <= clients_.MaxClients(); ++index) {
        if (index != preferredClientIndex && give(index)) return true;
    }
    return false;
}

bool CsGameState::DropBomb(const std::size_t clientIndex) noexcept {
    if (mapObjective_ != CsMapObjective::Bomb) return false;
    ClientRecord* const player = Player(clientIndex);
    if (!player || !player->hasC4 || !bomb_.DropFrom(clientIndex)) return false;
    player->hasC4 = false;
    (void)player->weapons.Remove(build4100::CSWeaponID::C4);
    return true;
}

bool CsGameState::PickupBomb(const std::size_t clientIndex) noexcept {
    if (mapObjective_ != CsMapObjective::Bomb) return false;
    ClientRecord* const player = Player(clientIndex);
    if (!player || player->isDead ||
        player->teamIndex != static_cast<int>(build4100::CSTeam::Terrorist) ||
        player->hasC4 || !bomb_.PickUp(clientIndex)) return false;
    ClearBombOwnership();
    if (!player->weapons.Give(build4100::CSWeaponID::C4, false)) return false;
    player->hasC4 = true;
    return true;
}

bool CsGameState::PlantBomb(const std::size_t clientIndex, const float now, const int siteIndex,
                            const build4100::Vector& origin) noexcept {
    if (mapObjective_ != CsMapObjective::Bomb) return false;
    ClientRecord* const player = Player(clientIndex);
    if (!player || player->isDead || !player->hasC4 ||
        player->teamIndex != static_cast<int>(build4100::CSTeam::Terrorist) ||
        roundPhase_ != CsRoundPhase::Live || !bomb_.Plant(clientIndex, now, siteIndex, origin)) return false;
    player->hasC4 = false;
    (void)player->weapons.Remove(build4100::CSWeaponID::C4);
    return true;
}

bool CsGameState::BeginDefuse(const std::size_t clientIndex, const float now) noexcept {
    if (mapObjective_ != CsMapObjective::Bomb) return false;
    ClientRecord* const player = Player(clientIndex);
    if (!player || player->isDead ||
        player->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist) ||
        roundPhase_ != CsRoundPhase::Live) return false;
    return bomb_.BeginDefuse(clientIndex, player->hasDefuser, now);
}

bool CsGameState::CancelDefuse(const std::size_t clientIndex) noexcept {
    return mapObjective_ == CsMapObjective::Bomb && bomb_.CancelDefuse(clientIndex);
}

BombObjectiveTransition CsGameState::TickObjective(const float now,
                                                   const float restartDelay) noexcept {
    if (mapObjective_ != CsMapObjective::Bomb) return BombObjectiveTransition::None;

    // ClientDisconnect routes through JoinTeam(Unassigned), but keep this guard
    // here as the authoritative objective invariant as well: a dead, missing or
    // non-CT client can never finish a stale defuse countdown.
    if (bomb_.DefuserIndex() > 0) {
        const std::size_t defuserIndex = static_cast<std::size_t>(bomb_.DefuserIndex());
        const ClientRecord* const defuser = Player(defuserIndex);
        if (!defuser || defuser->isDead ||
            defuser->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
            (void)bomb_.CancelDefuse(defuserIndex);
        }
    }

    const BombObjectiveTransition transition = bomb_.Tick(now);
    if (transition == BombObjectiveTransition::Defused) {
        (void)TerminateRound(build4100::CSRoundEndReason::BombDefused, now, restartDelay);
    } else if (transition == BombObjectiveTransition::Exploded) {
        (void)TerminateRound(build4100::CSRoundEndReason::TargetBombed, now, restartDelay);
    }
    return transition;
}

int CsGameState::WinningTeam(const build4100::CSRoundEndReason reason) noexcept {
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
}

bool CsGameState::TerminateRound(const build4100::CSRoundEndReason reason, const float now,
                                 const float restartDelay) noexcept {
    if (roundPhase_ == CsRoundPhase::Ended || reason == build4100::CSRoundEndReason::Invalid) {
        return false;
    }
    roundPhase_ = CsRoundPhase::Ended;
    freezePeriod_ = false;
    roundEndReason_ = reason;
    roundEndTime_ = now;
    scheduledRestartTime_ = now + std::max(0.0f, restartDelay);
    const int winner = WinningTeam(reason);
    if (build4100::IsPlayableTeam(winner)) {
        AwardRound(winner);
        QueueRoundEconomy(reason);
    }
    return true;
}

bool CsGameState::CheckWinConditions(const float now, const float restartDelay) noexcept {
    if (roundPhase_ == CsRoundPhase::Ended) return false;
    const CsTeamCounts counts = CountTeams();
    if (counts.terrorists == 0 || counts.counterTerrorists == 0) return false;

    if (counts.aliveCounterTerrorists == 0) {
        return TerminateRound(build4100::CSRoundEndReason::TerroristWin, now, restartDelay);
    }
    if (counts.aliveTerrorists == 0 &&
        (mapObjective_ != CsMapObjective::Bomb || bomb_.State() != BombObjectiveState::Planted)) {
        return TerminateRound(build4100::CSRoundEndReason::CTWin, now, restartDelay);
    }
    return CheckHostageWinConditions(now, restartDelay);
}

bool CsGameState::ExpireRoundTimer(const float now, const float restartDelay) noexcept {
    if (roundPhase_ == CsRoundPhase::Ended ||
        (mapObjective_ == CsMapObjective::Bomb && bomb_.State() == BombObjectiveState::Planted)) {
        return false;
    }

    build4100::CSRoundEndReason reason = build4100::CSRoundEndReason::Draw;
    if (mapObjective_ == CsMapObjective::Bomb) {
        reason = build4100::CSRoundEndReason::TargetSaved;
    } else if (mapObjective_ == CsMapObjective::Hostage) {
        reason = build4100::CSRoundEndReason::HostagesNotRescued;
    }
    return TerminateRound(reason, now, restartDelay);
}

void CsGameState::AwardRound(const int winningTeam) noexcept {
    if (winningTeam == static_cast<int>(build4100::CSTeam::Terrorist)) ++terroristScore_;
    else if (winningTeam == static_cast<int>(build4100::CSTeam::CounterTerrorist)) ++counterTerroristScore_;
}

CsTeamCounts CsGameState::CountTeams() const noexcept {
    CsTeamCounts counts;
    for (std::size_t index = 1; index <= clients_.MaxClients(); ++index) {
        const ClientRecord* const player = Player(index);
        if (!player) continue;
        if (player->teamIndex == static_cast<int>(build4100::CSTeam::Terrorist)) {
            ++counts.terrorists;
            if (!player->isDead) ++counts.aliveTerrorists;
        } else if (player->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
            ++counts.counterTerrorists;
            if (!player->isDead) ++counts.aliveCounterTerrorists;
        }
    }
    return counts;
}

}  // namespace cssv34::reconstructed
