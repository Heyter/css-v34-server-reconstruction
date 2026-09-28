#include "server/entity_world.h"

#include <algorithm>

namespace cssv34::reconstructed {

void GameEntity::Spawn(const float) noexcept {}
void GameEntity::Think(const float) noexcept { ++thinkCount_; }
void GameEntity::Touch(GameEntity&, const float) noexcept { ++touchCount_; }
void GameEntity::Use(GameEntity*, const float) noexcept { ++useCount_; }
int GameEntity::TakeDamage(const int damage, const build4100::HitGroup, const EntityHandle) noexcept {
    if (!alive_ || damage <= 0) return 0;
    const int applied = std::min(health_, damage);
    health_ -= applied;
    alive_ = health_ > 0;
    return applied;
}

PlayerEntity::PlayerEntity(const std::size_t clientIndex)
    : GameEntity(EntityKind::Player, "player"), clientIndex_(clientIndex) {}
void PlayerEntity::Spawn(const float now) noexcept {
    (void)now;
    SetHealth(100);
}

WeaponEntity::WeaponEntity(const build4100::CSWeaponID weapon)
    : GameEntity(EntityKind::Weapon,
                 build4100::WeaponIDToClassname(weapon) ? build4100::WeaponIDToClassname(weapon) : "weapon_unknown"),
      weapon_(weapon) {}

GrenadeEntity::GrenadeEntity(const GrenadeKind kind, const EntityHandle owner, const float detonateTime)
    : GameEntity(EntityKind::Grenade,
                 kind == GrenadeKind::HE ? "hegrenade_projectile" :
                 kind == GrenadeKind::Flash ? "flashbang_projectile" : "smokegrenade_projectile"),
      kind_(kind), owner_(owner), detonateTime_(detonateTime) {}
void GrenadeEntity::Think(const float now) noexcept {
    GameEntity::Think(now);
    if (!detonated_ && now >= detonateTime_) {
        detonated_ = true;
        Remove();
    }
}

EntityWorld::EntityWorld(const std::size_t maxEntities) : slots_(maxEntities + 1) {}
void EntityWorld::Reset() noexcept {
    for (auto& slot : slots_) slot.entity.reset();
    activeCount_ = 0;
    now_ = 0.0f;
}
std::size_t EntityWorld::FindFreeSlot() const noexcept {
    for (std::size_t i = 1; i < slots_.size(); ++i) if (!slots_[i].entity) return i;
    return 0;
}
GameEntity* EntityWorld::Get(const EntityHandle handle) noexcept {
    if (!handle.IsValid() || handle.index >= slots_.size()) return nullptr;
    Slot& slot = slots_[handle.index];
    return slot.serial == handle.serial ? slot.entity.get() : nullptr;
}
const GameEntity* EntityWorld::Get(const EntityHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= slots_.size()) return nullptr;
    const Slot& slot = slots_[handle.index];
    return slot.serial == handle.serial ? slot.entity.get() : nullptr;
}
bool EntityWorld::Remove(const EntityHandle handle) noexcept {
    if (!Get(handle)) return false;
    slots_[handle.index].entity.reset();
    --activeCount_;
    return true;
}
void EntityWorld::Tick(const float now) noexcept {
    now_ = now;
    for (std::size_t i = 1; i < slots_.size(); ++i) {
        if (slots_[i].entity) slots_[i].entity->Think(now);
    }
    for (std::size_t i = 1; i < slots_.size(); ++i) {
        if (slots_[i].entity && slots_[i].entity->PendingRemove()) {
            slots_[i].entity.reset();
            --activeCount_;
        }
    }
}
bool EntityWorld::Touch(const EntityHandle left, const EntityHandle right) noexcept {
    GameEntity* a = Get(left); GameEntity* b = Get(right);
    if (!a || !b) return false;
    a->Touch(*b, now_); b->Touch(*a, now_); return true;
}
bool EntityWorld::Use(const EntityHandle target, const EntityHandle activator) noexcept {
    GameEntity* t = Get(target); GameEntity* a = activator.IsValid() ? Get(activator) : nullptr;
    if (!t) return false;
    t->Use(a, now_); return true;
}

} // namespace cssv34::reconstructed
