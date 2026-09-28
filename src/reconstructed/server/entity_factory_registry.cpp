#include "server/entity_factory_registry.h"

#include <array>
#include <cstring>

namespace cssv34::reconstructed {
namespace {

struct OriginalFactoryMapping {
    const char* classname;
    const char* cppType;
};

#include "server/entity_factory_build4100.inc"

constexpr std::size_t kOriginalCppTypeCount = 387;

bool StartsWith(const char* value, const char* prefix) noexcept {
    if (!value || !prefix) return false;
    const std::size_t n = std::strlen(prefix);
    return std::strncmp(value, prefix, n) == 0;
}

const char* NetworkClassForCppType(const char* const type) noexcept {
    if (!type) return nullptr;
    struct Pair { const char* network; const char* table; const char* cppType; };
    static constexpr Pair kPairs[] = {
#include "abi/serverclass_build4100.inc"
    };
    for (const auto& entry : kPairs) {
        if (std::strcmp(entry.cppType, type) == 0) return entry.network;
    }
    // The three concrete grenade projectile C++ types inherit the common
    // build4100 network root rather than declaring dedicated ServerClasses.
    if (std::strcmp(type, "CFlashbangProjectile") == 0 ||
        std::strcmp(type, "CHEGrenadeProjectile") == 0 ||
        std::strcmp(type, "CSmokeGrenadeProjectile") == 0) {
        return "CBaseCSGrenadeProjectile";
    }
    return nullptr;
}

EntityFactoryKind Classify(const char* const classname, const char* const type,
                           const build4100::CSWeaponID weapon) noexcept {
    if (!classname) return EntityFactoryKind::Unknown;
    if (std::strcmp(classname, "worldspawn") == 0) return EntityFactoryKind::World;
    if (std::strcmp(classname, "player") == 0) return EntityFactoryKind::Player;
    if (weapon != build4100::CSWeaponID::None || StartsWith(classname, "weapon_")) {
        return EntityFactoryKind::Weapon;
    }
    if (std::strcmp(classname, "hostage_entity") == 0) return EntityFactoryKind::Hostage;
    if (std::strcmp(classname, "planted_c4") == 0 ||
        std::strcmp(classname, "func_bomb_target") == 0 ||
        std::strcmp(classname, "info_bomb_target") == 0 ||
        std::strcmp(classname, "func_hostage_rescue") == 0) {
        return EntityFactoryKind::Objective;
    }
    if (std::strcmp(classname, "cs_team_manager") == 0) return EntityFactoryKind::Team;
    if (std::strcmp(classname, "cs_player_manager") == 0) return EntityFactoryKind::Resource;
    if (std::strcmp(classname, "cs_ragdoll") == 0 || (type && std::strstr(type, "Ragdoll"))) {
        return EntityFactoryKind::Ragdoll;
    }
    if (std::strcmp(classname, "hegrenade_projectile") == 0 ||
        std::strcmp(classname, "flashbang_projectile") == 0 ||
        std::strcmp(classname, "smokegrenade_projectile") == 0 ||
        (type && std::strstr(type, "Projectile"))) {
        return EntityFactoryKind::GrenadeProjectile;
    }
    if (StartsWith(classname, "info_player_")) return EntityFactoryKind::SpawnPoint;
    if (StartsWith(classname, "func_")) return EntityFactoryKind::Brush;
    if (StartsWith(classname, "trigger_")) return EntityFactoryKind::Trigger;
    if (StartsWith(classname, "logic_") || StartsWith(classname, "math_") ||
        StartsWith(classname, "game_")) return EntityFactoryKind::Logic;
    if (StartsWith(classname, "env_") || StartsWith(classname, "light_")) {
        return EntityFactoryKind::Environment;
    }
    if (StartsWith(classname, "prop_")) return EntityFactoryKind::Prop;
    if (StartsWith(classname, "phys_")) return EntityFactoryKind::Physics;
    if (StartsWith(classname, "item_") || StartsWith(classname, "ammo_")) {
        return EntityFactoryKind::Item;
    }
    if (StartsWith(classname, "npc_") || StartsWith(classname, "ai_") ||
        (type && StartsWith(type, "CAI_"))) return EntityFactoryKind::Npc;
    if (StartsWith(classname, "filter_")) return EntityFactoryKind::Filter;
    if (StartsWith(classname, "point_") || StartsWith(classname, "path_") ||
        StartsWith(classname, "info_")) return EntityFactoryKind::Point;
    return EntityFactoryKind::Generic;
}

const OriginalFactoryMapping* FindOriginal(const char* const classname) noexcept {
    if (!classname || !*classname) return nullptr;
    // The generated list is sorted by classname, but linear lookup is adequate
    // for map entity parsing and avoids introducing STL allocation/locale state
    // into the game DLL ABI boundary.
    for (const auto& entry : kOriginalFactories) {
        if (std::strcmp(entry.classname, classname) == 0) return &entry;
    }
    return nullptr;
}


} // namespace

EntityFactoryDescriptor EntityFactoryRegistry::Resolve(const char* const classname) const noexcept {
    if (!classname || !*classname) return {};
    const auto* const original = FindOriginal(classname);
    if (!original) return {};

    const auto weapon = build4100::ClassnameToWeaponID(classname);
    const char* network = NetworkClassForCppType(original->cppType);
    return EntityFactoryDescriptor{
        original->classname,
        Classify(original->classname, original->cppType, weapon),
        network,
        weapon,
        original->cppType,
    };
}

bool EntityFactoryRegistry::IsKnown(const char* const classname) const noexcept {
    return FindOriginal(classname) != nullptr;
}

std::size_t EntityFactoryRegistry::ExactClassnameCount() const noexcept {
    return kOriginalFactories.size();
}

std::size_t EntityFactoryRegistry::ExactCppTypeCount() const noexcept {
    return kOriginalCppTypeCount;
}

std::size_t EntityFactoryRegistry::StructuralClassCount() const noexcept {
    std::size_t count = 0;
    for (const auto& entry : kOriginalFactories) {
        if (build4100::ClassnameToWeaponID(entry.classname) == build4100::CSWeaponID::None) ++count;
    }
    return count;
}

std::size_t EntityFactoryRegistry::WeaponClassCount() const noexcept {
    std::size_t count = 0;
    for (const auto& entry : kOriginalFactories) {
        if (build4100::ClassnameToWeaponID(entry.classname) != build4100::CSWeaponID::None) ++count;
    }
    return count;
}

const EntityFactoryRegistry& EntityFactories() noexcept {
    static const EntityFactoryRegistry value;
    return value;
}

} // namespace cssv34::reconstructed
