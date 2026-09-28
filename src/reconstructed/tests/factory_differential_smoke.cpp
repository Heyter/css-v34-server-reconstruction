#include <dlfcn.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "public/build4100/cs_weapon.h"
#include "public/build4100/edict.h"
#include "public/build4100/sendtable.h"
#include "public/build4100/vector.h"

using Factory = void* (*)(const char*, int*);

static void* Load(const std::string& path, const int flags) {
    void* h = dlopen(path.c_str(), flags);
    if (!h) std::cerr << dlerror() << '\n';
    return h;
}

template <typename Fn>
static Fn Slot(void* object, const std::size_t index) {
    auto** vt = *reinterpret_cast<void***>(object);
    Fn fn = nullptr;
    static_assert(sizeof(fn) == sizeof(vt[index]));
    std::memcpy(&fn, &vt[index], sizeof(fn));
    return fn;
}

static Factory GetFactory(void* h) {
    void* raw = dlsym(h, "CreateInterface");
    Factory fn = nullptr;
    static_assert(sizeof(fn) == sizeof(raw));
    std::memcpy(&fn, &raw, sizeof(fn));
    return fn;
}

static void* Get(Factory f, const char* name) {
    int rc = -1;
    void* p = f(name, &rc);
    return (p && rc == 0) ? p : nullptr;
}

static bool VtableNonNull(void* object, const std::size_t count) {
    if (!object) return false;
    auto** vt = *reinterpret_cast<void***>(object);
    if (!vt) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!vt[i]) return false;
    }
    return true;
}

static bool SameVector(const cssv34::build4100::Vector& a,
                       const cssv34::build4100::Vector& b) {
    return std::fabs(a.x - b.x) < 0.001f &&
           std::fabs(a.y - b.y) < 0.001f &&
           std::fabs(a.z - b.z) < 0.001f;
}

#if UINTPTR_MAX == 0xffffffffu
static void* gSyntheticBaseEntity = nullptr;
static void* SyntheticGetBaseEntity(void*) { return gSyntheticBaseEntity; }
#endif

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: rebuilt-server\n";
        return 2;
    }

    // IMPORTANT: do not dlopen() the original build4100 server here.
    // Its global/static initialization expects the real Source engine process.
    // Original-vs-rebuilt ELF/ABI comparison is performed by abi_inventory.py;
    // this executable validates only the rebuilt runtime contract.
    void* rebuilt = Load(argv[1], RTLD_NOW | RTLD_LOCAL);
    assert(rebuilt);

    Factory factory = GetFactory(rebuilt);
    assert(factory);

    struct InterfaceSpec { const char* name; std::size_t slots; };
    const std::vector<InterfaceSpec> specs = {
        {"ServerGameDLL006", 33},
        {"ServerGameClients003", 16},
        {"ServerGameClients004", 16},
        {"ServerGameEnts001", 8},
        {"PlayerInfoManager001", 2},
        {"PlayerInfoManager002", 2},
        {"GameMovement001", 6},
        {"HLTVDirector001", 10},
        {"VServerDllSharedAppSystems001", 3},
        {"PluginHelpersCheck001", 1},
    };
    for (const auto& spec : specs) {
        void* object = Get(factory, spec.name);
        if (!VtableNonNull(object, spec.slots)) {
            std::cerr << "factory/vtable mismatch " << spec.name << '\n';
            return 1;
        }
    }

    // Version aliases in build4100 resolve to the same singleton object.
    assert(Get(factory, "ServerGameClients003") == Get(factory, "ServerGameClients004"));
    assert(Get(factory, "PlayerInfoManager001") == Get(factory, "PlayerInfoManager002"));

    // Exact build4100 shared-app-systems contract recovered from the original
    // server_i486.so static initializer and vtable: two entries, in this order.
    using SharedCountFn = int (*)(void*);
    using SharedNameFn = const char* (*)(void*, int);
    void* shared = Get(factory, "VServerDllSharedAppSystems001");
    assert(Slot<SharedCountFn>(shared, 0)(shared) == 2);
    const std::array<const char*, 2> sharedDlls = {"soundemittersystem", "scenefilecache"};
    const std::array<const char*, 2> sharedInterfaces = {"VSoundEmitter002", "SceneFileCache001"};
    for (int i = 0; i < 2; ++i) {
        const char* dllName = Slot<SharedNameFn>(shared, 1)(shared, i);
        const char* interfaceName = Slot<SharedNameFn>(shared, 2)(shared, i);
        if (!dllName || !interfaceName ||
            std::strcmp(dllName, sharedDlls[static_cast<std::size_t>(i)]) != 0 ||
            std::strcmp(interfaceName, sharedInterfaces[static_cast<std::size_t>(i)]) != 0) {
            std::cerr << "shared app-system contract mismatch index=" << i << '\n';
            return 1;
        }
    }
    assert(Slot<SharedNameFn>(shared, 1)(shared, -1) == nullptr);
    assert(Slot<SharedNameFn>(shared, 2)(shared, 2) == nullptr);

    // build4100 IHLTVDirector slots 2..4: IsActive/SetHLTVServer/GetHLTVServer.
    using HltvActiveFn = bool (*)(void*);
    using HltvSetFn = void (*)(void*, void*);
    using HltvGetFn = void* (*)(void*);
    void* hltv = Get(factory, "HLTVDirector001");
    assert(!Slot<HltvActiveFn>(hltv, 2)(hltv));
    int hltvSentinel = 4100;
    Slot<HltvSetFn>(hltv, 3)(hltv, &hltvSentinel);
    assert(Slot<HltvGetFn>(hltv, 4)(hltv) == &hltvSentinel);
    assert(Slot<HltvActiveFn>(hltv, 2)(hltv));
    Slot<HltvSetFn>(hltv, 3)(hltv, nullptr);

    int unknownRc = -1;
    assert(!factory("NoSuchInterface999", &unknownRc));
    assert(unknownRc != 0);

    // v1.8.34zg live ServerClass/SendTable contract: retain the proven 15-root
    // prefix and expose the complete 177-class build4100 network root set.
    using GetAllServerClassesFn = cssv34::build4100::ServerClass* (*)(void*);
    void* serverDll = Get(factory, "ServerGameDLL006");
    auto* serverClass = Slot<GetAllServerClassesFn>(serverDll, 10)(serverDll);
    constexpr std::array<int, 15> kExpectedRootPropCounts = {
        11, 38, 1, 17, 11, 14, 6, 1, 3, 1, 4, 1, 1, 1, 2
    };
    std::size_t serverClassCount = 0;
    for (; serverClass; serverClass = serverClass->next) {
        if (!serverClass->networkName || !serverClass->table ||
            !serverClass->table->netTableName ||
            (serverClass->table->propCount > 0 && !serverClass->table->props)) {
            std::cerr << "ServerClass live SendTable contract mismatch index="
                      << serverClassCount << '\n';
            return 1;
        }
        if (serverClassCount < kExpectedRootPropCounts.size() &&
            serverClass->table->propCount != kExpectedRootPropCounts[serverClassCount]) {
            std::cerr << "ServerClass stable-prefix prop mismatch index="
                      << serverClassCount << '\n';
            return 1;
        }
        ++serverClassCount;
        if (serverClassCount > 177) {
            std::cerr << "ServerClass linked-list cycle\n";
            return 1;
        }
    }
    if (serverClassCount != 177) {
        std::cerr << "ServerClass count mismatch " << serverClassCount << '\n';
        return 1;
    }

    // Exact original build4100 startup contract: slot 28 returns null so the
    // engine uses its default send proxies; slot 29 PostInit is a no-op.
    using GetPointerFn = void* (*)(void*);
    using VoidFn = void (*)(void*);
    if (Slot<GetPointerFn>(serverDll, 28)(serverDll) != nullptr) {
        std::cerr << "GetStandardSendProxies must return null for build4100\n";
        return 1;
    }
    Slot<VoidFn>(serverDll, 29)(serverDll);

    // Runtime contract for IServerGameClients::GetPlayerLimits (slot 0).
    using GetPlayerLimitsFn = void (*)(void*, int&, int&, int&);
    void* clients = Get(factory, "ServerGameClients004");
    int minPlayers = -1, maxPlayers = -1, defaultPlayers = -1;
    Slot<GetPlayerLimitsFn>(clients, 0)(clients, minPlayers, maxPlayers, defaultPlayers);
    if (minPlayers < 1 || maxPlayers < minPlayers ||
        defaultPlayers < minPlayers || defaultPlayers > maxPlayers) {
        std::cerr << "GetPlayerLimits invalid contract\n";
        return 1;
    }

    // build4100 IGameMovement slots 3..5 are hull/view accessors.
    using HullFn = const cssv34::build4100::Vector& (*)(void*, bool);
    void* move = Get(factory, "GameMovement001");
    const cssv34::build4100::Vector standMins{-16.0f, -16.0f, 0.0f};
    const cssv34::build4100::Vector duckMins{-16.0f, -16.0f, 0.0f};
    const cssv34::build4100::Vector standMaxs{16.0f, 16.0f, 72.0f};
    const cssv34::build4100::Vector duckMaxs{16.0f, 16.0f, 36.0f};
    const cssv34::build4100::Vector standView{0.0f, 0.0f, 64.0f};
    const cssv34::build4100::Vector duckView{0.0f, 0.0f, 28.0f};
    const std::array<std::array<cssv34::build4100::Vector, 2>, 3> expected = {{
        {standMins, duckMins},
        {standMaxs, duckMaxs},
        {standView, duckView},
    }};
    for (std::size_t slot = 3; slot <= 5; ++slot) {
        for (int ducked = 0; ducked <= 1; ++ducked) {
            const auto& actual = Slot<HullFn>(move, slot)(move, ducked != 0);
            if (!SameVector(actual, expected[slot - 3][ducked])) {
                std::cerr << "GameMovement001 contract mismatch slot=" << slot
                          << " ducked=" << ducked << '\n';
                return 1;
            }
        }
    }

    // Exact entity bridge behavior recovered from original build4100 disassembly.
    using BaseEntityToEdictFn = cssv34::build4100::Edict* (*)(void*, void*);
#if UINTPTR_MAX == 0xffffffffu
    using EdictToBaseEntityFn = void* (*)(void*, cssv34::build4100::Edict*);
#endif
    void* ents = Get(factory, "ServerGameEnts001");
    cssv34::build4100::Edict syntheticEdict{};
    std::array<unsigned char, 0x170> syntheticEntity{};
    auto* syntheticEdictPtr = &syntheticEdict;
    std::memcpy(syntheticEntity.data() + 0x168, &syntheticEdictPtr, sizeof(syntheticEdictPtr));
    const auto fromEntity = Slot<BaseEntityToEdictFn>(ents, 5)(ents, syntheticEntity.data());
    if (fromEntity != syntheticEdictPtr) {
        std::cerr << "ServerGameEnts001 BaseEntityToEdict contract mismatch\n";
        return 1;
    }

#if UINTPTR_MAX == 0xffffffffu
    void* networkableVtable[7]{};
    networkableVtable[6] = reinterpret_cast<void*>(&SyntheticGetBaseEntity);
    struct SyntheticNetworkable { void** vtable; } networkable{networkableVtable};
    int baseEntitySentinel = 0x4100;
    gSyntheticBaseEntity = &baseEntitySentinel;
    syntheticEdict.networkable0c = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&networkable));
    const auto fromEdict = Slot<EdictToBaseEntityFn>(ents, 6)(ents, &syntheticEdict);
    if (fromEdict != gSyntheticBaseEntity) {
        std::cerr << "ServerGameEnts001 EdictToBaseEntity contract mismatch\n";
        return 1;
    }
#endif

    // Alias table is a direct source-owned transcription of original build4100.
    constexpr std::array<const char*, cssv34::build4100::kBuild4100WeaponIdMax + 1> aliases = {
        "none", "p228", "glock", "scout", "hegren", "xm1014", "c4", "mac10", "aug",
        "smokegrenade", "elite", "fiveseven", "ump45", "sg550", "galil", "famas", "usp",
        "awp", "mp5navy", "m249", "m3", "m4a1", "tmp", "g3sg1", "flash", "deagle",
        "sg552", "ak47", "knife", "p90", "shield", "kevlar", "assaultsuit", "nightvision",
    };
    for (int id = 0; id <= cssv34::build4100::kBuild4100WeaponIdMax; ++id) {
        const auto weapon = static_cast<cssv34::build4100::CSWeaponID>(id);
        const char* actual = cssv34::build4100::WeaponIDToAlias(weapon);
        if (!actual || std::strcmp(actual, aliases[static_cast<std::size_t>(id)]) != 0 ||
            cssv34::build4100::AliasToWeaponID(actual) != weapon) {
            std::cerr << "weapon alias contract mismatch id=" << id << '\n';
            return 1;
        }
    }

    std::cout << "REBUILT_FACTORY_CONTRACT=PASS interfaces=" << specs.size()
              << " weapon_aliases=34 player_limits=PASS gamemovement_slots_3_5=PASS"
              << " servergameents_bridge=PASS sharedapps2=PASS hltv_pluginhelpers=PASS"
              << " serverclasses=177_exact_roots stdproxies_null=PASS postinit_noop=PASS\n";
    return 0;
}
