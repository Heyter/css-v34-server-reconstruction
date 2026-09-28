#include "server/player_damage.h"

#include <algorithm>

namespace cssv34::reconstructed {

DamageModelResult ResolvePlayerDamage(const DamageModelInput& input) noexcept {
    DamageModelResult out;
    out.healthAfter = std::max(0, input.health);
    out.armorAfter = std::max(0, input.armor);
    out.headshot = input.hitGroup == build4100::HitGroup::Head;

    if (input.health <= 0 || input.baseDamage <= 0.0f) return out;

    out.scaledDamage = input.baseDamage * build4100::HitGroupDamageMultiplier(input.hitGroup);
    if (out.scaledDamage <= 0.0f) return out;

    const bool bodyArmor = build4100::HitGroupCanUseBodyArmor(input.hitGroup);
    const bool helmet = build4100::HitGroupCanUseHelmet(input.hitGroup) && input.hasHelmet;
    out.armorProtected = !input.bypassArmor && input.armor > 0 && (bodyArmor || helmet);

    if (out.armorProtected) {
        // Classic Source/Counter-Strike armor shape: weapon ArmorRatio is
        // halved for the health path and the prevented portion consumes armor
        // at a 0.5 armor-bonus rate. If the vest runs out, the remainder goes
        // directly to health.
        constexpr float kArmorBonus = 0.5f;
        const float healthRatio = std::clamp(input.weaponArmorRatio * 0.5f, 0.0f, 1.0f);
        float healthDamage = out.scaledDamage * healthRatio;
        float armorCost = (out.scaledDamage - healthDamage) * kArmorBonus;

        if (armorCost > static_cast<float>(input.armor)) {
            armorCost = static_cast<float>(input.armor);
            const float absorbedDamage = armorCost / kArmorBonus;
            healthDamage = std::max(0.0f, out.scaledDamage - absorbedDamage);
        }

        out.healthDamageFloat = healthDamage;
        out.armorDamageFloat = armorCost;
        out.armorAfter = std::max(0, static_cast<int>(static_cast<float>(input.armor) - armorCost));
    } else {
        out.healthDamageFloat = out.scaledDamage;
    }

    // Source ultimately stores player health/armor in integer network vars;
    // preserve truncation at the state boundary instead of rounding.
    out.healthDamage = std::max(0, static_cast<int>(out.healthDamageFloat));
    out.armorDamage = std::max(0, input.armor - out.armorAfter);
    out.healthAfter = std::max(0, input.health - out.healthDamage);
    out.lethal = out.healthDamage > 0 && out.healthAfter <= 0;
    return out;
}

}  // namespace cssv34::reconstructed
