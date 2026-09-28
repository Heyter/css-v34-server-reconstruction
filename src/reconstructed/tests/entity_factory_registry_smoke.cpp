#include <cassert>
#include <cstring>
#include <iostream>

#include "server/entity_factory_registry.h"

using namespace cssv34;
using namespace cssv34::reconstructed;

static void Expect(const char* classname, const char* originalType,
                   EntityFactoryKind kind, const char* networkClass = nullptr) {
    const auto value = EntityFactories().Resolve(classname);
    assert(value.kind == kind);
    assert(value.classname && std::strcmp(value.classname, classname) == 0);
    assert(value.originalType && std::strcmp(value.originalType, originalType) == 0);
    if (networkClass) {
        assert(value.networkClass && std::strcmp(value.networkClass, networkClass) == 0);
    }
}

int main() {
    const auto& factories = EntityFactories();
    assert(factories.ExactClassnameCount() == 418);
    assert(factories.ExactCppTypeCount() == 387);
    assert(factories.StructuralClassCount() > 350);
    assert(factories.WeaponClassCount() >= 30);

    Expect("worldspawn", "CWorld", EntityFactoryKind::World, "CWorld");
    Expect("player", "CCSPlayer", EntityFactoryKind::Player, "CCSPlayer");
    Expect("hostage_entity", "CHostage", EntityFactoryKind::Hostage, "CHostage");
    Expect("func_bomb_target", "CBombTarget", EntityFactoryKind::Objective);
    Expect("func_hostage_rescue", "CHostageRescueZone", EntityFactoryKind::Objective);
    Expect("planted_c4", "CPlantedC4", EntityFactoryKind::Objective, "CPlantedC4");
    Expect("prop_ragdoll", "CRagdollProp", EntityFactoryKind::Ragdoll, "CRagdollProp");
    Expect("func_water", "CBaseDoor", EntityFactoryKind::Brush, "CBaseDoor");
    Expect("env_particlescript", "CEnvParticleScript", EntityFactoryKind::Environment, "CEnvParticleScript");
    Expect("funCBaseFlex", "CBaseFlex", EntityFactoryKind::Generic, "CBaseFlex");

    const auto ak = factories.Resolve("weapon_ak47");
    assert(ak.kind == EntityFactoryKind::Weapon);
    assert(ak.weapon == build4100::CSWeaponID::Ak47);
    assert(ak.originalType && std::strcmp(ak.originalType, "CAK47") == 0);
    assert(ak.networkClass && std::strcmp(ak.networkClass, "CAK47") == 0);

    const auto deagle = factories.Resolve("weapon_deagle");
    assert(deagle.networkClass && std::strcmp(deagle.networkClass, "CDEagle") == 0);
    const auto m3 = factories.Resolve("weapon_m3");
    assert(m3.networkClass && std::strcmp(m3.networkClass, "CWeaponM3") == 0);

    Expect("flashbang_projectile", "CFlashbangProjectile", EntityFactoryKind::GrenadeProjectile,
           "CBaseCSGrenadeProjectile");
    Expect("hegrenade_projectile", "CHEGrenadeProjectile", EntityFactoryKind::GrenadeProjectile,
           "CBaseCSGrenadeProjectile");
    Expect("smokegrenade_projectile", "CSmokeGrenadeProjectile", EntityFactoryKind::GrenadeProjectile,
           "CBaseCSGrenadeProjectile");

    assert(factories.IsKnown("trigger_multiple"));
    assert(factories.IsKnown("ammo_9mm"));
    assert(factories.IsKnown("point_camera"));
    assert(!factories.IsKnown("__definitely_not_an_entity__"));

    std::cout << "ENTITY_FACTORY_REGISTRY=PASS exact_classnames="
              << factories.ExactClassnameCount() << " cpp_types="
              << factories.ExactCppTypeCount() << " structural="
              << factories.StructuralClassCount() << " weapons="
              << factories.WeaponClassCount() << "\n";
}
