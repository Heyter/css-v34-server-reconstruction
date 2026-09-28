#include "server/hostage_objective.h"

#include <algorithm>

namespace cssv34::reconstructed {

void HostageObjective::Configure(const std::size_t hostageCount, const int hostageHealth) {
    hostageHealth_ = std::max(1, hostageHealth);
    hostages_.assign(hostageCount, HostageRecord{});
    ResetRound();
}

void HostageObjective::ResetRound() noexcept {
    for (HostageRecord& hostage : hostages_) {
        hostage.health = hostageHealth_;
        hostage.state = HostageState::Alive;
        hostage.leaderClientIndex = -1;
        hostage.hasBeenUsed = false;
    }
}

HostageRecord* HostageObjective::Get(const std::size_t hostageIndex) noexcept {
    return hostageIndex < hostages_.size() ? &hostages_[hostageIndex] : nullptr;
}

const HostageRecord* HostageObjective::Get(const std::size_t hostageIndex) const noexcept {
    return hostageIndex < hostages_.size() ? &hostages_[hostageIndex] : nullptr;
}

bool HostageObjective::BeginFollow(const std::size_t hostageIndex,
                                   const std::size_t clientIndex) noexcept {
    HostageRecord* const hostage = Get(hostageIndex);
    if (!hostage || clientIndex == 0 || hostage->state != HostageState::Alive) return false;
    hostage->leaderClientIndex = static_cast<int>(clientIndex);
    hostage->hasBeenUsed = true;
    return true;
}

bool HostageObjective::DropFollow(const std::size_t hostageIndex,
                                  const std::size_t clientIndex) noexcept {
    HostageRecord* const hostage = Get(hostageIndex);
    if (!hostage || hostage->state != HostageState::Alive ||
        hostage->leaderClientIndex != static_cast<int>(clientIndex)) return false;
    hostage->leaderClientIndex = -1;
    return true;
}

std::size_t HostageObjective::DropLeader(const std::size_t clientIndex) noexcept {
    if (clientIndex == 0) return 0;
    std::size_t dropped = 0;
    for (HostageRecord& hostage : hostages_) {
        if (hostage.state == HostageState::Alive &&
            hostage.leaderClientIndex == static_cast<int>(clientIndex)) {
            hostage.leaderClientIndex = -1;
            ++dropped;
        }
    }
    return dropped;
}

bool HostageObjective::Rescue(const std::size_t hostageIndex,
                              const std::size_t clientIndex) noexcept {
    HostageRecord* const hostage = Get(hostageIndex);
    if (!hostage || clientIndex == 0 || hostage->state != HostageState::Alive ||
        hostage->leaderClientIndex != static_cast<int>(clientIndex)) return false;
    hostage->state = HostageState::Rescued;
    hostage->leaderClientIndex = -1;
    return true;
}

bool HostageObjective::Damage(const std::size_t hostageIndex, const int damage) noexcept {
    HostageRecord* const hostage = Get(hostageIndex);
    if (!hostage || damage <= 0 || hostage->state != HostageState::Alive) return false;
    hostage->health = std::max(0, hostage->health - damage);
    if (hostage->health == 0) {
        hostage->state = HostageState::Dead;
        hostage->leaderClientIndex = -1;
    }
    return true;
}

std::size_t HostageObjective::RescuedCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        hostages_.begin(), hostages_.end(),
        [](const HostageRecord& hostage) { return hostage.state == HostageState::Rescued; }));
}

std::size_t HostageObjective::DeadCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        hostages_.begin(), hostages_.end(),
        [](const HostageRecord& hostage) { return hostage.state == HostageState::Dead; }));
}

std::size_t HostageObjective::AliveUnrescuedCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        hostages_.begin(), hostages_.end(),
        [](const HostageRecord& hostage) { return hostage.state == HostageState::Alive; }));
}

std::size_t HostageObjective::RequiredRescues() const noexcept {
    return (hostages_.size() + 1u) / 2u;
}

bool HostageObjective::RescueWinReady() const noexcept {
    return !hostages_.empty() && AliveUnrescuedCount() == 0 &&
           RescuedCount() >= RequiredRescues();
}

}  // namespace cssv34::reconstructed
