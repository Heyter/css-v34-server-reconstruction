#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "public/build4100/cs_weapon.h"
#include "public/build4100/damage.h"
#include "public/build4100/vector.h"

namespace cssv34::reconstructed {

struct EntityHandle {
    std::uint16_t index = 0;
    std::uint16_t serial = 0;
    constexpr bool IsValid() const noexcept { return index != 0; }
    friend constexpr bool operator==(EntityHandle a, EntityHandle b) noexcept {
        return a.index == b.index && a.serial == b.serial;
    }
};

enum class EntityKind {
    Generic,
    Player,
    Weapon,
    Grenade,
};

class GameEntity {
public:
    GameEntity(EntityKind kind, std::string classname) : kind_(kind), classname_(std::move(classname)) {}
    virtual ~GameEntity() = default;

    virtual void Spawn(float now) noexcept;
    virtual void Think(float now) noexcept;
    virtual void Touch(GameEntity& other, float now) noexcept;
    virtual void Use(GameEntity* activator, float now) noexcept;
    virtual int TakeDamage(int damage, build4100::HitGroup hitGroup, EntityHandle attacker) noexcept;

    EntityHandle Handle() const noexcept { return handle_; }
    EntityKind Kind() const noexcept { return kind_; }
    const std::string& Classname() const noexcept { return classname_; }
    const build4100::Vector& Origin() const noexcept { return origin_; }
    const build4100::Vector& Velocity() const noexcept { return velocity_; }
    void SetOrigin(build4100::Vector value) noexcept { origin_ = value; }
    void SetVelocity(build4100::Vector value) noexcept { velocity_ = value; }
    bool IsAlive() const noexcept { return alive_; }
    int Health() const noexcept { return health_; }
    void SetHealth(int value) noexcept { health_ = value; alive_ = value > 0; }
    bool PendingRemove() const noexcept { return pendingRemove_; }
    void Remove() noexcept { pendingRemove_ = true; }
    std::size_t ThinkCount() const noexcept { return thinkCount_; }
    std::size_t TouchCount() const noexcept { return touchCount_; }
    std::size_t UseCount() const noexcept { return useCount_; }

private:
    friend class EntityWorld;
    EntityHandle handle_{};
    EntityKind kind_ = EntityKind::Generic;
    std::string classname_;
    build4100::Vector origin_{};
    build4100::Vector velocity_{};
    int health_ = 0;
    bool alive_ = false;
    bool pendingRemove_ = false;
    std::size_t thinkCount_ = 0;
    std::size_t touchCount_ = 0;
    std::size_t useCount_ = 0;
};

class PlayerEntity final : public GameEntity {
public:
    explicit PlayerEntity(std::size_t clientIndex);
    std::size_t ClientIndex() const noexcept { return clientIndex_; }
    void Spawn(float now) noexcept override;
private:
    std::size_t clientIndex_ = 0;
};

class WeaponEntity final : public GameEntity {
public:
    explicit WeaponEntity(build4100::CSWeaponID weapon);
    build4100::CSWeaponID Weapon() const noexcept { return weapon_; }
    EntityHandle Owner() const noexcept { return owner_; }
    void SetOwner(EntityHandle owner) noexcept { owner_ = owner; }
private:
    build4100::CSWeaponID weapon_ = build4100::CSWeaponID::None;
    EntityHandle owner_{};
};

enum class GrenadeKind {
    HE,
    Flash,
    Smoke,
};

class GrenadeEntity final : public GameEntity {
public:
    GrenadeEntity(GrenadeKind kind, EntityHandle owner, float detonateTime);
    void Think(float now) noexcept override;
    GrenadeKind GrenadeType() const noexcept { return kind_; }
    EntityHandle Owner() const noexcept { return owner_; }
    float DetonateTime() const noexcept { return detonateTime_; }
    bool Detonated() const noexcept { return detonated_; }
private:
    GrenadeKind kind_ = GrenadeKind::HE;
    EntityHandle owner_{};
    float detonateTime_ = 0.0f;
    bool detonated_ = false;
};

class EntityWorld final {
public:
    explicit EntityWorld(std::size_t maxEntities = 2048);
    void Reset() noexcept;

    template <typename T, typename... Args>
    EntityHandle Spawn(Args&&... args) {
        const std::size_t slot = FindFreeSlot();
        if (slot == 0) return {};
        Slot& target = slots_[slot];
        target.serial = static_cast<std::uint16_t>(target.serial + 1u);
        if (target.serial == 0) target.serial = 1;
        auto entity = std::make_unique<T>(std::forward<Args>(args)...);
        entity->handle_ = EntityHandle{static_cast<std::uint16_t>(slot), target.serial};
        entity->Spawn(now_);
        target.entity = std::move(entity);
        ++activeCount_;
        return EntityHandle{static_cast<std::uint16_t>(slot), target.serial};
    }

    GameEntity* Get(EntityHandle handle) noexcept;
    const GameEntity* Get(EntityHandle handle) const noexcept;
    bool Remove(EntityHandle handle) noexcept;
    void Tick(float now) noexcept;
    bool Touch(EntityHandle left, EntityHandle right) noexcept;
    bool Use(EntityHandle target, EntityHandle activator) noexcept;
    std::size_t ActiveCount() const noexcept { return activeCount_; }
    float Now() const noexcept { return now_; }

private:
    struct Slot {
        std::uint16_t serial = 0;
        std::unique_ptr<GameEntity> entity;
    };
    std::size_t FindFreeSlot() const noexcept;
    std::vector<Slot> slots_;
    std::size_t activeCount_ = 0;
    float now_ = 0.0f;
};

} // namespace cssv34::reconstructed
