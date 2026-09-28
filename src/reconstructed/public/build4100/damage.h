#pragma once

#include <cstdint>

namespace cssv34::build4100 {

// Source hitgroup numbering used by the build4100 branch.
enum class HitGroup : std::int32_t {
    Invalid = -1,
    Generic = 0,
    Head = 1,
    Chest = 2,
    Stomach = 3,
    LeftArm = 4,
    RightArm = 5,
    LeftLeg = 6,
    RightLeg = 7,
    Gear = 10,
};

constexpr float HitGroupDamageMultiplier(const HitGroup hitGroup) noexcept {
    switch (hitGroup) {
        case HitGroup::Head: return 4.0f;
        case HitGroup::Stomach: return 1.25f;
        case HitGroup::LeftLeg:
        case HitGroup::RightLeg: return 0.75f;
        case HitGroup::Gear: return 0.01f;
        default: return 1.0f;
    }
}

constexpr bool HitGroupCanUseBodyArmor(const HitGroup hitGroup) noexcept {
    switch (hitGroup) {
        case HitGroup::Generic:
        case HitGroup::Chest:
        case HitGroup::Stomach:
        case HitGroup::LeftArm:
        case HitGroup::RightArm:
            return true;
        default:
            return false;
    }
}

constexpr bool HitGroupCanUseHelmet(const HitGroup hitGroup) noexcept {
    return hitGroup == HitGroup::Head;
}

static_assert(static_cast<int>(HitGroup::Head) == 1);
static_assert(static_cast<int>(HitGroup::Gear) == 10);
static_assert(HitGroupDamageMultiplier(HitGroup::Head) == 4.0f);
static_assert(HitGroupDamageMultiplier(HitGroup::Stomach) == 1.25f);
static_assert(HitGroupDamageMultiplier(HitGroup::LeftLeg) == 0.75f);

}  // namespace cssv34::build4100
