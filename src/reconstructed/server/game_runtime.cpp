#include "server/game_runtime.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace cssv34::reconstructed {

bool GameRuntime::Initialize(const std::size_t maxClients) {
    if (maxClients == 0 || maxClients > 64) return false;
    if (!clients_.Configure(maxClients) || !weapons_.Configure(maxClients)) return false;
    players_.assign(maxClients + 1, RuntimePlayer{});
    game_.Configure();
    game_.ConfigureRoundTiming(6.0f, 180.0f, 5.0f);
    entities_.Reset(); events_.Clear(); activeGrenades_.clear(); smokeClouds_.clear(); now_ = 0.0f;
    PublishNetwork();
    return true;
}

bool GameRuntime::ConnectPlayer(const std::size_t index, const std::string& name, const std::string& address) {
    if (index == 0 || index >= players_.size()) return false;
    (void)weapons_.ResetClient(index);
    RuntimePlayer& runtime = players_[index];
    runtime.edict = build4100::Edict{};
    runtime.edict.stateFlags = 0;
    runtime.edict.networkSerial = static_cast<int>(index);
    char reject[128]{};
    if (!clients_.Connect(index, &runtime.edict, name.c_str(), address.c_str(), reject, sizeof(reject))) return false;
    if (!clients_.PutInServer(index, &runtime.edict, name.c_str())) return false;
    if (!clients_.Activate(index, &runtime.edict, false)) return false;
    ClientRecord* record = clients_.Get(index);
    if (!record) return false;
    record->userId = 1000 + static_cast<int>(index);
    if (!game_.OnClientPutInServer(index)) return false;
    runtime.entity = entities_.Spawn<PlayerEntity>(index);
    auto& event = events_.Emit("player_connect", now_);
    event.ints["userid"] = record->userId; event.strings["name"] = name;
    PublishNetwork();
    return runtime.entity.IsValid();
}

bool GameRuntime::DisconnectPlayer(const std::size_t index) {
    if (index == 0 || index >= players_.size()) return false;
    RuntimePlayer& runtime = players_[index];
    ClientRecord* record = clients_.Get(index);
    if (!record || record->state == ClientLifecycleState::Empty) return false;
    const int userId = record->userId;
    if (!clients_.Disconnect(index, &runtime.edict)) return false;
    (void)weapons_.ResetClient(index);
    if (runtime.entity.IsValid()) (void)entities_.Remove(runtime.entity);
    runtime = RuntimePlayer{};
    auto& event = events_.Emit("player_disconnect", now_); event.ints["userid"] = userId;
    PublishNetwork(); return true;
}

bool GameRuntime::JoinTeam(const std::size_t index, const int team) {
    if (!game_.JoinTeam(index, team)) return false;
    auto& event = events_.Emit("player_team", now_);
    event.ints["index"] = static_cast<int>(index); event.ints["team"] = team;
    PublishNetwork(); return true;
}

bool GameRuntime::EnterGame(const std::size_t index) {
    if (!game_.EnterGame(index)) return false;
    if (auto* entity = entities_.Get(PlayerEntityHandle(index))) entity->SetHealth(100);
    auto& event = events_.Emit("player_spawn", now_); event.ints["index"] = static_cast<int>(index);
    PublishNetwork(); return true;
}

void GameRuntime::ConfigureObjective(const CsMapObjective objective, const std::size_t hostageCount) {
    game_.ConfigureMapObjective(objective, hostageCount);
}

void GameRuntime::StartRound(const float now) {
    now_ = now; game_.RestartRound(now);
    auto& event = events_.Emit("round_start", now);
    event.ints["round"] = game_.RoundNumber();
    event.bools["freeze"] = game_.IsFreezePeriod();
    PublishNetwork();
}

CsRoundTickResult GameRuntime::Tick(const float now, const float frameTime) {
    now_ = now; ProcessGrenades(now); entities_.Tick(now); weapons_.Tick(frameTime);
    smokeClouds_.erase(std::remove_if(smokeClouds_.begin(), smokeClouds_.end(),
                                     [now](const SmokeCloud& smoke) { return now >= smoke.expireTime; }),
                       smokeClouds_.end());
    const CsRoundPhase before = game_.RoundPhase();
    const CsRoundTickResult result = game_.TickRound(now);
    if (result == CsRoundTickResult::FreezeEnded) events_.Emit("round_freeze_end", now);
    if (before != CsRoundPhase::Ended && game_.RoundPhase() == CsRoundPhase::Ended) {
        auto& event = events_.Emit("round_end", now);
        event.ints["reason"] = static_cast<int>(game_.RoundEndReason());
    }
    if (result == CsRoundTickResult::Restarted) {
        auto& event = events_.Emit("round_start", now); event.ints["round"] = game_.RoundNumber();
    }
    PublishNetwork(); return result;
}

DamageApplyResult GameRuntime::FireAt(const std::size_t attacker, const std::size_t victim,
                                      const build4100::HitGroup hitGroup, const float now) {
    ClientRecord* shooter = clients_.Get(attacker);
    if (!shooter) return DamageApplyResult::InvalidPlayer;
    const build4100::CSWeaponID weapon = shooter->weapons.ActiveWeapon();
    float damage = 0.0f;
    float armorRatio = 1.0f;
    if (weapon == build4100::CSWeaponID::Knife) {
        if (weapons_.Melee(attacker, now, weapon) != TimedWeaponResult::Fired) {
            return DamageApplyResult::NoDamage;
        }
        damage = 55.0f;
    } else {
        if (weapons_.Fire(attacker, now, game_, weapon) != TimedWeaponResult::Fired) {
            return DamageApplyResult::NoDamage;
        }
        const auto& profile = WeaponProfile(weapon);
        damage = profile.baseDamage;
        armorRatio = profile.armorRatio;
    }
    auto& fire = events_.Emit(weapon == build4100::CSWeaponID::Knife ? "knife_attack" : "weapon_fire", now);
    fire.ints["userid"] = shooter->userId; fire.strings["weapon"] = build4100::WeaponIDToAlias(weapon);
    const DamageApplyResult result = game_.DamagePlayer(victim, attacker, damage,
                                                        hitGroup, armorRatio, false);
    if (result == DamageApplyResult::Killed) {
        ClientRecord* killed = clients_.Get(victim);
        auto& death = events_.Emit("player_death", now);
        death.ints["userid"] = killed ? killed->userId : 0;
        death.ints["attacker"] = shooter->userId;
        death.bools["headshot"] = killed && killed->lastDeathWasHeadshot;
        death.strings["weapon"] = build4100::WeaponIDToAlias(weapon);
        (void)game_.CheckWinConditions(now);
    }
    PublishNetwork(); return result;
}

WeaponActionResult GameRuntime::Reload(const std::size_t clientIndex, const float now) {
    ClientRecord* player = clients_.Get(clientIndex);
    if (!player) return WeaponActionResult::InvalidPlayer;
    const auto weapon = player->weapons.ActiveWeapon();
    const auto result = weapons_.Reload(clientIndex, now, game_, weapon);
    if (result == WeaponActionResult::Reloaded) {
        auto& event = events_.Emit("weapon_reload", now);
        event.ints["userid"] = player->userId;
        event.strings["weapon"] = build4100::WeaponIDToAlias(weapon);
    }
    return result;
}

bool GameRuntime::SelectWeapon(const std::size_t clientIndex, const build4100::CSWeaponID weapon, const float now) {
    ClientRecord* player = clients_.Get(clientIndex);
    if (!player || !weapons_.Switch(clientIndex, now, game_, weapon)) return false;
    auto& event = events_.Emit("weapon_switch", now);
    event.ints["userid"] = player->userId;
    event.strings["weapon"] = build4100::WeaponIDToAlias(weapon);
    return true;
}

EntityHandle GameRuntime::DropWeapon(const std::size_t clientIndex, const build4100::CSWeaponID weapon, const float now) {
    ClientRecord* player = clients_.Get(clientIndex);
    if (!player || player->isDead || !player->weapons.Has(weapon) || weapon == build4100::CSWeaponID::Knife ||
        weapon == build4100::CSWeaponID::C4 || build4100::IsGrenadeWeapon(weapon)) return {};
    if (!player->weapons.Remove(weapon)) return {};
    const EntityHandle handle = entities_.Spawn<WeaponEntity>(weapon);
    if (!handle.IsValid()) { (void)player->weapons.Give(weapon, false); return {}; }
    if (auto* entity = entities_.Get(handle)) entity->SetOrigin(player->absOrigin);
    auto& event = events_.Emit("item_drop", now);
    event.ints["userid"] = player->userId; event.ints["entity"] = handle.index;
    event.strings["weapon"] = build4100::WeaponIDToAlias(weapon);
    return handle;
}

bool GameRuntime::PickupWeapon(const std::size_t clientIndex, const EntityHandle weaponEntity, const float now) {
    ClientRecord* player = clients_.Get(clientIndex);
    auto* entity = dynamic_cast<WeaponEntity*>(entities_.Get(weaponEntity));
    if (!player || player->isDead || !entity) return false;
    const auto weapon = entity->Weapon();
    if (!game_.GiveWeapon(clientIndex, weapon, false)) return false;
    if (!entities_.Remove(weaponEntity)) return false;
    auto& event = events_.Emit("item_pickup", now);
    event.ints["userid"] = player->userId;
    event.strings["weapon"] = build4100::WeaponIDToAlias(weapon);
    return true;
}

bool GameRuntime::ThrowGrenade(const std::size_t clientIndex, const build4100::CSWeaponID grenade, const float now) {
    ClientRecord* player = clients_.Get(clientIndex);
    if (!player || player->isDead || !build4100::IsGrenadeWeapon(grenade) || !player->weapons.Has(grenade)) return false;
    GrenadeKind kind = GrenadeKind::HE; float fuse = 1.5f;
    if (grenade == build4100::CSWeaponID::Flashbang) kind = GrenadeKind::Flash;
    else if (grenade == build4100::CSWeaponID::SmokeGrenade) { kind = GrenadeKind::Smoke; fuse = 3.0f; }
    const EntityHandle handle = entities_.Spawn<GrenadeEntity>(kind, PlayerEntityHandle(clientIndex), now + fuse);
    if (!handle.IsValid() || !player->weapons.Remove(grenade)) { if (handle.IsValid()) entities_.Remove(handle); return false; }
    if (auto* entity = entities_.Get(handle)) entity->SetOrigin(player->absOrigin);
    activeGrenades_.push_back(ActiveGrenade{handle, kind, clientIndex, player->absOrigin, now + fuse});
    auto& event = events_.Emit("grenade_thrown", now);
    event.ints["userid"] = player->userId; event.ints["entity"] = handle.index;
    event.strings["weapon"] = build4100::WeaponIDToAlias(grenade);
    return true;
}

bool GameRuntime::PlantBomb(const std::size_t clientIndex, const float now) {
    if (!game_.PlantBomb(clientIndex, now)) return false;
    auto& e = events_.Emit("bomb_planted", now); e.ints["index"] = static_cast<int>(clientIndex);
    PublishNetwork(); return true;
}
bool GameRuntime::BeginDefuse(const std::size_t clientIndex, const float now) {
    if (!game_.BeginDefuse(clientIndex, now)) return false;
    auto& e = events_.Emit("bomb_begindefuse", now); e.ints["index"] = static_cast<int>(clientIndex);
    return true;
}

CommandResult GameRuntime::ClientCommand(const std::size_t index, const std::string& command, const float now) {
    now_ = now; const auto result = commands_.ExecuteClient(index, command, now); PublishNetwork(); return result;
}
CommandResult GameRuntime::ServerCommand(const std::string& command, const float now) {
    now_ = now;
    const auto result = commands_.ExecuteServer(command, now);
    if (result == CommandResult::Executed && command.rfind("mp_restartgame", 0) == 0) StartRound(now);
    return result;
}

void GameRuntime::ProcessGrenades(const float now) {
    for (const ActiveGrenade& grenade : activeGrenades_) {
        if (now < grenade.detonateTime) continue;
        const char* eventName = grenade.kind == GrenadeKind::HE ? "hegrenade_detonate" :
                                grenade.kind == GrenadeKind::Flash ? "flashbang_detonate" : "smokegrenade_detonate";
        auto& event = events_.Emit(eventName, now);
        event.ints["entity"] = grenade.handle.index;
        event.ints["owner"] = static_cast<int>(grenade.ownerIndex);
        int affected = 0;
        for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
            ClientRecord* player = clients_.Get(i);
            if (!player || player->state == ClientLifecycleState::Empty || player->isDead) continue;
            const float dx = player->absOrigin.x - grenade.origin.x;
            const float dy = player->absOrigin.y - grenade.origin.y;
            const float dz = player->absOrigin.z - grenade.origin.z;
            const float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (grenade.kind == GrenadeKind::HE && distance <= 350.0f) {
                const float damage = std::max(1.0f, 100.0f * (1.0f - distance / 350.0f));
                const auto result = game_.DamagePlayer(i, grenade.ownerIndex, damage,
                                                       build4100::HitGroup::Generic, 1.0f, false);
                if (result == DamageApplyResult::Applied || result == DamageApplyResult::Killed) ++affected;
                if (result == DamageApplyResult::Killed) {
                    auto& death = events_.Emit("player_death", now);
                    death.ints["userid"] = player->userId;
                    death.ints["attacker"] = clients_.Get(grenade.ownerIndex) ? clients_.Get(grenade.ownerIndex)->userId : 0;
                    death.strings["weapon"] = "hegrenade";
                }
            } else if (grenade.kind == GrenadeKind::Flash && distance <= 1500.0f) {
                player->flashBlindUntil = std::max(player->flashBlindUntil, now + 3.0f);
                ++affected;
            }
        }
        if (grenade.kind == GrenadeKind::Smoke) {
            smokeClouds_.push_back(SmokeCloud{grenade.origin, now + 18.0f});
        }
        event.ints["affected"] = affected;
    }
    activeGrenades_.erase(std::remove_if(activeGrenades_.begin(), activeGrenades_.end(),
                                         [now](const ActiveGrenade& grenade) { return now >= grenade.detonateTime; }),
                           activeGrenades_.end());
}

void GameRuntime::PublishNetwork() { (void)network_.Capture(clients_, game_); }
EntityHandle GameRuntime::PlayerEntityHandle(const std::size_t index) const noexcept {
    return index < players_.size() ? players_[index].entity : EntityHandle{};
}
const RuntimePlayer* GameRuntime::Player(const std::size_t index) const noexcept {
    return index < players_.size() ? &players_[index] : nullptr;
}

} // namespace cssv34::reconstructed
