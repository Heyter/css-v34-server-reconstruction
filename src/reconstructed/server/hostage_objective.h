#pragma once

#include <cstddef>
#include <vector>

namespace cssv34::reconstructed {

enum class HostageState {
    Alive,
    Dead,
    Rescued,
};

struct HostageRecord {
    int health = 100;
    HostageState state = HostageState::Alive;
    int leaderClientIndex = -1;
    bool hasBeenUsed = false;
};

// Deterministic CS:S hostage objective state. The win rule mirrors the
// build4100-era CCSGameRules behavior: a rescue win is possible only when no
// living unrescued hostages remain and at least half of all hostages were
// rescued. Entity/NPC navigation is intentionally left to the later entity
// reconstruction layer.
class HostageObjective final {
public:
    void Configure(std::size_t hostageCount, int hostageHealth = 100);
    void ResetRound() noexcept;

    bool BeginFollow(std::size_t hostageIndex, std::size_t clientIndex) noexcept;
    bool DropFollow(std::size_t hostageIndex, std::size_t clientIndex) noexcept;
    std::size_t DropLeader(std::size_t clientIndex) noexcept;
    bool Rescue(std::size_t hostageIndex, std::size_t clientIndex) noexcept;
    bool Damage(std::size_t hostageIndex, int damage) noexcept;

    std::size_t Total() const noexcept { return hostages_.size(); }
    std::size_t RescuedCount() const noexcept;
    std::size_t DeadCount() const noexcept;
    std::size_t AliveUnrescuedCount() const noexcept;
    std::size_t RequiredRescues() const noexcept;
    bool RescueWinReady() const noexcept;

    const HostageRecord* Get(std::size_t hostageIndex) const noexcept;
    HostageRecord* Get(std::size_t hostageIndex) noexcept;

private:
    int hostageHealth_ = 100;
    std::vector<HostageRecord> hostages_;
};

}  // namespace cssv34::reconstructed
