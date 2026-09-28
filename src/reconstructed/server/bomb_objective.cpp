#include "server/bomb_objective.h"

#include <algorithm>

namespace cssv34::reconstructed {

void BombObjective::Configure(const float timerSeconds) noexcept {
    timerSeconds_ = std::max(0.0f, timerSeconds);
    ResetRound();
}

void BombObjective::ResetRound() noexcept {
    state_ = BombObjectiveState::Unavailable;
    carrierIndex_ = -1;
    planterIndex_ = -1;
    defuserIndex_ = -1;
    plantTime_ = 0.0f;
    explodeTime_ = 0.0f;
    defuseCompleteTime_ = 0.0f;
    defuseLength_ = 0.0f;
    siteIndex_ = -1;
    plantOrigin_ = build4100::Vector{};
}

bool BombObjective::GiveTo(const std::size_t clientIndex) noexcept {
    if (clientIndex == 0 || state_ == BombObjectiveState::Planted ||
        state_ == BombObjectiveState::Defused || state_ == BombObjectiveState::Exploded) {
        return false;
    }
    state_ = BombObjectiveState::Carried;
    carrierIndex_ = static_cast<int>(clientIndex);
    return true;
}

bool BombObjective::DropFrom(const std::size_t clientIndex) noexcept {
    if (state_ != BombObjectiveState::Carried ||
        carrierIndex_ != static_cast<int>(clientIndex)) {
        return false;
    }
    state_ = BombObjectiveState::Dropped;
    carrierIndex_ = -1;
    return true;
}

bool BombObjective::PickUp(const std::size_t clientIndex) noexcept {
    if (clientIndex == 0 || state_ != BombObjectiveState::Dropped) return false;
    state_ = BombObjectiveState::Carried;
    carrierIndex_ = static_cast<int>(clientIndex);
    return true;
}

bool BombObjective::Plant(const std::size_t clientIndex, const float now, const int siteIndex,
                          const build4100::Vector& origin) noexcept {
    if (state_ != BombObjectiveState::Carried ||
        carrierIndex_ != static_cast<int>(clientIndex)) {
        return false;
    }
    state_ = BombObjectiveState::Planted;
    planterIndex_ = static_cast<int>(clientIndex);
    carrierIndex_ = -1;
    defuserIndex_ = -1;
    plantTime_ = now;
    explodeTime_ = now + timerSeconds_;
    defuseCompleteTime_ = 0.0f;
    defuseLength_ = 0.0f;
    siteIndex_ = siteIndex;
    plantOrigin_ = origin;
    return true;
}

bool BombObjective::BeginDefuse(const std::size_t clientIndex, const bool hasDefuser,
                                const float now) noexcept {
    if (clientIndex == 0 || state_ != BombObjectiveState::Planted) return false;

    // Only one C4 defuse owner exists at a time. Repeated usercmd packets from
    // the same player must be idempotent: restarting the deadline every packet
    // makes a held +use impossible to complete. A different CT cannot steal an
    // active defuse without the current owner cancelling first.
    if (defuserIndex_ > 0) {
        return defuserIndex_ == static_cast<int>(clientIndex);
    }

    defuserIndex_ = static_cast<int>(clientIndex);
    defuseLength_ = hasDefuser ? kDefuseWithKitSeconds : kDefuseSeconds;
    defuseCompleteTime_ = now + defuseLength_;
    return true;
}

bool BombObjective::CancelDefuse(const std::size_t clientIndex) noexcept {
    if (defuserIndex_ != static_cast<int>(clientIndex)) return false;
    defuserIndex_ = -1;
    defuseCompleteTime_ = 0.0f;
    defuseLength_ = 0.0f;
    return true;
}

BombObjectiveTransition BombObjective::Tick(const float now) noexcept {
    if (state_ != BombObjectiveState::Planted) return BombObjectiveTransition::None;

    // The defuse completes first when both deadlines are reached on the same tick.
    if (defuserIndex_ > 0 && now >= defuseCompleteTime_ &&
        defuseCompleteTime_ <= explodeTime_) {
        state_ = BombObjectiveState::Defused;
        defuserIndex_ = -1;
        return BombObjectiveTransition::Defused;
    }

    if (now >= explodeTime_) {
        state_ = BombObjectiveState::Exploded;
        defuserIndex_ = -1;
        return BombObjectiveTransition::Exploded;
    }

    return BombObjectiveTransition::None;
}

}  // namespace cssv34::reconstructed
