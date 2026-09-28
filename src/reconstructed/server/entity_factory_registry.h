#pragma once

#include <cstddef>

#include "public/build4100/cs_weapon.h"

namespace cssv34::reconstructed {

enum class EntityFactoryKind {
    Unknown,
    World,
    Player,
    SpawnPoint,
    Hostage,
    Objective,
    Weapon,
    GrenadeProjectile,
    Brush,
    Trigger,
    Logic,
    Environment,
    Prop,
    Physics,
    Point,
    Team,
    Resource,
    Ragdoll,
    Item,
    Npc,
    Filter,
    Generic,
};

struct EntityFactoryDescriptor {
    const char* classname = nullptr;
    EntityFactoryKind kind = EntityFactoryKind::Unknown;
    const char* networkClass = nullptr;
    build4100::CSWeaponID weapon = build4100::CSWeaponID::None;
    const char* originalType = nullptr;
};

class EntityFactoryRegistry final {
public:
    EntityFactoryDescriptor Resolve(const char* classname) const noexcept;
    bool IsKnown(const char* classname) const noexcept;

    // Exact build4100 LINK_ENTITY_TO_CLASS dictionary recovered from the
    // original server ELF.  Several C++ types have multiple map aliases.
    std::size_t ExactClassnameCount() const noexcept;
    std::size_t ExactCppTypeCount() const noexcept;

    // Backward-compatible summary counters used by the earlier source-owned
    // runtime tests.
    std::size_t StructuralClassCount() const noexcept;
    std::size_t WeaponClassCount() const noexcept;
};

const EntityFactoryRegistry& EntityFactories() noexcept;

} // namespace cssv34::reconstructed
