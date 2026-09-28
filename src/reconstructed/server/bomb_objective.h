#pragma once

#include <cstddef>

#include "public/build4100/vector.h"

namespace cssv34::reconstructed {

enum class BombObjectiveState {
    Unavailable,
    Carried,
    Dropped,
    Planted,
    Defused,
    Exploded,
};

enum class BombObjectiveTransition {
    None,
    Defused,
    Exploded,
};

// Source-owned deterministic state for the CS:S C4 objective. This deliberately
// stops at gameplay semantics; the entity/sendtable ABI for weapon_c4 and
// planted_c4 is a later layer.
class BombObjective final {
public:
    void Configure(float timerSeconds = 45.0f) noexcept;
    void ResetRound() noexcept;

    bool GiveTo(std::size_t clientIndex) noexcept;
    bool DropFrom(std::size_t clientIndex) noexcept;
    bool PickUp(std::size_t clientIndex) noexcept;
    bool Plant(std::size_t clientIndex, float now, int siteIndex = -1,
               const build4100::Vector& origin = {}) noexcept;

    bool BeginDefuse(std::size_t clientIndex, bool hasDefuser, float now) noexcept;
    bool CancelDefuse(std::size_t clientIndex) noexcept;
    BombObjectiveTransition Tick(float now) noexcept;

    BombObjectiveState State() const noexcept { return state_; }
    int CarrierIndex() const noexcept { return carrierIndex_; }
    int PlanterIndex() const noexcept { return planterIndex_; }
    int DefuserIndex() const noexcept { return defuserIndex_; }
    float TimerSeconds() const noexcept { return timerSeconds_; }
    float PlantTime() const noexcept { return plantTime_; }
    float ExplodeTime() const noexcept { return explodeTime_; }
    float DefuseCompleteTime() const noexcept { return defuseCompleteTime_; }
    float DefuseLength() const noexcept { return defuseLength_; }
    int SiteIndex() const noexcept { return siteIndex_; }
    const build4100::Vector& PlantOrigin() const noexcept { return plantOrigin_; }
    bool IsDefusing() const noexcept { return defuserIndex_ > 0; }

private:
    static constexpr float kDefuseSeconds = 10.0f;
    static constexpr float kDefuseWithKitSeconds = 5.0f;

    float timerSeconds_ = 45.0f;
    BombObjectiveState state_ = BombObjectiveState::Unavailable;
    int carrierIndex_ = -1;
    int planterIndex_ = -1;
    int defuserIndex_ = -1;
    float plantTime_ = 0.0f;
    float explodeTime_ = 0.0f;
    float defuseCompleteTime_ = 0.0f;
    float defuseLength_ = 0.0f;
    int siteIndex_ = -1;
    build4100::Vector plantOrigin_{};
};

}  // namespace cssv34::reconstructed
