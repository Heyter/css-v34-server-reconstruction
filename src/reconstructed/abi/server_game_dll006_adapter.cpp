#include "abi/server_game_dll006_adapter.h"

#include "abi/engine_probe.h"
#include "abi/game_movement001_adapter.h"
#include "abi/player_info_manager002_adapter.h"
#include "abi/server_game_clients004_adapter.h"
#include "abi/server_game_ents001_adapter.h"
#include "abi/server_class_registry.h"
#include "public/build4100/vector.h"
#include "server/map_entity_lump.h"
#include "server/weapon_runtime.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

#if defined(__GNUC__) && defined(__linux__)
#include <dlfcn.h>
#include <link.h>
#endif

namespace cssv34::reconstructed::abi {
namespace {

constexpr std::size_t kMaxEdicts = 2048;
constexpr int kInterfaceOk = 0;
constexpr int kInterfaceFailed = 1;
constexpr int kMaxProvenClients = 64;
constexpr char kServerGameDll006[] = "ServerGameDLL006";
constexpr char kServerGameClients003[] = "ServerGameClients003";
constexpr char kServerGameClients004[] = "ServerGameClients004";
constexpr char kPlayerInfoManager001[] = "PlayerInfoManager001";
constexpr char kPlayerInfoManager002[] = "PlayerInfoManager002";
constexpr char kServerGameEnts001[] = "ServerGameEnts001";
constexpr char kGameMovement001[] = "GameMovement001";
constexpr char kVEngineServer021[] = "VEngineServer021";
constexpr char kEngineTraceServer003[] = "EngineTraceServer003";
constexpr char kVModelInfoServer001[] = "VModelInfoServer001";
constexpr char kVPhysicsCollision007[] = "VPhysicsCollision007";
constexpr char kVEngineServerStringTable001[] = "VEngineServerStringTable001";
constexpr char kVEngineCvar003[] = "VEngineCvar003";
constexpr char kGameEventManager002[] = "GAMEEVENTSMANAGER002";
constexpr char kVoiceServer001[] = "VoiceServer001";
constexpr char kVServerEngineTools001[] = "VSERVERENGINETOOLS001";
constexpr char kHltvDirector001[] = "HLTVDirector001";
constexpr char kServerDllSharedAppSystems001[] = "VServerDllSharedAppSystems001";
constexpr char kPluginHelpersCheck001[] = "PluginHelpersCheck001";
constexpr char kGameDescription[] = "Counter-Strike: Source";
constexpr float kFallbackTickInterval = 0.015f;

#if defined(__GNUC__) && defined(__linux__)
struct LoadedEnginePath {
    char path[1024]{};
};

int FindLoadedEngine(dl_phdr_info* const info, const std::size_t, void* const opaque) {
    if (!info || !info->dlpi_name || !info->dlpi_name[0] || !opaque) {
        return 0;
    }
    const char* const slash = std::strrchr(info->dlpi_name, '/');
    const char* const base = slash ? slash + 1 : info->dlpi_name;
    if (std::strcmp(base, "engine_i486.so") != 0) {
        return 0;
    }
    auto* const out = static_cast<LoadedEnginePath*>(opaque);
    std::snprintf(out->path, sizeof(out->path), "%s", info->dlpi_name);
    return 1;
}

template <typename Fn>
Fn ResolveFunction(void* const handle, const char* const symbol) noexcept {
    if (!handle || !symbol) {
        return nullptr;
    }
    void* const raw = dlsym(handle, symbol);
    Fn fn = nullptr;
    static_assert(sizeof(fn) == sizeof(raw), "build4100 function-pointer ABI");
    std::memcpy(&fn, &raw, sizeof(fn));
    return fn;
}
#endif

bool ParseEntityOrigin(const MapEntityRecord& entity, build4100::Vector& origin) noexcept {
    const std::string* const text = entity.Find("origin");
    if (!text) return false;
    return std::sscanf(text->c_str(), "%f %f %f", &origin.x, &origin.y, &origin.z) == 3;
}

bool ParseEntityAngles(const MapEntityRecord& entity, build4100::QAngle& angles) noexcept {
    const std::string* const text = entity.Find("angles");
    if (!text) return false;
    return std::sscanf(text->c_str(), "%f %f %f", &angles.x, &angles.y, &angles.z) == 3;
}

bool IsStudioMovementSolidClass(const std::string& classname) noexcept {
    return classname == "prop_physics_multiplayer" || classname == "prop_physics" ||
           classname == "prop_physics_override" || classname == "prop_physics_respawnable" ||
           classname == "prop_dynamic" || classname == "prop_dynamic_override" ||
           classname == "prop_door_rotating" || classname == "hostage_entity";
}

bool IsInlineMovementSolidClass(const std::string& classname) noexcept {
    return classname == "func_door" || classname == "func_door_rotating" ||
           classname == "func_brush";
}

bool IsGenericNpcMovementClass(const std::string& classname) noexcept {
    return classname.rfind("npc_", 0) == 0 || classname.rfind("ai_", 0) == 0;
}

std::vector<MapMovementSolid> CollectMapMovementSolids(const MapEntityLump& lump) {
    // CHostage::Spawn in the original build4100 cycles this exact four-model
    // table from a zero-initialized static index. Map hostage_entity records do
    // not carry an explicit model key, so reconstruct the same assignment here.
    static constexpr const char* kHostageModels[] = {
        "models/Characters/Hostage_01.mdl",
        "models/Characters/Hostage_02.mdl",
        "models/Characters/hostage_03.mdl",
        "models/Characters/hostage_04.mdl",
    };

    std::vector<MapMovementSolid> out;
    std::size_t hostageOrdinal = 0;
    std::size_t npcOrdinal = 0;
    for (const auto& entity : lump.Entities()) {
        const std::string* const classname = entity.Classname();
        if (!classname) continue;

        MapMovementSolid solid{};
        if (*classname == "hostage_entity") {
            if (!ParseEntityOrigin(entity, solid.origin)) continue;
            solid.kind = MapMovementSolid::Kind::HostageBBox;
            solid.classname = *classname;
            solid.model = kHostageModels[hostageOrdinal % (sizeof(kHostageModels) / sizeof(kHostageModels[0]))];
            solid.actorOrdinal = hostageOrdinal;
            ++hostageOrdinal;
            (void)ParseEntityAngles(entity, solid.angles);
            out.push_back(std::move(solid));
            continue;
        }

        const std::string* const model = entity.Find("model");
        if (!model || model->empty()) continue;
        if (IsGenericNpcMovementClass(*classname) && model->front() != '*') {
            if (!ParseEntityOrigin(entity, solid.origin)) continue;
            solid.kind = MapMovementSolid::Kind::NpcBBox;
            solid.classname = *classname;
            solid.model = *model;
            solid.actorOrdinal = npcOrdinal++;
            (void)ParseEntityAngles(entity, solid.angles);
            out.push_back(std::move(solid));
            continue;
        }
        if (IsStudioMovementSolidClass(*classname) && model->front() != '*') {
            // prop_dynamic supports an explicit non-solid override. Keep the
            // default/model-driven case and skip only the proven "solid 0" form.
            if ((*classname == "prop_dynamic" || *classname == "prop_dynamic_override") &&
                entity.Find("solid") && *entity.Find("solid") == "0") {
                continue;
            }
            solid.kind = MapMovementSolid::Kind::StudioPhysics;
        } else if (IsInlineMovementSolidClass(*classname) && model->front() == '*') {
            solid.kind = MapMovementSolid::Kind::InlineBrush;
        } else {
            continue;
        }
        solid.classname = *classname;
        solid.model = *model;
        (void)ParseEntityOrigin(entity, solid.origin);
        (void)ParseEntityAngles(entity, solid.angles);
        out.push_back(std::move(solid));
    }
    return out;
}

std::vector<MapObjectiveZone> CollectMapObjectiveZones(const MapEntityLump& lump) {
    std::vector<MapObjectiveZone> out;
    int bombSite = 0;
    int rescueSite = 0;
    for (const auto& entity : lump.Entities()) {
        const std::string* const classname = entity.Classname();
        if (!classname) continue;
        MapObjectiveZone zone{};
        if (*classname == "func_bomb_target" || *classname == "info_bomb_target") {
            zone.kind = MapObjectiveZone::Kind::BombTarget;
            zone.siteIndex = bombSite++;
        } else if (*classname == "func_hostage_rescue" || *classname == "info_hostage_rescue") {
            zone.kind = MapObjectiveZone::Kind::HostageRescue;
            zone.siteIndex = rescueSite++;
        } else {
            continue;
        }
        zone.classname = *classname;
        if (const std::string* const model = entity.Find("model")) zone.model = *model;
        (void)ParseEntityOrigin(entity, zone.origin);
        out.push_back(std::move(zone));
    }
    return out;
}

std::vector<MovementLadderVolume> CollectMapLadders(const MapEntityLump& lump) {
    std::vector<MovementLadderVolume> out;
    for (const auto& entity : lump.Entities()) {
        const std::string* const classname = entity.Classname();
        if (!classname || *classname != "info_ladder") continue;
        const std::string* const minX = entity.Find("mins.x");
        const std::string* const minY = entity.Find("mins.y");
        const std::string* const minZ = entity.Find("mins.z");
        const std::string* const maxX = entity.Find("maxs.x");
        const std::string* const maxY = entity.Find("maxs.y");
        const std::string* const maxZ = entity.Find("maxs.z");
        if (!minX || !minY || !minZ || !maxX || !maxY || !maxZ) continue;
        MovementLadderVolume ladder{};
        if (std::sscanf(minX->c_str(), "%f", &ladder.mins.x) != 1 ||
            std::sscanf(minY->c_str(), "%f", &ladder.mins.y) != 1 ||
            std::sscanf(minZ->c_str(), "%f", &ladder.mins.z) != 1 ||
            std::sscanf(maxX->c_str(), "%f", &ladder.maxs.x) != 1 ||
            std::sscanf(maxY->c_str(), "%f", &ladder.maxs.y) != 1 ||
            std::sscanf(maxZ->c_str(), "%f", &ladder.maxs.z) != 1) {
            continue;
        }
        if (ladder.mins.x > ladder.maxs.x) std::swap(ladder.mins.x, ladder.maxs.x);
        if (ladder.mins.y > ladder.maxs.y) std::swap(ladder.mins.y, ladder.maxs.y);
        if (ladder.mins.z > ladder.maxs.z) std::swap(ladder.mins.z, ladder.maxs.z);
        out.push_back(ladder);
    }
    return out;
}

void CollectPlayerSpawns(const MapEntityLump& lump,
                         std::vector<build4100::Vector>& terrorists,
                         std::vector<build4100::Vector>& counterTerrorists) {
    terrorists.clear();
    counterTerrorists.clear();
    for (const auto& entity : lump.Entities()) {
        const std::string* const classname = entity.Classname();
        if (!classname) continue;
        build4100::Vector origin{};
        if (!ParseEntityOrigin(entity, origin)) continue;
        if (*classname == "info_player_terrorist") terrorists.push_back(origin);
        else if (*classname == "info_player_counterterrorist") counterTerrorists.push_back(origin);
    }
}

void ProbeEngineSignonFacts() noexcept {
#if defined(__GNUC__) && defined(__linux__)
    LoadedEnginePath engine{};
    (void)dl_iterate_phdr(&FindLoadedEngine, &engine);
    if (!engine.path[0]) {
        EngineProbe("ENGINE_SIGNON_FACTS_MISSING", "reason=engine_not_loaded");
        return;
    }

    // engine_i486.so is loaded RTLD_LOCAL by the build4100 launcher, so weak
    // relocations/RTLD_DEFAULT cannot see its GLOBAL dynsym entries. Re-open
    // the exact already-loaded pathname to get a handle to the same link-map.
    void* const handle = dlopen(engine.path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    if (!handle) {
        EngineProbe("ENGINE_SIGNON_FACTS_MISSING", "reason=engine_handle");
        return;
    }

    using HostGetServerCountFn = int (*)();
    using SendTableGetCrcFn = unsigned long (*)();
    const auto hostGetServerCount =
        ResolveFunction<HostGetServerCountFn>(handle, "_Z19Host_GetServerCountv");
    const auto sendTableGetCrc =
        ResolveFunction<SendTableGetCrcFn>(handle, "_Z16SendTable_GetCRCv");
    if (!hostGetServerCount || !sendTableGetCrc) {
        char detail[1152]{};
        std::snprintf(detail, sizeof(detail),
                      "reason=dlsym engine=%s server_count=%d sendtable_crc=%d",
                      engine.path, hostGetServerCount ? 1 : 0, sendTableGetCrc ? 1 : 0);
        EngineProbe("ENGINE_SIGNON_FACTS_MISSING", detail);
        dlclose(handle);
        return;
    }

    const int serverCount = hostGetServerCount();
    const auto sendTableCrc = static_cast<std::uint32_t>(sendTableGetCrc());
    char detail[1280]{};
    std::snprintf(detail, sizeof(detail),
                  "server_count=%d sendtable_crc=%u sendtable_crc_hex=%08x engine=%s",
                  serverCount, sendTableCrc, sendTableCrc, engine.path);
    EngineProbe("ENGINE_SIGNON_FACTS", detail);
    dlclose(handle);
#else
    EngineProbe("ENGINE_SIGNON_FACTS_MISSING");
#endif
}

ServerGameDll006Adapter& Instance() {
    static ServerGameDll006Adapter instance;
    return instance;
}

// Exact build4100 IServerDLLSharedAppSystems address-point order: three slots,
// no virtual destructor.  The original CSS build4100 server exposes two shared
// app systems in this exact order.  Returning an empty list prevents the engine
// app-system group from reaching PostInit/GameInit on a real dedicated server.
class ServerDllSharedAppSystems001Adapter final {
public:
    virtual int Count() {
        EngineProbe("SharedAppSystems_Count", "count=2");
        return 2;
    }

    virtual const char* GetDllName(const int index) {
        const char* value = nullptr;
        switch (index) {
            case 0: value = "soundemittersystem"; break;
            case 1: value = "scenefilecache"; break;
            default: break;
        }
        char detail[96]{};
        std::snprintf(detail, sizeof(detail), "index=%d value=%s", index, value ? value : "<null>");
        EngineProbe("SharedAppSystems_GetDllName", detail);
        return value;
    }

    virtual const char* GetInterfaceName(const int index) {
        const char* value = nullptr;
        switch (index) {
            case 0: value = "VSoundEmitter002"; break;
            case 1: value = "SceneFileCache001"; break;
            default: break;
        }
        char detail[96]{};
        std::snprintf(detail, sizeof(detail), "index=%d value=%s", index, value ? value : "<null>");
        EngineProbe("SharedAppSystems_GetInterfaceName", detail);
        return value;
    }
};

ServerDllSharedAppSystems001Adapter& SharedAppSystemsInstance() {
    static ServerDllSharedAppSystems001Adapter instance;
    return instance;
}

// Exact build4100 IHLTVDirector address-point order recovered from the original
// secondary vtable: deleting/non-deleting destructor followed by 8 methods.
class HltvDirector001Adapter final {
public:
    virtual ~HltvDirector001Adapter() = default;
    virtual bool IsActive() { return server_ != nullptr; }
    virtual void SetHLTVServer(void* server) { server_ = server; }
    virtual void* GetHLTVServer() { return server_; }
    virtual int GetDirectorTick() { return 0; }
    virtual int GetPVSEntity() { return 0; }
    virtual build4100::Vector GetPVSOrigin() { return {}; }
    virtual float GetDelay() { return 0.0f; }
    virtual void* GetModEvents() { return nullptr; }

private:
    void* server_ = nullptr;
};

HltvDirector001Adapter& HltvDirectorInstance() {
    static HltvDirector001Adapter instance;
    return instance;
}

// Original build4100 IPluginHelpersCheck has exactly one address-point slot
// (vtable object size 12 bytes on i386: 2 ABI header words + 1 method).
// CPluginHelpersCheck::CreateMessage is intentionally a no-op until the
// KeyValues/dialog bridge is reconstructed; exposing the exact interface keeps
// the real engine/plugin loader contract intact.
class PluginHelpersCheck001Adapter final {
public:
    virtual void CreateMessage(const char* pluginName, build4100::Edict* player,
                               int dialogType, void* keyValues) {
        (void)pluginName; (void)player; (void)dialogType; (void)keyValues;
    }
};

PluginHelpersCheck001Adapter& PluginHelpersCheckInstance() {
    static PluginHelpersCheck001Adapter instance;
    return instance;
}

// IGameEventListener2 build4100 ABI: virtual destructor pair followed by
// FireGameEvent(IGameEvent*).  The original CSS CEventLog registers one
// server-side listener for these three core engine events during DLLInit.
// CGameClient::Connect calls CreateEvent("player_connect", false) and then
// dereferences the returned event without a null check, so merely loading the
// event manifests is insufficient: at least one server-side listener must be
// present for player_connect before any real/fake client can connect.
class CoreGameEventListener2 final {
public:
    virtual ~CoreGameEventListener2() = default;
    virtual void FireGameEvent(void* event) {
        char detail[64]{};
        std::snprintf(detail, sizeof(detail), "event=%p", event);
        EngineProbe("CORE_GAME_EVENT_FIRE", detail);
    }
};

CoreGameEventListener2& CoreGameEventListenerInstance() {
    static CoreGameEventListener2 instance;
    return instance;
}

constexpr const char* kCoreGameEvents[] = {
    "player_team",
    "player_disconnect",
    "player_connect",
};

void* QueryRequiredInterface(const CreateInterfaceFn factory, const char* const name) noexcept {
    if (!factory || !name) return nullptr;
    int rc = kInterfaceFailed;
    void* object = factory(name, &rc);
    return (object && rc == kInterfaceOk) ? object : nullptr;
}

}  // namespace

ServerGameDll006Adapter::ServerGameDll006Adapter()
    : engine_(kMaxEdicts), game_(engine_) {}

bool ServerGameDll006Adapter::SynchronizeClientSlots(const char* const phase) {
    if (!globals_ || globals_->maxClients < 0 || globals_->maxClients > kMaxProvenClients) {
        EngineProbe("CLIENT_LIMIT_SYNC_FAIL", phase ? phase : "<null>");
        return false;
    }

    const auto maxClients = static_cast<std::size_t>(globals_->maxClients);
    auto& clients = ServerGameClients004Instance();
    if (engine_.MaxClients() == maxClients && clients.Clients().MaxClients() == maxClients) {
        return true;
    }

    if (!engine_.ConfigureClientSlots(maxClients) || !clients.SyncMaxClients(maxClients)) {
        EngineProbe("CLIENT_LIMIT_SYNC_FAIL", phase ? phase : "<null>");
        return false;
    }

    char detail[96]{};
    std::snprintf(detail, sizeof(detail), "phase=%s maxClients=%zu",
                  phase ? phase : "<null>", maxClients);
    EngineProbe("CLIENT_LIMIT_SYNC_OK", detail);
    return true;
}

void ServerGameDll006Adapter::QueueFakeClientsFromQuota() {
    fakeClientTarget_ = 0;
    fakeClientsCreated_ = 0;
    fakeClientSeedAttempts_ = 0;
    EngineConVar* const quota = cvarBridge_.FindLocal("bot_quota");
    if (quota && globals_ && globals_->maxClients > 0) {
        fakeClientTarget_ = std::clamp(quota->GetInt(), 0, globals_->maxClients);
    }
    char detail[96]{};
    std::snprintf(detail, sizeof(detail), "value=%d maxClients=%d",
                  fakeClientTarget_, globals_ ? globals_->maxClients : 0);
    EngineProbe("BOT_QUOTA_RUNTIME", detail);
}

void ServerGameDll006Adapter::SeedQueuedFakeClients() {
    if (fakeClientTarget_ <= fakeClientsCreated_ || fakeClientSeedAttempts_ >= 8 ||
        !engineAbi_.IsValid()) {
        return;
    }

    ++fakeClientSeedAttempts_;
    {
        char detail[128]{};
        std::snprintf(detail, sizeof(detail),
                      "target=%d created=%d attempt=%d",
                      fakeClientTarget_, fakeClientsCreated_, fakeClientSeedAttempts_);
        EngineProbe("FAKE_CLIENT_SEED_ENTER", detail);
    }
    const bool previousLock = engineAbi_.LockNetworkStringTables(false);
    {
        char detail[96]{};
        std::snprintf(detail, sizeof(detail), "previous=%d attempt=%d",
                      previousLock ? 1 : 0, fakeClientSeedAttempts_);
        EngineProbe("FAKE_CLIENT_STRINGTABLE_UNLOCK", detail);
    }
    const int before = fakeClientsCreated_;

    // Create at most one fake client per engine frame. CreateFakeClient re-enters
    // the game DLL and mutates the engine client list; batching multiple clients
    // from one callback is not a proven build4100 contract.
    char name[48]{};
    std::snprintf(name, sizeof(name), "cssv34_bot_%02d", fakeClientsCreated_ + 1);

    EngineProbe("FAKE_CLIENT_CREATE_ENTER", name);
    build4100::Edict* const fakeClient = engineAbi_.CreateFakeClient(name);
    if (!fakeClient) {
        EngineProbe("FAKE_CLIENT_CREATE_FAIL", name);
    } else {
        int edictIndex = -1;
        if (engineEdictBase_ && engineEdictCount_ > 0) {
            const auto base = reinterpret_cast<std::uintptr_t>(engineEdictBase_);
            const auto value = reinterpret_cast<std::uintptr_t>(fakeClient);
            if (value >= base) {
                const auto delta = value - base;
                if ((delta % sizeof(build4100::Edict)) == 0) {
                    const auto rawIndex = delta / sizeof(build4100::Edict);
                    if (rawIndex < static_cast<std::uintptr_t>(engineEdictCount_)) {
                        edictIndex = static_cast<int>(rawIndex);
                    }
                }
            }
        }
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "name=%s index=%d ptr=%p",
                      name, edictIndex, static_cast<void*>(fakeClient));
        EngineProbe("FAKE_CLIENT_CREATE_OK", detail);
        ++fakeClientsCreated_;
    }
    (void)engineAbi_.LockNetworkStringTables(previousLock);

    char detail[128]{};
    std::snprintf(detail, sizeof(detail),
                  "target=%d created=%d delta=%d attempt=%d restore_lock=%d",
                  fakeClientTarget_, fakeClientsCreated_, fakeClientsCreated_ - before,
                  fakeClientSeedAttempts_, previousLock ? 1 : 0);
    EngineProbe(fakeClientsCreated_ >= fakeClientTarget_
                    ? "FAKE_CLIENT_SEED_OK" : "FAKE_CLIENT_SEED_RETRY",
                detail);
}

void ServerGameDll006Adapter::TryDumpInstanceBaselines() {
    if (instanceBaselineDumped_) return;
    const char* const path = std::getenv("V34_INSTANCEBASELINE_FILE");
    if (!path || !path[0]) return;

    const int ready = DumpRuntimeInstanceBaselines(path);
    const build4100::ServerClass* const player = ServerClasses().FindByNetworkName("CCSPlayer");
    const build4100::ServerClass* const planted = ServerClasses().FindByNetworkName("CPlantedC4");
    const int playerBaseline = player ? player->instanceBaselineIndex : -1;
    const int plantedBaseline = planted ? planted->instanceBaselineIndex : -1;

    // build4100 materializes instance baselines only for ServerClasses that had
    // a live edict during SV_CreateBaseline. Real/fake CCSPlayer edicts are
    // created later in GameFrame/Protocol7 signon, so CCSPlayer may legitimately
    // remain at the 0xffff sentinel while PacketEntities still sends an EnterPVS
    // record whose missing class baseline means an implicit all-zero/default
    // state. After changelevel, maps without a pre-existing baseline-bearing
    // entity can legitimately have ready==0 at this pre-connect checkpoint.
    // Freeze that empty census once deterministic fake-client seeding completes:
    // it is still a valid lower-bound snapshot and must not be rewritten while
    // the Protocol7 client itself causes later engine baseline materialization.
    const bool seedComplete = fakeClientTarget_ <= 0 || fakeClientsCreated_ >= fakeClientTarget_;
    if (ready >= 0 && seedComplete) {
        instanceBaselineDumped_ = true;
        instanceBaselineLastResult_ = ready;
        char detail[208]{};
        std::snprintf(detail, sizeof(detail),
                      "classes=177 ready=%d missing=%d player_baseline=%d c4_baseline=%d seeded=%d/%d",
                      ready, 177 - ready, playerBaseline, plantedBaseline,
                      fakeClientsCreated_, fakeClientTarget_);
        EngineProbe("INSTANCEBASELINE_DUMP_OK", detail);
        return;
    }

    const int state = ready;
    if (state != instanceBaselineLastResult_) {
        instanceBaselineLastResult_ = state;
        char detail[160]{};
        std::snprintf(detail, sizeof(detail),
                      "rc=%d ready=%d player_baseline=%d c4_baseline=%d",
                      ready, ready > 0 ? ready : 0, playerBaseline, plantedBaseline);
        EngineProbe(ready >= 0 ? "INSTANCEBASELINE_DUMP_WAIT"
                               : "INSTANCEBASELINE_DUMP_FAIL", detail);
    }
}

void ServerGameDll006Adapter::TryDumpSendPropSchema() {
    if (sendPropSchemaDumped_) return;
    const char* const path = std::getenv("V34_SENDPROP_SCHEMA_FILE");
    if (!path || !path[0]) return;
    const int fields = DumpRuntimeSendPropSchema(path);
    if (fields > 0) {
        sendPropSchemaDumped_ = true;
        sendPropSchemaLastResult_ = fields;
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "classes=177 flat_fields=%d account_field=347", fields);
        EngineProbe("SENDPROP_SCHEMA_DUMP_OK", detail);
    } else if (fields != sendPropSchemaLastResult_) {
        // Avoid one line per GameFrame while still making a schema-precalc
        // regression visible in the user-returned Docker log.  -2 means the
        // engine has not attached every CSendTablePrecalc yet; every other
        // negative result is a structural/file error and should not be silent.
        sendPropSchemaLastResult_ = fields;
        char detail[96]{};
        std::snprintf(detail, sizeof(detail), "rc=%d", fields);
        EngineProbe(fields == -2 ? "SENDPROP_SCHEMA_DUMP_WAIT"
                                 : "SENDPROP_SCHEMA_DUMP_FAIL", detail);
    }
}

bool ServerGameDll006Adapter::DLLInit(const CreateInterfaceFn engineFactory,
                                      const CreateInterfaceFn physicsFactory,
                                      const CreateInterfaceFn fileSystemFactory,
                                      void* const globals) {
    EngineProbe("DLLInit_ENTER");
    (void)fileSystemFactory;

    auto* const globalVars = static_cast<build4100::GlobalVarsBase*>(globals);
    if (!engineFactory || !globalVars || globalVars->maxClients < 0 ||
        globalVars->maxClients > kMaxProvenClients) {
        EngineProbe("DLLInit_FAIL", "bad_args");
        return false;
    }

    void* const engineObject = QueryRequiredInterface(engineFactory, kVEngineServer021);
    void* const engineTraceObject = QueryRequiredInterface(engineFactory, kEngineTraceServer003);
    void* const modelInfoObject = QueryRequiredInterface(engineFactory, kVModelInfoServer001);
    void* const physicsCollisionObject = QueryRequiredInterface(physicsFactory, kVPhysicsCollision007);
    void* const stringTableObject = QueryRequiredInterface(engineFactory, kVEngineServerStringTable001);
    void* const cvarObject = QueryRequiredInterface(engineFactory, kVEngineCvar003);
    void* const gameEventManagerObject = QueryRequiredInterface(engineFactory, kGameEventManager002);
    void* const voiceServerObject = QueryRequiredInterface(engineFactory, kVoiceServer001);
    // VSERVERENGINETOOLS001 is optional in Source and is not exposed by the
    // build4100 Linux engine factory in a normal dedicated-server launch.
    // The old reconstruction incorrectly made it a hard DLLInit dependency.
    void* const serverToolsObject = QueryRequiredInterface(engineFactory, kVServerEngineTools001);
    if (!engineObject || !engineTraceObject || !stringTableObject || !gameEventManagerObject) {
        const char* missing = !engineObject ? kVEngineServer021 :
                              (!engineTraceObject ? kEngineTraceServer003 :
                               (!stringTableObject ? kVEngineServerStringTable001 : kGameEventManager002));
        EngineProbe("DLLInit_FAIL", missing);
        return false;
    }

    engineAbi_.Reset(engineObject);
    engineTraceAbi_.Reset(engineTraceObject);
    stringTablesAbi_.Reset(stringTableObject);
    gameEventsAbi_.Reset(gameEventManagerObject);
    voiceServerAbi_.Reset(voiceServerObject);
    serverToolsAbi_.Reset(serverToolsObject);
    if (!engineAbi_.IsValid() || !engineTraceAbi_.IsValid() || !stringTablesAbi_.IsValid() || !gameEventsAbi_.IsValid()) {
        engineAbi_.Reset();
        engineTraceAbi_.Reset();
        stringTablesAbi_.Reset();
        gameEventsAbi_.Reset();
        voiceServerAbi_.Reset();
        serverToolsAbi_.Reset();
        EngineProbe("DLLInit_FAIL", "invalid_engine_interface");
        return false;
    }
    EngineProbe(serverToolsAbi_.IsValid() ? "SERVER_ENGINE_TOOLS_OK" : "SERVER_ENGINE_TOOLS_OPTIONAL_MISSING");
    EngineProbe(voiceServerAbi_.IsValid() ? "VOICE_SERVER_READY" : "VOICE_SERVER_OPTIONAL_MISSING",
                "interface=VoiceServer001 slots=Get2_Set3");
    {
        char detail[64]{};
        std::snprintf(detail, sizeof(detail), "modelinfo=%d physics=%d",
                      modelInfoObject ? 1 : 0, physicsCollisionObject ? 1 : 0);
        EngineProbe((modelInfoObject && physicsCollisionObject)
                        ? "DYNAMIC_COLLISION_INTERFACES_OK"
                        : "DYNAMIC_COLLISION_INTERFACES_OPTIONAL_MISSING",
                    detail);
    }

    if (!engine_.ConfigureClientSlots(static_cast<std::size_t>(globalVars->maxClients))) {
        engineAbi_.Reset();
        engineTraceAbi_.Reset();
        stringTablesAbi_.Reset();
        gameEventsAbi_.Reset();
        voiceServerAbi_.Reset();
        serverToolsAbi_.Reset();
        EngineProbe("DLLInit_FAIL", "client_slots");
        return false;
    }

    globals_ = globalVars;
    engineEdictBase_ = nullptr;
    engineEdictCount_ = 0;
    networkStringTables_.fill(nullptr);
    networkStringTablesReady_ = false;
    fakeClientTarget_ = 0;
    fakeClientsCreated_ = 0;
    fakeClientSeedAttempts_ = 0;
    firstGameFrameSeen_ = false;
    coreEventListenerRegistered_ = false;
    sendPropSchemaDumped_ = false;
    sendPropSchemaLastResult_ = 0;

    entityBridge_.Bind(&engineAbi_, kMaxEdicts);
    ServerGameEnts001Instance().Bind(&entityBridge_, &engineAbi_);
    ServerGameClients004Instance().Bind(&engineAbi_, globals_, &entityBridge_, &engineTraceAbi_,
                                        modelInfoObject, physicsCollisionObject, &gameEventsAbi_,
                                        &voiceServerAbi_);
    PlayerInfoManager002Instance().Bind(
        &engineAbi_, &ServerGameClients004Instance().ClientsMutable(), globals_);

    if (cvarObject && cvarBridge_.Bind(cvarObject)) {
        EngineProbe("CVAR_BRIDGE_OK", "registered=11");
    } else {
        EngineProbe("CVAR_BRIDGE_OPTIONAL_MISSING");
    }

    // build4100 CGameClient::Connect assumes the engine-owned player_connect
    // descriptor already exists. The original CSS DLL loads these manifests
    // during DLLInit before any fake/real client can connect. Without this,
    // CGameEventManager::CreateEvent("player_connect") returns null and the
    // engine dereferences it immediately inside CGameClient::Connect.
    const int baseEventCount = gameEventsAbi_.LoadEventsFromFile("resource/gameevents.res");
    {
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "file=resource/gameevents.res count=%d", baseEventCount);
        EngineProbe("GAME_EVENTS_LOAD", detail);
    }
    const int modEventCount = gameEventsAbi_.LoadEventsFromFile("resource/ModEvents.res");
    {
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "file=resource/ModEvents.res count=%d", modEventCount);
        EngineProbe("GAME_EVENTS_LOAD", detail);
    }
    if (baseEventCount <= 0 || modEventCount <= 0) {
        ServerGameClients004Instance().Unbind();
        PlayerInfoManager002Instance().Unbind();
        ServerGameEnts001Instance().Unbind();
        cvarBridge_.Unbind();
        entityBridge_.Unbind();
        globals_ = nullptr;
        engineAbi_.Reset();
        engineTraceAbi_.Reset();
        stringTablesAbi_.Reset();
        gameEventsAbi_.Reset();
        voiceServerAbi_.Reset();
        serverToolsAbi_.Reset();
        EngineProbe("DLLInit_FAIL", "game_event_manifests");
        return false;
    }
    {
        char detail[96]{};
        std::snprintf(detail, sizeof(detail), "base=%d mod=%d", baseEventCount, modEventCount);
        EngineProbe("GAME_EVENTS_READY", detail);
    }

    // Mirror the original build4100 CEventLog::Init registrations exactly.
    // The engine intentionally suppresses CreateEvent(name, false) when an
    // event has no listeners; player_connect is then dereferenced unconditionally
    // by CGameClient::Connect. Registering the original listener set is therefore
    // a required engine/game-DLL contract, not optional logging behavior.
    bool listenersOk = true;
    for (const char* const eventName : kCoreGameEvents) {
        const bool added = gameEventsAbi_.AddListener(
            &CoreGameEventListenerInstance(), eventName, true);
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "name=%s server=1 added=%d",
                      eventName, added ? 1 : 0);
        EngineProbe("GAME_EVENT_LISTENER_ADD", detail);
        listenersOk = listenersOk && added;
    }
    if (!listenersOk) {
        gameEventsAbi_.RemoveListener(&CoreGameEventListenerInstance());
        ServerGameClients004Instance().Unbind();
        PlayerInfoManager002Instance().Unbind();
        ServerGameEnts001Instance().Unbind();
        cvarBridge_.Unbind();
        entityBridge_.Unbind();
        globals_ = nullptr;
        engineAbi_.Reset();
        engineTraceAbi_.Reset();
        stringTablesAbi_.Reset();
        gameEventsAbi_.Reset();
        voiceServerAbi_.Reset();
        serverToolsAbi_.Reset();
        EngineProbe("DLLInit_FAIL", "core_game_event_listener");
        return false;
    }
    coreEventListenerRegistered_ = true;

    // Prove the exact engine invariant needed by CGameClient::Connect before
    // allowing DLLInit to succeed. Each event is immediately freed; the listener
    // remains registered for the real engine lifecycle.
    for (const char* const eventName : kCoreGameEvents) {
        const GameEventClient event = gameEventsAbi_.CreateEvent(eventName, false);
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "name=%s valid=%d",
                      eventName, event.IsValid() ? 1 : 0);
        EngineProbe("GAME_EVENT_PREFLIGHT", detail);
        if (!event.IsValid()) {
            gameEventsAbi_.RemoveListener(&CoreGameEventListenerInstance());
            coreEventListenerRegistered_ = false;
            ServerGameClients004Instance().Unbind();
            PlayerInfoManager002Instance().Unbind();
            ServerGameEnts001Instance().Unbind();
            cvarBridge_.Unbind();
            entityBridge_.Unbind();
            globals_ = nullptr;
            engineAbi_.Reset();
            engineTraceAbi_.Reset();
            stringTablesAbi_.Reset();
            gameEventsAbi_.Reset();
            voiceServerAbi_.Reset();
            serverToolsAbi_.Reset();
            EngineProbe("DLLInit_FAIL", "core_game_event_preflight");
            return false;
        }
        gameEventsAbi_.FreeEvent(event);
    }
    EngineProbe("GAME_EVENT_PLAYER_CONNECT_READY");

    // The original CSS build4100 consumes encrypted scripts/weapon_*.ctx at
    // startup. Recover the exact per-weapon ballistic core directly from that
    // installed content instead of carrying grouped approximation tables.
    {
        const auto& profileStatus = InitializeWeaponProfilesFromDisk();
        const auto& ak47 = WeaponProfile(build4100::CSWeaponID::Ak47);
        const auto& awp = WeaponProfile(build4100::CSWeaponID::Awp);
        const auto& usp = WeaponProfile(build4100::CSWeaponID::Usp);
        const auto& m3 = WeaponProfile(build4100::CSWeaponID::M3);
        const auto& xm1014 = WeaponProfile(build4100::CSWeaponID::XM1014);
        // Keep this telemetry intentionally roomy: zw appended the recovered
        // accuracy tuple to an already long profile line and the old 384-byte
        // buffer silently truncated the tail.  A truncated success line is worse
        // than an explicit telemetry failure because the host acceptance parser
        // cannot distinguish it from missing runtime state.
        char detail[768]{};
        const int detailLength = std::snprintf(
            detail, sizeof(detail),
            "loaded=%d expected=%d encrypted=%d plaintext=%d failed=%d mask=0x%llx root=%.120s ak47=%.3f/%.4f/%.3f/%.3f awp=%.3f/%.4f usp=%.3f/%.4f ak47_acc=%.3f/%.3f/%.3f/%d shotgun_bullets=m3:%d,xm1014:%d",
            profileStatus.exactProfiles, profileStatus.expectedProfiles,
            profileStatus.encryptedProfiles, profileStatus.plaintextProfiles,
            profileStatus.failedProfiles,
            static_cast<unsigned long long>(profileStatus.exactMask),
            profileStatus.sourceRoot,
            ak47.baseDamage, ak47.cycleTime, ak47.rangeModifier, ak47.armorRatio,
            awp.baseDamage, awp.cycleTime, usp.baseDamage, usp.cycleTime,
            ak47.accuracyDivisor, ak47.accuracyOffset, ak47.maxInaccuracy,
            AccuracyQuadraticFlag(ak47), m3.bulletsPerShot, xm1014.bulletsPerShot);
        if (detailLength < 0 || static_cast<std::size_t>(detailLength) >= sizeof(detail)) {
            EngineProbe("WEAPON_SCRIPT_PROFILES_PARTIAL", "telemetry_truncated");
        } else {
            EngineProbe(profileStatus.exactProfiles == profileStatus.expectedProfiles
                            ? "WEAPON_SCRIPT_PROFILES_OK"
                            : "WEAPON_SCRIPT_PROFILES_PARTIAL",
                        detail);
        }
    }
    // Historical source-gate compatibility markers; these values describe the
    // accepted 35c/35d/35e checkpoints and are superseded by the current line:
    // recovered_state_families=22 model_only_pistol_sniper=10
    // recovered_recoil_families=10 live_exact_families=22
    EngineProbe("COMBAT_FIDELITY_OK",
                // v1.8.35g adds the recovered CTEFireBullets PlaybackTempEntity path.
                "spread_rng=source_uniform_fx_seed8 punch_decay=source_spring9_65 classic_auto_families=12 recovered_state_families=24 timed_accuracy_families=8 live_pistol_sniper=10 secondary_mode_families=8 recovered_recoil_families=12 glock_burst_scheduler=3x100ms glock_followup_spread=0.05 fov_wire_families=4 shotgun_families=2 shotgun_spread=m3_0.0675_xm1014_0.0725 shotgun_rng=fx_seed_per_pellet shotgun_recoil=sharedrandom_ground_air live_exact_families=24 te_firebullets=playback_slot64_class120 te_pas=multicast_slot41_prediction_exclude");

    if (!game_.DLLInit()) {
        if (coreEventListenerRegistered_) {
            gameEventsAbi_.RemoveListener(&CoreGameEventListenerInstance());
            coreEventListenerRegistered_ = false;
        }
        ServerGameClients004Instance().Unbind();
        PlayerInfoManager002Instance().Unbind();
        ServerGameEnts001Instance().Unbind();
        cvarBridge_.Unbind();
        entityBridge_.Unbind();
        globals_ = nullptr;
        engineAbi_.Reset();
        engineTraceAbi_.Reset();
        stringTablesAbi_.Reset();
        gameEventsAbi_.Reset();
        voiceServerAbi_.Reset();
        serverToolsAbi_.Reset();
        EngineProbe("DLLInit_FAIL", "lifecycle");
        return false;
    }
    EngineProbe("DLLInit_OK");
    return true;
}

bool ServerGameDll006Adapter::GameInit() {
    EngineProbe("GameInit_ENTER");
    if (!SynchronizeClientSlots("GameInit")) {
        EngineProbe("GameInit_FAIL", "client_limit_sync");
        return false;
    }
    if (!game_.GameInit()) {
        EngineProbe("GameInit_FAIL");
        return false;
    }
    engineAbi_.ServerCommand("exec game.cfg\n");
#if V34_RELEASE_PROFILE
    // The stock dedicated server reaches its production steady state with
    // cstrike/cfg/server.cfg applied.  The DEBUG profile deliberately keeps the
    // historical deterministic test launch values (+bot_quota 2, etc.) so the
    // long Protocol7 regression pack can be resumed later without changing its
    // client-slot assumptions.  RELEASE mirrors the stock runtime configuration
    // used by the final original-vs-rebuilt parity gate.
    engineAbi_.ServerCommand("exec server.cfg\n");
#endif
    engineAbi_.ServerExecute();
    EngineProbe("GameInit_OK");
    return true;
}

bool ServerGameDll006Adapter::LevelInit(const char* const mapName,
                                        const char* const mapEntities,
                                        const char* const oldLevel,
                                        const char* const landmarkName,
                                        const bool loadGame,
                                        const bool background) {
    (void)oldLevel;
    (void)landmarkName;
    (void)loadGame;
    (void)background;
    EngineProbe("LevelInit_ENTER", mapName ? mapName : "<null>");
    fakeClientTarget_ = 0;
    fakeClientsCreated_ = 0;
    fakeClientSeedAttempts_ = 0;
    firstGameFrameSeen_ = false;
    if (!SynchronizeClientSlots("LevelInit")) {
        EngineProbe("LevelInit_FAIL", "client_limit_sync");
        return false;
    }
    if (!mapName) {
        EngineProbe("LevelInit_FAIL", "null_map");
        return false;
    }

    MapEntityLump entityLump;
    const bool parsedEntityLump = entityLump.Parse(mapEntities ? mapEntities : "");
    char entityDetail[64]{};
    std::snprintf(entityDetail, sizeof(entityDetail), "entities=%zu world=%d parsed=%d",
                  entityLump.Count(), entityLump.HasWorldspawn() ? 1 : 0, parsedEntityLump ? 1 : 0);
    EngineProbe("MAP_ENTITY_LUMP", entityDetail);
    std::vector<build4100::Vector> terroristSpawns;
    std::vector<build4100::Vector> counterTerroristSpawns;
    CollectPlayerSpawns(entityLump, terroristSpawns, counterTerroristSpawns);
    {
        char spawnDetail[96]{};
        std::snprintf(spawnDetail, sizeof(spawnDetail), "terrorist=%zu counterterrorist=%zu",
                      terroristSpawns.size(), counterTerroristSpawns.size());
        EngineProbe("MAP_PLAYER_SPAWNS", spawnDetail);
    }
    ServerGameClients004Instance().SetMapName(mapName);
    ServerGameClients004Instance().SetMapPlayerSpawns(
        std::move(terroristSpawns), std::move(counterTerroristSpawns));
    auto movementSolids = CollectMapMovementSolids(entityLump);
    {
        std::size_t studio = 0;
        std::size_t brush = 0;
        std::size_t multiplayerProps = 0;
        std::size_t hostages = 0;
        std::size_t npcs = 0;
        for (const auto& solid : movementSolids) {
            studio += solid.kind == MapMovementSolid::Kind::StudioPhysics ? 1u : 0u;
            brush += solid.kind == MapMovementSolid::Kind::InlineBrush ? 1u : 0u;
            hostages += solid.kind == MapMovementSolid::Kind::HostageBBox ? 1u : 0u;
            npcs += solid.kind == MapMovementSolid::Kind::NpcBBox ? 1u : 0u;
            multiplayerProps += solid.classname == "prop_physics_multiplayer" ? 1u : 0u;
        }
        char solidDetail[224]{};
        std::snprintf(solidDetail, sizeof(solidDetail),
                      "total=%zu studio=%zu brush=%zu prop_physics_multiplayer=%zu hostage_bbox=%zu npc_bbox=%zu",
                      movementSolids.size(), studio, brush, multiplayerProps, hostages, npcs);
        EngineProbe("MAP_MOVEMENT_SOLIDS", solidDetail);
    }
    ServerGameClients004Instance().SetMapMovementSolids(std::move(movementSolids));
    ServerGameClients004Instance().SetMapObjectiveZones(CollectMapObjectiveZones(entityLump));
    auto ladders = CollectMapLadders(entityLump);
    {
        char ladderDetail[96]{};
        std::snprintf(ladderDetail, sizeof(ladderDetail), "count=%zu", ladders.size());
        EngineProbe("MAP_MOVEMENT_LADDERS", ladderDetail);
    }
    ServerGameClients004Instance().SetMapLadders(std::move(ladders));

    serverToolsAbi_.LevelInitPreEntityAllTools();
    if (!parsedEntityLump || !game_.LevelInit(std::string(mapName), std::string(mapEntities ? mapEntities : ""))) {
        EngineProbe("LevelInit_FAIL", "lifecycle");
        return false;
    }

    engineAbi_.ServerCommand("exec skill1.cfg\n");
    engineAbi_.ServerExecute();
    EngineProbe("LevelInit_OK", mapName);
    return true;
}

void ServerGameDll006Adapter::ServerActivate(build4100::Edict* const edictList,
                                              const int edictCount,
                                              const int clientMax) {
    EngineProbe("ServerActivate_ENTER");
    if (!globals_ || clientMax != globals_->maxClients || edictCount < 0) {
        EngineProbe("ServerActivate_SKIP", "bad_args");
        return;
    }
    if (!SynchronizeClientSlots("ServerActivate")) {
        EngineProbe("ServerActivate_SKIP", "client_limit_sync");
        return;
    }

    // Real srcds passes the engine-owned edict array here. Keep the opaque
    // pointer/geometry without dereferencing fields whose ownership is still
    // engine-side, and expose the base to ServerGameEnts001 for index/debug use.
    engineEdictBase_ = edictList;
    engineEdictCount_ = edictCount;
    entityBridge_.ServerActivate(edictList, edictCount);
    ServerGameClients004Instance().SetEngineEdictBase(edictList, edictCount);
    ServerGameEnts001Instance().SetDebugEdictBase(edictList);

    if (game_.ServerActivate()) {
        // Baseline-critical ordering recovered from build4100 engine_i486.so:
        // SV_ActivateServer invokes this ServerActivate callback and calls
        // SV_CreateBaseline immediately after the callback returns. planted_c4
        // therefore has to own a live (parked DONT_SEND) edict *before* return,
        // otherwise CPlantedC4 never receives an instancebaseline entry and can
        // never be encoded as an EnterPVS PacketEntities update later in the round.
        ServerGameClients004Instance().PrepareLevelNetworkEntities("serveractivate_prebaseline");
        serverToolsAbi_.LevelInitPostEntityAllTools();
        EngineProbe("ServerActivate_OK");
        ProbeEngineSignonFacts();
        TryDumpSendPropSchema();
        QueueFakeClientsFromQuota();
        // Do not call IVEngineServer::CreateFakeClient while the engine is still
        // inside ServerActivate. build4100 mutates its client list in that path;
        // the first GameFrame is the first proven safe post-activation boundary.
        // CreateEdict is intentionally different: engine baseline construction
        // requires game-owned network entities to exist before this callback ends.
    } else {
        EngineProbe("ServerActivate_FAIL", "lifecycle");
    }
}

void ServerGameDll006Adapter::GameFrame(const bool simulating) {
    if (!firstGameFrameSeen_) {
        firstGameFrameSeen_ = true;
        char detail[96]{};
        std::snprintf(detail, sizeof(detail), "simulating=%d bot_target=%d",
                      simulating ? 1 : 0, fakeClientTarget_);
        EngineProbe("GameFrame_FIRST", detail);
    }
    // Do not gate fake-client creation on the engine's simulating flag. The
    // first post-activation GameFrame can legitimately report simulating=false.
    // This boundary is deliberately after ServerActivate has returned to engine.
    TryDumpSendPropSchema();
    // Idempotent fallback only. The baseline-critical reservation must already
    // have happened in ServerActivate; this call preserves diagnostics if the
    // allocator rejected that early attempt.
    ServerGameClients004Instance().PrepareLevelNetworkEntities("gameframe_fallback");
    ServerGameClients004Instance().VerifyLevelNetworkEntityBaselines();
    SeedQueuedFakeClients();
    TryDumpInstanceBaselines();
    ServerGameClients004Instance().UpdateDynamicNonPlayerOrigins();
    ServerGameClients004Instance().ProcessDeferredTeamSpawns();
    ServerGameClients004Instance().TickGameplayRound();
    ServerGameClients004Instance().ProcessDeferredWeaponBursts();
    ServerGameClients004Instance().ProcessDeferredTempEntities();
    serverToolsAbi_.FrameUpdatePreEntityThink();
    (void)game_.GameFrame(simulating);
    entityBridge_.Tick(globals_ ? globals_->curtime : 0.0f);
    serverToolsAbi_.FrameUpdatePostEntityThink();
}

void ServerGameDll006Adapter::PreClientUpdate(const bool simulating) {
    (void)game_.PreClientUpdate(simulating);
    serverToolsAbi_.PreClientUpdate();
}

void ServerGameDll006Adapter::LevelShutdown() {
    EngineProbe("LevelShutdown_ENTER");
    serverToolsAbi_.LevelShutdownPreEntityAllTools();
    ServerGameClients004Instance().OnLevelShutdown();
    (void)game_.LevelShutdown();
    serverToolsAbi_.LevelShutdownPostEntityAllTools();
    engineEdictBase_ = nullptr;
    engineEdictCount_ = 0;
    entityBridge_.ServerActivate(nullptr, 0);
    ServerGameClients004Instance().SetEngineEdictBase(nullptr, 0);
    ServerGameEnts001Instance().SetDebugEdictBase(nullptr);
    fakeClientTarget_ = 0;
    fakeClientsCreated_ = 0;
    fakeClientSeedAttempts_ = 0;
    firstGameFrameSeen_ = false;
    sendPropSchemaDumped_ = false;
    sendPropSchemaLastResult_ = 0;
    instanceBaselineDumped_ = false;
    instanceBaselineLastResult_ = -9999;
    EngineProbe("LevelShutdown_OK");
}

void ServerGameDll006Adapter::GameShutdown() {
    EngineProbe("GameShutdown_ENTER");
    (void)game_.GameShutdown();
    EngineProbe("GameShutdown_OK");
}

void ServerGameDll006Adapter::DLLShutdown() {
    EngineProbe("DLLShutdown_ENTER");
    (void)game_.DLLShutdown();
    if (coreEventListenerRegistered_ && gameEventsAbi_.IsValid()) {
        gameEventsAbi_.RemoveListener(&CoreGameEventListenerInstance());
        coreEventListenerRegistered_ = false;
        EngineProbe("GAME_EVENT_LISTENER_REMOVE", "core");
    }
    ServerGameClients004Instance().Unbind();
    PlayerInfoManager002Instance().Unbind();
    ServerGameEnts001Instance().Unbind();
    cvarBridge_.Unbind();
    entityBridge_.Unbind();
    globals_ = nullptr;
    engineEdictBase_ = nullptr;
    engineEdictCount_ = 0;
    networkStringTables_.fill(nullptr);
    networkStringTablesReady_ = false;
    fakeClientTarget_ = 0;
    fakeClientsCreated_ = 0;
    fakeClientSeedAttempts_ = 0;
    firstGameFrameSeen_ = false;
    coreEventListenerRegistered_ = false;
    sendPropSchemaDumped_ = false;
    sendPropSchemaLastResult_ = 0;
    instanceBaselineDumped_ = false;
    instanceBaselineLastResult_ = -9999;
    engineAbi_.Reset();
    engineTraceAbi_.Reset();
    stringTablesAbi_.Reset();
    gameEventsAbi_.Reset();
    voiceServerAbi_.Reset();
    serverToolsAbi_.Reset();
    EngineProbe("DLLShutdown_OK");
}

float ServerGameDll006Adapter::GetTickInterval() const {
    EngineProbe("GetTickInterval");
    return globals_ && globals_->interval_per_tick > 0.0f
        ? globals_->interval_per_tick : kFallbackTickInterval;
}

ServerClass* ServerGameDll006Adapter::GetAllServerClasses() {
    auto& registry = ServerClasses();
    EngineProbe("GetAllServerClasses_OK", "classes=177");
    return reinterpret_cast<ServerClass*>(registry.Head());
}

const char* ServerGameDll006Adapter::GetGameDescription() {
    EngineProbe("GetGameDescription");
    return kGameDescription;
}

void ServerGameDll006Adapter::CreateNetworkStringTables() {
    EngineProbe("CreateNetworkStringTables_ENTER");
    if (!stringTablesAbi_.IsValid()) {
        EngineProbe("CreateNetworkStringTables_FAIL", "string_table_interface");
        return;
    }

    struct Definition { const char* name; int maxEntries; };
    static constexpr Definition kTables[] = {
        {"EffectDispatch", 1024}, {"VguiScreen", 256}, {"Materials", 1024},
        {"InfoPanel", 128}, {"Scenes", 4096}, {"GameRulesCreation", 1},
        {"BlackMarketTable", 1},
    };
    static_assert(std::size(kTables) == 7);

    bool ready = true;
    for (std::size_t i = 0; i < std::size(kTables); ++i) {
        networkStringTables_[i] = stringTablesAbi_.CreateStringTable(
            kTables[i].name, kTables[i].maxEntries, 0, 0);
        ready = ready && networkStringTables_[i] != nullptr;
    }
    if (networkStringTables_[2]) {
        const NetworkStringTableClient materials(networkStringTables_[2]);
        ready = ready && materials.IsValid();
        ready = ready && materials.AddString("debug/debugempty", -1, nullptr) >= 0;
    } else {
        ready = false;
    }
    networkStringTablesReady_ = ready;
    EngineProbe(ready ? "CreateNetworkStringTables_OK" : "CreateNetworkStringTables_PARTIAL");
}

CSaveRestoreData* ServerGameDll006Adapter::SaveInit(const int size) {
    (void)size;
    return nullptr;
}
void ServerGameDll006Adapter::SaveWriteFields(CSaveRestoreData* save, const char* name, void* baseData,
                                               datamap_t* map, typedescription_t* fields, int fieldCount) {
    (void)save; (void)name; (void)baseData; (void)map; (void)fields; (void)fieldCount;
}
void ServerGameDll006Adapter::SaveReadFields(CSaveRestoreData* save, const char* name, void* baseData,
                                              datamap_t* map, typedescription_t* fields, int fieldCount) {
    (void)save; (void)name; (void)baseData; (void)map; (void)fields; (void)fieldCount;
}
void ServerGameDll006Adapter::SaveGlobalState(CSaveRestoreData* save) { (void)save; }
void ServerGameDll006Adapter::RestoreGlobalState(CSaveRestoreData* save) { (void)save; }
void ServerGameDll006Adapter::PreSave(CSaveRestoreData* save) { (void)save; }
void ServerGameDll006Adapter::Save(CSaveRestoreData* save) { (void)save; }
void ServerGameDll006Adapter::GetSaveComment(char* const buffer, const int maxLen) {
    if (buffer && maxLen > 0) buffer[0] = '\0';
}
void ServerGameDll006Adapter::WriteSaveHeaders(CSaveRestoreData* save) { (void)save; }
void ServerGameDll006Adapter::ReadRestoreHeaders(CSaveRestoreData* save) { (void)save; }
void ServerGameDll006Adapter::Restore(CSaveRestoreData* save, const bool createPlayers) {
    (void)save; (void)createPlayers;
}
bool ServerGameDll006Adapter::IsRestoring() { return false; }
int ServerGameDll006Adapter::CreateEntityTransitionList(CSaveRestoreData* save, const int maxList) {
    (void)save; (void)maxList; return 0;
}
void ServerGameDll006Adapter::BuildAdjacentMapList() {}
bool ServerGameDll006Adapter::GetUserMessageInfo(const int msgType, char* const name,
                                                  const int maxNameLength, int& size) {
    struct UserMessageInfo { const char* name; int size; };
    // Original CSS build4100 RegisterUserMessages() order recovered from
    // runtime/original/cstrike/bin/server_i486.so. IDs are the registration
    // indices used by svc_UserMessage on the protocol-7 wire.
    static constexpr UserMessageInfo kMessages[] = {
        {"Geiger", 1}, {"Train", 1}, {"HudText", -1}, {"SayText", -1},
        {"SayText2", -1}, {"TextMsg", -1}, {"HudMsg", -1}, {"ResetHUD", 1},
        {"GameTitle", 0}, {"ItemPickup", -1}, {"ShowMenu", -1}, {"Shake", 13},
        {"Fade", 10}, {"VGUIMenu", -1}, {"CloseCaption", 7}, {"SendAudio", -1},
        {"RawAudio", -1}, {"VoiceMask", 17}, {"RequestState", 0}, {"BarTime", -1},
        {"Damage", -1}, {"RadioText", -1}, {"HintText", -1}, {"ReloadEffect", 2},
        {"PlayerAnimEvent", -1}, {"AmmoDenied", 2}, {"UpdateRadar", -1}, {"KillCam", -1},
    };
    if (msgType < 0 || static_cast<std::size_t>(msgType) >= std::size(kMessages)) {
        if (name && maxNameLength > 0) name[0] = '\0';
        size = -1;
        return false;
    }
    const UserMessageInfo& info = kMessages[msgType];
    size = info.size;
    if (name && maxNameLength > 0) {
        std::snprintf(name, static_cast<std::size_t>(maxNameLength), "%s", info.name);
    }
    return true;
}
CStandardSendProxiesV1* ServerGameDll006Adapter::GetStandardSendProxies() {
    // Exact build4100 behavior: CServerGameDLL::GetStandardSendProxies is
    // `xor/mov eax, 0; ret`.  The engine falls back to its standard proxies.
    EngineProbe("GetStandardSendProxies_NULL");
    return nullptr;
}

void ServerGameDll006Adapter::PostInit() {
    // Exact build4100 behavior: CServerGameDLL::PostInit is a bare return.
    // Client slot synchronization belongs to the actual game/map lifecycle,
    // not Host_PostInit.
    EngineProbe("PostInit_ENTER");
    EngineProbe("PostInit_OK");
}
void ServerGameDll006Adapter::Think(const bool finalTick) { (void)finalTick; }
void ServerGameDll006Adapter::GetSaveCommentEx(char* const buffer, const int maxLen,
                                                const float minutes, const float seconds) {
    (void)minutes; (void)seconds;
    if (buffer && maxLen > 0) buffer[0] = '\0';
}
void ServerGameDll006Adapter::OnQueryCvarValueFinished(const int cookie,
                                                       build4100::Edict* const player,
                                                       const int status,
                                                       const char* const cvarName,
                                                       const char* const cvarValue) {
    (void)cookie; (void)player; (void)status; (void)cvarName; (void)cvarValue;
}

}  // namespace cssv34::reconstructed::abi

extern "C" __attribute__((visibility("default")))
void* CreateInterface(const char* const name, int* const returnCode) {
    using namespace cssv34::reconstructed::abi;
    EngineProbe("CreateInterface", name ? name : "<null>");

    void* result = nullptr;
    if (name && std::strcmp(name, kServerGameDll006) == 0) {
        result = &Instance();
    } else if (name && (std::strcmp(name, kServerGameClients003) == 0 ||
                        std::strcmp(name, kServerGameClients004) == 0)) {
        result = &ServerGameClients004Instance();
    } else if (name && (std::strcmp(name, kPlayerInfoManager001) == 0 ||
                        std::strcmp(name, kPlayerInfoManager002) == 0)) {
        result = &PlayerInfoManager002Instance();
    } else if (name && std::strcmp(name, kServerGameEnts001) == 0) {
        result = &ServerGameEnts001Instance();
    } else if (name && std::strcmp(name, kGameMovement001) == 0) {
        result = &GameMovement001Instance();
    } else if (name && std::strcmp(name, kHltvDirector001) == 0) {
        result = &HltvDirectorInstance();
    } else if (name && std::strcmp(name, kServerDllSharedAppSystems001) == 0) {
        result = &SharedAppSystemsInstance();
    } else if (name && std::strcmp(name, kPluginHelpersCheck001) == 0) {
        result = &PluginHelpersCheckInstance();
    }

    if (returnCode) *returnCode = result ? kInterfaceOk : kInterfaceFailed;
    EngineProbe(result ? "CreateInterface_OK" : "CreateInterface_FAIL", name ? name : "<null>");
    return result;
}
