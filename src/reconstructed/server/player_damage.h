#pragma once

#include "public/build4100/damage.h"

namespace cssv34::reconstructed {

struct DamageModelInput {
    int health = 0;
    int armor = 0;
    bool hasHelmet = false;
    float baseDamage = 0.0f;
    build4100::HitGroup hitGroup = build4100::HitGroup::Generic;

    // Source weapon scripts expose ArmorRatio around 1.x. The player armor
    // path uses ArmorRatio * 0.5 as the amount that reaches health while
    // protected. Passing 2.0 means no reduction from armor.
    float weaponArmorRatio = 1.0f;
    bool bypassArmor = false;
};

struct DamageModelResult {
    float scaledDamage = 0.0f;
    float healthDamageFloat = 0.0f;
    float armorDamageFloat = 0.0f;
    int healthDamage = 0;
    int armorDamage = 0;
    int healthAfter = 0;
    int armorAfter = 0;
    bool armorProtected = false;
    bool headshot = false;
    bool lethal = false;
};

DamageModelResult ResolvePlayerDamage(const DamageModelInput& input) noexcept;

}  // namespace cssv34::reconstructed
