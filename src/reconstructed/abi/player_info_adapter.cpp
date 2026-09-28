#include "abi/player_info_adapter.h"

#include <algorithm>
#include <cstring>

namespace cssv34::reconstructed::abi {

void PlayerInfoAdapter::Bind(const EngineServer021Client* const engine,
                             ClientRegistry* const clients,
                             const std::size_t clientIndex) noexcept {
    engine_ = engine;
    clients_ = clients;
    clientIndex_ = clientIndex;
    networkIdCache_[0] = '\0';
}

void PlayerInfoAdapter::Unbind() noexcept {
    engine_ = nullptr;
    clients_ = nullptr;
    clientIndex_ = 0;
    networkIdCache_[0] = '\0';
}

ClientRecord* PlayerInfoAdapter::Record() noexcept {
    return clients_ ? clients_->Get(clientIndex_) : nullptr;
}

const ClientRecord* PlayerInfoAdapter::Record() const noexcept {
    return clients_ ? clients_->Get(clientIndex_) : nullptr;
}

const char* PlayerInfoAdapter::GetName() {
    const ClientRecord* const record = Record();
    return record && record->state != ClientLifecycleState::Empty ? record->name.c_str() : "";
}

int PlayerInfoAdapter::GetUserID() {
    const ClientRecord* const record = Record();
    if (!record || record->state == ClientLifecycleState::Empty || !record->edict) return -1;
    if (engine_ && engine_->IsValid()) return engine_->GetPlayerUserId(record->edict);
    return record->userId;
}

const char* PlayerInfoAdapter::GetNetworkIDString() {
    const ClientRecord* const record = Record();
    if (!record || record->state == ClientLifecycleState::Empty) return "";

    const char* source = nullptr;
    if (engine_ && engine_->IsValid() && record->edict) {
        source = engine_->GetPlayerNetworkIdString(record->edict);
    }
    if (!source || !*source) source = record->networkId.c_str();

    const std::size_t count = std::min(sizeof(networkIdCache_) - 1, std::strlen(source));
    std::memcpy(networkIdCache_, source, count);
    networkIdCache_[count] = '\0';
    return networkIdCache_;
}

int PlayerInfoAdapter::GetTeamIndex() {
    const ClientRecord* const record = Record();
    return record ? record->teamIndex : 0;
}

void PlayerInfoAdapter::ChangeTeam(const int team) {
    ClientRecord* const record = Record();
    if (!record || record->state == ClientLifecycleState::Empty) return;
    if (team < static_cast<int>(build4100::CSTeam::Unassigned) ||
        team > static_cast<int>(build4100::CSTeam::CounterTerrorist)) return;
    record->teamIndex = team;
    record->isObserver = team == static_cast<int>(build4100::CSTeam::Spectator);
    if (record->isObserver) {
        record->csState = build4100::CSPlayerState::ObserverMode;
        record->isDead = true;
        record->health = 0;
        record->hasC4 = false;
        record->weapons.Clear();
    } else if (build4100::IsPlayableTeam(team)) {
        record->csState = build4100::CSPlayerState::PickingClass;
    } else {
        record->csState = build4100::CSPlayerState::PickingTeam;
    }
    (void)clients_->SyncPlayerState(clientIndex_);
}

int PlayerInfoAdapter::GetFragCount() {
    const ClientRecord* const record = Record();
    return record ? record->fragCount : 0;
}

int PlayerInfoAdapter::GetDeathCount() {
    const ClientRecord* const record = Record();
    return record ? record->deathCount : 0;
}

bool PlayerInfoAdapter::IsConnected() {
    const ClientRecord* const record = Record();
    return record && record->state != ClientLifecycleState::Empty;
}

int PlayerInfoAdapter::GetArmorValue() {
    const ClientRecord* const record = Record();
    return record ? record->armorValue : 0;
}

bool PlayerInfoAdapter::IsHLTV() {
    const ClientRecord* const record = Record();
    return record && record->isHLTV;
}

bool PlayerInfoAdapter::IsPlayer() {
    return IsConnected();
}

bool PlayerInfoAdapter::IsFakeClient() {
    const ClientRecord* const record = Record();
    return record && record->isFakeClient;
}

bool PlayerInfoAdapter::IsDead() {
    const ClientRecord* const record = Record();
    return record && record->isDead;
}

bool PlayerInfoAdapter::IsInAVehicle() {
    const ClientRecord* const record = Record();
    return record && record->isInVehicle;
}

bool PlayerInfoAdapter::IsObserver() {
    const ClientRecord* const record = Record();
    return record && record->isObserver;
}

build4100::Vector PlayerInfoAdapter::GetAbsOrigin() {
    const ClientRecord* const record = Record();
    return record ? record->absOrigin : build4100::Vector{};
}

build4100::QAngle PlayerInfoAdapter::GetAbsAngles() {
    const ClientRecord* const record = Record();
    return record ? record->absAngles : build4100::QAngle{};
}

build4100::Vector PlayerInfoAdapter::GetPlayerMins() {
    const ClientRecord* const record = Record();
    return record ? record->playerMins : build4100::Vector{};
}

build4100::Vector PlayerInfoAdapter::GetPlayerMaxs() {
    const ClientRecord* const record = Record();
    return record ? record->playerMaxs : build4100::Vector{};
}

const char* PlayerInfoAdapter::GetWeaponName() {
    const ClientRecord* const record = Record();
    return record ? record->weapons.ActiveWeaponClassname() : nullptr;
}

const char* PlayerInfoAdapter::GetModelName() {
    const ClientRecord* const record = Record();
    return record ? record->modelName.c_str() : "";
}

int PlayerInfoAdapter::GetHealth() {
    const ClientRecord* const record = Record();
    return record ? record->health : 0;
}

int PlayerInfoAdapter::GetMaxHealth() {
    const ClientRecord* const record = Record();
    return record ? record->maxHealth : 0;
}

build4100::BotCmd PlayerInfoAdapter::GetLastUserCommand() {
    const ClientRecord* const record = Record();
    return record ? record->lastUserCommand : build4100::BotCmd{};
}

}  // namespace cssv34::reconstructed::abi
