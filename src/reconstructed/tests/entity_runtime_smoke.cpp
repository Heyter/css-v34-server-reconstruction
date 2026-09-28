#include <cassert>
#include <iostream>
#include "server/entity_world.h"
using namespace cssv34::reconstructed;
int main() {
    EntityWorld world(32);
    const auto player = world.Spawn<PlayerEntity>(1);
    const auto weapon = world.Spawn<WeaponEntity>(cssv34::build4100::CSWeaponID::Ak47);
    const auto grenade = world.Spawn<GrenadeEntity>(GrenadeKind::HE, player, 1.0f);
    assert(player.IsValid() && weapon.IsValid() && grenade.IsValid());
    assert(world.ActiveCount() == 3);
    assert(world.Touch(player, weapon));
    assert(world.Use(weapon, player));
    auto* p = world.Get(player); assert(p && p->Health() == 100);
    assert(p->TakeDamage(25, cssv34::build4100::HitGroup::Chest, {}) == 25);
    assert(p->Health() == 75);
    world.Tick(0.5f); assert(world.Get(grenade));
    world.Tick(1.0f); assert(!world.Get(grenade));
    const auto old = weapon; assert(world.Remove(weapon));
    const auto replacement = world.Spawn<WeaponEntity>(cssv34::build4100::CSWeaponID::M4A1);
    assert(replacement.index == old.index && replacement.serial != old.serial);
    assert(!world.Get(old) && world.Get(replacement));
    std::cout << "ENTITY_RUNTIME=PASS\n";
}
