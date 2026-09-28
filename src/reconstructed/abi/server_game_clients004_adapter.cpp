#include "abi/server_game_clients004_adapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <vector>
#include <utility>

#include "abi/engine_entity_bridge.h"
#include "abi/game_event_manager002_client.h"
#include "abi/voice_server001_client.h"
#include "abi/engine_probe.h"
#include "abi/server_class_registry.h"
#include "public/build4100/bitbuf.h"
#include "public/build4100/check_transmit.h"
#include "server/usercmd_codec.h"

namespace cssv34::reconstructed::abi {
namespace {

constexpr int kPlantedC4PreClientReservationAttempts = 8;

struct VCollide4100 {
    std::uint16_t solidCountAndFlags = 0;
    std::uint16_t descSize = 0;
    const void* const* solids = nullptr;
    const char* keyValues = nullptr;
    std::size_t SolidCount() const noexcept {
        return static_cast<std::size_t>(solidCountAndFlags & 0x7fffu);
    }
};
#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(VCollide4100) == 12, "build4100 vcollide_t size");
#endif

class SingleClientRecipientFilter final {
public:
    explicit SingleClientRecipientFilter(const int clientIndex) noexcept : clientIndex_(clientIndex) {}
    virtual ~SingleClientRecipientFilter() = default;
    virtual bool IsReliable() const noexcept { return true; }
    virtual bool IsInitMessage() const noexcept { return false; }
    virtual int GetRecipientCount() const noexcept { return 1; }
    virtual int GetRecipientIndex(const int slot) const noexcept {
        return slot == 0 ? clientIndex_ : -1;
    }

private:
    int clientIndex_ = -1;
};

class FireBulletsRecipientFilter final {
public:
    FireBulletsRecipientFilter(const ClientRegistry& clients, const EngineServer021Client& engine,
                               const std::size_t shooterIndex,
                               const build4100::Vector& origin) noexcept {
        // Original CRecipientFilter::AddRecipientsByPAS @ server_i486.so
        // 0x79aa20 zeroes CBitVec<255> and calls VEngineServer021 slot41
        // Message_DetermineMulticastRecipients(usepas=1, origin, bits).
        Build4100PlayerBitVec pasRecipients{};
        engine.MessageDetermineMulticastRecipients(true, origin, pasRecipients);
        for (std::size_t index = 1; index <= clients.MaxClients() && count_ < recipients_.size(); ++index) {
            const std::size_t pasBit = index - 1u;
            if (!pasRecipients.Test(pasBit)) continue;
            // CRecipientFilter::UsePredictionRules @ 0x799a00 removes the
            // predicting local player. For this server-side weapon effect that
            // is the shooter, which already predicts FX_FireBullets locally.
            if (index == shooterIndex) continue;
            const ClientRecord* const client = clients.Get(index);
            if (!client || !client->edict || client->state != ClientLifecycleState::Active ||
                client->isFakeClient || client->isHLTV) continue;
            recipients_[count_++] = static_cast<int>(index);
        }
    }
    virtual ~FireBulletsRecipientFilter() = default;
    virtual bool IsReliable() const noexcept { return false; }
    virtual bool IsInitMessage() const noexcept { return false; }
    virtual int GetRecipientCount() const noexcept { return static_cast<int>(count_); }
    virtual int GetRecipientIndex(const int slot) const noexcept {
        return slot >= 0 && static_cast<std::size_t>(slot) < count_
            ? recipients_[static_cast<std::size_t>(slot)] : -1;
    }

private:
    std::array<int, 64> recipients_{};
    std::size_t count_ = 0;
};

// PlaybackTempEntity consumes IRecipientFilter synchronously and copies its
// player set into the engine-owned CEventInfo. The deferred game-frame path
// therefore only needs a small read-only filter over the fire-time snapshot.
class CapturedFireBulletsRecipientFilter final {
public:
    CapturedFireBulletsRecipientFilter(const int* const recipients,
                                       const std::size_t count) noexcept
        : recipients_(recipients), count_(count) {}
    virtual ~CapturedFireBulletsRecipientFilter() = default;
    virtual bool IsReliable() const noexcept { return false; }
    virtual bool IsInitMessage() const noexcept { return false; }
    virtual int GetRecipientCount() const noexcept { return static_cast<int>(count_); }
    virtual int GetRecipientIndex(const int slot) const noexcept {
        return recipients_ && slot >= 0 && static_cast<std::size_t>(slot) < count_
            ? recipients_[static_cast<std::size_t>(slot)] : -1;
    }

private:
    const int* recipients_ = nullptr;
    std::size_t count_ = 0;
};

// Exact CTEFireBullets sender data layout recovered from TE_FireBullets
// (server_i486.so 0x009f8b40): player@+0x0c, origin@+0x10,
// angles@+0x1c, weapon@+0x28, mode@+0x2c, seed@+0x30, spread@+0x34.
struct FireBulletsTempEntityData {
    std::uint8_t base[12]{};
    std::int32_t player = 0;
    build4100::Vector origin{};
    build4100::QAngle angles{};
    std::int32_t weaponId = 0;
    std::int32_t mode = 0;
    std::int32_t seed = 0;
    float spread = 0.0f;
};
static_assert(offsetof(FireBulletsTempEntityData, player) == 12);
static_assert(offsetof(FireBulletsTempEntityData, origin) == 16);
static_assert(offsetof(FireBulletsTempEntityData, angles) == 28);
static_assert(offsetof(FireBulletsTempEntityData, weaponId) == 40);
static_assert(offsetof(FireBulletsTempEntityData, mode) == 44);
static_assert(offsetof(FireBulletsTempEntityData, seed) == 48);
static_assert(offsetof(FireBulletsTempEntityData, spread) == 52);

struct BfWriteApi {
    using WriteByteFn = void (*)(void*, int);
    using WriteStringFn = bool (*)(void*, const char*);

    WriteByteFn writeByte = nullptr;
    WriteStringFn writeString = nullptr;

    bool Resolve(const EngineServer021Client& engine) noexcept {
        if (!writeByte) {
            writeByte = reinterpret_cast<WriteByteFn>(dlsym(RTLD_DEFAULT, "_ZN8bf_write9WriteByteEi"));
            if (!writeByte) {
                writeByte = reinterpret_cast<WriteByteFn>(
                    engine.ResolveEngineSymbol("_ZN8bf_write9WriteByteEi"));
            }
        }
        if (!writeString) {
            writeString = reinterpret_cast<WriteStringFn>(dlsym(RTLD_DEFAULT, "_ZN8bf_write11WriteStringEPKc"));
            if (!writeString) {
                writeString = reinterpret_cast<WriteStringFn>(
                    engine.ResolveEngineSymbol("_ZN8bf_write11WriteStringEPKc"));
            }
        }
        return writeByte != nullptr && writeString != nullptr;
    }
};

BfWriteApi& EngineBfWriteApi() noexcept {
    static BfWriteApi api;
    return api;
}

void CopyTrace(const build4100::EngineGameTrace& source, PlayerHullTraceResult& out) noexcept {
    out.endPos = source.endPos;
    out.planeNormal = source.plane.normal;
    out.fraction = source.fraction;
    out.contents = source.contents;
    out.allSolid = source.allSolid;
    out.startSolid = source.startSolid;
}

bool EarlierTrace(const PlayerHullTraceResult& candidate,
                  const PlayerHullTraceResult& current) noexcept {
    if (candidate.startSolid != current.startSolid) return candidate.startSolid;
    if (candidate.allSolid != current.allSolid && candidate.startSolid) return candidate.allSolid;
    return candidate.fraction < current.fraction;
}

// Source build4100 SOLID_BBOX clipping reduces to a swept hull against the
// target's world-aligned bounds.  Expanding the target by the moving hull is
// the same Minkowski construction already used for player-vs-player clipping.
bool SweepHullAgainstAabb(const build4100::Vector& from, const build4100::Vector& to,
                          const build4100::Vector& movingMins,
                          const build4100::Vector& movingMaxs,
                          const build4100::Vector& targetOrigin,
                          const build4100::Vector& targetMins,
                          const build4100::Vector& targetMaxs,
                          PlayerHullTraceResult& trace) noexcept {
    constexpr float kSweepEpsilon = 1.0e-4f;
    const build4100::Vector expandedMins{
        targetOrigin.x + targetMins.x - movingMaxs.x,
        targetOrigin.y + targetMins.y - movingMaxs.y,
        targetOrigin.z + targetMins.z - movingMaxs.z,
    };
    const build4100::Vector expandedMaxs{
        targetOrigin.x + targetMaxs.x - movingMins.x,
        targetOrigin.y + targetMaxs.y - movingMins.y,
        targetOrigin.z + targetMaxs.z - movingMins.z,
    };
    const build4100::Vector delta{to.x - from.x, to.y - from.y, to.z - from.z};

    const auto strictInside = [&](const build4100::Vector& point) noexcept {
        return point.x > expandedMins.x + kSweepEpsilon &&
               point.x < expandedMaxs.x - kSweepEpsilon &&
               point.y > expandedMins.y + kSweepEpsilon &&
               point.y < expandedMaxs.y - kSweepEpsilon &&
               point.z > expandedMins.z + kSweepEpsilon &&
               point.z < expandedMaxs.z - kSweepEpsilon;
    };
    if (strictInside(from)) {
        trace = PlayerHullTraceResult{};
        trace.endPos = from;
        trace.fraction = 0.0f;
        trace.startSolid = true;
        trace.allSolid = strictInside(to);
        return true;
    }

    float enter = 0.0f;
    float leave = 1.0f;
    build4100::Vector enterNormal{};
    bool haveEnterNormal = false;
    for (int axis = 0; axis < 3; ++axis) {
        const float start = axis == 0 ? from.x : axis == 1 ? from.y : from.z;
        const float move = axis == 0 ? delta.x : axis == 1 ? delta.y : delta.z;
        const float mins = axis == 0 ? expandedMins.x : axis == 1 ? expandedMins.y : expandedMins.z;
        const float maxs = axis == 0 ? expandedMaxs.x : axis == 1 ? expandedMaxs.y : expandedMaxs.z;
        if (std::fabs(move) <= kSweepEpsilon) {
            if (start <= mins + kSweepEpsilon || start >= maxs - kSweepEpsilon) return false;
            continue;
        }

        float t1 = (mins - start) / move;
        float t2 = (maxs - start) / move;
        build4100::Vector normal{};
        if (t1 > t2) {
            std::swap(t1, t2);
            if (axis == 0) normal.x = 1.0f;
            else if (axis == 1) normal.y = 1.0f;
            else normal.z = 1.0f;
        } else {
            if (axis == 0) normal.x = -1.0f;
            else if (axis == 1) normal.y = -1.0f;
            else normal.z = -1.0f;
        }
        if (t1 > enter) {
            enter = t1;
            enterNormal = normal;
            haveEnterNormal = true;
        }
        leave = std::min(leave, t2);
        if (enter > leave) return false;
    }

    if (!haveEnterNormal || enter < 0.0f || enter > 1.0f || leave < 0.0f) return false;
    trace = PlayerHullTraceResult{};
    trace.fraction = enter;
    trace.endPos = build4100::Vector{
        from.x + delta.x * enter,
        from.y + delta.y * enter,
        from.z + delta.z * enter,
    };
    trace.planeNormal = enterNormal;
    return true;
}

build4100::HitGroup HitscanHitGroup(const build4100::Vector& origin,
                                     const build4100::Vector& mins,
                                     const build4100::Vector& maxs,
                                     const build4100::Vector& impact) noexcept {
    const float minsZ = mins.z;
    const float maxsZ = maxs.z > minsZ ? maxs.z : 72.0f;
    const float height = std::max(1.0f, maxsZ - minsZ);
    const float relative = (impact.z - (origin.z + minsZ)) / height;
    if (relative >= 0.78f) return build4100::HitGroup::Head;
    if (relative >= 0.58f) return build4100::HitGroup::Chest;
    if (relative >= 0.38f) return build4100::HitGroup::Stomach;
    return build4100::HitGroup::LeftLeg;
}

}  // namespace

ServerGameClients004Adapter& ServerGameClients004Instance() noexcept {
    static ServerGameClients004Adapter instance;
    return instance;
}

void ServerGameClients004Adapter::Bind(const EngineServer021Client* const engine,
                                       build4100::GlobalVarsBase* const globals,
                                       EngineEntityBridge* const entityBridge,
                                       const EngineTraceServer003Client* const engineTrace,
                                       void* const modelInfo, void* const physicsCollision,
                                       const GameEventManager002Client* const gameEvents,
                                       const VoiceServer001Client* const voiceServer) {
    engine_ = engine;
    globals_ = globals;
    entityBridge_ = entityBridge;
    engineTrace_ = engineTrace;
    modelInfo_ = modelInfo;
    physicsCollision_ = physicsCollision;
    gameEvents_ = gameEvents;
    voiceServer_ = voiceServer;
    const std::size_t maxClients = globals_ && globals_->maxClients > 0
        ? static_cast<std::size_t>(globals_->maxClients) : 0;
    (void)clients_.Configure(maxClients);
    (void)weaponRuntime_.Configure(maxClients);
    (void)lagHistory_.Configure(maxClients);
    clients_.BindMovementTrace(
        this,
        [](void* context, const build4100::Vector& start, const build4100::Vector& end,
           const build4100::Vector& mins, const build4100::Vector& maxs,
           const std::uint32_t mask, PlayerHullTraceResult& out) noexcept -> bool {
            auto* const adapter = static_cast<ServerGameClients004Adapter*>(context);
            return adapter && adapter->TraceMovementHull(start, end, mins, maxs, mask, out);
        },
        [](void* context, const build4100::Vector& point) noexcept -> std::int32_t {
            auto* const adapter = static_cast<ServerGameClients004Adapter*>(context);
            if (!adapter) return 0;
            if (adapter->waterProbeSyntheticContents_) {
                const build4100::Vector& o = adapter->waterProbeSyntheticOrigin_;
                if (std::fabs(point.x - o.x) <= 128.0f &&
                    std::fabs(point.y - o.y) <= 128.0f &&
                    point.z >= o.z - 16.0f && point.z <= o.z + 96.0f) {
                    return static_cast<std::int32_t>(build4100::kContentsWater);
                }
            }
            return adapter->engineTrace_ && adapter->engineTrace_->IsValid()
                ? adapter->engineTrace_->PointContentsWorld(point) : 0;
        });
    waterProbeSyntheticContents_ = false;
    waterProbeSyntheticOrigin_ = build4100::Vector{};
    postClientMessagesCalls_ = 0;
    lastValidatedUserName_.clear();
    lastValidatedNetworkId_.clear();
    edictBase_ = nullptr;
    edictCount_ = 0;
    visibilityProbeDone_.fill(false);
    gameplayCommandProbeDone_.fill(false);
    gameplayUsercmdProbeDone_.fill(false);
    gameplayMovementProbeDone_.fill(false);
    gameplayFrictionProbeDone_.fill(false);
    gameplayJumpProbeDone_.fill(false);
    gameplayJumpHoldProbeDone_.fill(false);
    gameplayAirAccelProbeDone_.fill(false);
    gameplayWeaponFireProbeDone_.fill(false);
    gameplayFireBulletsTempEntityProbeDone_.fill(false);
    gameplayWeaponReloadProbeDone_.fill(false);
    gameplayDamageProbeDone_.fill(false);
    deferredTeamSpawn_.fill(0);
    pendingFireBulletsTempEntities_.clear();
    combatProbeActive_ = false;
    combatProbeAttacker_ = 0;
    combatProbeTarget_ = 0;
    engineTraceWorldProbeDone_ = false;
    advancedMovementGeometryProbeDone_ = false;
    dynamicPlayerCollisionProbeDone_ = false;
    dynamicNonPlayerCollisionProbeDone_ = false;
    hostageCollisionProbeDone_ = false;
    hostageFollowingProbeDone_ = false;
    genericNpcProbeDone_ = false;
    waterMovementProbeDone_ = false;
    duckMovementProbeDone_ = false;
    ladderMovementProbeDone_ = false;
    mapName_.clear();
    gameState_.Configure();
    BindGameplayClientRegistry(&clients_);
    BindGameplayGameState(&gameState_);
    SetGameplayPlantedC4EntityIndex(-1);
    plantedC4Edict_ = nullptr;
    plantedC4EntityIndex_ = -1;
    plantedC4SpawnPending_ = false;
    plantedC4Active_ = false;
    plantedC4ReservationAttempts_ = 0;
    plantedC4PreBaselineProbeDone_ = false;
    plantedC4BaselineProbeDone_ = false;
}

void ServerGameClients004Adapter::Unbind() noexcept {
    ReleasePlantedC4Reservation();
    clients_.Reset();
    engine_ = nullptr;
    globals_ = nullptr;
    entityBridge_ = nullptr;
    engineTrace_ = nullptr;
    modelInfo_ = nullptr;
    physicsCollision_ = nullptr;
    gameEvents_ = nullptr;
    voiceServer_ = nullptr;
    clients_.BindMovementTrace(nullptr, nullptr);
    terroristSpawns_.clear();
    counterTerroristSpawns_.clear();
    mapMovementSolids_.clear();
    mapObjectiveZones_.clear();
    mapLadders_.clear();
    clients_.SetMovementLadders({});
    postClientMessagesCalls_ = 0;
    lastValidatedUserName_.clear();
    lastValidatedNetworkId_.clear();
    edictBase_ = nullptr;
    edictCount_ = 0;
    visibilityProbeDone_.fill(false);
    gameplayCommandProbeDone_.fill(false);
    gameplayUsercmdProbeDone_.fill(false);
    gameplayMovementProbeDone_.fill(false);
    gameplayFrictionProbeDone_.fill(false);
    gameplayJumpProbeDone_.fill(false);
    gameplayJumpHoldProbeDone_.fill(false);
    gameplayAirAccelProbeDone_.fill(false);
    gameplayWeaponFireProbeDone_.fill(false);
    gameplayFireBulletsTempEntityProbeDone_.fill(false);
    gameplayWeaponReloadProbeDone_.fill(false);
    gameplayDamageProbeDone_.fill(false);
    combatProbeActive_ = false;
    combatProbeAttacker_ = 0;
    combatProbeTarget_ = 0;
    weaponRuntime_.Reset();
    lagHistory_.Reset();
    deferredTeamSpawn_.fill(0);
    pendingFireBulletsTempEntities_.clear();
    engineTraceWorldProbeDone_ = false;
    advancedMovementGeometryProbeDone_ = false;
    dynamicPlayerCollisionProbeDone_ = false;
    dynamicNonPlayerCollisionProbeDone_ = false;
    hostageCollisionProbeDone_ = false;
    hostageFollowingProbeDone_ = false;
    genericNpcProbeDone_ = false;
    waterMovementProbeDone_ = false;
    duckMovementProbeDone_ = false;
    ladderMovementProbeDone_ = false;
    mapName_.clear();
    BindGameplayClientRegistry(nullptr);
    BindGameplayGameState(nullptr);
    SetGameplayPlantedC4EntityIndex(-1);
}

void ServerGameClients004Adapter::SetEngineEdictBase(build4100::Edict* const base, const int count) noexcept {
    edictBase_ = base;
    edictCount_ = count > 0 ? count : 0;
}

void ServerGameClients004Adapter::PrepareLevelNetworkEntities(const char* const phase) noexcept {
    // build4100's SV_ActivateServer calls IServerGameDLL::ServerActivate and then,
    // before returning to the frame loop, immediately calls SV_CreateBaseline.
    // SV_CreateBaseline only materializes an instancebaseline entry for a
    // ServerClass represented by a live edict at that exact point. Late-spawn
    // classes such as CCSPlayer may legally use the implicit zero/default class
    // baseline, but CPlantedC4 is a baseline-sensitive synthetic objective entity
    // whose stable reserved edict is intentionally present for the one-shot pass.
    // Keep one DONT_SEND shell alive before ServerActivate returns, then reuse
    // that same edict for every plant/defuse/explosion cycle.
    if (gameState_.MapObjective() == CsMapObjective::Bomb && !plantedC4Edict_ &&
        plantedC4ReservationAttempts_ < kPlantedC4PreClientReservationAttempts) {
        (void)ReservePlantedC4Edict();
    }

    if (gameState_.MapObjective() != CsMapObjective::Bomb || !plantedC4Edict_ ||
        plantedC4PreBaselineProbeDone_ || !phase ||
        std::strcmp(phase, "serveractivate_prebaseline") != 0) {
        return;
    }
    const build4100::ServerClass* const plantedClass =
        ServerClasses().FindByNetworkName("CPlantedC4");
    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "entity=%d classid=%d baseline=%d phase=%s flags=0x%08x mode=dontsend",
                  plantedC4EntityIndex_, plantedClass ? plantedClass->classId : -1,
                  plantedClass ? plantedClass->instanceBaselineIndex : -1,
                  phase ? phase : "unknown", plantedC4Edict_->stateFlags);
    EngineProbe("GAMEPLAY_PLANTED_C4_PREBASELINE_READY", detail);
    plantedC4PreBaselineProbeDone_ = true;
}

void ServerGameClients004Adapter::VerifyLevelNetworkEntityBaselines() noexcept {
    if (plantedC4BaselineProbeDone_ || gameState_.MapObjective() != CsMapObjective::Bomb) return;
    plantedC4BaselineProbeDone_ = true;

    const build4100::ServerClass* const plantedClass =
        ServerClasses().FindByNetworkName("CPlantedC4");
    const int baseline = plantedClass ? plantedClass->instanceBaselineIndex : -1;
    const bool ready = plantedC4Edict_ && plantedClass && baseline >= 0 && baseline != 0xffff;
    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "entity=%d classid=%d baseline=%d reserved=%d flags=0x%08x phase=first_gameframe",
                  plantedC4EntityIndex_, plantedClass ? plantedClass->classId : -1,
                  baseline, plantedC4Edict_ ? 1 : 0,
                  plantedC4Edict_ ? plantedC4Edict_->stateFlags : 0u);
    EngineProbe(ready ? "GAMEPLAY_PLANTED_C4_BASELINE_READY"
                      : "GAMEPLAY_PLANTED_C4_BASELINE_MISSING", detail);
}

void ServerGameClients004Adapter::SetMapPlayerSpawns(
    std::vector<build4100::Vector> terroristSpawns,
    std::vector<build4100::Vector> counterTerroristSpawns) {
    terroristSpawns_ = std::move(terroristSpawns);
    counterTerroristSpawns_ = std::move(counterTerroristSpawns);
}

void ServerGameClients004Adapter::SetMapMovementSolids(std::vector<MapMovementSolid> solids) {
    mapMovementSolids_ = std::move(solids);
    std::size_t hostageCount = 0;
    for (const auto& solid : mapMovementSolids_) {
        if (solid.kind == MapMovementSolid::Kind::HostageBBox) ++hostageCount;
    }
    gameState_.ConfigureMapObjective(hostageCount ? CsMapObjective::Hostage : CsMapObjective::Bomb,
                                    hostageCount, 100);

    // LevelInit is the first engine-facing point where the reconstructed core
    // knows the map objective. Configure() leaves CsGameState in Waiting, so
    // without an initial round transition objective actions such as hostage USE
    // can never enter their Live-only path. Start the first round here and end
    // the synthetic freeze immediately; the engine owns the real freeze timing
    // while this state object owns gameplay eligibility for the reconstructed
    // callbacks. Subsequent LevelInit calls reset the round deterministically.
    const float roundStart = globals_ ? globals_->curtime : 0.0f;
    gameState_.RestartRound(roundStart);
    gameState_.EndFreezePeriod();

    dynamicNonPlayerCollisionProbeDone_ = false;
    hostageCollisionProbeDone_ = false;
    hostageFollowingProbeDone_ = false;
    genericNpcProbeDone_ = false;
}

void ServerGameClients004Adapter::SetMapObjectiveZones(std::vector<MapObjectiveZone> zones) {
    mapObjectiveZones_ = std::move(zones);
    std::size_t bomb = 0, rescue = 0;
    for (const auto& zone : mapObjectiveZones_) {
        bomb += zone.kind == MapObjectiveZone::Kind::BombTarget ? 1u : 0u;
        rescue += zone.kind == MapObjectiveZone::Kind::HostageRescue ? 1u : 0u;
    }
    char detail[128];
    std::snprintf(detail, sizeof(detail), "bomb=%zu rescue=%zu total=%zu", bomb, rescue, mapObjectiveZones_.size());
    EngineProbe("MAP_OBJECTIVE_ZONES", detail);
}

void ServerGameClients004Adapter::SetMapLadders(std::vector<MovementLadderVolume> ladders) {
    mapLadders_ = std::move(ladders);
    clients_.SetMovementLadders(mapLadders_);
    ladderMovementProbeDone_ = false;
}

void ServerGameClients004Adapter::SetMapName(const char* const mapName) {
    mapName_ = mapName ? mapName : "";
    advancedMovementGeometryProbeDone_ = false;
    dynamicPlayerCollisionProbeDone_ = false;
    dynamicNonPlayerCollisionProbeDone_ = false;
    hostageCollisionProbeDone_ = false;
    hostageFollowingProbeDone_ = false;
    genericNpcProbeDone_ = false;
    waterMovementProbeDone_ = false;
    duckMovementProbeDone_ = false;
    ladderMovementProbeDone_ = false;
}

bool ServerGameClients004Adapter::ResolveStudioCollision(MapMovementSolid& solid) noexcept {
    if (solid.kind != MapMovementSolid::Kind::StudioPhysics) return false;
    if (solid.resolveAttempted) return solid.vcollide != nullptr;
    solid.resolveAttempted = true;
    if (!modelInfo_ || solid.model.empty()) return false;

    using FindOrLoadModelFn = const void* (*)(void*, const char*);
    using GetVCollideFn = const void* (*)(void*, const void*);
    const FindOrLoadModelFn findOrLoad = detail::VTableSlot<FindOrLoadModelFn>(modelInfo_, 39);
    const GetVCollideFn getVCollide = detail::VTableSlot<GetVCollideFn>(modelInfo_, 5);
    if (!findOrLoad || !getVCollide) return false;
    solid.modelHandle = findOrLoad(modelInfo_, solid.model.c_str());
    if (!solid.modelHandle) return false;
    solid.vcollide = getVCollide(modelInfo_, solid.modelHandle);
    if (!solid.vcollide) return false;
    const auto* const collide = static_cast<const VCollide4100*>(solid.vcollide);
    return collide->solids && collide->SolidCount() > 0 && collide->SolidCount() <= 64;
}

bool ServerGameClients004Adapter::ResolveActorBounds(MapMovementSolid& solid) noexcept {
    if (solid.kind != MapMovementSolid::Kind::HostageBBox &&
        solid.kind != MapMovementSolid::Kind::NpcBBox) return false;
    if (solid.boundsResolved) return true;
    if (solid.resolveAttempted) return false;
    solid.resolveAttempted = true;
    if (!modelInfo_ || solid.model.empty()) return false;

    // Original build4100 CBaseEntity::SetModel -> UTIL_SetModel calls
    // VModelInfoServer001::GetModelBounds at address-point slot 7 and then
    // CBaseEntity::SetCollisionBounds. CHostage::Spawn uses SOLID_BBOX (2),
    // FSOLID_NOT_STANDABLE, MOVETYPE_STEP and COLLISION_GROUP_PLAYER (5).
    using FindOrLoadModelFn = const void* (*)(void*, const char*);
    using GetModelBoundsFn = void (*)(void*, const void*, build4100::Vector*, build4100::Vector*);
    const FindOrLoadModelFn findOrLoad = detail::VTableSlot<FindOrLoadModelFn>(modelInfo_, 39);
    const GetModelBoundsFn getModelBounds = detail::VTableSlot<GetModelBoundsFn>(modelInfo_, 7);
    if (!findOrLoad || !getModelBounds) return false;
    solid.modelHandle = findOrLoad(modelInfo_, solid.model.c_str());
    if (!solid.modelHandle) return false;
    build4100::Vector mins{};
    build4100::Vector maxs{};
    getModelBounds(modelInfo_, solid.modelHandle, &mins, &maxs);
    if (!(maxs.x > mins.x) || !(maxs.y > mins.y) || !(maxs.z > mins.z)) return false;
    solid.bboxMins = mins;
    solid.bboxMaxs = maxs;
    solid.boundsResolved = true;
    return true;
}

bool ServerGameClients004Adapter::TraceMovementHull(
    const build4100::Vector& start, const build4100::Vector& end,
    const build4100::Vector& mins, const build4100::Vector& maxs,
    const std::uint32_t mask, PlayerHullTraceResult& out) noexcept {
    PlayerHullTraceResult best{};
    best.endPos = end;
    best.fraction = 1.0f;
    bool traced = false;

    if (engineTrace_ && engineTrace_->IsValid()) {
        build4100::EngineGameTrace world{};
        if (engineTrace_->TraceWorldHull(start, end, mins, maxs, mask, world)) {
            CopyTrace(world, best);
            best.hitDynamicSolidIndex = 0;
            traced = true;
        }
    }

    if (mapMovementSolids_.empty()) {
        out = best;
        return traced;
    }

    build4100::EngineTraceRay ray{};
    ray.Init(start, end, mins, maxs);
    using TraceBoxFn = void (*)(void*, const build4100::EngineTraceRay*, const void*,
                                const build4100::Vector*, const build4100::QAngle*,
                                build4100::EngineGameTrace*);
    const TraceBoxFn traceBox = detail::VTableSlot<TraceBoxFn>(physicsCollision_, 22);

    for (std::size_t i = 0; i < mapMovementSolids_.size(); ++i) {
        MapMovementSolid& solid = mapMovementSolids_[i];
        if (solid.kind == MapMovementSolid::Kind::StudioPhysics) {
            if ((mask & build4100::kContentsMoveable) == 0u || !traceBox ||
                !ResolveStudioCollision(solid)) {
                continue;
            }
            const auto* const collide = static_cast<const VCollide4100*>(solid.vcollide);
            const std::size_t count = collide->SolidCount();
            for (std::size_t part = 0; part < count; ++part) {
                const void* const phys = collide->solids[part];
                if (!phys) continue;
                build4100::EngineGameTrace dynamic{};
                dynamic.fraction = 1.0f;
                traceBox(physicsCollision_, &ray, phys, &solid.origin, &solid.angles, &dynamic);
                if (!dynamic.startSolid && !dynamic.allSolid && dynamic.fraction >= 1.0f) continue;
                PlayerHullTraceResult candidate{};
                CopyTrace(dynamic, candidate);
                candidate.contents = static_cast<std::int32_t>(build4100::kContentsMoveable);
                candidate.hitDynamicSolidIndex = i + 1;
                if (!traced || EarlierTrace(candidate, best)) best = candidate;
                traced = true;
            }
        } else if (solid.kind == MapMovementSolid::Kind::HostageBBox ||
                   solid.kind == MapMovementSolid::Kind::NpcBBox) {
            // CHostage and generic CAI/NPC actors use SOLID_BBOX-like movement
            // collision here. Hostages are collision group PLAYER (5); generic
            // NPCs are exposed through CONTENTS_MONSTER for MASK_PLAYERSOLID.
            // build4100 TryPlayerMove filter is group PLAYER_MOVEMENT (8), so
            // MASK_PLAYERSOLID reaches hostages through CONTENTS_MONSTER.
            if ((mask & build4100::kContentsMonster) == 0u || !ResolveActorBounds(solid)) {
                continue;
            }
            PlayerHullTraceResult candidate{};
            if (!SweepHullAgainstAabb(start, end, mins, maxs, solid.origin,
                                      solid.bboxMins, solid.bboxMaxs, candidate)) {
                continue;
            }
            candidate.contents = static_cast<std::int32_t>(build4100::kContentsMonster);
            candidate.hitDynamicSolidIndex = i + 1;
            if (!traced || EarlierTrace(candidate, best)) best = candidate;
            traced = true;
        } else if (solid.kind == MapMovementSolid::Kind::InlineBrush && engineTrace_) {
            build4100::EngineGameTrace dynamic{};
            if (!engineTrace_->TraceInlineModelHull(solid.model.c_str(), solid.origin, solid.angles,
                                                     start, end, mins, maxs, mask, dynamic)) {
                continue;
            }
            if (!dynamic.startSolid && !dynamic.allSolid && dynamic.fraction >= 1.0f) continue;
            PlayerHullTraceResult candidate{};
            CopyTrace(dynamic, candidate);
            candidate.hitDynamicSolidIndex = i + 1;
            if (!traced || EarlierTrace(candidate, best)) best = candidate;
            traced = true;
        }
    }

    out = best;
    return traced;
}

bool ServerGameClients004Adapter::AdvanceDynamicActor(MapMovementSolid& solid,
                                                       const float frameTime) noexcept {
    if ((solid.kind != MapMovementSolid::Kind::HostageBBox &&
         solid.kind != MapMovementSolid::Kind::NpcBBox) ||
        solid.followClientIndex == 0 || frameTime <= 0.0f) {
        return false;
    }
    ClientRecord* const leader = clients_.Get(solid.followClientIndex);
    if (!leader || leader->state == ClientLifecycleState::Empty || leader->isDead ||
        !build4100::IsPlayableTeam(leader->teamIndex) || !ResolveActorBounds(solid)) {
        solid.followClientIndex = 0;
        return false;
    }

    constexpr float kPi = 3.14159265358979323846f;
    const float yaw = leader->absAngles.y * (kPi / 180.0f);
    const float followDistance = solid.kind == MapMovementSolid::Kind::HostageBBox ? 52.0f : 64.0f;
    const build4100::Vector desired{
        leader->absOrigin.x - std::cos(yaw) * followDistance,
        leader->absOrigin.y - std::sin(yaw) * followDistance,
        leader->absOrigin.z - solid.bboxMins.z + 0.03125f,
    };
    const build4100::Vector delta{
        desired.x - solid.origin.x,
        desired.y - solid.origin.y,
        desired.z - solid.origin.z,
    };
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (distance <= 8.0f) return false;

    const float speed = solid.kind == MapMovementSolid::Kind::HostageBBox ? 120.0f : 100.0f;
    const float step = std::min(distance, speed * frameTime);
    const float scale = step / distance;
    const build4100::Vector candidate{
        solid.origin.x + delta.x * scale,
        solid.origin.y + delta.y * scale,
        solid.origin.z + delta.z * scale,
    };

    build4100::Vector moved = candidate;
    if (engineTrace_ && engineTrace_->IsValid()) {
        build4100::EngineGameTrace world{};
        if (!engineTrace_->TraceWorldHull(solid.origin, candidate, solid.bboxMins, solid.bboxMaxs,
                                           build4100::kMaskPlayerSolidBrushOnly, world) ||
            world.startSolid || world.allSolid) {
            return false;
        }
        moved = world.endPos;
    }
    const float travel = std::sqrt((moved.x - solid.origin.x) * (moved.x - solid.origin.x) +
                                   (moved.y - solid.origin.y) * (moved.y - solid.origin.y) +
                                   (moved.z - solid.origin.z) * (moved.z - solid.origin.z));
    if (travel <= 0.001f) return false;
    solid.origin = moved;
    return true;
}

void ServerGameClients004Adapter::UpdateDynamicNonPlayerOrigins() noexcept {
    const float frameTime = globals_ && globals_->interval_per_tick > 0.0f
        ? globals_->interval_per_tick : 0.015f;
    for (auto& solid : mapMovementSolids_) {
        if (solid.followClientIndex == 0) continue;
        (void)AdvanceDynamicActor(solid, frameTime);
    }
}

bool ServerGameClients004Adapter::ResolveObjectiveZoneBounds(MapObjectiveZone& zone) noexcept {
    if (zone.boundsResolved) return true;
    if (zone.resolveAttempted) return false;
    zone.resolveAttempted = true;
    if (modelInfo_ && !zone.model.empty() && zone.model.front() == '*') {
        using FindOrLoadModelFn = const void* (*)(void*, const char*);
        using GetModelBoundsFn = void (*)(void*, const void*, build4100::Vector*, build4100::Vector*);
        const FindOrLoadModelFn findOrLoad = detail::VTableSlot<FindOrLoadModelFn>(modelInfo_, 39);
        const GetModelBoundsFn getBounds = detail::VTableSlot<GetModelBoundsFn>(modelInfo_, 7);
        if (findOrLoad && getBounds) {
            zone.modelHandle = findOrLoad(modelInfo_, zone.model.c_str());
            if (zone.modelHandle) {
                getBounds(modelInfo_, zone.modelHandle, &zone.mins, &zone.maxs);
                zone.boundsResolved = true;
                return true;
            }
        }
    }
    // Point objective fallback. info_bomb_target-style entities are rare in
    // stock CSS maps, but a bounded volume keeps the reconstruction useful on
    // custom BSPs that do not use inline brush targets.
    zone.mins = build4100::Vector{-128.0f, -128.0f, -64.0f};
    zone.maxs = build4100::Vector{128.0f, 128.0f, 96.0f};
    zone.boundsResolved = true;
    return true;
}

int ServerGameClients004Adapter::FindObjectiveZone(const build4100::Vector& point,
                                                    const MapObjectiveZone::Kind kind) noexcept {
    for (std::size_t i = 0; i < mapObjectiveZones_.size(); ++i) {
        auto& zone = mapObjectiveZones_[i];
        if (zone.kind != kind || !ResolveObjectiveZoneBounds(zone)) continue;
        // Inline brush bounds returned by build4100 modelinfo are normally in
        // map space; point objectives use local bounds around origin. Accept
        // both interpretations for custom maps with translated brush models.
        const auto inside = [&](const build4100::Vector& mins, const build4100::Vector& maxs) {
            return point.x >= mins.x && point.x <= maxs.x &&
                   point.y >= mins.y && point.y <= maxs.y &&
                   point.z >= mins.z && point.z <= maxs.z;
        };
        if (inside(zone.mins, zone.maxs)) return static_cast<int>(i);
        const build4100::Vector translatedMin{zone.mins.x + zone.origin.x, zone.mins.y + zone.origin.y,
                                              zone.mins.z + zone.origin.z};
        const build4100::Vector translatedMax{zone.maxs.x + zone.origin.x, zone.maxs.y + zone.origin.y,
                                              zone.maxs.z + zone.origin.z};
        if (inside(translatedMin, translatedMax)) return static_cast<int>(i);
    }
    return -1;
}

build4100::Vector ServerGameClients004Adapter::ObjectiveZoneCenter(const int zoneIndex) noexcept {
    if (zoneIndex < 0 || static_cast<std::size_t>(zoneIndex) >= mapObjectiveZones_.size()) return {};
    auto& zone = mapObjectiveZones_[static_cast<std::size_t>(zoneIndex)];
    if (!ResolveObjectiveZoneBounds(zone)) return zone.origin;
    build4100::Vector center{(zone.mins.x + zone.maxs.x) * 0.5f,
                             (zone.mins.y + zone.maxs.y) * 0.5f,
                             (zone.mins.z + zone.maxs.z) * 0.5f};
    // Point/fallback bounds are local; brush bounds in stock maps are already
    // world-space. A non-zero origin on a non-brush target therefore translates.
    if (zone.model.empty() || zone.model.front() != '*') {
        center.x += zone.origin.x; center.y += zone.origin.y; center.z += zone.origin.z;
    }
    return center;
}

bool ServerGameClients004Adapter::FireObjectiveEvent(const char* const name, const int userid,
                                                      const int site, const int hostage,
                                                      const int winner, const int reason,
                                                      const bool hasKit) noexcept {
    if (!name || !gameEvents_ || !gameEvents_->IsValid()) return false;
    const GameEventClient event = gameEvents_->CreateEvent(name, true);
    if (!event.IsValid()) return false;
    if (userid > 0) event.SetInt("userid", userid);
    if (site >= 0) event.SetInt("site", site);
    if (hostage >= 0) event.SetInt("hostage", hostage);
    if (winner >= 0) event.SetInt("winner", winner);
    if (reason >= 0) event.SetInt("reason", reason);
    if (std::strcmp(name, "bomb_begindefuse") == 0) event.SetBool("haskit", hasKit);
    if (std::strcmp(name, "bomb_beep") == 0 && plantedC4EntityIndex_ > 0)
        event.SetInt("entindex", plantedC4EntityIndex_);
    if (std::strcmp(name, "round_start") == 0) {
        event.SetInt("timelimit", static_cast<int>(gameState_.RoundTimeSeconds()));
        event.SetInt("fraglimit", 0);
        event.SetString("objective", gameState_.MapObjective() == CsMapObjective::Hostage ? "hostage" : "bomb");
    }
    if (std::strcmp(name, "round_end") == 0) event.SetString("message", "");
    const bool fired = gameEvents_->FireEvent(event, false);
    if (!fired) gameEvents_->FreeEvent(event);
    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "name=%s userid=%d site=%d hostage=%d winner=%d reason=%d haskit=%d fired=%d",
                  name, userid, site, hostage, winner, reason, hasKit ? 1 : 0, fired ? 1 : 0);
    EngineProbe(fired ? "GAMEPLAY_OBJECTIVE_EVENT" : "GAMEPLAY_OBJECTIVE_EVENT_FAIL", detail);
    return fired;
}

bool ServerGameClients004Adapter::FireWeaponGameEvent(const char* const name,
                                                        const std::size_t clientIndex,
                                                        const build4100::CSWeaponID weapon,
                                                        const bool includeWeapon) noexcept {
    if (!name || !gameEvents_ || !gameEvents_->IsValid() || !engine_) return false;
    const ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict) return false;
    const int userid = engine_->GetPlayerUserId(client->edict);
    const GameEventClient event = gameEvents_->CreateEvent(name, true);
    if (!event.IsValid()) return false;
    event.SetInt("userid", userid);
    if (includeWeapon) {
        const char* const alias = build4100::WeaponIDToAlias(weapon);
        event.SetString("weapon", alias ? alias : "");
    }
    const bool fired = gameEvents_->FireEvent(event, false);
    if (!fired) gameEvents_->FreeEvent(event);
    return fired;
}

bool ServerGameClients004Adapter::FirePlayerHurtEvent(
    const std::size_t attackerIndex, const std::size_t victimIndex,
    const build4100::CSWeaponID weapon) noexcept {
    if (!gameEvents_ || !gameEvents_->IsValid() || !engine_) return false;
    const ClientRecord* const attacker = clients_.Get(attackerIndex);
    const ClientRecord* const victim = clients_.Get(victimIndex);
    if (!attacker || !attacker->edict || !victim || !victim->edict) return false;
    const GameEventClient event = gameEvents_->CreateEvent("player_hurt", true);
    if (!event.IsValid()) return false;
    event.SetInt("userid", engine_->GetPlayerUserId(victim->edict));
    event.SetInt("attacker", engine_->GetPlayerUserId(attacker->edict));
    event.SetInt("health", victim->health);
    event.SetInt("armor", victim->armorValue);
    const char* const alias = build4100::WeaponIDToAlias(weapon);
    event.SetString("weapon", alias ? alias : "");
    const bool fired = gameEvents_->FireEvent(event, false);
    if (!fired) gameEvents_->FreeEvent(event);
    return fired;
}

bool ServerGameClients004Adapter::FirePlayerDeathEvent(
    const std::size_t attackerIndex, const std::size_t victimIndex,
    const build4100::CSWeaponID weapon) noexcept {
    if (!gameEvents_ || !gameEvents_->IsValid() || !engine_) return false;
    const ClientRecord* const attacker = clients_.Get(attackerIndex);
    const ClientRecord* const victim = clients_.Get(victimIndex);
    if (!attacker || !attacker->edict || !victim || !victim->edict) return false;
    const GameEventClient event = gameEvents_->CreateEvent("player_death", true);
    if (!event.IsValid()) return false;
    event.SetInt("userid", engine_->GetPlayerUserId(victim->edict));
    event.SetInt("attacker", engine_->GetPlayerUserId(attacker->edict));
    const char* const alias = build4100::WeaponIDToAlias(weapon);
    event.SetString("weapon", alias ? alias : "");
    event.SetBool("headshot", victim->lastDeathWasHeadshot);
    const bool fired = gameEvents_->FireEvent(event, false);
    if (!fired) gameEvents_->FreeEvent(event);
    return fired;
}

bool ServerGameClients004Adapter::ApplyShotgunAttack(
    const std::size_t attackerIndex, const build4100::CSWeaponID weapon,
    const build4100::UserCmd& command) noexcept {
    if (weapon != build4100::CSWeaponID::M3 && weapon != build4100::CSWeaponID::XM1014) {
        return false;
    }
    ClientRecord* const attacker = clients_.Get(attackerIndex);
    if (!attacker || !attacker->edict || attacker->isDead || attacker->isObserver ||
        !build4100::IsPlayableTeam(attacker->teamIndex)) return false;

    const WeaponRuntimeProfile& profile = WeaponProfile(weapon);
    const int pelletCount = std::clamp(profile.bulletsPerShot, 1, 16);
    const PlayerWeaponRuntime* const runtime = weaponRuntime_.State(attackerIndex);
    if (!runtime || !runtime->classicBallistics) return false;

    const float eyeHeight = attacker->movementDucked ? 46.0f : 64.0f;
    const build4100::Vector eye{attacker->absOrigin.x, attacker->absOrigin.y,
                                attacker->absOrigin.z + eyeHeight};
    const float speed2D = std::sqrt(attacker->absVelocity.x * attacker->absVelocity.x +
                                    attacker->absVelocity.y * attacker->absVelocity.y);
    const WeaponMovementState movement{
        attacker->movementOnGround, attacker->movementDucked, runtime->silenced, speed2D,
        runtime->zoomLevel != 0, runtime->burstMode};

    const std::int32_t currentTick = globals_ ? globals_->tickcount : command.tickCount;
    const float interval = globals_ && globals_->interval_per_tick > 0.0f
        ? globals_->interval_per_tick : 0.015f;
    const std::int32_t maxRewindTicks = std::max(1, static_cast<std::int32_t>(std::ceil(0.2f / interval)));

    struct PelletImpact {
        std::size_t victimIndex = 0;
        std::size_t pelletIndex = 0;
        float fraction = 1.0f;
        build4100::Vector impact{};
        build4100::Vector victimOrigin{};
        build4100::Vector victimMins{};
        build4100::Vector victimMaxs{};
        build4100::HitGroup hitGroup = build4100::HitGroup::Generic;
        float distance = 0.0f;
        float ballisticDamage = 0.0f;
        WeaponShotSolution shot{};
        std::int32_t historyTick = 0;
        bool lagComp = false;
        int healthBefore = 0;
        int armorBefore = 0;
    };
    std::array<PelletImpact, 16> impacts{};
    std::size_t impactCount = 0;

    for (int pellet = 0; pellet < pelletCount; ++pellet) {
        const WeaponShotSolution shot = SolveBuild4100WeaponPellet(
            weapon, profile, attacker->absAngles, movement, runtime->shotAccuracy,
            runtime->punchAngle, command.randomSeed, static_cast<std::size_t>(pellet),
            runtime->shotSpreadOverride);
        const build4100::Vector end{eye.x + shot.direction.x * shot.range,
                                    eye.y + shot.direction.y * shot.range,
                                    eye.z + shot.direction.z * shot.range};

        float blockingFraction = 1.0f;
        if (engineTrace_ && engineTrace_->IsValid()) {
            build4100::EngineGameTrace world{};
            const build4100::Vector zero{};
            if (engineTrace_->TraceWorldHull(eye, end, zero, zero,
                                             build4100::kMaskPlayerSolidBrushOnly, world) &&
                (world.startSolid || world.allSolid || world.fraction < blockingFraction)) {
                blockingFraction = std::clamp(world.fraction, 0.0f, 1.0f);
            }
        }

        std::size_t victimIndex = 0;
        float victimFraction = blockingFraction;
        build4100::Vector impact{};
        build4100::Vector victimTraceOrigin{};
        build4100::Vector victimTraceMins{};
        build4100::Vector victimTraceMaxs{};
        std::int32_t selectedHistoryTick = currentTick;
        bool usedLagCompensation = false;

        for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
            if (i == attackerIndex) continue;
            ClientRecord* const candidate = clients_.Get(i);
            if (!candidate || !candidate->edict || candidate->state != ClientLifecycleState::Active ||
                candidate->isDead || candidate->isObserver ||
                !build4100::IsPlayableTeam(candidate->teamIndex)) continue;

            build4100::Vector traceOrigin = candidate->absOrigin;
            build4100::Vector mins = candidate->playerMaxs.z > candidate->playerMins.z
                ? candidate->playerMins : build4100::Vector{-16.0f, -16.0f, 0.0f};
            build4100::Vector maxs = candidate->playerMaxs.z > candidate->playerMins.z
                ? candidate->playerMaxs : build4100::Vector{16.0f, 16.0f, 72.0f};
            std::int32_t historyTick = currentTick;
            bool historyUsed = false;
            LagCompensationRecord history{};
            if (lagHistory_.Select(i, command.tickCount, currentTick, maxRewindTicks, history) && history.alive) {
                traceOrigin = history.origin;
                if (history.maxs.z > history.mins.z) {
                    mins = history.mins;
                    maxs = history.maxs;
                }
                historyTick = history.tick;
                historyUsed = history.tick != currentTick;
            }

            PlayerHullTraceResult hit{};
            const build4100::Vector zero{};
            if (!SweepHullAgainstAabb(eye, end, zero, zero, traceOrigin, mins, maxs, hit)) continue;
            if (hit.startSolid || hit.fraction < 0.0f || hit.fraction >= victimFraction) continue;
            victimIndex = i;
            victimFraction = hit.fraction;
            impact = hit.endPos;
            victimTraceOrigin = traceOrigin;
            victimTraceMins = mins;
            victimTraceMaxs = maxs;
            selectedHistoryTick = historyTick;
            usedLagCompensation = historyUsed;
        }
        if (victimIndex == 0 || impactCount >= impacts.size()) continue;

        ClientRecord* const victim = clients_.Get(victimIndex);
        if (!victim) continue;
        PelletImpact& out = impacts[impactCount++];
        out.victimIndex = victimIndex;
        out.pelletIndex = static_cast<std::size_t>(pellet);
        out.fraction = victimFraction;
        out.impact = impact;
        out.victimOrigin = victimTraceOrigin;
        out.victimMins = victimTraceMins;
        out.victimMaxs = victimTraceMaxs;
        out.hitGroup = HitscanHitGroup(victimTraceOrigin, victimTraceMins, victimTraceMaxs, impact);
        const float dx = impact.x - eye.x;
        const float dy = impact.y - eye.y;
        const float dz = impact.z - eye.z;
        out.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        out.ballisticDamage = BallisticDamageAtDistance(profile, out.distance);
        out.shot = shot;
        out.historyTick = selectedHistoryTick;
        out.lagComp = usedLagCompensation;
        out.healthBefore = victim->health;
        out.armorBefore = victim->armorValue;
    }
    if (impactCount == 0) return false;

    std::array<bool, 65> damaged{};
    std::array<bool, 65> killed{};
    std::size_t appliedPellets = 0;
    float rawDamageTotal = 0.0f;
    for (std::size_t i = 0; i < impactCount; ++i) {
        const PelletImpact& hit = impacts[i];
        rawDamageTotal += hit.ballisticDamage;
        const DamageApplyResult damage = gameState_.DamagePlayer(
            hit.victimIndex, attackerIndex, hit.ballisticDamage, hit.hitGroup,
            profile.armorRatio, false);
        if (damage != DamageApplyResult::Applied && damage != DamageApplyResult::Killed) continue;
        ++appliedPellets;
        if (hit.victimIndex < damaged.size()) {
            damaged[hit.victimIndex] = true;
            if (damage == DamageApplyResult::Killed) killed[hit.victimIndex] = true;
        }
    }
    if (appliedPellets == 0) return false;

    std::size_t uniqueVictims = 0;
    for (std::size_t i = 1; i <= clients_.MaxClients() && i < damaged.size(); ++i) {
        if (!damaged[i]) continue;
        ++uniqueVictims;
        (void)FirePlayerHurtEvent(attackerIndex, i, weapon);
        if (killed[i]) {
            (void)FirePlayerDeathEvent(attackerIndex, i, weapon);
            const float now = globals_ ? globals_->curtime : 0.0f;
            (void)gameState_.CheckWinConditions(now);
        }
        ClientRecord* const victim = clients_.Get(i);
        if (victim && victim->edict) {
            victim->edict->stateFlags |= build4100::kEdictChanged;
            if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(static_cast<int>(i));
        }
    }
    ClientRecord* const shooter = clients_.Get(attackerIndex);
    if (shooter && shooter->edict) {
        shooter->edict->stateFlags |= build4100::kEdictChanged;
        if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(static_cast<int>(attackerIndex));
    }

    if (attackerIndex < gameplayDamageProbeDone_.size() && !gameplayDamageProbeDone_[attackerIndex]) {
        gameplayDamageProbeDone_[attackerIndex] = true;
        const PelletImpact& first = impacts[0];
        const ClientRecord* const firstVictim = clients_.Get(first.victimIndex);
        const char* const alias = build4100::WeaponIDToAlias(weapon);
        char detail[768];
        std::snprintf(detail, sizeof(detail),
                      "attacker=%zu weapon=%s pellets=%d traced_hits=%zu applied_hits=%zu unique_victims=%zu first_victim=%zu first_hitgroup=%d health_before=%d health_after=%d armor_before=%d armor_after=%d raw_damage_total=%.3f spread=%.5f first_spread_xy=%.5f,%.5f recoil=%.3f,%.3f first_distance=%.2f cmd_tick=%d history_tick=%d rewind_ticks=%d lagcomp=%d",
                      attackerIndex, alias ? alias : "none", pelletCount, impactCount, appliedPellets,
                      uniqueVictims, first.victimIndex, static_cast<int>(first.hitGroup),
                      first.healthBefore, firstVictim ? firstVictim->health : -1,
                      first.armorBefore, firstVictim ? firstVictim->armorValue : -1,
                      rawDamageTotal, first.shot.spread, first.shot.spreadX, first.shot.spreadY,
                      first.shot.recoilPitch, first.shot.recoilYaw, first.distance, command.tickCount,
                      first.historyTick, std::max(0, currentTick - first.historyTick), first.lagComp ? 1 : 0);
        EngineProbe("GAMEPLAY_SHOTGUN_DAMAGE", detail);
    }
    return true;
}

bool ServerGameClients004Adapter::ApplyHitscanAttack(
    const std::size_t attackerIndex, const build4100::CSWeaponID weapon,
    const build4100::UserCmd& command) noexcept {
    if (weapon == build4100::CSWeaponID::M3 || weapon == build4100::CSWeaponID::XM1014) {
        return ApplyShotgunAttack(attackerIndex, weapon, command);
    }
    ClientRecord* const attacker = clients_.Get(attackerIndex);
    if (!attacker || !attacker->edict || attacker->isDead || attacker->isObserver ||
        !build4100::IsPlayableTeam(attacker->teamIndex)) return false;

    const float eyeHeight = attacker->movementDucked ? 46.0f : 64.0f;
    const build4100::Vector eye{attacker->absOrigin.x, attacker->absOrigin.y,
                                attacker->absOrigin.z + eyeHeight};
    const WeaponRuntimeProfile& profile = WeaponProfile(weapon);
    const PlayerWeaponRuntime* const runtime = weaponRuntime_.State(attackerIndex);
    const float speed2D = std::sqrt(attacker->absVelocity.x * attacker->absVelocity.x +
                                    attacker->absVelocity.y * attacker->absVelocity.y);
    const WeaponMovementState movement{
        attacker->movementOnGround, attacker->movementDucked, runtime ? runtime->silenced : false, speed2D,
        runtime ? runtime->zoomLevel != 0 : false, runtime ? runtime->burstMode : false};
    const WeaponShotSolution shot = runtime && runtime->classicBallistics
        ? SolveBuild4100WeaponShot(
              weapon, profile, attacker->absAngles, movement, runtime->shotAccuracy,
              runtime->punchAngle, command.randomSeed, command.commandNumber, runtime->shotsFired,
              runtime->shotSpreadOverride)
        : SolveWeaponShot(
              profile, attacker->absAngles, runtime ? runtime->accuracyPenalty : 0.0f,
              runtime ? runtime->recoilIndex : 0.0f, command.randomSeed, command.commandNumber,
              runtime ? runtime->shotsFired : 0u);
    const build4100::Vector end{eye.x + shot.direction.x * shot.range,
                                eye.y + shot.direction.y * shot.range,
                                eye.z + shot.direction.z * shot.range};

    float blockingFraction = 1.0f;
    if (engineTrace_ && engineTrace_->IsValid()) {
        build4100::EngineGameTrace world{};
        const build4100::Vector zero{};
        if (engineTrace_->TraceWorldHull(eye, end, zero, zero,
                                         build4100::kMaskPlayerSolidBrushOnly, world) &&
            (world.startSolid || world.allSolid || world.fraction < blockingFraction)) {
            blockingFraction = std::clamp(world.fraction, 0.0f, 1.0f);
        }
    }

    const std::int32_t currentTick = globals_ ? globals_->tickcount : command.tickCount;
    const float interval = globals_ && globals_->interval_per_tick > 0.0f
        ? globals_->interval_per_tick : 0.015f;
    const std::int32_t maxRewindTicks = std::max(1, static_cast<std::int32_t>(std::ceil(0.2f / interval)));

    std::size_t victimIndex = 0;
    float victimFraction = blockingFraction;
    build4100::Vector impact{};
    build4100::Vector victimTraceOrigin{};
    build4100::Vector victimTraceMins{};
    build4100::Vector victimTraceMaxs{};
    std::int32_t selectedHistoryTick = currentTick;
    bool usedLagCompensation = false;

    for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
        if (i == attackerIndex) continue;
        ClientRecord* const candidate = clients_.Get(i);
        if (!candidate || !candidate->edict || candidate->state != ClientLifecycleState::Active ||
            candidate->isDead || candidate->isObserver ||
            !build4100::IsPlayableTeam(candidate->teamIndex)) continue;

        build4100::Vector traceOrigin = candidate->absOrigin;
        build4100::Vector mins = candidate->playerMaxs.z > candidate->playerMins.z
            ? candidate->playerMins : build4100::Vector{-16.0f, -16.0f, 0.0f};
        build4100::Vector maxs = candidate->playerMaxs.z > candidate->playerMins.z
            ? candidate->playerMaxs : build4100::Vector{16.0f, 16.0f, 72.0f};
        std::int32_t historyTick = currentTick;
        bool historyUsed = false;
        LagCompensationRecord history{};
        if (lagHistory_.Select(i, command.tickCount, currentTick, maxRewindTicks, history) && history.alive) {
            traceOrigin = history.origin;
            if (history.maxs.z > history.mins.z) {
                mins = history.mins;
                maxs = history.maxs;
            }
            historyTick = history.tick;
            historyUsed = history.tick != currentTick;
        }

        PlayerHullTraceResult hit{};
        const build4100::Vector zero{};
        if (!SweepHullAgainstAabb(eye, end, zero, zero, traceOrigin, mins, maxs, hit)) continue;
        if (hit.startSolid || hit.fraction < 0.0f || hit.fraction >= victimFraction) continue;
        victimIndex = i;
        victimFraction = hit.fraction;
        impact = hit.endPos;
        victimTraceOrigin = traceOrigin;
        victimTraceMins = mins;
        victimTraceMaxs = maxs;
        selectedHistoryTick = historyTick;
        usedLagCompensation = historyUsed;
    }
    if (victimIndex == 0) return false;

    ClientRecord* const victimBefore = clients_.Get(victimIndex);
    if (!victimBefore) return false;
    const int healthBefore = victimBefore->health;
    const int armorBefore = victimBefore->armorValue;
    const build4100::HitGroup hitGroup = HitscanHitGroup(
        victimTraceOrigin, victimTraceMins, victimTraceMaxs, impact);
    const float dx = impact.x - eye.x;
    const float dy = impact.y - eye.y;
    const float dz = impact.z - eye.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float ballisticDamage = BallisticDamageAtDistance(profile, distance);
    const DamageApplyResult damage = gameState_.DamagePlayer(
        victimIndex, attackerIndex, ballisticDamage, hitGroup, profile.armorRatio, false);
    if (damage != DamageApplyResult::Applied && damage != DamageApplyResult::Killed) return false;

    ClientRecord* const victim = clients_.Get(victimIndex);
    ClientRecord* const shooter = clients_.Get(attackerIndex);
    const bool hurtEvent = FirePlayerHurtEvent(attackerIndex, victimIndex, weapon);
    bool deathEvent = false;
    if (damage == DamageApplyResult::Killed) {
        deathEvent = FirePlayerDeathEvent(attackerIndex, victimIndex, weapon);
        const float now = globals_ ? globals_->curtime : 0.0f;
        (void)gameState_.CheckWinConditions(now);
    }
    if (victim && victim->edict) {
        victim->edict->stateFlags |= build4100::kEdictChanged;
        if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(static_cast<int>(victimIndex));
    }
    if (shooter && shooter->edict) {
        shooter->edict->stateFlags |= build4100::kEdictChanged;
        if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(static_cast<int>(attackerIndex));
    }

    if (attackerIndex < gameplayDamageProbeDone_.size() && !gameplayDamageProbeDone_[attackerIndex]) {
        gameplayDamageProbeDone_[attackerIndex] = true;
        const char* const alias = build4100::WeaponIDToAlias(weapon);
        char detail[640];
        std::snprintf(detail, sizeof(detail),
                      "attacker=%zu victim=%zu weapon=%s hitgroup=%d health_before=%d health_after=%d armor_before=%d armor_after=%d killed=%d hurt_event=%d death_event=%d fraction=%.5f distance=%.2f damage=%.3f spread=%.5f spread_xy=%.5f,%.5f recoil=%.3f,%.3f cmd_tick=%d history_tick=%d rewind_ticks=%d lagcomp=%d",
                      attackerIndex, victimIndex, alias ? alias : "none", static_cast<int>(hitGroup),
                      healthBefore, victim ? victim->health : -1, armorBefore, victim ? victim->armorValue : -1,
                      damage == DamageApplyResult::Killed ? 1 : 0, hurtEvent ? 1 : 0, deathEvent ? 1 : 0,
                      victimFraction, distance, ballisticDamage, shot.spread, shot.spreadX, shot.spreadY,
                      shot.recoilPitch, shot.recoilYaw, command.tickCount, selectedHistoryTick,
                      std::max(0, currentTick - selectedHistoryTick), usedLagCompensation ? 1 : 0);
        EngineProbe("GAMEPLAY_HITSCAN_DAMAGE", detail);
    }
    return true;
}

bool ServerGameClients004Adapter::SetupCombatProbe(const std::size_t attackerIndex) noexcept {
    if (combatProbeActive_) ResetCombatProbe();
    ClientRecord* const attacker = clients_.Get(attackerIndex);
    if (!attacker || !attacker->edict || attacker->state != ClientLifecycleState::Active ||
        !build4100::IsPlayableTeam(attacker->teamIndex)) return false;

    std::size_t targetIndex = 0;
    ClientRecord* target = nullptr;
    for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
        if (i == attackerIndex) continue;
        ClientRecord* const candidate = clients_.Get(i);
        if (!candidate || !candidate->edict || candidate->state != ClientLifecycleState::Active ||
            !candidate->isFakeClient || candidate->teamIndex != attacker->teamIndex) continue;
        targetIndex = i;
        target = candidate;
        break;
    }
    if (!target) return false;

    combatProbeActive_ = true;
    combatProbeAttacker_ = attackerIndex;
    combatProbeTarget_ = targetIndex;
    combatProbeSavedFriendlyFire_ = gameState_.FriendlyFire();
    combatProbeSavedTargetOrigin_ = target->absOrigin;
    combatProbeSavedTargetVelocity_ = target->absVelocity;
    combatProbeSavedTargetMins_ = target->playerMins;
    combatProbeSavedTargetMaxs_ = target->playerMaxs;
    combatProbeSavedTargetHealth_ = target->health;
    combatProbeSavedTargetArmor_ = target->armorValue;
    combatProbeSavedTargetDeaths_ = target->deathCount;
    combatProbeSavedTargetTeam_ = target->teamIndex;
    combatProbeSavedTargetState_ = target->csState;
    combatProbeSavedTargetDead_ = target->isDead;
    combatProbeSavedTargetObserver_ = target->isObserver;
    combatProbeSavedTargetLastHitGroup_ = target->lastHitGroup;
    combatProbeSavedTargetLastAttacker_ = target->lastAttackerIndex;
    combatProbeSavedTargetLastDamage_ = target->lastDamageTaken;
    combatProbeSavedTargetLastHeadshot_ = target->lastDeathWasHeadshot;
    combatProbeSavedAttackerAngles_ = attacker->absAngles;
    combatProbeSavedAttackerAccount_ = attacker->account;
    combatProbeSavedAttackerFrags_ = attacker->fragCount;
    combatProbeSavedAttackerTeamKills_ = attacker->teamKillCount;

    gameState_.SetFriendlyFire(true);
    attacker->absAngles = build4100::QAngle{0.0f, 90.0f, 0.0f};
    target->health = 30;
    target->armorValue = 0;
    target->isDead = false;
    target->isObserver = false;
    target->csState = build4100::CSPlayerState::Active;
    target->playerMins = build4100::Vector{-16.0f, -16.0f, 0.0f};
    target->playerMaxs = build4100::Vector{16.0f, 16.0f, 72.0f};
    target->absOrigin = build4100::Vector{attacker->absOrigin.x, attacker->absOrigin.y + 40.0f,
                                         attacker->absOrigin.z};
    (void)clients_.SyncPlayerState(targetIndex);
    (void)clients_.SyncPlayerState(attackerIndex);
    target->edict->stateFlags |= build4100::kEdictChanged;
    attacker->edict->stateFlags |= build4100::kEdictChanged;
    if (engine_ && engine_->IsValid()) {
        engine_->NotifyEdictFlagsChange(static_cast<int>(targetIndex));
        engine_->NotifyEdictFlagsChange(static_cast<int>(attackerIndex));
    }
    RecordLagCompensationFrame();

    char detail[320];
    std::snprintf(detail, sizeof(detail),
                  "attacker=%zu target=%zu target_health=%d friendlyfire=1 attacker_origin=%.3f,%.3f,%.3f target_origin=%.3f,%.3f,%.3f yaw=90",
                  attackerIndex, targetIndex, target->health, attacker->absOrigin.x, attacker->absOrigin.y,
                  attacker->absOrigin.z, target->absOrigin.x, target->absOrigin.y, target->absOrigin.z);
    EngineProbe("GAMEPLAY_COMBAT_SETUP", detail);
    return true;
}

void ServerGameClients004Adapter::ResetCombatProbe() noexcept {
    if (!combatProbeActive_) return;
    ClientRecord* const attacker = clients_.Get(combatProbeAttacker_);
    ClientRecord* const target = clients_.Get(combatProbeTarget_);
    gameState_.SetFriendlyFire(combatProbeSavedFriendlyFire_);
    if (target) {
        target->absOrigin = combatProbeSavedTargetOrigin_;
        target->absVelocity = combatProbeSavedTargetVelocity_;
        target->playerMins = combatProbeSavedTargetMins_;
        target->playerMaxs = combatProbeSavedTargetMaxs_;
        target->health = combatProbeSavedTargetHealth_;
        target->armorValue = combatProbeSavedTargetArmor_;
        target->deathCount = combatProbeSavedTargetDeaths_;
        target->teamIndex = combatProbeSavedTargetTeam_;
        target->csState = combatProbeSavedTargetState_;
        target->isDead = combatProbeSavedTargetDead_;
        target->isObserver = combatProbeSavedTargetObserver_;
        target->lastHitGroup = combatProbeSavedTargetLastHitGroup_;
        target->lastAttackerIndex = combatProbeSavedTargetLastAttacker_;
        target->lastDamageTaken = combatProbeSavedTargetLastDamage_;
        target->lastDeathWasHeadshot = combatProbeSavedTargetLastHeadshot_;
        (void)clients_.SyncPlayerState(combatProbeTarget_);
        if (target->edict) {
            target->edict->stateFlags |= build4100::kEdictChanged;
            if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(static_cast<int>(combatProbeTarget_));
        }
    }
    if (attacker) {
        attacker->absAngles = combatProbeSavedAttackerAngles_;
        attacker->account = combatProbeSavedAttackerAccount_;
        attacker->fragCount = combatProbeSavedAttackerFrags_;
        attacker->teamKillCount = combatProbeSavedAttackerTeamKills_;
        (void)clients_.SyncPlayerState(combatProbeAttacker_);
        if (attacker->edict) {
            attacker->edict->stateFlags |= build4100::kEdictChanged;
            if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(static_cast<int>(combatProbeAttacker_));
        }
    }
    char detail[192];
    std::snprintf(detail, sizeof(detail), "attacker=%zu target=%zu friendlyfire=%d",
                  combatProbeAttacker_, combatProbeTarget_, gameState_.FriendlyFire() ? 1 : 0);
    EngineProbe("GAMEPLAY_COMBAT_RESET", detail);
    RecordLagCompensationFrame();
    combatProbeActive_ = false;
    combatProbeAttacker_ = 0;
    combatProbeTarget_ = 0;
}

bool ServerGameClients004Adapter::EmitFireBulletsTempEntity(
    const std::size_t clientIndex, const build4100::CSWeaponID weapon,
    const build4100::UserCmd& command) noexcept {
    if (!engine_ || !engine_->IsValid()) return false;
    ClientRecord* const client = clients_.Get(clientIndex);
    const PlayerWeaponRuntime* const runtime = weaponRuntime_.State(clientIndex);
    if (!client || !client->edict || !runtime || !runtime->classicBallistics) return false;

    auto* const serverClass = ServerClasses().FindByNetworkName("CTEFireBullets");
    if (!serverClass || serverClass->classId != 120 || !serverClass->table) return false;

    const float eyeHeight = client->movementDucked ? 46.0f : 64.0f;
    const build4100::Vector fireOrigin{client->absOrigin.x, client->absOrigin.y,
                                       client->absOrigin.z + eyeHeight};
    FireBulletsRecipientFilter filter(clients_, *engine_, clientIndex, fireOrigin);
    // Original TE_FireBullets constructs the PAS/prediction filter and then
    // calls CBaseTempEntity::Create unconditionally. A lone predicting shooter
    // therefore legitimately leaves an empty recipient list; that is still a
    // successful temp-entity emission path even though no packet is addressed.
    // original_empty_filter_create=1
    const int recipientCount = filter.GetRecipientCount();
    // Diagnostic oracle for the final observer-wire gate.  IRecipientFilter
    // recipient indices are 1-based build4100 player indices; surface the
    // first two values without changing filtering or gameplay behavior.
    const int recipient0 = filter.GetRecipientIndex(0);
    const int recipient1 = filter.GetRecipientIndex(1);

    const float speed2D = std::sqrt(client->absVelocity.x * client->absVelocity.x +
                                    client->absVelocity.y * client->absVelocity.y);
    const WeaponMovementState movement{
        client->movementOnGround, client->movementDucked, runtime->silenced, speed2D,
        runtime->zoomLevel != 0, runtime->burstMode};
    const WeaponRuntimeProfile& profile = WeaponProfile(weapon);
    const WeaponShotSolution shot = SolveBuild4100WeaponShot(
        weapon, profile, client->absAngles, movement, runtime->shotAccuracy,
        runtime->punchAngle, command.randomSeed, command.commandNumber,
        runtime->shotsFired, runtime->shotSpreadOverride);

    FireBulletsTempEntityData data{};
    data.player = static_cast<std::int32_t>(clientIndex) - 1;
    data.origin = fireOrigin;
    data.angles = build4100::QAngle{
        client->absAngles.x + runtime->punchAngle.x * 2.0f,
        client->absAngles.y + runtime->punchAngle.y * 2.0f,
        client->absAngles.z + runtime->punchAngle.z * 2.0f};
    data.weaponId = static_cast<std::int32_t>(weapon);
    if (weapon == build4100::CSWeaponID::M4A1 || weapon == build4100::CSWeaponID::Usp) {
        data.mode = runtime->silenced ? 1 : 0;
    } else if (weapon == build4100::CSWeaponID::Glock || weapon == build4100::CSWeaponID::Famas) {
        data.mode = runtime->burstMode ? 1 : 0;
    }
    data.seed = command.randomSeed;
    data.spread = shot.spread;

    PendingFireBulletsTempEntity pending{};
    pending.shooterIndex = clientIndex;
    pending.recipientCount = std::min<std::size_t>(
        recipientCount > 0 ? static_cast<std::size_t>(recipientCount) : 0u,
        pending.recipients.size());
    for (std::size_t i = 0; i < pending.recipientCount; ++i) {
        pending.recipients[i] = filter.GetRecipientIndex(static_cast<int>(i));
    }
    pending.player = data.player;
    pending.origin = data.origin;
    pending.angles = data.angles;
    pending.weaponId = data.weaponId;
    pending.mode = data.mode;
    pending.seed = data.seed;
    pending.spread = data.spread;
    pendingFireBulletsTempEntities_.push_back(pending);

    if (clientIndex < gameplayFireBulletsTempEntityProbeDone_.size() &&
        !gameplayFireBulletsTempEntityProbeDone_[clientIndex]) {
        gameplayFireBulletsTempEntityProbeDone_[clientIndex] = true;
        char detail[384];
        std::snprintf(detail, sizeof(detail),
                      "index=%zu classid=%d weapon=%d mode=%d seed=%d spread=%.6f recipients=%d predicted_shooter_excluded=1 pas=engine_slot41 origin=%.3f,%.3f,%.3f angles=%.3f,%.3f recipient0=%d recipient1=%d",
                      clientIndex, serverClass->classId, data.weaponId, data.mode, data.seed,
                      data.spread, recipientCount, data.origin.x, data.origin.y, data.origin.z,
                      data.angles.x, data.angles.y, recipient0, recipient1);
        EngineProbe("GAMEPLAY_TE_FIREBULLETS", detail);
    }
    return true;
}

void ServerGameClients004Adapter::HandleWeaponButtons(const std::size_t clientIndex,
                                                       const std::int32_t beforeButtons,
                                                       const build4100::UserCmd& command) noexcept {
    constexpr std::int32_t kAttack = 1 << 0;
    constexpr std::int32_t kAttack2 = 1 << 11;
    constexpr std::int32_t kReload = 1 << 13;
    const std::int32_t afterButtons = command.buttons;
    const bool attackPressed = (afterButtons & kAttack) != 0 && (beforeButtons & kAttack) == 0;
    const bool attack2Pressed = (afterButtons & kAttack2) != 0 && (beforeButtons & kAttack2) == 0;
    const bool reloadPressed = (afterButtons & kReload) != 0 && (beforeButtons & kReload) == 0;
    if (!attackPressed && !attack2Pressed && !reloadPressed) return;

    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || client->isDead || client->csState != build4100::CSPlayerState::Active) return;
    const build4100::CSWeaponID weapon = client->weapons.ActiveWeapon();
    if (weapon == build4100::CSWeaponID::None) return;
    const float now = globals_ ? globals_->curtime : 0.0f;
    const char* const alias = build4100::WeaponIDToAlias(weapon);
    const auto ammoType = build4100::AmmoTypeForWeapon(weapon);

    if (attack2Pressed) {
        const SecondaryWeaponResult result = weaponRuntime_.SecondaryAttack(clientIndex, now, weapon);
        if (result == SecondaryWeaponResult::Toggled) {
            const PlayerWeaponRuntime* const runtime = weaponRuntime_.State(clientIndex);
            if (runtime && (weapon == build4100::CSWeaponID::Awp || weapon == build4100::CSWeaponID::Scout ||
                            weapon == build4100::CSWeaponID::G3Sg1 || weapon == build4100::CSWeaponID::Sg550)) {
                if (runtime->zoomLevel == 0) client->currentFov = 0;
                else if (runtime->zoomLevel == 1) client->currentFov = 40;
                else client->currentFov = (weapon == build4100::CSWeaponID::Awp) ? 10 : 15;
                client->defaultFov = 90;
                if (client->edict) {
                    client->edict->stateFlags |= build4100::kEdictChanged;
                    if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(static_cast<int>(clientIndex));
                }
            }
            char detail[288];
            std::snprintf(detail, sizeof(detail),
                          "index=%zu weapon=%s silenced=%d burst=%d zoom=%u fov=%d next_secondary=%.3f",
                          clientIndex, alias ? alias : "none",
                          runtime && runtime->silenced ? 1 : 0,
                          runtime && runtime->burstMode ? 1 : 0,
                          runtime ? static_cast<unsigned>(runtime->zoomLevel) : 0u, client->currentFov,
                          runtime ? runtime->nextSecondaryAttackTime : 0.0f);
            EngineProbe("GAMEPLAY_WEAPON_SECONDARY_EVENT", detail);
        }
    }

    if (attackPressed) {
        const int clipBefore = client->weapons.Clip(weapon);
        const TimedWeaponResult result = weaponRuntime_.Fire(clientIndex, now, gameState_, weapon);
        if (result == TimedWeaponResult::Fired) {
            const int clipAfter = client->weapons.Clip(weapon);
            const bool eventFired = FireWeaponGameEvent("weapon_fire", clientIndex, weapon, true);
            const bool tempEntityFired = EmitFireBulletsTempEntity(clientIndex, weapon, command);
            const bool hitscanApplied = ApplyHitscanAttack(clientIndex, weapon, command);
            (void)hitscanApplied;
            const float speed2D = std::sqrt(client->absVelocity.x * client->absVelocity.x +
                                            client->absVelocity.y * client->absVelocity.y);
            const PlayerWeaponRuntime* const postFireRuntime = weaponRuntime_.State(clientIndex);
            const WeaponMovementState movement{
                client->movementOnGround, client->movementDucked,
                postFireRuntime ? postFireRuntime->silenced : false, speed2D,
                postFireRuntime ? postFireRuntime->zoomLevel != 0 : false,
                postFireRuntime ? postFireRuntime->burstMode : false};
            // build4100 computes this shot from the old PunchAngle, then applies
            // CCSPlayer::KickBack for the following shot.
            weaponRuntime_.ApplyPostFireKickBack(
                clientIndex, weapon, movement, command.randomSeed);
            if (clientIndex < gameplayWeaponFireProbeDone_.size() && !gameplayWeaponFireProbeDone_[clientIndex]) {
                gameplayWeaponFireProbeDone_[clientIndex] = true;
                const PlayerWeaponRuntime* const runtime = weaponRuntime_.State(clientIndex);
                char detail[256];
                std::snprintf(detail, sizeof(detail),
                              "index=%zu weapon=%s clip_before=%d clip_after=%d shots=%zu event=%d te_firebullets=%d",
                              clientIndex, alias ? alias : "none", clipBefore, clipAfter,
                              runtime ? runtime->shotsFired : 0u, eventFired ? 1 : 0, tempEntityFired ? 1 : 0);
                EngineProbe("GAMEPLAY_WEAPON_FIRE_EVENT", detail);
            }
        }
    }

    if (reloadPressed) {
        const int clipBefore = client->weapons.Clip(weapon);
        const int reserveBefore = build4100::IsValidAmmoType(ammoType) ? client->weapons.Ammo(ammoType) : 0;
        const WeaponActionResult result = weaponRuntime_.Reload(clientIndex, now, gameState_, weapon);
        if (result == WeaponActionResult::Reloaded) {
            const int clipAfter = client->weapons.Clip(weapon);
            const int reserveAfter = build4100::IsValidAmmoType(ammoType) ? client->weapons.Ammo(ammoType) : 0;
            const bool eventFired = FireWeaponGameEvent("weapon_reload", clientIndex, weapon, false);
            if (clientIndex < gameplayWeaponReloadProbeDone_.size() && !gameplayWeaponReloadProbeDone_[clientIndex]) {
                gameplayWeaponReloadProbeDone_[clientIndex] = true;
                char detail[288];
                std::snprintf(detail, sizeof(detail),
                              "index=%zu weapon=%s clip_before=%d clip_after=%d reserve_before=%d reserve_after=%d event=%d",
                              clientIndex, alias ? alias : "none", clipBefore, clipAfter,
                              reserveBefore, reserveAfter, eventFired ? 1 : 0);
                EngineProbe("GAMEPLAY_WEAPON_RELOAD_EVENT", detail);
            }
        }
    }
}

void ServerGameClients004Adapter::ProcessDeferredWeaponBursts() noexcept {
    const float now = globals_ ? globals_->curtime : 0.0f;
    for (std::size_t clientIndex = 1; clientIndex <= clients_.MaxClients(); ++clientIndex) {
        ClientRecord* const client = clients_.Get(clientIndex);
        if (!client || !client->edict || client->state != ClientLifecycleState::Active ||
            client->isDead || client->isObserver) continue;
        const build4100::CSWeaponID weapon = client->weapons.ActiveWeapon();
        if (weapon != build4100::CSWeaponID::Glock) continue;
        const PlayerWeaponRuntime* before = weaponRuntime_.State(clientIndex);
        if (!before || before->burstShotsRemaining <= 0 || before->nextBurstShotTime <= 0.0f ||
            now + 0.0001f < before->nextBurstShotTime) continue;

        build4100::UserCmd synthetic{};
        synthetic.commandNumber = client->lastUserCommand.commandNumber;
        synthetic.tickCount = globals_ ? globals_->tickcount : client->lastUserCommand.tickCount;
        synthetic.viewAngles = client->lastUserCommand.viewAngles;
        synthetic.forwardMove = client->lastUserCommand.forwardMove;
        synthetic.sideMove = client->lastUserCommand.sideMove;
        synthetic.upMove = client->lastUserCommand.upMove;
        synthetic.buttons = client->lastUserCommand.buttons;
        synthetic.impulse = client->lastUserCommand.impulse;
        synthetic.weaponSelect = client->lastUserCommand.weaponSelect;
        synthetic.weaponSubtype = client->lastUserCommand.weaponSubtype;
        synthetic.randomSeed = client->lastUserCommand.randomSeed;
        synthetic.mouseDx = client->lastUserCommand.mouseDx;
        synthetic.mouseDy = client->lastUserCommand.mouseDy;
        synthetic.hasBeenPredicted = client->lastUserCommand.hasBeenPredicted;
        if (!weaponRuntime_.FireScheduledGlockBurst(clientIndex, now, gameState_, weapon)) continue;
        const bool eventFired = FireWeaponGameEvent("weapon_fire", clientIndex, weapon, true);
        (void)EmitFireBulletsTempEntity(clientIndex, weapon, synthetic);
        (void)ApplyHitscanAttack(clientIndex, weapon, synthetic);
        const float speed2D = std::sqrt(client->absVelocity.x * client->absVelocity.x +
                                        client->absVelocity.y * client->absVelocity.y);
        const PlayerWeaponRuntime* runtime = weaponRuntime_.State(clientIndex);
        const WeaponMovementState movement{client->movementOnGround, client->movementDucked,
            runtime ? runtime->silenced : false, speed2D, runtime ? runtime->zoomLevel != 0 : false,
            runtime ? runtime->burstMode : false};
        weaponRuntime_.ApplyPostFireKickBack(clientIndex, weapon, movement, synthetic.randomSeed);
        if (clientIndex < gameplayGlockBurstProbeDone_.size() && !gameplayGlockBurstProbeDone_[clientIndex]) {
            gameplayGlockBurstProbeDone_[clientIndex] = true;
            runtime = weaponRuntime_.State(clientIndex);
            char detail[256];
            std::snprintf(detail, sizeof(detail),
                          "index=%zu shot_ordinal=%d remaining=%d cadence=0.100 event=%d",
                          clientIndex, runtime ? runtime->burstShotOrdinal : -1,
                          runtime ? runtime->burstShotsRemaining : -1, eventFired ? 1 : 0);
            EngineProbe("GAMEPLAY_GLOCK_BURST_EVENT", detail);
        }
    }
}

void ServerGameClients004Adapter::ProcessDeferredTempEntities() noexcept {
    if (pendingFireBulletsTempEntities_.empty()) return;

    // Move the queue aside before entering engine code. This preserves any
    // events queued re-entrantly for the following frame and gives the sender
    // data/filter stable storage for the synchronous SendTable_Encode call.
    std::vector<PendingFireBulletsTempEntity> pending;
    pending.swap(pendingFireBulletsTempEntities_);
    if (!engine_ || !engine_->IsValid()) return;

    const build4100::ServerClass* const serverClass =
        ServerClasses().FindByNetworkName("CTEFireBullets");
    if (!serverClass || serverClass->classId != 120 || !serverClass->table) return;

    for (const PendingFireBulletsTempEntity& event : pending) {
        CapturedFireBulletsRecipientFilter filter(
            event.recipients.data(), event.recipientCount);
        FireBulletsTempEntityData data{};
        data.player = event.player;
        data.origin = event.origin;
        data.angles = event.angles;
        data.weaponId = event.weaponId;
        data.mode = event.mode;
        data.seed = event.seed;
        data.spread = event.spread;

        engine_->PlaybackTempEntity(
            &filter, 0.0f, &data, serverClass->table, serverClass->classId);

        char detail[320];
        std::snprintf(detail, sizeof(detail),
                      "index=%zu classid=%d weapon=%d seed=%d recipients=%zu phase=gameframe recipient0=%d",
                      event.shooterIndex, serverClass->classId, data.weaponId, data.seed,
                      event.recipientCount,
                      event.recipientCount > 0 ? event.recipients[0] : -1);
        EngineProbe("GAMEPLAY_TE_FIREBULLETS_PLAYBACK", detail);
    }
}

bool ServerGameClients004Adapter::ReservePlantedC4Edict() noexcept {
    if (plantedC4Edict_) return true;
    if (!engine_ || !engine_->IsValid() || !entityBridge_ || !globals_) return false;

    ++plantedC4ReservationAttempts_;

    // IVEngineServer::CreateEdict uses -1 as the normal allocator path. A forced
    // free index is only a request for that exact slot and build4100 returns null
    // when that slot is not currently allocatable. v1.8.34h incorrectly forced
    // maxClients+1 and therefore never established the pre-client reservation on
    // the real engine. Let ED_Alloc choose the first legal non-player edict, then
    // verify that it is outside 1..maxClients before attaching our networkable.
    const int preferredIndex = globals_->maxClients + 1;
    if (preferredIndex <= 0 || preferredIndex >= static_cast<int>(build4100::kMaxEdicts)) {
        EngineProbe("GAMEPLAY_PLANTED_C4_RESERVE_FAIL", "stage=preferred_index");
        return false;
    }

    build4100::Edict* const edict = engine_->CreateEdict(-1);
    if (!edict) {
        EngineProbe("GAMEPLAY_PLANTED_C4_RESERVE_FAIL", "stage=create_edict");
        return false;
    }
    const int actualIndex = engine_->IndexOfEdict(edict);
    if (actualIndex <= globals_->maxClients ||
        actualIndex >= static_cast<int>(build4100::kMaxEdicts)) {
        engine_->RemoveEdict(edict);
        char invalidDetail[160];
        std::snprintf(invalidDetail, sizeof(invalidDetail),
                      "stage=nonclient_index entity=%d maxClients=%d attempt=%d",
                      actualIndex, globals_->maxClients, plantedC4ReservationAttempts_);
        EngineProbe("GAMEPLAY_PLANTED_C4_RESERVE_FAIL", invalidDetail);
        return false;
    }

    const EntityHandle handle = entityBridge_->AttachGeneric(
        edict, "planted_c4", build4100::kEdictTransmitDontSend);
    if (!handle.IsValid()) {
        engine_->RemoveEdict(edict);
        EngineProbe("GAMEPLAY_PLANTED_C4_RESERVE_FAIL", "stage=attach_generic");
        return false;
    }

    plantedC4Edict_ = edict;
    plantedC4EntityIndex_ = actualIndex;
    plantedC4Active_ = false;
    plantedC4SpawnPending_ = false;
    SetGameplayPlantedC4EntityIndex(-1);
    plantedC4Edict_->stateFlags |= build4100::kEdictChanged;
    engine_->NotifyEdictFlagsChange(plantedC4EntityIndex_);

    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "entity=%d preferred=%d maxClients=%d allocator=auto attempt=%d flags=0x%08x mode=dontsend changed=1",
                  actualIndex, preferredIndex, globals_->maxClients, plantedC4ReservationAttempts_,
                  plantedC4Edict_->stateFlags);
    EngineProbe("GAMEPLAY_PLANTED_C4_RESERVED", detail);
    return true;
}

bool ServerGameClients004Adapter::SpawnPlantedC4() noexcept {
    // ClientCommand/ProcessUsercmds may run inside netchan traversal. Do not
    // allocate here. The level reservation is normally already present; if not,
    // materialization gets one last safe allocation attempt from GameFrame.
    if (!engine_ || !engine_->IsValid() || !entityBridge_ ||
        gameState_.Bomb().State() != BombObjectiveState::Planted) {
        return false;
    }
    plantedC4SpawnPending_ = true;
    const BombObjective& bomb = gameState_.Bomb();
    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "site=%d origin=%.3f,%.3f,%.3f blow=%.3f timer=%.3f reserved=%d active=%d",
                  bomb.SiteIndex(), bomb.PlantOrigin().x, bomb.PlantOrigin().y, bomb.PlantOrigin().z,
                  bomb.ExplodeTime(), bomb.TimerSeconds(), plantedC4Edict_ ? 1 : 0,
                  plantedC4Active_ ? 1 : 0);
    EngineProbe("GAMEPLAY_PLANTED_C4_DEFERRED", detail);
    return true;
}

bool ServerGameClients004Adapter::MaterializePlantedC4() noexcept {
    if (!plantedC4SpawnPending_) return plantedC4Edict_ && plantedC4Active_;
    if (!engine_ || !engine_->IsValid() || !entityBridge_ ||
        gameState_.Bomb().State() != BombObjectiveState::Planted) {
        plantedC4SpawnPending_ = false;
        EngineProbe("GAMEPLAY_PLANTED_C4_SPAWN_FAIL", "stage=precondition");
        return false;
    }
    if (!plantedC4Edict_ && !ReservePlantedC4Edict()) {
        plantedC4SpawnPending_ = false;
        EngineProbe("GAMEPLAY_PLANTED_C4_SPAWN_FAIL", "stage=reserve");
        return false;
    }

    const int index = plantedC4EntityIndex_;
    if (index <= 0) {
        plantedC4SpawnPending_ = false;
        EngineProbe("GAMEPLAY_PLANTED_C4_SPAWN_FAIL", "stage=index_of_edict");
        return false;
    }

    if (!entityBridge_->SetGenericTransmitState(plantedC4Edict_, build4100::kEdictTransmitAlways)) {
        plantedC4SpawnPending_ = false;
        EngineProbe("GAMEPLAY_PLANTED_C4_SPAWN_FAIL", "stage=transmit_state");
        return false;
    }
    plantedC4Active_ = true;
    plantedC4SpawnPending_ = false;
    SetGameplayPlantedC4EntityIndex(index);
    const bool fullChanged = engine_->MarkEdictStateChangedFull(plantedC4Edict_);
    engine_->NotifyEdictFlagsChange(index);

    {
        char transmitDetail[192];
        std::snprintf(transmitDetail, sizeof(transmitDetail),
                      "entity=%d flags=0x%08x mode=%s changed=%d fullchanged=%d",
                      index, plantedC4Edict_->stateFlags,
                      build4100::HasEdictTransmitState(*plantedC4Edict_, build4100::kEdictTransmitAlways)
                          ? "always" : "unexpected",
                      (plantedC4Edict_->stateFlags & build4100::kEdictChanged) ? 1 : 0,
                      fullChanged && (plantedC4Edict_->stateFlags & build4100::kEdictFullChanged) ? 1 : 0);
        EngineProbe("GAMEPLAY_PLANTED_C4_TRANSMIT_STATE", transmitDetail);
    }
    const BombObjective& bomb = gameState_.Bomb();
    char detail[320];
    std::snprintf(detail, sizeof(detail),
                  "entity=%d class=CPlantedC4 site=%d origin=%.3f,%.3f,%.3f ticking=1 blow=%.3f timer=%.3f reused=1",
                  index, bomb.SiteIndex(), bomb.PlantOrigin().x, bomb.PlantOrigin().y, bomb.PlantOrigin().z,
                  bomb.ExplodeTime(), bomb.TimerSeconds());
    EngineProbe("GAMEPLAY_PLANTED_C4_SPAWN", detail);
    EngineProbe("GAMEPLAY_PLANTED_C4_SENDSTATE", detail);
    return true;
}

void ServerGameClients004Adapter::MarkPlantedC4Changed() noexcept {
    if (!plantedC4Edict_ || !plantedC4Active_ || plantedC4EntityIndex_ <= 0 ||
        !engine_ || !engine_->IsValid()) return;
    (void)engine_->MarkEdictStateChangedFull(plantedC4Edict_);
}

void ServerGameClients004Adapter::RemovePlantedC4() noexcept {
    plantedC4SpawnPending_ = false;
    SetGameplayPlantedC4EntityIndex(-1);
    if (!plantedC4Edict_) {
        plantedC4Active_ = false;
        return;
    }

    // Round cleanup must not shrink/recycle the engine entity range. Park the
    // reserved edict in DONT_SEND and reuse the same serial/index on the next
    // plant. This keeps delta baselines stable across defuse/replant/explosion.
    plantedC4Active_ = false;
    if (entityBridge_) {
        (void)entityBridge_->SetGenericTransmitState(
            plantedC4Edict_, build4100::kEdictTransmitDontSend);
    }
    if (engine_ && engine_->IsValid() && plantedC4EntityIndex_ > 0) {
        (void)engine_->MarkEdictStateChangedFull(plantedC4Edict_);
        engine_->NotifyEdictFlagsChange(plantedC4EntityIndex_);
    }
    char detail[160];
    std::snprintf(detail, sizeof(detail),
                  "entity=%d flags=0x%08x mode=dontsend retained=1",
                  plantedC4EntityIndex_, plantedC4Edict_->stateFlags);
    EngineProbe("GAMEPLAY_PLANTED_C4_DEACTIVATED", detail);
}

void ServerGameClients004Adapter::ReleasePlantedC4Reservation() noexcept {
    plantedC4SpawnPending_ = false;
    plantedC4Active_ = false;
    SetGameplayPlantedC4EntityIndex(-1);
    build4100::Edict* const edict = plantedC4Edict_;
    const int index = plantedC4EntityIndex_;
    plantedC4Edict_ = nullptr;
    plantedC4EntityIndex_ = -1;
    plantedC4ReservationAttempts_ = 0;
    plantedC4PreBaselineProbeDone_ = false;
    plantedC4BaselineProbeDone_ = false;
    if (edict && entityBridge_) {
        char detail[144];
        std::snprintf(detail, sizeof(detail),
                      "entity=%d flags=0x%08x remove_engine_edict=1",
                      index, edict->stateFlags);
        EngineProbe("GAMEPLAY_PLANTED_C4_RESERVATION_RELEASE_BEGIN", detail);

        const bool detached = entityBridge_->Detach(edict, true);
        std::snprintf(detail, sizeof(detail),
                      "entity=%d released=%d callback_path=removeedict",
                      index, detached ? 1 : 0);
        EngineProbe("GAMEPLAY_PLANTED_C4_RESERVATION_RELEASED", detail);
    }
}

bool ServerGameClients004Adapter::RunBombRoundsScenario(const std::size_t commandClient) noexcept {
    if (gameState_.MapObjective() != CsMapObjective::Bomb || objectiveScenario_ != "") return false;
    int targetZone = -1;
    for (std::size_t i = 0; i < mapObjectiveZones_.size(); ++i) {
        if (mapObjectiveZones_[i].kind == MapObjectiveZone::Kind::BombTarget) { targetZone = static_cast<int>(i); break; }
    }
    if (targetZone < 0) return false;
    std::size_t terrorist = 0, ct = 0;
    if (const ClientRecord* const caller = clients_.Get(commandClient)) {
        if (caller->teamIndex == static_cast<int>(build4100::CSTeam::Terrorist)) terrorist = commandClient;
        if (caller->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist)) ct = commandClient;
    }
    for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
        const ClientRecord* const c = clients_.Get(i);
        if (!c || c->state == ClientLifecycleState::Empty) continue;
        if (!terrorist && c->teamIndex == static_cast<int>(build4100::CSTeam::Terrorist)) terrorist = i;
        if (!ct && c->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist)) ct = i;
    }
    if (!terrorist || !ct) return false;
    const float now = globals_ ? globals_->curtime : 0.0f;
    RemovePlantedC4();
    gameState_.RestartRound(now);
    gameState_.EndFreezePeriod();
    (void)FireObjectiveEvent("round_start");
    if (!gameState_.AssignBombCarrier(terrorist)) return false;
    ClientRecord* const planter = clients_.Get(terrorist);
    const int prePlantUserId = planter && planter->edict ? engine_->GetPlayerUserId(planter->edict) : 0;
    if (!gameState_.DropBomb(terrorist)) return false;
    (void)FireObjectiveEvent("bomb_dropped", prePlantUserId);
    if (!gameState_.PickupBomb(terrorist)) return false;
    (void)FireObjectiveEvent("bomb_pickup", prePlantUserId);
    const build4100::Vector center = ObjectiveZoneCenter(targetZone);
    if (!planter) return false;
    planter->absOrigin = center;
    if (planter->edict) { planter->edict->stateFlags |= build4100::kEdictChanged; engine_->NotifyEdictFlagsChange(static_cast<int>(terrorist)); }
    const int detected = FindObjectiveZone(center, MapObjectiveZone::Kind::BombTarget);
    if (detected < 0) return false;
    const int siteIndex = mapObjectiveZones_[static_cast<std::size_t>(detected)].siteIndex;
    const int planterUserId = planter->edict ? engine_->GetPlayerUserId(planter->edict) : 0;
    // Exercise the complete build4100 planting event family before committing
    // the deterministic scenario plant. The core objective is still atomic;
    // begin/abort here are wire-oracle transitions for the engine event layer.
    (void)FireObjectiveEvent("bomb_beginplant", planterUserId, siteIndex);
    (void)FireObjectiveEvent("bomb_abortplant", planterUserId, siteIndex);
    (void)FireObjectiveEvent("bomb_beginplant", planterUserId, siteIndex);
    if (!gameState_.PlantBomb(terrorist, now + 0.01f, siteIndex, center)) return false;
    (void)FireObjectiveEvent("bomb_planted", planterUserId, gameState_.Bomb().SiteIndex());
    if (!SpawnPlantedC4()) return false;
    ClientRecord* const defuser = clients_.Get(ct);
    if (!defuser) return false;
    defuser->absOrigin = center;
    (void)gameState_.GiveDefuser(ct, true);
    if (defuser->edict) { defuser->edict->stateFlags |= build4100::kEdictChanged; engine_->NotifyEdictFlagsChange(static_cast<int>(ct)); }
    if (!gameState_.BeginDefuse(ct, now + 0.02f)) return false;
    const int defuserUserId = defuser->edict ? engine_->GetPlayerUserId(defuser->edict) : 0;
    (void)FireObjectiveEvent("bomb_begindefuse", defuserUserId, gameState_.Bomb().SiteIndex(), -1, -1, -1, true);
    if (!gameState_.CancelDefuse(ct)) return false;
    (void)FireObjectiveEvent("bomb_abortdefuse", defuserUserId, gameState_.Bomb().SiteIndex());
    if (!gameState_.BeginDefuse(ct, now + 0.03f)) return false;
    (void)FireObjectiveEvent("bomb_begindefuse", defuserUserId, gameState_.Bomb().SiteIndex(), -1, -1, -1, true);
    if (plantedC4Edict_) { plantedC4Edict_->stateFlags |= build4100::kEdictChanged; engine_->NotifyEdictFlagsChange(plantedC4EntityIndex_); }
    char detectDetail[224];
    std::snprintf(detectDetail, sizeof(detectDetail),
                  "scenario=bomb_rounds caller=%zu planter=%zu defuser=%zu zone=%d site=%d center=%.1f,%.1f,%.1f",
                  commandClient, terrorist, ct, detected, gameState_.Bomb().SiteIndex(), center.x, center.y, center.z);
    EngineProbe("OBJECTIVE_BOMBSITE_DETECTION", detectDetail);
    objectiveScenario_ = "bomb_rounds";
    objectiveScenarioClient_ = commandClient;
    objectiveScenarioStep_ = 1;
    objectiveScenarioNextTime_ = now + 0.25f;
    return true;
}

bool ServerGameClients004Adapter::RunHostageRescueScenario(const std::size_t commandClient) noexcept {
    if (gameState_.MapObjective() != CsMapObjective::Hostage || objectiveScenario_ != "") return false;
    int rescueZone = -1;
    for (std::size_t i = 0; i < mapObjectiveZones_.size(); ++i) {
        if (mapObjectiveZones_[i].kind == MapObjectiveZone::Kind::HostageRescue) { rescueZone = static_cast<int>(i); break; }
    }
    if (rescueZone < 0) return false;
    std::size_t ct = commandClient;
    const ClientRecord* chosen = clients_.Get(ct);
    if (!chosen || chosen->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist)) {
        ct = 0;
        for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
            const ClientRecord* const c = clients_.Get(i);
            if (c && c->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist)) { ct = i; break; }
        }
    }
    if (!ct) return false;
    const float now = globals_ ? globals_->curtime : 0.0f;
    gameState_.RestartRound(now);
    gameState_.EndFreezePeriod();
    (void)FireObjectiveEvent("round_start");
    ClientRecord* const rescuer = clients_.Get(ct);
    if (!rescuer) return false;
    rescuer->absOrigin = ObjectiveZoneCenter(rescueZone);
    if (FindObjectiveZone(rescuer->absOrigin, MapObjectiveZone::Kind::HostageRescue) < 0) return false;
    const int userId = rescuer->edict ? engine_->GetPlayerUserId(rescuer->edict) : 0;
    const std::size_t hostageTotal = gameState_.Hostages().Total();
    if (hostageTotal == 0) return false;
    // Exercise hurt/killed events in a disposable round, then restart so the
    // rescue/win path still starts from the stock all-alive hostage state.
    if (!gameState_.DamageHostage(0, 1, now + 0.001f, 5.0f)) return false;
    (void)FireObjectiveEvent("hostage_hurt", userId, -1, 0);
    if (!gameState_.DamageHostage(0, 1000, now + 0.002f, 5.0f)) return false;
    (void)FireObjectiveEvent("hostage_killed", userId, -1, 0);
    gameState_.RestartRound(now + 0.003f);
    gameState_.EndFreezePeriod();
    (void)FireObjectiveEvent("round_start");

    std::size_t rescued = 0;
    for (std::size_t hostage = 0; hostage < hostageTotal; ++hostage) {
        if (!gameState_.BeginHostageFollow(hostage, ct)) continue;
        (void)FireObjectiveEvent("hostage_follows", userId, -1, static_cast<int>(hostage));
        if (hostage == 0) {
            if (!gameState_.DropHostage(hostage, ct)) return false;
            (void)FireObjectiveEvent("hostage_stops_following", userId, -1, static_cast<int>(hostage));
            if (!gameState_.BeginHostageFollow(hostage, ct)) return false;
            (void)FireObjectiveEvent("hostage_follows", userId, -1, static_cast<int>(hostage));
        }
        if (!gameState_.RescueHostage(hostage, ct, now + 0.01f * static_cast<float>(hostage + 1), 5.0f)) continue;
        ++rescued;
        (void)FireObjectiveEvent("hostage_rescued", userId, rescueZone, static_cast<int>(hostage));
    }
    if (rescued == 0 || gameState_.RoundEndReason() != build4100::CSRoundEndReason::HostagesRescued) return false;
    (void)FireObjectiveEvent("hostage_rescued_all", userId, rescueZone);
    (void)FireObjectiveEvent("round_end", -1, -1, -1,
                             static_cast<int>(build4100::CSTeam::CounterTerrorist),
                             static_cast<int>(gameState_.RoundEndReason()));
    char detail[192];
    std::snprintf(detail, sizeof(detail), "scenario=hostage_rescue caller=%zu rescuer=%zu rescued=%zu zone=%d reason=%d",
                  commandClient, ct, rescued, rescueZone, static_cast<int>(gameState_.RoundEndReason()));
    EngineProbe("GAMEPLAY_HOSTAGE_RESCUE_ROUND", detail);
    objectiveScenario_ = "hostage_rescue";
    objectiveScenarioStep_ = 100; // completed marker emitted by TickGameplayRound
    objectiveScenarioNextTime_ = now;
    objectiveScenarioClient_ = commandClient;
    return true;
}

bool ServerGameClients004Adapter::RunRoundCycleScenario() noexcept {
    const float now = globals_ ? globals_->curtime : 0.0f;
    gameState_.RestartRound(now);
    (void)FireObjectiveEvent("round_start");
    gameState_.EndFreezePeriod();
    (void)FireObjectiveEvent("round_freeze_end");
    if (!gameState_.TerminateRound(build4100::CSRoundEndReason::TargetSaved, now + 0.01f, 0.0f)) return false;
    (void)FireObjectiveEvent("round_end", -1, -1, -1,
                             static_cast<int>(build4100::CSTeam::CounterTerrorist),
                             static_cast<int>(gameState_.RoundEndReason()));
    gameState_.RestartRound(now + 0.02f);
    (void)FireObjectiveEvent("round_start");
    gameState_.EndFreezePeriod();
    return true;
}


void ServerGameClients004Adapter::RecordLagCompensationFrame() noexcept {
    if (!globals_) return;
    const std::int32_t tick = globals_->tickcount;
    const float now = globals_->curtime;
    for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
        const ClientRecord* const client = clients_.Get(i);
        if (!client || !client->edict || client->state != ClientLifecycleState::Active) continue;
        LagCompensationRecord record{};
        record.tick = tick;
        record.simulationTime = now;
        record.origin = client->absOrigin;
        record.mins = client->playerMaxs.z > client->playerMins.z
            ? client->playerMins : build4100::Vector{-16.0f, -16.0f, 0.0f};
        record.maxs = client->playerMaxs.z > client->playerMins.z
            ? client->playerMaxs : build4100::Vector{16.0f, 16.0f, 72.0f};
        record.alive = !client->isDead && !client->isObserver &&
                       build4100::IsPlayableTeam(client->teamIndex);
        (void)lagHistory_.Record(i, record);
    }
}

void ServerGameClients004Adapter::TickGameplayRound() noexcept {
    RecordLagCompensationFrame();
    const float now = globals_ ? globals_->curtime : 0.0f;
    // A pre-client reservation means plantedC4Edict_ is intentionally non-null
    // before the bomb exists. Pending materialization must therefore be keyed by
    // plantedC4SpawnPending_, not by edict absence; otherwise the reserved slot
    // never transitions DONT_SEND -> ALWAYS and bomb_beep is never emitted.
    if (plantedC4SpawnPending_) {
        if (!MaterializePlantedC4()) {
            if (objectiveScenario_ == "bomb_rounds") {
                EngineProbe("GAMEPLAY_OBJECTIVE_SCENARIO_FAIL", "scenario=bomb_rounds phase=materialize_c4");
                objectiveScenario_.clear();
                objectiveScenarioStep_ = 0;
            }
            return;
        }
        const int planter = gameState_.Bomb().PlanterIndex();
        const ClientRecord* const c = planter > 0 ? clients_.Get(static_cast<std::size_t>(planter)) : nullptr;
        const int userid = c && c->edict ? engine_->GetPlayerUserId(c->edict) : 0;
        (void)FireObjectiveEvent("bomb_beep", userid, gameState_.Bomb().SiteIndex());
    }
    if (objectiveScenario_ == "bomb_rounds" && now >= objectiveScenarioNextTime_) {
        if (objectiveScenarioStep_ == 1) {
            EngineProbe("GAMEPLAY_OBJECTIVE_STAGE", "scenario=bomb_rounds step=1 action=finish_defuse");
            const int site = gameState_.Bomb().SiteIndex();
            const int defuser = gameState_.Bomb().DefuserIndex();
            const ClientRecord* const c = defuser > 0 ? clients_.Get(static_cast<std::size_t>(defuser)) : nullptr;
            const int userid = c && c->edict ? engine_->GetPlayerUserId(c->edict) : 0;
            const float finish = gameState_.Bomb().DefuseCompleteTime();
            if (gameState_.TickObjective(finish, 5.0f) != BombObjectiveTransition::Defused) {
                EngineProbe("GAMEPLAY_OBJECTIVE_SCENARIO_FAIL", "scenario=bomb_rounds phase=defuse");
                objectiveScenario_.clear(); return;
            }
            if (plantedC4Edict_) { plantedC4Edict_->stateFlags |= build4100::kEdictChanged; engine_->NotifyEdictFlagsChange(plantedC4EntityIndex_); }
            (void)FireObjectiveEvent("bomb_defused", userid, site);
            (void)FireObjectiveEvent("round_end", -1, -1, -1,
                                     static_cast<int>(build4100::CSTeam::CounterTerrorist),
                                     static_cast<int>(gameState_.RoundEndReason()));
            objectiveScenarioStep_ = 2; objectiveScenarioNextTime_ = now + 0.25f; return;
        }
        if (objectiveScenarioStep_ == 2) {
            EngineProbe("GAMEPLAY_OBJECTIVE_STAGE", "scenario=bomb_rounds step=2 action=recycle_c4_and_replant");
            int zone = -1; for (std::size_t i=0;i<mapObjectiveZones_.size();++i) if (mapObjectiveZones_[i].kind==MapObjectiveZone::Kind::BombTarget) { zone=static_cast<int>(i); break; }
            std::size_t t=0; for (std::size_t i=1;i<=clients_.MaxClients();++i) { const auto* c=clients_.Get(i); if(c&&c->teamIndex==static_cast<int>(build4100::CSTeam::Terrorist)){t=i;break;} }
            if (zone < 0 || !t) { objectiveScenario_.clear(); return; }
            RemovePlantedC4(); gameState_.RestartRound(now); gameState_.EndFreezePeriod(); (void)FireObjectiveEvent("round_start");
            (void)gameState_.AssignBombCarrier(t); const build4100::Vector center=ObjectiveZoneCenter(zone); ClientRecord* p=clients_.Get(t); if(!p){objectiveScenario_.clear();return;} p->absOrigin=center;
            if (!gameState_.PlantBomb(t, now+0.01f, mapObjectiveZones_[static_cast<std::size_t>(zone)].siteIndex, center)) { objectiveScenario_.clear(); return; }
            const int uid=p->edict?engine_->GetPlayerUserId(p->edict):0; (void)FireObjectiveEvent("bomb_planted",uid,gameState_.Bomb().SiteIndex());
            if(!SpawnPlantedC4()){objectiveScenario_.clear();return;}
            EngineProbe("GAMEPLAY_OBJECTIVE_STAGE", "scenario=bomb_rounds step=2 action=replant_deferred_ok");
            objectiveScenarioStep_=3; objectiveScenarioNextTime_=now+0.25f; return;
        }
        if (objectiveScenarioStep_ == 3) {
            EngineProbe("GAMEPLAY_OBJECTIVE_STAGE", "scenario=bomb_rounds step=3 action=force_explosion");
            const int site=gameState_.Bomb().SiteIndex(); const int planter=gameState_.Bomb().PlanterIndex(); const ClientRecord* c=planter>0?clients_.Get(static_cast<std::size_t>(planter)):nullptr; const int uid=c&&c->edict?engine_->GetPlayerUserId(c->edict):0;
            if(gameState_.TickObjective(gameState_.Bomb().ExplodeTime(),5.0f)!=BombObjectiveTransition::Exploded){objectiveScenario_.clear();return;}
            if(plantedC4Edict_){plantedC4Edict_->stateFlags|=build4100::kEdictChanged;engine_->NotifyEdictFlagsChange(plantedC4EntityIndex_);} (void)FireObjectiveEvent("bomb_exploded",uid,site);
            (void)FireObjectiveEvent("round_end",-1,-1,-1,static_cast<int>(build4100::CSTeam::Terrorist),static_cast<int>(gameState_.RoundEndReason()));
            objectiveScenarioStep_=4; objectiveScenarioNextTime_=now+0.25f; return;
        }
        if (objectiveScenarioStep_ == 4) {
            EngineProbe("GAMEPLAY_OBJECTIVE_STAGE", "scenario=bomb_rounds step=4 action=round_cycle_and_cleanup");
            RemovePlantedC4();
            if (!RunRoundCycleScenario()) { objectiveScenario_.clear(); return; }
            EngineProbe("GAMEPLAY_OBJECTIVE_SCENARIO", "scenario=bomb_rounds plant=1 defuse=1 explode=1 round_cycle=1 sendprop_c4=1");
            objectiveScenario_.clear(); objectiveScenarioStep_=0; return;
        }
    } else if (objectiveScenario_ == "hostage_rescue" && objectiveScenarioStep_ == 100) {
        EngineProbe("GAMEPLAY_OBJECTIVE_SCENARIO", "scenario=hostage_rescue follows=1 rescued=1 rescued_all=1 round_end=1");
        objectiveScenario_.clear(); objectiveScenarioStep_=0; return;
    }

    // Normal process-owned round timing. Objective scenarios above intentionally
    // stage transitions over multiple snapshots; outside them this keeps the
    // reconstructed freeze/restart state live in the real engine.
    const CsRoundPhase before = gameState_.RoundPhase();
    const BombObjectiveState bombBefore = gameState_.Bomb().State();
    const int bombSiteBefore = gameState_.Bomb().SiteIndex();
    const int bombPlanterBefore = gameState_.Bomb().PlanterIndex();
    const int bombDefuserBefore = gameState_.Bomb().DefuserIndex();
    const CsRoundTickResult result = gameState_.TickRound(now);
    if (result == CsRoundTickResult::FreezeEnded) (void)FireObjectiveEvent("round_freeze_end");
    if (result == CsRoundTickResult::ObjectiveResolved && bombBefore == BombObjectiveState::Planted) {
        const BombObjectiveState bombAfter = gameState_.Bomb().State();
        int actor = bombAfter == BombObjectiveState::Defused ? bombDefuserBefore : bombPlanterBefore;
        const ClientRecord* const player = actor > 0 ? clients_.Get(static_cast<std::size_t>(actor)) : nullptr;
        const int userid = player && player->edict ? engine_->GetPlayerUserId(player->edict) : 0;
        if (bombAfter == BombObjectiveState::Defused) {
            (void)FireObjectiveEvent("bomb_defused", userid, bombSiteBefore);
        } else if (bombAfter == BombObjectiveState::Exploded) {
            (void)FireObjectiveEvent("bomb_exploded", userid, bombSiteBefore);
        }
        if (plantedC4Edict_) {
            plantedC4Edict_->stateFlags |= build4100::kEdictChanged;
            engine_->NotifyEdictFlagsChange(plantedC4EntityIndex_);
        }
    }
    if (before != CsRoundPhase::Ended && gameState_.RoundPhase() == CsRoundPhase::Ended) {
        (void)FireObjectiveEvent("round_end", -1, -1, -1,
                                 CsGameState::WinningTeam(gameState_.RoundEndReason()),
                                 static_cast<int>(gameState_.RoundEndReason()));
    }
    if (result == CsRoundTickResult::Restarted) {
        RemovePlantedC4();
        (void)FireObjectiveEvent("round_start");
    }
}

void ServerGameClients004Adapter::HandleObjectiveButtons(const std::size_t clientIndex,
                                                         const std::int32_t beforeButtons,
                                                         const std::int32_t afterButtons) noexcept {
    constexpr std::int32_t kAttack = 1 << 0;
    constexpr std::int32_t kUse = 1 << 5;
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || client->isDead || gameState_.RoundPhase() != CsRoundPhase::Live) return;
    const float now = globals_ ? globals_->curtime : 0.0f;
    if (gameState_.MapObjective() == CsMapObjective::Bomb &&
        client->teamIndex == static_cast<int>(build4100::CSTeam::Terrorist) && client->hasC4 &&
        (afterButtons & kAttack) && !(beforeButtons & kAttack)) {
        const int zone = FindObjectiveZone(client->absOrigin, MapObjectiveZone::Kind::BombTarget);
        if (zone >= 0) {
            const int uid = client->edict ? engine_->GetPlayerUserId(client->edict) : 0;
            const int site = mapObjectiveZones_[static_cast<std::size_t>(zone)].siteIndex;
            (void)FireObjectiveEvent("bomb_beginplant", uid, site);
            if (gameState_.PlantBomb(clientIndex, now, site, client->absOrigin)) {
                (void)FireObjectiveEvent("bomb_planted", uid, gameState_.Bomb().SiteIndex());
                (void)SpawnPlantedC4();
            } else {
                (void)FireObjectiveEvent("bomb_abortplant", uid, site);
            }
        }
    }
    if (gameState_.MapObjective() == CsMapObjective::Bomb &&
        client->teamIndex == static_cast<int>(build4100::CSTeam::CounterTerrorist) &&
        gameState_.Bomb().State() == BombObjectiveState::Planted) {
        const auto& origin = gameState_.Bomb().PlantOrigin();
        const float dx = client->absOrigin.x - origin.x;
        const float dy = client->absOrigin.y - origin.y;
        const float dz = client->absOrigin.z - origin.z;
        const bool near = dx * dx + dy * dy + dz * dz <= 96.0f * 96.0f;
        const bool useDown = (afterButtons & kUse) != 0;
        const bool usePressed = useDown && (beforeButtons & kUse) == 0;
        const bool ownsDefuse = gameState_.Bomb().DefuserIndex() == static_cast<int>(clientIndex);

        if (ownsDefuse && (!useDown || !near)) {
            if (gameState_.CancelDefuse(clientIndex)) {
                const int uid = client->edict ? engine_->GetPlayerUserId(client->edict) : 0;
                (void)FireObjectiveEvent("bomb_abortdefuse", uid, gameState_.Bomb().SiteIndex());
                MarkPlantedC4Changed();
                EngineProbe(!useDown ? "GAMEPLAY_DEFUSE_CANCEL_RELEASE" :
                                       "GAMEPLAY_DEFUSE_CANCEL_RANGE",
                            !useDown ? "reason=use_released" : "reason=left_96u_radius");
            }
        } else if (!ownsDefuse && near && usePressed && gameState_.BeginDefuse(clientIndex, now)) {
            const int uid = client->edict ? engine_->GetPlayerUserId(client->edict) : 0;
            (void)FireObjectiveEvent("bomb_begindefuse", uid, gameState_.Bomb().SiteIndex(),
                                     -1, -1, -1, client->hasDefuser);
            MarkPlantedC4Changed();
        }
    }
}

void ServerGameClients004Adapter::HandleHostageUse(const std::size_t clientIndex,
                                                    const std::int32_t beforeButtons,
                                                    const std::int32_t afterButtons) noexcept {
    constexpr std::int32_t kUseButton = 1 << 5;
    if ((afterButtons & kUseButton) == 0 || (beforeButtons & kUseButton) != 0) return;
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || client->isDead ||
        client->teamIndex != static_cast<int>(build4100::CSTeam::CounterTerrorist)) return;

    std::size_t selected = mapMovementSolids_.size();
    float bestDistanceSq = 96.0f * 96.0f;
    for (std::size_t i = 0; i < mapMovementSolids_.size(); ++i) {
        const auto& solid = mapMovementSolids_[i];
        if (solid.kind != MapMovementSolid::Kind::HostageBBox) continue;
        const float dx = solid.origin.x - client->absOrigin.x;
        const float dy = solid.origin.y - client->absOrigin.y;
        const float dz = solid.origin.z - client->absOrigin.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            selected = i;
        }
    }
    if (selected >= mapMovementSolids_.size()) return;

    MapMovementSolid& hostage = mapMovementSolids_[selected];
    bool following = false;
    const char* action = "begin";
    const int userId = client->edict ? engine_->GetPlayerUserId(client->edict) : 0;
    if (hostage.followClientIndex == clientIndex) {
        const bool dropped = gameState_.DropHostage(hostage.actorOrdinal, clientIndex);
        if (dropped) {
            hostage.followClientIndex = 0;
            (void)FireObjectiveEvent("hostage_stops_following", userId, -1,
                                     static_cast<int>(hostage.actorOrdinal));
        }
        following = !dropped;
        action = "drop";
    } else if (gameState_.BeginHostageFollow(hostage.actorOrdinal, clientIndex)) {
        hostage.followClientIndex = clientIndex;
        following = true;
        (void)FireObjectiveEvent("hostage_follows", userId, -1,
                                 static_cast<int>(hostage.actorOrdinal));
    }

    char detail[256]{};
    std::snprintf(detail, sizeof(detail),
                  "client=%zu hostage=%zu action=%s following=%d distance=%.2f",
                  clientIndex, hostage.actorOrdinal, action, following ? 1 : 0,
                  std::sqrt(bestDistanceSq));
    EngineProbe("GAMEPLAY_HOSTAGE_USE", detail);
}

void ServerGameClients004Adapter::ProcessDeferredTeamSpawns() noexcept {
    if (!engine_ || !engine_->IsValid()) return;
    const std::size_t limit = clients_.MaxClients() < deferredTeamSpawn_.size()
        ? clients_.MaxClients() : deferredTeamSpawn_.size() - 1;
    for (std::size_t clientIndex = 1; clientIndex <= limit; ++clientIndex) {
        const int team = deferredTeamSpawn_[clientIndex];
        if (!build4100::IsPlayableTeam(team)) continue;

        // Clear before entering the engine trace. If build4100 faults or stalls
        // here, the callback probe's BEGIN marker is enough to localize the ABI
        // boundary and we never accidentally retry the same request every frame.
        deferredTeamSpawn_[clientIndex] = 0;
        ClientRecord* const client = clients_.Get(clientIndex);
        if (!client || !client->edict || client->state == ClientLifecycleState::Empty) continue;

        char beginDetail[160];
        std::snprintf(beginDetail, sizeof(beginDetail),
                      "index=%zu team=%d phase=gameframe", clientIndex, team);
        EngineProbe("GAMEPLAY_SPAWN_TRACE_BEGIN", beginDetail);

        const bool placed = PlaceClientAtTeamSpawn(clientIndex, team, "jointeam");
        if (!placed) {
            char failDetail[160];
            std::snprintf(failDetail, sizeof(failDetail),
                          "index=%zu team=%d phase=gameframe", clientIndex, team);
            EngineProbe("GAMEPLAY_SPAWN_TRACE_FAIL", failDetail);
            continue;
        }

        if (mapName_ == "de_dust2") {
            ProbeAdvancedMovementGeometry(clientIndex);
            ProbeDynamicNonPlayerCollision(clientIndex);
        } else if (mapName_ == "cs_office") {
            ProbeHostageCollision(clientIndex);
            ProbeHostageFollowing(clientIndex);
            ProbeGenericNpcMovement(clientIndex);
        } else if (mapName_ == "de_aztec" || mapName_ == "fy_pool_day") {
            ProbeWaterMovement(clientIndex);
        }
        ProbeDuckMovement(clientIndex);
        if (mapName_ == "cs_office" && !mapLadders_.empty()) ProbeLadderMovement(clientIndex);
        ProbeDynamicPlayerCollision(clientIndex);

        client->edict->stateFlags |= build4100::kEdictChanged;
        engine_->NotifyEdictFlagsChange(static_cast<int>(clientIndex));
        char endDetail[160];
        std::snprintf(endDetail, sizeof(endDetail),
                      "index=%zu team=%d phase=gameframe", clientIndex, team);
        EngineProbe("GAMEPLAY_SPAWN_TRACE_END", endDetail);
    }
}

void ServerGameClients004Adapter::ProbeAdvancedMovementGeometry(
    const std::size_t clientIndex) noexcept {
    if (advancedMovementGeometryProbeDone_) return;
    advancedMovementGeometryProbeDone_ = true;

    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !engineTrace_ || !engineTrace_->IsValid()) {
        EngineProbe("GAMEPLAY_ADVANCED_MOVEMENT_PROBE=FAIL", "reason=interfaces");
        return;
    }

    const build4100::Vector mins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector maxs{16.0f, 16.0f, 72.0f};

    // de_dust2's CT spawn set contains a deterministic walkable slope. Search
    // the actual map data rather than hard-coding its spawn index so the probe
    // remains robust to entity-order changes.
    bool slopeFound = false;
    std::size_t slopeSpawnIndex = 0;
    build4100::EngineGameTrace slopeTrace{};
    build4100::Vector slopeSpawn{};
    for (std::size_t i = 0; i < counterTerroristSpawns_.size(); ++i) {
        const build4100::Vector start = counterTerroristSpawns_[i];
        build4100::Vector end = start;
        end.z -= 128.0f;
        build4100::EngineGameTrace trace{};
        if (!engineTrace_->TraceWorldHull(start, end, mins, maxs,
                                          build4100::kMaskPlayerSolidBrushOnly, trace)) {
            continue;
        }
        if (trace.startSolid || trace.allSolid || trace.fraction >= 1.0f ||
            trace.plane.normal.z < 0.7f || trace.plane.normal.z >= 0.999f) {
            continue;
        }
        const float horizontalNormal = std::sqrt(trace.plane.normal.x * trace.plane.normal.x +
                                                 trace.plane.normal.y * trace.plane.normal.y);
        if (horizontalNormal < 0.01f) continue;
        slopeFound = true;
        slopeSpawnIndex = i;
        slopeTrace = trace;
        slopeSpawn = start;
        break;
    }

    bool slopeMovePass = false;
    if (slopeFound) {
        const build4100::Vector savedOrigin = client->absOrigin;
        const build4100::Vector savedVelocity = client->absVelocity;
        const bool savedOnGround = client->movementOnGround;
        const bool savedWorldTrace = client->movementWorldTraceEnabled;
        const std::int32_t savedOldButtons = client->movementOldButtons;

        const float horizontalNormal = std::sqrt(
            slopeTrace.plane.normal.x * slopeTrace.plane.normal.x +
            slopeTrace.plane.normal.y * slopeTrace.plane.normal.y);
        client->absOrigin = slopeTrace.endPos;
        client->absVelocity = build4100::Vector{
            100.0f * slopeTrace.plane.normal.x / horizontalNormal,
            100.0f * slopeTrace.plane.normal.y / horizontalNormal,
            0.0f,
        };
        client->movementOnGround = true;
        client->movementWorldTraceEnabled = true;
        client->movementOldButtons = 0;
        const build4100::Vector before = client->absOrigin;
        build4100::UserCmd cmd{};
        cmd.commandNumber = 0x1801;
        cmd.tickCount = 0x1801;
        const float interval = globals_ && globals_->interval_per_tick > 0.0f
            ? globals_->interval_per_tick : 0.015f;
        const bool moved = clients_.ApplyUsercmdMovement(
            clientIndex, client->edict, &cmd, 1, 1, interval, false);
        const build4100::Vector after = client->absOrigin;
        const float horizontalMove = std::sqrt(
            (after.x - before.x) * (after.x - before.x) +
            (after.y - before.y) * (after.y - before.y));
        const float dz = after.z - before.z;
        slopeMovePass = moved && client->movementOnGround && horizontalMove > 0.25f && dz < -0.01f;

        char detail[448];
        std::snprintf(detail, sizeof(detail),
                      "spawn_index=%zu spawn=%.3f,%.3f,%.3f origin=%.3f,%.3f,%.3f normal=%.4f,%.4f,%.4f move=%.3f dz=%.3f onground=%d",
                      slopeSpawnIndex, slopeSpawn.x, slopeSpawn.y, slopeSpawn.z,
                      before.x, before.y, before.z, slopeTrace.plane.normal.x,
                      slopeTrace.plane.normal.y, slopeTrace.plane.normal.z,
                      horizontalMove, dz, client->movementOnGround ? 1 : 0);
        EngineProbe(slopeMovePass ? "GAMEPLAY_WALKABLE_SLOPE=PASS" :
                                    "GAMEPLAY_WALKABLE_SLOPE=FAIL", detail);

        client->absOrigin = savedOrigin;
        client->absVelocity = savedVelocity;
        client->movementOnGround = savedOnGround;
        client->movementWorldTraceEnabled = savedWorldTrace;
        client->movementOldButtons = savedOldButtons;
    } else {
        EngineProbe("GAMEPLAY_WALKABLE_SLOPE=FAIL", "reason=no_ct_walkable_slope");
    }

    // The original de_dust2 BSP contains the staircase at x=256..384,
    // y=1648..1792 with 8-unit risers every 12 units. Locate the transition
    // using the real build4100 world hull, then run one temporary movement tick
    // across it. The client's real network state is restored immediately.
    struct GroundSample {
        float y = 0.0f;
        build4100::EngineGameTrace trace{};
        bool valid = false;
    };
    GroundSample previous{};
    GroundSample low{};
    GroundSample high{};
    bool stepFound = false;
    constexpr float kStepProbeX = 320.0f;
    for (int yi = 1620; yi <= 1700; ++yi) {
        build4100::Vector start{kStepProbeX, static_cast<float>(yi), 128.0f};
        build4100::Vector end = start;
        end.z = -128.0f;
        build4100::EngineGameTrace trace{};
        const bool traced = engineTrace_->TraceWorldHull(
            start, end, mins, maxs, build4100::kMaskPlayerSolidBrushOnly, trace);
        GroundSample current{};
        current.y = static_cast<float>(yi);
        current.trace = trace;
        current.valid = traced && !trace.startSolid && !trace.allSolid &&
                        trace.fraction < 1.0f && trace.plane.normal.z >= 0.7f;
        if (previous.valid && current.valid) {
            const float rise = current.trace.endPos.z - previous.trace.endPos.z;
            if (rise > 0.5f && rise <= 18.1f) {
                low = previous;
                high = current;
                stepFound = true;
                break;
            }
        }
        previous = current;
    }

    bool stepMovePass = false;
    if (stepFound) {
        const build4100::Vector savedOrigin = client->absOrigin;
        const build4100::Vector savedVelocity = client->absVelocity;
        const bool savedOnGround = client->movementOnGround;
        const bool savedWorldTrace = client->movementWorldTraceEnabled;
        const std::int32_t savedOldButtons = client->movementOldButtons;

        client->absOrigin = low.trace.endPos;
        client->absOrigin.x = kStepProbeX;
        client->absOrigin.y = low.y;
        client->absVelocity = build4100::Vector{0.0f, 250.0f, 0.0f};
        client->movementOnGround = true;
        client->movementWorldTraceEnabled = true;
        client->movementOldButtons = 0;
        const build4100::Vector before = client->absOrigin;
        build4100::UserCmd cmd{};
        cmd.commandNumber = 0x1802;
        cmd.tickCount = 0x1802;
        const float interval = globals_ && globals_->interval_per_tick > 0.0f
            ? globals_->interval_per_tick : 0.015f;
        const bool moved = clients_.ApplyUsercmdMovement(
            clientIndex, client->edict, &cmd, 1, 1, interval, false);
        const build4100::Vector after = client->absOrigin;
        const float horizontalMove = after.y - before.y;
        const float dz = after.z - before.z;
        const float expectedRise = high.trace.endPos.z - low.trace.endPos.z;
        stepMovePass = moved && client->movementOnGround && horizontalMove > 1.0f &&
                       dz > 0.5f && dz <= 18.2f &&
                       std::fabs(dz - expectedRise) < 1.0f;

        char detail[448];
        std::snprintf(detail, sizeof(detail),
                      "x=%.1f low_y=%.1f high_y=%.1f low_z=%.3f high_z=%.3f expected_rise=%.3f move=%.3f dz=%.3f origin=%.3f,%.3f,%.3f onground=%d",
                      kStepProbeX, low.y, high.y, low.trace.endPos.z, high.trace.endPos.z,
                      expectedRise, horizontalMove, dz, after.x, after.y, after.z,
                      client->movementOnGround ? 1 : 0);
        EngineProbe(stepMovePass ? "GAMEPLAY_STEPMOVE=PASS" : "GAMEPLAY_STEPMOVE=FAIL", detail);

        client->absOrigin = savedOrigin;
        client->absVelocity = savedVelocity;
        client->movementOnGround = savedOnGround;
        client->movementWorldTraceEnabled = savedWorldTrace;
        client->movementOldButtons = savedOldButtons;
    } else {
        EngineProbe("GAMEPLAY_STEPMOVE=FAIL", "reason=de_dust2_stair_transition_not_found");
    }

    char summary[192];
    std::snprintf(summary, sizeof(summary), "slope=%d step=%d", slopeMovePass ? 1 : 0,
                  stepMovePass ? 1 : 0);
    EngineProbe((slopeMovePass && stepMovePass) ? "GAMEPLAY_ADVANCED_MOVEMENT_PROBE=PASS" :
                                                 "GAMEPLAY_ADVANCED_MOVEMENT_PROBE=FAIL",
                summary);
}

void ServerGameClients004Adapter::ProbeDynamicPlayerCollision(
    const std::size_t clientIndex) noexcept {
    if (dynamicPlayerCollisionProbeDone_) return;
    dynamicPlayerCollisionProbeDone_ = true;

    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !engineTrace_ || !engineTrace_->IsValid()) {
        EngineProbe("GAMEPLAY_DYNAMIC_PLAYER_COLLISION=FAIL", "reason=interfaces");
        return;
    }

    std::size_t blockerIndex = 0;
    ClientRecord* blocker = nullptr;
    for (std::size_t i = 1; i <= clients_.MaxClients(); ++i) {
        if (i == clientIndex) continue;
        ClientRecord* const candidate = clients_.Get(i);
        if (!candidate || candidate->state != ClientLifecycleState::Active ||
            candidate->isDead || candidate->isObserver) {
            continue;
        }
        blockerIndex = i;
        blocker = candidate;
        break;
    }
    if (!blocker) {
        EngineProbe("GAMEPLAY_DYNAMIC_PLAYER_COLLISION=FAIL", "reason=no_active_blocker");
        return;
    }

    struct SavedMovementState {
        build4100::Vector origin{};
        build4100::Vector velocity{};
        build4100::Vector mins{};
        build4100::Vector maxs{};
        bool onGround = false;
        bool worldTrace = false;
        std::int32_t oldButtons = 0;
        bool dead = false;
        bool observer = false;
    };
    const auto save = [](const ClientRecord& record) noexcept {
        return SavedMovementState{record.absOrigin, record.absVelocity,
                                  record.playerMins, record.playerMaxs,
                                  record.movementOnGround, record.movementWorldTraceEnabled,
                                  record.movementOldButtons, record.isDead, record.isObserver};
    };
    const auto restore = [](ClientRecord& record, const SavedMovementState& state) noexcept {
        record.absOrigin = state.origin;
        record.absVelocity = state.velocity;
        record.playerMins = state.mins;
        record.playerMaxs = state.maxs;
        record.movementOnGround = state.onGround;
        record.movementWorldTraceEnabled = state.worldTrace;
        record.movementOldButtons = state.oldButtons;
        record.isDead = state.dead;
        record.isObserver = state.observer;
    };

    const SavedMovementState savedClient = save(*client);
    const SavedMovementState savedBlocker = save(*blocker);
    const build4100::Vector mins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector maxs{16.0f, 16.0f, 72.0f};
    const build4100::Vector directions[] = {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
    };

    build4100::Vector direction{};
    bool clearDirection = false;
    for (const build4100::Vector& candidate : directions) {
        build4100::Vector end = client->absOrigin;
        end.x += candidate.x * 4.0f;
        end.y += candidate.y * 4.0f;
        build4100::EngineGameTrace trace{};
        if (engineTrace_->TraceWorldHull(client->absOrigin, end, mins, maxs,
                                         build4100::kMaskPlayerSolidBrushOnly, trace) &&
            !trace.startSolid && !trace.allSolid && trace.fraction >= 0.999f) {
            direction = candidate;
            clearDirection = true;
            break;
        }
    }
    if (!clearDirection) {
        EngineProbe("GAMEPLAY_DYNAMIC_PLAYER_COLLISION=FAIL", "reason=no_clear_direction");
        return;
    }

    const build4100::Vector before = client->absOrigin;
    client->playerMins = mins;
    client->playerMaxs = maxs;
    client->absVelocity = build4100::Vector{direction.x * 200.0f,
                                            direction.y * 200.0f, 0.0f};
    client->movementOnGround = true;
    client->movementWorldTraceEnabled = true;
    client->movementOldButtons = 0;

    blocker->playerMins = mins;
    blocker->playerMaxs = maxs;
    blocker->absOrigin = build4100::Vector{before.x + direction.x * 34.0f,
                                          before.y + direction.y * 34.0f,
                                          before.z};
    blocker->absVelocity = build4100::Vector{};
    blocker->isDead = false;
    blocker->isObserver = false;

    build4100::UserCmd cmd{};
    cmd.commandNumber = 0x1901;
    cmd.tickCount = 0x1901;
    const float interval = globals_ && globals_->interval_per_tick > 0.0f
        ? globals_->interval_per_tick : 0.015f;
    const bool moved = clients_.ApplyUsercmdMovement(
        clientIndex, client->edict, &cmd, 1, 1, interval, false);
    const build4100::Vector after = client->absOrigin;
    const float travel = (after.x - before.x) * direction.x +
                         (after.y - before.y) * direction.y;
    const float remainingSpeed = client->absVelocity.x * direction.x +
                                 client->absVelocity.y * direction.y;
    const bool pass = moved && client->movementOnGround &&
                      travel > 1.90f && travel < 2.10f &&
                      std::fabs(remainingSpeed) < 0.01f;

    char detail[448];
    std::snprintf(detail, sizeof(detail),
                  "mover=%zu blocker=%zu dir=%.0f,%.0f before=%.3f,%.3f,%.3f after=%.3f,%.3f,%.3f travel=%.3f remaining_speed=%.3f onground=%d",
                  clientIndex, blockerIndex, direction.x, direction.y,
                  before.x, before.y, before.z, after.x, after.y, after.z,
                  travel, remainingSpeed, client->movementOnGround ? 1 : 0);
    EngineProbe(pass ? "GAMEPLAY_DYNAMIC_PLAYER_COLLISION=PASS" :
                       "GAMEPLAY_DYNAMIC_PLAYER_COLLISION=FAIL", detail);

    restore(*client, savedClient);
    restore(*blocker, savedBlocker);
}

void ServerGameClients004Adapter::ProbeDynamicNonPlayerCollision(
    const std::size_t clientIndex) noexcept {
    if (dynamicNonPlayerCollisionProbeDone_) return;
    dynamicNonPlayerCollisionProbeDone_ = true;

    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !engineTrace_ || !engineTrace_->IsValid() ||
        !modelInfo_ || !physicsCollision_) {
        EngineProbe("GAMEPLAY_DYNAMIC_NONPLAYER_COLLISION=FAIL", "reason=interfaces");
        return;
    }
    if (mapMovementSolids_.empty()) {
        EngineProbe("GAMEPLAY_DYNAMIC_NONPLAYER_COLLISION=FAIL", "reason=no_map_solids");
        return;
    }

    using CollideGetAabbFn = void (*)(void*, build4100::Vector*, build4100::Vector*,
                                      const void*, const build4100::Vector*,
                                      const build4100::QAngle*);
    const CollideGetAabbFn collideGetAabb =
        detail::VTableSlot<CollideGetAabbFn>(physicsCollision_, 19);
    if (!collideGetAabb) {
        EngineProbe("GAMEPLAY_DYNAMIC_NONPLAYER_COLLISION=FAIL", "reason=physics_aabb_slot");
        return;
    }

    const build4100::Vector mins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector maxs{16.0f, 16.0f, 72.0f};
    for (std::size_t i = 0; i < mapMovementSolids_.size(); ++i) {
        MapMovementSolid& solid = mapMovementSolids_[i];
        if (solid.kind != MapMovementSolid::Kind::StudioPhysics ||
            !ResolveStudioCollision(solid)) {
            continue;
        }
        const auto* const collide = static_cast<const VCollide4100*>(solid.vcollide);
        const std::size_t count = collide->SolidCount();
        build4100::Vector aabbMins{std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::max()};
        build4100::Vector aabbMaxs{-std::numeric_limits<float>::max(),
                                  -std::numeric_limits<float>::max(),
                                  -std::numeric_limits<float>::max()};
        bool haveBounds = false;
        for (std::size_t part = 0; part < count; ++part) {
            const void* const phys = collide->solids[part];
            if (!phys) continue;
            build4100::Vector partMins{};
            build4100::Vector partMaxs{};
            collideGetAabb(physicsCollision_, &partMins, &partMaxs, phys,
                           &solid.origin, &solid.angles);
            aabbMins.x = std::min(aabbMins.x, partMins.x);
            aabbMins.y = std::min(aabbMins.y, partMins.y);
            aabbMins.z = std::min(aabbMins.z, partMins.z);
            aabbMaxs.x = std::max(aabbMaxs.x, partMaxs.x);
            aabbMaxs.y = std::max(aabbMaxs.y, partMaxs.y);
            aabbMaxs.z = std::max(aabbMaxs.z, partMaxs.z);
            haveBounds = true;
        }
        if (!haveBounds) continue;

        const build4100::Vector center{(aabbMins.x + aabbMaxs.x) * 0.5f,
                                       (aabbMins.y + aabbMaxs.y) * 0.5f,
                                       (aabbMins.z + aabbMaxs.z) * 0.5f};
        // Low props can be shorter than the 72-unit player hull. A single
        // center-36 origin would then put the player's feet below the prop (and
        // often below the floor), producing a world startSolid before the
        // vphysics mesh is exercised. Try three origins that all vertically
        // overlap the prop but bias the hull above its lower bound.
        const float probeZ[] = {
            aabbMins.z + 0.03125f,
            center.z - 36.0f,
            aabbMaxs.z - 71.96875f,
        };
        for (const float z : probeZ) {
            for (int axis = 0; axis < 2; ++axis) {
                for (int sign = -1; sign <= 1; sign += 2) {
                    build4100::Vector start = center;
                    build4100::Vector end = center;
                    start.z = z;
                    end.z = z;
                    const float lo = axis == 0 ? aabbMins.x : aabbMins.y;
                    const float hi = axis == 0 ? aabbMaxs.x : aabbMaxs.y;
                    if (axis == 0) {
                        start.x = sign < 0 ? lo - 64.0f : hi + 64.0f;
                        end.x = sign < 0 ? hi + 64.0f : lo - 64.0f;
                    } else {
                        start.y = sign < 0 ? lo - 64.0f : hi + 64.0f;
                        end.y = sign < 0 ? hi + 64.0f : lo - 64.0f;
                    }

                    build4100::EngineGameTrace world{};
                    if (!engineTrace_->TraceWorldHull(start, end, mins, maxs,
                                                       build4100::kMaskPlayerSolid, world) ||
                        world.startSolid || world.allSolid) {
                        continue;
                    }
                    PlayerHullTraceResult dynamic{};
                    if (!TraceMovementHull(start, end, mins, maxs,
                                           build4100::kMaskPlayerSolid, dynamic) ||
                        dynamic.startSolid || dynamic.allSolid ||
                        dynamic.hitDynamicSolidIndex == 0 || dynamic.fraction >= 1.0f ||
                        !(world.fraction > dynamic.fraction + 0.01f)) {
                        continue;
                    }

                    const std::size_t hitIndex = dynamic.hitDynamicSolidIndex - 1;
                    const MapMovementSolid& hit = mapMovementSolids_[hitIndex];
                    char detail[768];
                    std::snprintf(detail, sizeof(detail),
                                  "mover=%zu solid=%zu class=%s model=%s axis=%c sign=%d z=%.3f fraction=%.4f world_fraction=%.4f normal=%.3f,%.3f,%.3f solids=%zu",
                                  clientIndex, dynamic.hitDynamicSolidIndex,
                                  hit.classname.c_str(), hit.model.c_str(), axis == 0 ? 'x' : 'y', sign, z,
                                  dynamic.fraction, world.fraction,
                                  dynamic.planeNormal.x, dynamic.planeNormal.y, dynamic.planeNormal.z,
                                  count);
                    EngineProbe("GAMEPLAY_DYNAMIC_NONPLAYER_COLLISION=PASS", detail);
                    return;
                }
            }
        }
    }

    char detail[160];
    std::snprintf(detail, sizeof(detail), "reason=no_unobstructed_prop solids=%zu",
                  mapMovementSolids_.size());
    EngineProbe("GAMEPLAY_DYNAMIC_NONPLAYER_COLLISION=FAIL", detail);
}

void ServerGameClients004Adapter::ProbeHostageCollision(
    const std::size_t clientIndex) noexcept {
    if (hostageCollisionProbeDone_) return;

    bool haveHostage = false;
    for (const auto& solid : mapMovementSolids_) {
        if (solid.kind == MapMovementSolid::Kind::HostageBBox) {
            haveHostage = true;
            break;
        }
    }
    if (!haveHostage) {
        hostageCollisionProbeDone_ = true;
        return;
    }
    hostageCollisionProbeDone_ = true;

    if (!engineTrace_ || !engineTrace_->IsValid() || !modelInfo_) {
        EngineProbe("GAMEPLAY_HOSTAGE_COLLISION=FAIL", "reason=interfaces");
        return;
    }

    const build4100::Vector mins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector maxs{16.0f, 16.0f, 72.0f};
    std::size_t hostageCount = 0;
    for (std::size_t i = 0; i < mapMovementSolids_.size(); ++i) {
        MapMovementSolid& solid = mapMovementSolids_[i];
        if (solid.kind != MapMovementSolid::Kind::HostageBBox) continue;
        ++hostageCount;
        if (!ResolveActorBounds(solid)) continue;

        const build4100::Vector worldMins{
            solid.origin.x + solid.bboxMins.x,
            solid.origin.y + solid.bboxMins.y,
            solid.origin.z + solid.bboxMins.z,
        };
        const build4100::Vector worldMaxs{
            solid.origin.x + solid.bboxMaxs.x,
            solid.origin.y + solid.bboxMaxs.y,
            solid.origin.z + solid.bboxMaxs.z,
        };
        const build4100::Vector center{
            (worldMins.x + worldMaxs.x) * 0.5f,
            (worldMins.y + worldMaxs.y) * 0.5f,
            (worldMins.z + worldMaxs.z) * 0.5f,
        };
        const float probeZ[] = {
            std::max(solid.origin.z, worldMins.z + 0.03125f),
            center.z - 36.0f,
            worldMaxs.z - 71.96875f,
        };
        for (const float z : probeZ) {
            for (int axis = 0; axis < 2; ++axis) {
                for (int sign = -1; sign <= 1; sign += 2) {
                    build4100::Vector start = center;
                    build4100::Vector end = center;
                    start.z = z;
                    end.z = z;
                    const float lo = axis == 0 ? worldMins.x : worldMins.y;
                    const float hi = axis == 0 ? worldMaxs.x : worldMaxs.y;
                    if (axis == 0) {
                        start.x = sign < 0 ? lo - 80.0f : hi + 80.0f;
                        end.x = sign < 0 ? hi + 80.0f : lo - 80.0f;
                    } else {
                        start.y = sign < 0 ? lo - 80.0f : hi + 80.0f;
                        end.y = sign < 0 ? hi + 80.0f : lo - 80.0f;
                    }

                    build4100::EngineGameTrace world{};
                    if (!engineTrace_->TraceWorldHull(start, end, mins, maxs,
                                                       build4100::kMaskPlayerSolid, world) ||
                        world.startSolid || world.allSolid) {
                        continue;
                    }
                    PlayerHullTraceResult dynamic{};
                    if (!TraceMovementHull(start, end, mins, maxs,
                                           build4100::kMaskPlayerSolid, dynamic) ||
                        dynamic.startSolid || dynamic.allSolid ||
                        dynamic.hitDynamicSolidIndex != i + 1 || dynamic.fraction >= 1.0f ||
                        !(world.fraction > dynamic.fraction + 0.01f)) {
                        continue;
                    }

                    char detail[896];
                    std::snprintf(detail, sizeof(detail),
                                  "mover=%zu hostage=%zu model=%s origin=%.3f,%.3f,%.3f bounds=%.3f,%.3f,%.3f:%.3f,%.3f,%.3f axis=%c sign=%d z=%.3f fraction=%.4f world_fraction=%.4f normal=%.3f,%.3f,%.3f hostages=%zu",
                                  clientIndex, i + 1, solid.model.c_str(),
                                  solid.origin.x, solid.origin.y, solid.origin.z,
                                  solid.bboxMins.x, solid.bboxMins.y, solid.bboxMins.z,
                                  solid.bboxMaxs.x, solid.bboxMaxs.y, solid.bboxMaxs.z,
                                  axis == 0 ? 'x' : 'y', sign, z, dynamic.fraction, world.fraction,
                                  dynamic.planeNormal.x, dynamic.planeNormal.y, dynamic.planeNormal.z,
                                  hostageCount);
                    EngineProbe("GAMEPLAY_HOSTAGE_COLLISION=PASS", detail);
                    return;
                }
            }
        }
    }

    char detail[192];
    std::snprintf(detail, sizeof(detail), "reason=no_unobstructed_hostage hostages=%zu", hostageCount);
    EngineProbe("GAMEPLAY_HOSTAGE_COLLISION=FAIL", detail);
}

void ServerGameClients004Adapter::ProbeHostageFollowing(
    const std::size_t clientIndex) noexcept {
    if (hostageFollowingProbeDone_) return;
    hostageFollowingProbeDone_ = true;
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !engineTrace_ || !engineTrace_->IsValid() || !globals_) {
        EngineProbe("GAMEPLAY_HOSTAGE_FOLLOW=FAIL", "reason=interfaces");
        return;
    }

    std::size_t solidIndex = mapMovementSolids_.size();
    for (std::size_t i = 0; i < mapMovementSolids_.size(); ++i) {
        if (mapMovementSolids_[i].kind == MapMovementSolid::Kind::HostageBBox &&
            ResolveActorBounds(mapMovementSolids_[i])) {
            solidIndex = i;
            break;
        }
    }
    if (solidIndex >= mapMovementSolids_.size()) {
        EngineProbe("GAMEPLAY_HOSTAGE_FOLLOW=FAIL", "reason=no_resolved_hostage");
        return;
    }

    MapMovementSolid& hostage = mapMovementSolids_[solidIndex];
    const MapMovementSolid savedHostage = hostage;
    const build4100::Vector savedOrigin = client->absOrigin;
    const build4100::QAngle savedAngles = client->absAngles;
    const build4100::Vector playerMins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector playerMaxs{16.0f, 16.0f, 72.0f};
    const auto clearHull = [&](const build4100::Vector& at,
                               const build4100::Vector& mins,
                               const build4100::Vector& maxs) noexcept {
        build4100::Vector tiny = at;
        tiny.z += 0.03125f;
        build4100::EngineGameTrace trace{};
        return engineTrace_->TraceWorldHull(at, tiny, mins, maxs,
                                            build4100::kMaskPlayerSolidBrushOnly, trace) &&
               !trace.startSolid && !trace.allSolid;
    };

    build4100::Vector leaderOrigin{};
    build4100::Vector actorOrigin{};
    bool found = false;
    for (int gy = -4; gy <= 4 && !found; ++gy) {
        for (int gx = -4; gx <= 4 && !found; ++gx) {
            const build4100::Vector leader{
                savedOrigin.x + static_cast<float>(gx * 64),
                savedOrigin.y + static_cast<float>(gy * 64),
                savedOrigin.z,
            };
            const build4100::Vector actor{
                leader.x - 104.0f,
                leader.y,
                leader.z - hostage.bboxMins.z + 0.03125f,
            };
            if (!clearHull(leader, playerMins, playerMaxs) ||
                !clearHull(actor, hostage.bboxMins, hostage.bboxMaxs)) {
                continue;
            }
            build4100::Vector target = actor;
            target.x = leader.x - 52.0f;
            target.z = leader.z - hostage.bboxMins.z + 0.03125f;
            build4100::EngineGameTrace path{};
            if (!engineTrace_->TraceWorldHull(actor, target, hostage.bboxMins, hostage.bboxMaxs,
                                               build4100::kMaskPlayerSolidBrushOnly, path) ||
                path.startSolid || path.allSolid || path.fraction < 0.99f) {
                continue;
            }
            leaderOrigin = leader;
            actorOrigin = actor;
            found = true;
        }
    }
    if (!found) {
        EngineProbe("GAMEPLAY_HOSTAGE_FOLLOW=FAIL", "reason=no_clear_follow_pair");
        return;
    }

    client->absOrigin = leaderOrigin;
    client->absAngles = build4100::QAngle{0.0f, 0.0f, 0.0f};
    hostage.origin = actorOrigin;
    hostage.followClientIndex = 0;
    const bool begin = gameState_.BeginHostageFollow(hostage.actorOrdinal, clientIndex);
    if (begin) hostage.followClientIndex = clientIndex;
    const build4100::Vector before = hostage.origin;
    const float frameTime = globals_->interval_per_tick > 0.0f ? globals_->interval_per_tick : 0.015f;
    const bool moved = begin && AdvanceDynamicActor(hostage, frameTime);
    const build4100::Vector after = hostage.origin;
    const float travel = std::sqrt((after.x - before.x) * (after.x - before.x) +
                                   (after.y - before.y) * (after.y - before.y) +
                                   (after.z - before.z) * (after.z - before.z));

    build4100::Vector sweepStart = after;
    build4100::Vector sweepEnd = after;
    // Approach from the leader side: the placement search already proved this
    // corridor clear, so the moved actor must become the first dynamic hit.
    sweepStart.x += 64.0f;
    sweepEnd.x -= 64.0f;
    build4100::EngineGameTrace world{};
    PlayerHullTraceResult dynamic{};
    const bool worldOk = engineTrace_->TraceWorldHull(sweepStart, sweepEnd, playerMins, playerMaxs,
                                                       build4100::kMaskPlayerSolid, world) &&
                         !world.startSolid && !world.allSolid;
    const bool dynamicOk = TraceMovementHull(sweepStart, sweepEnd, playerMins, playerMaxs,
                                              build4100::kMaskPlayerSolid, dynamic) &&
                           !dynamic.startSolid && !dynamic.allSolid && dynamic.fraction < 1.0f &&
                           dynamic.hitDynamicSolidIndex == solidIndex + 1 &&
                           (!worldOk || world.fraction > dynamic.fraction + 0.01f);
    const HostageRecord* const state = gameState_.Hostages().Get(hostage.actorOrdinal);
    const bool stateOk = state && state->leaderClientIndex == static_cast<int>(clientIndex);
    const bool pass = begin && moved && travel > 1.0f && stateOk && dynamicOk;

    char detail[640]{};
    std::snprintf(detail, sizeof(detail),
                  "mover=%zu hostage=%zu ordinal=%zu before=%.3f,%.3f,%.3f after=%.3f,%.3f,%.3f travel=%.3f state=%d dynamic_origin=%d collision=%d",
                  clientIndex, solidIndex + 1, hostage.actorOrdinal,
                  before.x, before.y, before.z, after.x, after.y, after.z, travel,
                  stateOk ? 1 : 0, moved ? 1 : 0, dynamicOk ? 1 : 0);
    EngineProbe(pass ? "GAMEPLAY_HOSTAGE_FOLLOW=PASS" : "GAMEPLAY_HOSTAGE_FOLLOW=FAIL", detail);

    if (begin) (void)gameState_.DropHostage(hostage.actorOrdinal, clientIndex);
    hostage = savedHostage;
    client->absOrigin = savedOrigin;
    client->absAngles = savedAngles;
}

void ServerGameClients004Adapter::ProbeGenericNpcMovement(
    const std::size_t clientIndex) noexcept {
    if (genericNpcProbeDone_) return;
    genericNpcProbeDone_ = true;
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !engineTrace_ || !engineTrace_->IsValid() || !globals_) {
        EngineProbe("GAMEPLAY_GENERIC_NPC=FAIL", "reason=interfaces");
        return;
    }

    const build4100::Vector savedOrigin = client->absOrigin;
    const build4100::QAngle savedAngles = client->absAngles;
    const build4100::Vector playerMins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector playerMaxs{16.0f, 16.0f, 72.0f};
    MapMovementSolid npc{};
    npc.kind = MapMovementSolid::Kind::NpcBBox;
    npc.classname = "npc_furniture";
    npc.origin = savedOrigin;
    npc.bboxMins = build4100::Vector{-16.0f, -16.0f, 0.0f};
    npc.bboxMaxs = build4100::Vector{16.0f, 16.0f, 64.0f};
    npc.boundsResolved = true;
    npc.resolveAttempted = true;
    npc.followClientIndex = clientIndex;

    const auto clearHull = [&](const build4100::Vector& at,
                               const build4100::Vector& mins,
                               const build4100::Vector& maxs) noexcept {
        build4100::Vector tiny = at;
        tiny.z += 0.03125f;
        build4100::EngineGameTrace trace{};
        return engineTrace_->TraceWorldHull(at, tiny, mins, maxs,
                                            build4100::kMaskPlayerSolidBrushOnly, trace) &&
               !trace.startSolid && !trace.allSolid;
    };

    build4100::Vector leaderOrigin{};
    build4100::Vector actorOrigin{};
    bool found = false;
    for (int gy = -4; gy <= 4 && !found; ++gy) {
        for (int gx = -4; gx <= 4 && !found; ++gx) {
            const build4100::Vector leader{
                savedOrigin.x + static_cast<float>(gx * 64),
                savedOrigin.y + static_cast<float>(gy * 64),
                savedOrigin.z,
            };
            const build4100::Vector actor{leader.x - 128.0f, leader.y, leader.z + 0.03125f};
            if (!clearHull(leader, playerMins, playerMaxs) ||
                !clearHull(actor, npc.bboxMins, npc.bboxMaxs)) continue;
            build4100::Vector target = actor;
            target.x = leader.x - 64.0f;
            build4100::EngineGameTrace path{};
            if (!engineTrace_->TraceWorldHull(actor, target, npc.bboxMins, npc.bboxMaxs,
                                               build4100::kMaskPlayerSolidBrushOnly, path) ||
                path.startSolid || path.allSolid || path.fraction < 0.99f) continue;
            leaderOrigin = leader;
            actorOrigin = actor;
            found = true;
        }
    }
    if (!found) {
        EngineProbe("GAMEPLAY_GENERIC_NPC=FAIL", "reason=no_clear_npc_pair");
        return;
    }

    client->absOrigin = leaderOrigin;
    client->absAngles = build4100::QAngle{0.0f, 0.0f, 0.0f};
    npc.origin = actorOrigin;
    mapMovementSolids_.push_back(npc);
    const std::size_t solidIndex = mapMovementSolids_.size() - 1;
    MapMovementSolid& liveNpc = mapMovementSolids_.back();
    const build4100::Vector before = liveNpc.origin;
    const float frameTime = globals_->interval_per_tick > 0.0f ? globals_->interval_per_tick : 0.015f;
    const bool moved = AdvanceDynamicActor(liveNpc, frameTime);
    const build4100::Vector after = liveNpc.origin;
    const float travel = std::sqrt((after.x - before.x) * (after.x - before.x) +
                                   (after.y - before.y) * (after.y - before.y) +
                                   (after.z - before.z) * (after.z - before.z));

    build4100::Vector sweepStart = after;
    build4100::Vector sweepEnd = after;
    // Approach from the leader side: the placement search already proved this
    // corridor clear, so the moved actor must become the first dynamic hit.
    sweepStart.x += 64.0f;
    sweepEnd.x -= 64.0f;
    build4100::EngineGameTrace world{};
    PlayerHullTraceResult dynamic{};
    const bool worldOk = engineTrace_->TraceWorldHull(sweepStart, sweepEnd, playerMins, playerMaxs,
                                                       build4100::kMaskPlayerSolid, world) &&
                         !world.startSolid && !world.allSolid;
    const bool dynamicOk = TraceMovementHull(sweepStart, sweepEnd, playerMins, playerMaxs,
                                              build4100::kMaskPlayerSolid, dynamic) &&
                           !dynamic.startSolid && !dynamic.allSolid && dynamic.fraction < 1.0f &&
                           dynamic.hitDynamicSolidIndex == solidIndex + 1 &&
                           (!worldOk || world.fraction > dynamic.fraction + 0.01f);
    const bool pass = moved && travel > 1.0f && dynamicOk;
    char detail[512]{};
    std::snprintf(detail, sizeof(detail),
                  "mover=%zu solid=%zu class=%s before=%.3f,%.3f,%.3f after=%.3f,%.3f,%.3f travel=%.3f dynamic_origin=%d collision=%d",
                  clientIndex, solidIndex + 1, liveNpc.classname.c_str(),
                  before.x, before.y, before.z, after.x, after.y, after.z, travel,
                  moved ? 1 : 0, dynamicOk ? 1 : 0);
    EngineProbe(pass ? "GAMEPLAY_GENERIC_NPC=PASS" : "GAMEPLAY_GENERIC_NPC=FAIL", detail);
    mapMovementSolids_.pop_back();
    client->absOrigin = savedOrigin;
    client->absAngles = savedAngles;
}

void ServerGameClients004Adapter::ProbeDuckMovement(const std::size_t clientIndex) noexcept {
    if (duckMovementProbeDone_) return;
    duckMovementProbeDone_ = true;
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !globals_) {
        EngineProbe("GAMEPLAY_DUCK_MOVEMENT=FAIL", "reason=interfaces");
        return;
    }

    struct SavedState {
        build4100::Vector origin{};
        build4100::Vector velocity{};
        build4100::Vector mins{};
        build4100::Vector maxs{};
        build4100::Vector baseVelocity{};
        bool onGround = false;
        bool worldTrace = false;
        bool ducked = false;
        bool onLadder = false;
        std::size_t ladderIndex = 0;
        std::int32_t oldButtons = 0;
        std::uint8_t waterLevel = 0;
        std::int32_t waterType = 0;
    };
    const SavedState saved{client->absOrigin, client->absVelocity, client->playerMins,
                           client->playerMaxs, client->movementBaseVelocity,
                           client->movementOnGround, client->movementWorldTraceEnabled,
                           client->movementDucked, client->movementOnLadder,
                           client->movementLadderIndex, client->movementOldButtons,
                           client->movementWaterLevel, client->movementWaterType};
    const auto restore = [&]() noexcept {
        client->absOrigin = saved.origin;
        client->absVelocity = saved.velocity;
        client->playerMins = saved.mins;
        client->playerMaxs = saved.maxs;
        client->movementBaseVelocity = saved.baseVelocity;
        client->movementOnGround = saved.onGround;
        client->movementWorldTraceEnabled = saved.worldTrace;
        client->movementDucked = saved.ducked;
        client->movementOnLadder = saved.onLadder;
        client->movementLadderIndex = saved.ladderIndex;
        client->movementOldButtons = saved.oldButtons;
        client->movementWaterLevel = saved.waterLevel;
        client->movementWaterType = saved.waterType;
    };

    client->absVelocity = {};
    client->playerMins = build4100::Vector{-16.0f, -16.0f, 0.0f};
    client->playerMaxs = build4100::Vector{16.0f, 16.0f, 72.0f};
    client->movementWorldTraceEnabled = true;
    client->movementOnGround = true;
    client->movementDucked = false;
    client->movementOnLadder = false;
    client->movementLadderIndex = 0;
    client->movementOldButtons = 0;

    build4100::UserCmd duck{};
    duck.commandNumber = 0x2301;
    duck.tickCount = 0x2301;
    duck.buttons = 1 << 2;
    const bool duckMoved = clients_.ApplyUsercmdMovement(
        clientIndex, client->edict, &duck, 1, 1, globals_->interval_per_tick, false);
    const bool enteredDuck = duckMoved && client->movementDucked &&
        std::fabs(client->playerMaxs.z - 36.0f) < 0.01f;
    const float duckHeight = client->playerMaxs.z;

    build4100::UserCmd stand{};
    stand.commandNumber = 0x2302;
    stand.tickCount = 0x2302;
    const bool standMoved = clients_.ApplyUsercmdMovement(
        clientIndex, client->edict, &stand, 1, 1, globals_->interval_per_tick, false);
    const bool leftDuck = standMoved && !client->movementDucked &&
        std::fabs(client->playerMaxs.z - 72.0f) < 0.01f;
    const float standHeight = client->playerMaxs.z;
    const bool pass = enteredDuck && leftDuck;

    char detail[256]{};
    std::snprintf(detail, sizeof(detail),
                  "mover=%zu ducked=%d duck_hull=%.1f unducked=%d stand_hull=%.1f map=%s",
                  clientIndex, enteredDuck ? 1 : 0, duckHeight, leftDuck ? 1 : 0,
                  standHeight, mapName_.c_str());
    EngineProbe(pass ? "GAMEPLAY_DUCK_MOVEMENT=PASS" : "GAMEPLAY_DUCK_MOVEMENT=FAIL", detail);
    restore();
}

void ServerGameClients004Adapter::ProbeLadderMovement(const std::size_t clientIndex) noexcept {
    if (ladderMovementProbeDone_) return;
    ladderMovementProbeDone_ = true;
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !globals_ || mapLadders_.empty()) {
        EngineProbe("GAMEPLAY_LADDER_MOVEMENT=FAIL", "reason=no_ladder_or_interfaces");
        return;
    }

    struct SavedState {
        build4100::Vector origin{};
        build4100::Vector velocity{};
        build4100::Vector mins{};
        build4100::Vector maxs{};
        build4100::Vector baseVelocity{};
        bool onGround = false;
        bool worldTrace = false;
        bool ducked = false;
        bool onLadder = false;
        std::size_t ladderIndex = 0;
        std::int32_t oldButtons = 0;
        std::uint8_t waterLevel = 0;
        std::int32_t waterType = 0;
    };
    const SavedState saved{client->absOrigin, client->absVelocity, client->playerMins,
                           client->playerMaxs, client->movementBaseVelocity,
                           client->movementOnGround, client->movementWorldTraceEnabled,
                           client->movementDucked, client->movementOnLadder,
                           client->movementLadderIndex, client->movementOldButtons,
                           client->movementWaterLevel, client->movementWaterType};
    const auto restore = [&]() noexcept {
        client->absOrigin = saved.origin;
        client->absVelocity = saved.velocity;
        client->playerMins = saved.mins;
        client->playerMaxs = saved.maxs;
        client->movementBaseVelocity = saved.baseVelocity;
        client->movementOnGround = saved.onGround;
        client->movementWorldTraceEnabled = saved.worldTrace;
        client->movementDucked = saved.ducked;
        client->movementOnLadder = saved.onLadder;
        client->movementLadderIndex = saved.ladderIndex;
        client->movementOldButtons = saved.oldButtons;
        client->movementWaterLevel = saved.waterLevel;
        client->movementWaterType = saved.waterType;
    };

    client->playerMins = build4100::Vector{-16.0f, -16.0f, 0.0f};
    client->playerMaxs = build4100::Vector{16.0f, 16.0f, 72.0f};
    client->movementWorldTraceEnabled = true;
    client->movementDucked = false;
    client->movementOldButtons = 0;

    std::size_t selected = 0;
    build4100::Vector selectedOrigin{};
    std::size_t testedCandidates = 0;
    std::size_t solidCandidates = 0;

    // info_ladder mins/maxs are the actual ladder volume, not its center plane.
    // The previous probe offset by 18 units from the volume center. On cs_office
    // the two ladders are 4 units thick, so a 16-unit player hull at center+18
    // touches the ladder/wall face instead of standing within Source's 2-unit
    // LadderDistance(). Put the hull one unit OUTSIDE the selected volume face:
    // 16 player half-width + 1 clear unit. The resulting hull face remains one
    // unit from the ladder, which is inside the reconstructed 2-unit attach range.
    constexpr float kProbeSurfaceGap = 1.0f;
    const float playerHalfX = (client->playerMaxs.x - client->playerMins.x) * 0.5f;
    const float playerHalfY = (client->playerMaxs.y - client->playerMins.y) * 0.5f;
    const float playerHeight = client->playerMaxs.z - client->playerMins.z;

    for (std::size_t i = 0; i < mapLadders_.size() && selected == 0; ++i) {
        const MovementLadderVolume& ladder = mapLadders_[i];
        const float height = ladder.maxs.z - ladder.mins.z;
        if (height < 24.0f) continue;

        const float widthX = ladder.maxs.x - ladder.mins.x;
        const float widthY = ladder.maxs.y - ladder.mins.y;
        const float centerX = (ladder.mins.x + ladder.maxs.x) * 0.5f;
        const float centerY = (ladder.mins.y + ladder.maxs.y) * 0.5f;
        const float centerZ = (ladder.mins.z + ladder.maxs.z) * 0.5f;

        // Try the vertical center first (player center aligned to ladder center),
        // then lower/upper positions. This avoids floor lips at the ladder base
        // without hard-coding cs_office coordinates.
        const float zCandidates[] = {
            centerZ - playerHeight * 0.5f,
            ladder.mins.z + std::min(24.0f, height * 0.25f),
            ladder.maxs.z - std::min(playerHeight + 8.0f, height * 0.50f),
        };

        for (const float rawZ : zCandidates) {
            // Keep enough vertical overlap for findLadder() while staying away
            // from accidental extreme values on short custom ladder volumes.
            const float z = std::clamp(rawZ, ladder.mins.z + 1.0f, ladder.maxs.z - 1.0f);
            for (const int sign : {-1, 1}) {
                build4100::Vector candidate{centerX, centerY, z};
                if (widthX <= widthY) {
                    const float face = sign < 0 ? ladder.mins.x : ladder.maxs.x;
                    candidate.x = face + static_cast<float>(sign) * (playerHalfX + kProbeSurfaceGap);
                } else {
                    const float face = sign < 0 ? ladder.mins.y : ladder.maxs.y;
                    candidate.y = face + static_cast<float>(sign) * (playerHalfY + kProbeSurfaceGap);
                }

                ++testedCandidates;
                build4100::Vector tiny = candidate;
                tiny.z += 0.01f;
                PlayerHullTraceResult trace{};
                const bool traced = TraceMovementHull(
                    candidate, tiny, client->playerMins, client->playerMaxs,
                    build4100::kMaskPlayerSolid, trace);
                if (traced && (trace.startSolid || trace.allSolid)) {
                    ++solidCandidates;
                    continue;
                }

                selected = i + 1;
                selectedOrigin = candidate;
                break;
            }
            if (selected != 0) break;
        }
    }
    if (selected == 0) {
        char detail[384]{};
        if (!mapLadders_.empty()) {
            const MovementLadderVolume& ladder = mapLadders_.front();
            std::snprintf(detail, sizeof(detail),
                          "reason=no_clear_ladder_side tested=%zu solid=%zu first_bounds=%.1f,%.1f,%.1f..%.1f,%.1f,%.1f",
                          testedCandidates, solidCandidates,
                          ladder.mins.x, ladder.mins.y, ladder.mins.z,
                          ladder.maxs.x, ladder.maxs.y, ladder.maxs.z);
        } else {
            std::snprintf(detail, sizeof(detail),
                          "reason=no_clear_ladder_side tested=%zu solid=%zu",
                          testedCandidates, solidCandidates);
        }
        EngineProbe("GAMEPLAY_LADDER_MOVEMENT=FAIL", detail);
        restore();
        return;
    }

    client->absOrigin = selectedOrigin;
    client->absVelocity = {};
    client->movementOnGround = false;
    client->movementOnLadder = false;
    client->movementLadderIndex = 0;
    client->movementWaterLevel = 0;
    client->movementWaterType = 0;
    client->movementBaseVelocity = {};

    build4100::UserCmd climb{};
    climb.commandNumber = 0x2401;
    climb.tickCount = 0x2401;
    climb.upMove = 200.0f;
    const float beforeZ = client->absOrigin.z;
    const bool moved = clients_.ApplyUsercmdMovement(
        clientIndex, client->edict, &climb, 1, 1, globals_->interval_per_tick, false);
    const float dz = client->absOrigin.z - beforeZ;
    const bool pass = moved && client->movementOnLadder &&
        client->movementLadderIndex == selected && dz > 1.0f && client->absVelocity.z > 50.0f;

    const MovementLadderVolume& ladder = mapLadders_[selected - 1];
    char detail[384]{};
    std::snprintf(detail, sizeof(detail),
                  "mover=%zu ladder=%zu map=%s bounds=%.1f,%.1f,%.1f..%.1f,%.1f,%.1f origin=%.1f,%.1f,%.1f dz=%.3f vz=%.3f attached=%d",
                  clientIndex, selected, mapName_.c_str(),
                  ladder.mins.x, ladder.mins.y, ladder.mins.z,
                  ladder.maxs.x, ladder.maxs.y, ladder.maxs.z,
                  selectedOrigin.x, selectedOrigin.y, selectedOrigin.z,
                  dz, client->absVelocity.z, client->movementOnLadder ? 1 : 0);
    EngineProbe(pass ? "GAMEPLAY_LADDER_MOVEMENT=PASS" : "GAMEPLAY_LADDER_MOVEMENT=FAIL", detail);
    restore();
}

void ServerGameClients004Adapter::ProbeWaterMovement(const std::size_t clientIndex) noexcept {
    if (waterMovementProbeDone_) return;
    waterMovementProbeDone_ = true;
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !client->edict || !engineTrace_ || !engineTrace_->IsValid()) {
        EngineProbe("GAMEPLAY_WATER_MOVEMENT=FAIL", "reason=interfaces");
        return;
    }

    const bool requireRealWaterLevel2 = mapName_ == "fy_pool_day";
    const int requiredRealLevel = requireRealWaterLevel2 ? 2 : 1;
    const build4100::Vector mins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector maxs{16.0f, 16.0f, 72.0f};
    std::vector<build4100::Vector> refs = terroristSpawns_;
    refs.insert(refs.end(), counterTerroristSpawns_.begin(), counterTerroristSpawns_.end());
    if (refs.empty()) {
        EngineProbe("GAMEPLAY_WATER_MOVEMENT=FAIL", "reason=no_spawns");
        return;
    }

    float minX = refs[0].x, maxX = minX, minY = refs[0].y, maxY = minY;
    float minZ = refs[0].z, maxZ = minZ;
    for (const auto& q : refs) {
        minX = std::min(minX, q.x); maxX = std::max(maxX, q.x);
        minY = std::min(minY, q.y); maxY = std::max(maxY, q.y);
        minZ = std::min(minZ, q.z); maxZ = std::max(maxZ, q.z);
    }
    minX -= 1536.0f; maxX += 1536.0f;
    minY -= 1536.0f; maxY += 1536.0f;
    minZ -= 768.0f;  maxZ += 256.0f;

    const auto wet = [](const std::int32_t c) noexcept {
        return (static_cast<std::uint32_t>(c) & build4100::kMaskWater) != 0u;
    };

    // de_aztec is the shallow-water oracle (level 1). fy_pool_day is the
    // real swimming-depth oracle: the candidate must have waist/eyes water so
    // checkWater() can be proven against engine CM_PointContents without a
    // synthetic contents overlay.
    build4100::Vector realWaterOrigin{};
    std::int32_t realFeet = 0, realWaist = 0, realEyes = 0;
    int realLevel = 0;
    bool realWaterFound = false;
    for (float z = minZ; z <= maxZ && !realWaterFound; z += 8.0f) {
        for (float y = minY; y <= maxY && !realWaterFound; y += 128.0f) {
            for (float x = minX; x <= maxX && !realWaterFound; x += 128.0f) {
                const build4100::Vector candidate{x, y, z};
                const std::int32_t feet = engineTrace_->PointContentsWorld(
                    build4100::Vector{x, y, z + 1.0f});
                if (!wet(feet)) continue;
                const std::int32_t waist = engineTrace_->PointContentsWorld(
                    build4100::Vector{x, y, z + 36.0f});
                const std::int32_t eyes = engineTrace_->PointContentsWorld(
                    build4100::Vector{x, y, z + 64.0f});
                const int level = wet(eyes) ? 3 : wet(waist) ? 2 : 1;
                if (level < requiredRealLevel) continue;

                build4100::Vector tiny = candidate;
                tiny.z += 0.01f;
                build4100::EngineGameTrace stand{};
                if (!engineTrace_->TraceWorldHull(candidate, tiny, mins, maxs,
                                                   build4100::kMaskPlayerSolidBrushOnly, stand) ||
                    stand.startSolid || stand.allSolid) {
                    continue;
                }
                realWaterOrigin = candidate;
                realFeet = feet;
                realWaist = waist;
                realEyes = eyes;
                realLevel = level;
                realWaterFound = true;
            }
        }
    }
    if (!realWaterFound) {
        char detail[192];
        std::snprintf(detail, sizeof(detail), "reason=no_real_water_point map=%s required_level=%d",
                      mapName_.c_str(), requiredRealLevel);
        EngineProbe("GAMEPLAY_WATER_CONTENTS=FAIL", detail);
        EngineProbe("GAMEPLAY_WATER_MOVEMENT=FAIL", detail);
        if (requireRealWaterLevel2) EngineProbe("GAMEPLAY_WATER_LEVEL2=FAIL", detail);
        return;
    }
    {
        char detail[448];
        std::snprintf(detail, sizeof(detail),
                      "origin=%.1f,%.1f,%.1f level=%d feet=0x%x waist=0x%x eyes=0x%x map=%s required_level=%d",
                      realWaterOrigin.x, realWaterOrigin.y, realWaterOrigin.z, realLevel,
                      realFeet, realWaist, realEyes, mapName_.c_str(), requiredRealLevel);
        EngineProbe("GAMEPLAY_WATER_CONTENTS=PASS", detail);
    }

    const ClientRecord saved = *client;
    const float interval = globals_ && globals_->interval_per_tick > 0.0f
        ? globals_->interval_per_tick : 0.015f;
    const auto resetAt = [&](const build4100::Vector& at) noexcept {
        client->absOrigin = at;
        client->absVelocity = {};
        client->playerMins = mins;
        client->playerMaxs = maxs;
        client->movementOnGround = false;
        client->movementWorldTraceEnabled = true;
        client->movementOldButtons = 0;
        client->movementWaterLevel = 0;
        client->movementWaterType = 0;
        client->movementBaseVelocity = {};
    };

    // fy_pool_day must prove the actual reconstructed movement path observes
    // waterLevel >= 2 from the real BSP, not from the synthetic level-3 oracle.
    if (requireRealWaterLevel2) {
        waterProbeSyntheticContents_ = false;
        waterProbeSyntheticOrigin_ = build4100::Vector{};
        resetAt(realWaterOrigin);
        build4100::UserCmd realIdle{};
        realIdle.commandNumber = 0x2210;
        realIdle.tickCount = 0x2210;
        const bool moved = clients_.ApplyUsercmdMovement(
            clientIndex, client->edict, &realIdle, 1, 1, interval, false);
        const int movementLevel = client->movementWaterLevel;
        const float movementVz = client->absVelocity.z;
        const float movementDz = client->absOrigin.z - realWaterOrigin.z;
        const bool realPass = moved && realLevel >= 2 && movementLevel >= 2;
        char detail[512];
        std::snprintf(detail, sizeof(detail),
                      "map=%s origin=%.1f,%.1f,%.1f point_level=%d movement_level=%d vz=%.3f dz=%.3f",
                      mapName_.c_str(), realWaterOrigin.x, realWaterOrigin.y, realWaterOrigin.z,
                      realLevel, movementLevel, movementVz, movementDz);
        EngineProbe(realPass ? "GAMEPLAY_WATER_LEVEL2=PASS" : "GAMEPLAY_WATER_LEVEL2=FAIL", detail);
        if (!realPass) {
            *client = saved;
            return;
        }
    }

    // Keep the existing deterministic WaterMove oracle. Collision remains real
    // BSP/dynamic TraceMovementHull, while PointContents is overlaid to level 3
    // so acceleration, swim gravity and water jump are tested independently of
    // a particular map's water depth.
    const build4100::Vector dirs[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    build4100::Vector origin{}, dir{};
    bool foundClear = false;
    for (const auto& q : refs) {
        for (const float dz : {0.0f, 32.0f, 64.0f, 96.0f}) {
            build4100::Vector candidate{q.x, q.y, q.z + dz};
            build4100::Vector tiny = candidate; tiny.z += 0.01f;
            build4100::EngineGameTrace stand{};
            if (!engineTrace_->TraceWorldHull(candidate, tiny, mins, maxs,
                                               build4100::kMaskPlayerSolidBrushOnly, stand) ||
                stand.startSolid || stand.allSolid) continue;
            build4100::Vector down = candidate; down.z -= 16.0f;
            build4100::EngineGameTrace downTrace{};
            if (!engineTrace_->TraceWorldHull(candidate, down, mins, maxs,
                                               build4100::kMaskPlayerSolidBrushOnly, downTrace) ||
                downTrace.startSolid || downTrace.allSolid || downTrace.fraction < 0.95f) continue;
            for (const auto& d : dirs) {
                build4100::Vector end{candidate.x + d.x * 64.0f,
                                      candidate.y + d.y * 64.0f,
                                      candidate.z};
                build4100::EngineGameTrace tr{};
                if (!engineTrace_->TraceWorldHull(candidate, end, mins, maxs,
                                                   build4100::kMaskPlayerSolidBrushOnly, tr) ||
                    tr.startSolid || tr.allSolid || tr.fraction < 0.95f) continue;
                origin = candidate;
                dir = d;
                foundClear = true;
                break;
            }
            if (foundClear) break;
        }
        if (foundClear) break;
    }
    if (!foundClear) {
        EngineProbe("GAMEPLAY_WATER_MOVEMENT=FAIL", "reason=no_clear_real_hull_for_overlay");
        *client = saved;
        return;
    }

    waterProbeSyntheticOrigin_ = origin;
    waterProbeSyntheticContents_ = true;

    resetAt(origin);
    build4100::UserCmd idle{};
    idle.commandNumber = 0x2201; idle.tickCount = 0x2201;
    const bool a = clients_.ApplyUsercmdMovement(clientIndex, client->edict, &idle, 1, 1, interval, false);
    const float idleVz = client->absVelocity.z;
    const float idleDz = client->absOrigin.z - origin.z;
    const int idleLevel = client->movementWaterLevel;

    resetAt(origin);
    build4100::UserCmd swim{};
    swim.commandNumber = 0x2202; swim.tickCount = 0x2202;
    swim.forwardMove = 200.0f;
    swim.viewAngles.y = dir.x > 0.5f ? 0.0f : dir.x < -0.5f ? 180.0f :
                        dir.y > 0.5f ? 90.0f : 270.0f;
    const bool b = clients_.ApplyUsercmdMovement(clientIndex, client->edict, &swim, 1, 1, interval, false);
    const float swimTravel = (client->absOrigin.x - origin.x) * dir.x +
                             (client->absOrigin.y - origin.y) * dir.y;
    const float swimSpeed = client->absVelocity.x * dir.x + client->absVelocity.y * dir.y;
    const int swimLevel = client->movementWaterLevel;

    resetAt(origin);
    build4100::UserCmd jump{};
    jump.commandNumber = 0x2203; jump.tickCount = 0x2203;
    jump.buttons = 1 << 1;
    const bool c = clients_.ApplyUsercmdMovement(clientIndex, client->edict, &jump, 1, 1, interval, false);
    const float jumpVz = client->absVelocity.z;
    const float jumpDz = client->absOrigin.z - origin.z;
    const int jumpLevel = client->movementWaterLevel;

    waterProbeSyntheticContents_ = false;
    waterProbeSyntheticOrigin_ = build4100::Vector{};
    const bool realDepthPass = requireRealWaterLevel2 ? realLevel >= 2 : realLevel == 1;
    const bool pass = a && b && c && realDepthPass &&
                      idleLevel >= 2 && swimLevel >= 2 && jumpLevel >= 2 &&
                      idleVz < -0.1f && idleDz < -0.001f &&
                      swimSpeed > 1.0f && swimTravel > 0.01f &&
                      jumpVz > 50.0f && jumpDz > 0.01f;
    char detail[1024];
    std::snprintf(detail, sizeof(detail),
                  "mover=%zu oracle=real_hull_synthetic_level3 origin=%.1f,%.1f,%.1f real_level=%d required_real_level=%d map=%s idle_level=%d idle_vz=%.3f idle_dz=%.3f swim_level=%d dir=%.1f,%.1f swim_speed=%.3f swim_travel=%.3f jump_level=%d jump_vz=%.3f jump_dz=%.3f",
                  clientIndex, origin.x, origin.y, origin.z, realLevel, requiredRealLevel,
                  mapName_.c_str(), idleLevel, idleVz, idleDz, swimLevel, dir.x, dir.y,
                  swimSpeed, swimTravel, jumpLevel, jumpVz, jumpDz);
    EngineProbe(pass ? "GAMEPLAY_WATER_MOVEMENT=PASS" : "GAMEPLAY_WATER_MOVEMENT=FAIL", detail);
    *client = saved;
}

bool ServerGameClients004Adapter::ProbeMapWorldTrace() const noexcept {
    if (!engineTrace_ || !engineTrace_->IsValid()) return false;
    const build4100::Vector* start = nullptr;
    if (!counterTerroristSpawns_.empty()) start = &counterTerroristSpawns_.front();
    else if (!terroristSpawns_.empty()) start = &terroristSpawns_.front();
    if (!start) return false;

    const build4100::Vector mins{-16.0f, -16.0f, 0.0f};
    const build4100::Vector maxs{16.0f, 16.0f, 72.0f};
    build4100::Vector end = *start;
    end.z -= 128.0f;
    build4100::EngineGameTrace trace{};
    if (!engineTrace_->TraceWorldHull(*start, end, mins, maxs,
                                      build4100::kMaskPlayerSolidBrushOnly, trace)) return false;
    const bool pass = !trace.allSolid && !trace.startSolid && trace.fraction < 1.0f &&
                      trace.plane.normal.z >= 0.7f;
    char detail[320];
    std::snprintf(detail, sizeof(detail),
                  "start=%.3f,%.3f,%.3f end=%.3f,%.3f,%.3f fraction=%.4f normalz=%.3f startsolid=%d allsolid=%d",
                  start->x, start->y, start->z, trace.endPos.x, trace.endPos.y, trace.endPos.z,
                  trace.fraction, trace.plane.normal.z, trace.startSolid ? 1 : 0, trace.allSolid ? 1 : 0);
    EngineProbe(pass ? "ENGINE_TRACE_WORLD_HULL=PASS" : "ENGINE_TRACE_WORLD_HULL=FAIL", detail);
    return pass;
}

bool ServerGameClients004Adapter::PlaceClientAtTeamSpawn(const std::size_t clientIndex,
                                                          const int team,
                                                          const char* const reason) noexcept {
    ClientRecord* const client = clients_.Get(clientIndex);
    if (!client || !engineTrace_ || !engineTrace_->IsValid()) return false;
    const auto& spawns = team == static_cast<int>(build4100::CSTeam::Terrorist)
        ? terroristSpawns_ : counterTerroristSpawns_;
    if (spawns.empty()) return false;

    // Keep the established protocol7 movement regression deterministic by
    // selecting a near-horizontal spawn. v1.8.18 validates sloped movement
    // independently against a real de_dust2 CT slope in
    // ProbeAdvancedMovementGeometry(), so the old flat-floor wire oracle remains
    // directly comparable across reconstruction blocks.
    constexpr float kProbeFlatGroundNormalZ = 0.999f;
    const std::size_t preferredSpawn = (clientIndex - 1) % spawns.size();

    client->playerMins = build4100::Vector{-16.0f, -16.0f, 0.0f};
    client->playerMaxs = build4100::Vector{16.0f, 16.0f, 72.0f};

    bool haveCandidate = false;
    bool flatCandidate = false;
    std::size_t selectedSpawn = preferredSpawn;
    build4100::Vector spawn{};
    build4100::EngineGameTrace trace{};
    float bestNormalZ = -1.0f;

    for (std::size_t offset = 0; offset < spawns.size(); ++offset) {
        const std::size_t candidateIndex = (preferredSpawn + offset) % spawns.size();
        const build4100::Vector candidateSpawn = spawns[candidateIndex];
        build4100::Vector candidateEnd = candidateSpawn;
        candidateEnd.z -= 128.0f;
        build4100::EngineGameTrace candidateTrace{};
        const bool traced = engineTrace_->TraceWorldHull(
            candidateSpawn, candidateEnd, client->playerMins, client->playerMaxs,
            build4100::kMaskPlayerSolidBrushOnly, candidateTrace);
        const bool walkable = traced && !candidateTrace.allSolid && !candidateTrace.startSolid &&
                              candidateTrace.fraction < 1.0f &&
                              candidateTrace.plane.normal.z >= 0.7f;
        if (!walkable) continue;

        if (!haveCandidate || candidateTrace.plane.normal.z > bestNormalZ) {
            haveCandidate = true;
            selectedSpawn = candidateIndex;
            spawn = candidateSpawn;
            trace = candidateTrace;
            bestNormalZ = candidateTrace.plane.normal.z;
        }
        if (candidateTrace.plane.normal.z >= kProbeFlatGroundNormalZ) {
            flatCandidate = true;
            selectedSpawn = candidateIndex;
            spawn = candidateSpawn;
            trace = candidateTrace;
            break;
        }
    }

    if (!haveCandidate) {
        char failDetail[256];
        std::snprintf(failDetail, sizeof(failDetail),
                      "index=%zu team=%d reason=%s candidates=%zu preferred=%zu no_walkable_spawn=1",
                      clientIndex, team, reason ? reason : "", spawns.size(), preferredSpawn);
        EngineProbe("ENGINE_TRACE_WORLD_HULL=FAIL", failDetail);
        return false;
    }

    client->absOrigin = trace.endPos;
    client->absVelocity = build4100::Vector{};
    client->movementOnGround = true;
    client->movementWorldTraceEnabled = true;
    client->movementOldButtons = 0;
    if (!engineTraceWorldProbeDone_) {
        engineTraceWorldProbeDone_ = true;
        char traceDetail[384];
        std::snprintf(traceDetail, sizeof(traceDetail),
                      "start=%.3f,%.3f,%.3f end=%.3f,%.3f,%.3f fraction=%.4f normalz=%.3f startsolid=%d allsolid=%d spawn_index=%zu preferred=%zu flat=%d",
                      spawn.x, spawn.y, spawn.z, trace.endPos.x, trace.endPos.y, trace.endPos.z,
                      trace.fraction, trace.plane.normal.z, trace.startSolid ? 1 : 0,
                      trace.allSolid ? 1 : 0, selectedSpawn, preferredSpawn, flatCandidate ? 1 : 0);
        EngineProbe("ENGINE_TRACE_WORLD_HULL=PASS", traceDetail);
    }
    char detail[448];
    std::snprintf(detail, sizeof(detail),
                  "index=%zu team=%d reason=%s spawn=%.3f,%.3f,%.3f origin=%.3f,%.3f,%.3f fraction=%.4f normalz=%.3f spawn_index=%zu preferred=%zu flat=%d worldtrace=1",
                  clientIndex, team, reason ? reason : "", spawn.x, spawn.y, spawn.z,
                  client->absOrigin.x, client->absOrigin.y, client->absOrigin.z,
                  trace.fraction, trace.plane.normal.z, selectedSpawn, preferredSpawn,
                  flatCandidate ? 1 : 0);
    EngineProbe("GAMEPLAY_SPAWN_TRACE_EVENT", detail);
    return true;
}

void ServerGameClients004Adapter::OnLevelShutdown() noexcept {
    ResetCombatProbe();
    pendingFireBulletsTempEntities_.clear();
    ReleasePlantedC4Reservation();
    objectiveScenario_.clear();
    objectiveScenarioStep_ = 0;
    objectiveScenarioClient_ = 0;
    weaponRuntime_.Reset();
    lagHistory_.Reset();
    mapObjectiveZones_.clear();
    std::size_t fakeReleased = 0;
    std::size_t fakeDetached = 0;
    std::size_t retainedReal = 0;

    // build4100 recreates fake clients after changelevel without routing the old
    // bots through ClientDisconnect. Their source-owned lifecycle records would
    // otherwise remain Active, so the new ClientPutInServer callbacks for slots
    // 1/2 are rejected as duplicate occupants and the next full snapshot loses
    // the deterministic bot SendProp baselines. Release only fake clients here;
    // real network clients may legitimately survive a map transition.
    for (std::size_t index = 1; index <= clients_.MaxClients(); ++index) {
        ClientRecord* const client = clients_.Get(index);
        if (!client || client->state == ClientLifecycleState::Empty) continue;
        if (!client->isFakeClient) {
            ++retainedReal;
            continue;
        }

        build4100::Edict* const edict = client->edict;
        for (auto& solid : mapMovementSolids_) {
            if (solid.followClientIndex == index) solid.followClientIndex = 0;
        }
        if (entityBridge_ && edict && entityBridge_->Detach(edict, false)) {
            ++fakeDetached;
        }
        if (edict && clients_.Disconnect(index, edict)) {
            ++fakeReleased;
        }
        if (index < deferredTeamSpawn_.size()) deferredTeamSpawn_[index] = 0;
        if (index < visibilityProbeDone_.size()) visibilityProbeDone_[index] = false;
        if (index < gameplayCommandProbeDone_.size()) gameplayCommandProbeDone_[index] = false;
        if (index < gameplayUsercmdProbeDone_.size()) gameplayUsercmdProbeDone_[index] = false;
        if (index < gameplayMovementProbeDone_.size()) gameplayMovementProbeDone_[index] = false;
        if (index < gameplayFrictionProbeDone_.size()) gameplayFrictionProbeDone_[index] = false;
        if (index < gameplayJumpProbeDone_.size()) gameplayJumpProbeDone_[index] = false;
        if (index < gameplayJumpHoldProbeDone_.size()) gameplayJumpHoldProbeDone_[index] = false;
        if (index < gameplayAirAccelProbeDone_.size()) gameplayAirAccelProbeDone_[index] = false;
        if (index < gameplayWeaponFireProbeDone_.size()) gameplayWeaponFireProbeDone_[index] = false;
        if (index < gameplayFireBulletsTempEntityProbeDone_.size()) gameplayFireBulletsTempEntityProbeDone_[index] = false;
        if (index < gameplayWeaponReloadProbeDone_.size()) gameplayWeaponReloadProbeDone_[index] = false;
        if (index < gameplayDamageProbeDone_.size()) gameplayDamageProbeDone_[index] = false;
    }

    // These probes describe map geometry rather than process lifetime. Allow the
    // next LevelInit to prove the new BSP independently.
    engineTraceWorldProbeDone_ = false;
    advancedMovementGeometryProbeDone_ = false;
    dynamicPlayerCollisionProbeDone_ = false;
    dynamicNonPlayerCollisionProbeDone_ = false;
    hostageCollisionProbeDone_ = false;
    hostageFollowingProbeDone_ = false;
    genericNpcProbeDone_ = false;
    waterMovementProbeDone_ = false;
    duckMovementProbeDone_ = false;
    ladderMovementProbeDone_ = false;
    waterProbeSyntheticContents_ = false;
    waterProbeSyntheticOrigin_ = build4100::Vector{};

    char detail[160];
    std::snprintf(detail, sizeof(detail),
                  "fake_released=%zu fake_detached=%zu retained_real=%zu remaining=%zu",
                  fakeReleased, fakeDetached, retainedReal, clients_.ConnectedCount());
    EngineProbe("LEVEL_CLIENT_MAP_RESET", detail);
}

bool ServerGameClients004Adapter::SyncMaxClients(const std::size_t maxClients) {
    if (clients_.MaxClients() == maxClients) return true;
    // The engine finalizes maxplayers after DLLInit. Rebuild the source-owned
    // client registry only before any clients have entered it.
    if (clients_.ConnectedCount() != 0) return false;
    if (!clients_.Configure(maxClients)) return false;
    if (!weaponRuntime_.Configure(maxClients)) return false;
    return lagHistory_.Configure(maxClients);
}

int ServerGameClients004Adapter::ResolveClientIndex(build4100::Edict* const entity) const noexcept {
    if (!globals_ || !entity) return -1;

    // During CreateFakeClient the engine re-enters ServerGameClients004. Calling
    // back into IVEngineServer from these callbacks is unnecessary and can hit
    // partially-constructed CGameClient state. ServerActivate's `count` is only
    // the high-water mark at activation time; later fake clients still occupy the
    // same fixed MAX_EDICTS array. Resolve them from base geometry instead.
    if (edictBase_) {
        const auto base = reinterpret_cast<std::uintptr_t>(edictBase_);
        const auto value = reinterpret_cast<std::uintptr_t>(entity);
        if (value >= base) {
            const auto delta = value - base;
            if ((delta % sizeof(build4100::Edict)) == 0) {
                const auto rawIndex = delta / sizeof(build4100::Edict);
                if (rawIndex > 0 && rawIndex < build4100::kMaxEdicts &&
                    rawIndex <= static_cast<std::uintptr_t>(globals_->maxClients)) {
                    return static_cast<int>(rawIndex);
                }
            }
        }
    }

    // Fallback is retained for unit tests / pre-activation calls only.
    if (!engine_ || !engine_->IsValid()) return -1;
    const int index = engine_->IndexOfEdict(entity);
    if (index <= 0 || index > globals_->maxClients) return -1;
    return index;
}

void ServerGameClients004Adapter::GetPlayerLimits(int& minPlayers, int& maxPlayers,
                                                  int& defaultMaxPlayers) const {
    minPlayers = 1;
    maxPlayers = 64;
    defaultMaxPlayers = 32;
    EngineProbe("GetPlayerLimits", "min=1 max=64 default=32");
}

bool ServerGameClients004Adapter::ClientConnect(build4100::Edict* const entity,
                                                const char* const name,
                                                const char* const address,
                                                char* const reject,
                                                const int maxRejectLen) {
    {
        char detail[224];
        std::snprintf(detail, sizeof(detail), "entity=%p name=%s address=%s",
                      static_cast<void*>(entity), name ? name : "", address ? address : "<null>");
        EngineProbe("ClientConnect_ENTER", detail);
    }
    const int index = ResolveClientIndex(entity);
    if (index < 0) {
        if (reject && maxRejectLen > 0) reject[0] = '\0';
        char detail[192];
        std::snprintf(detail, sizeof(detail), "index=%d name=%s reason=resolve",
                      index, name ? name : "");
        EngineProbe("ClientConnect_REJECT", detail);
        return false;
    }
    const bool accepted = clients_.Connect(static_cast<std::size_t>(index), entity, name ? name : "unnamed",
                                           address ? address : "none", reject, maxRejectLen);
    // Do not query IVEngineServer for network-id while CreateFakeClient is
    // re-entering the game DLL. Our own seeded bots have deterministic names.
    if (accepted && name && std::strncmp(name, "cssv34_bot_", 11) == 0) {
        (void)clients_.MarkFakeClient(static_cast<std::size_t>(index), entity);
    }
    char detail[256];
    std::snprintf(detail, sizeof(detail), "index=%d name=%s address=%s",
                  index, name ? name : "", address ? address : "<null>");
    EngineProbe(accepted ? "ClientConnect_OK" : "ClientConnect_REJECT", detail);
    return accepted;
}

void ServerGameClients004Adapter::ClientActive(build4100::Edict* const entity,
                                               const bool loadGame) {
    {
        char detail[96];
        std::snprintf(detail, sizeof(detail), "entity=%p loadGame=%d", static_cast<void*>(entity), loadGame ? 1 : 0);
        EngineProbe("ClientActive_ENTER", detail);
    }
    const int index = ResolveClientIndex(entity);
    if (index <= 0) {
        EngineProbe("ClientActive_SKIP", "reason=resolve");
        return;
    }
    if (clients_.Activate(static_cast<std::size_t>(index), entity, loadGame)) {
        char detail[64];
        std::snprintf(detail, sizeof(detail), "index=%d", index);
        EngineProbe("ClientActive_OK", detail);

        // build4100 CGameClient::IsHearingClient consults an engine-owned
        // per-sender bitmask at +0x210. CGameClient::Clear zeroes it, and the
        // original CSS voice game manager repopulates it through VoiceServer001.
        // Reconstruct the permissive all-active-clients matrix here so engine
        // CLC_VoiceData processing can forward SVC_VoiceData without any
        // synthetic transport path in the game DLL.
        if (voiceServer_ && voiceServer_->IsValid()) {
            int peers = 0;
            int setCalls = 0;
            for (std::size_t other = 1; other <= clients_.MaxClients(); ++other) {
                const ClientRecord* const record = clients_.Get(other);
                if (!record || record->state == ClientLifecycleState::Empty) continue;
                ++peers;
                if (other != static_cast<std::size_t>(index)) {
                    setCalls += voiceServer_->SetClientListening(index, static_cast<int>(other), true) ? 1 : 0;
                    setCalls += voiceServer_->SetClientListening(static_cast<int>(other), index, true) ? 1 : 0;
                }
            }
            char voiceDetail[96];
            std::snprintf(voiceDetail, sizeof(voiceDetail),
                          "index=%d peers=%d set_calls=%d", index, peers, setCalls);
            EngineProbe("VOICE_LISTENING_READY", voiceDetail);
        }
    } else {
        char detail[96];
        std::snprintf(detail, sizeof(detail), "index=%d reason=lifecycle", index);
        EngineProbe("ClientActive_SKIP", detail);
    }
}

void ServerGameClients004Adapter::ClientDisconnect(build4100::Edict* const entity) {
    {
        char detail[64];
        std::snprintf(detail, sizeof(detail), "entity=%p", static_cast<void*>(entity));
        EngineProbe("ClientDisconnect_ENTER", detail);
    }
    const int index = ResolveClientIndex(entity);
    if (index > 0) {
        const std::size_t clientIndex = static_cast<std::size_t>(index);
        if (combatProbeActive_ && (combatProbeAttacker_ == clientIndex || combatProbeTarget_ == clientIndex)) {
            ResetCombatProbe();
        }
        if (voiceServer_ && voiceServer_->IsValid()) {
            for (std::size_t other = 1; other <= clients_.MaxClients(); ++other) {
                if (other == clientIndex) continue;
                (void)voiceServer_->SetClientListening(index, static_cast<int>(other), false);
                (void)voiceServer_->SetClientListening(static_cast<int>(other), index, false);
            }
        }
        (void)gameState_.JoinTeam(clientIndex, static_cast<int>(build4100::CSTeam::Unassigned));
        for (auto& solid : mapMovementSolids_) {
            if (solid.followClientIndex == clientIndex) solid.followClientIndex = 0;
        }
        if (entityBridge_) (void)entityBridge_->Detach(entity, false);
        if (clients_.Disconnect(clientIndex, entity)) {
            if (static_cast<std::size_t>(index) < deferredTeamSpawn_.size()) {
                deferredTeamSpawn_[static_cast<std::size_t>(index)] = 0;
            }
            // These are per-client lifecycle diagnostics, not per-edict lifetime
            // diagnostics. A real reconnect can reuse the same edict index, so
            // allow the second occupant to prove the callback/movement path too.
            if (clientIndex < gameplayCommandProbeDone_.size()) gameplayCommandProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayUsercmdProbeDone_.size()) gameplayUsercmdProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayMovementProbeDone_.size()) gameplayMovementProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayFrictionProbeDone_.size()) gameplayFrictionProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayJumpProbeDone_.size()) gameplayJumpProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayJumpHoldProbeDone_.size()) gameplayJumpHoldProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayAirAccelProbeDone_.size()) gameplayAirAccelProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayWeaponFireProbeDone_.size()) gameplayWeaponFireProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayFireBulletsTempEntityProbeDone_.size()) gameplayFireBulletsTempEntityProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayWeaponReloadProbeDone_.size()) gameplayWeaponReloadProbeDone_[clientIndex] = false;
            if (clientIndex < gameplayDamageProbeDone_.size()) gameplayDamageProbeDone_[clientIndex] = false;
            (void)weaponRuntime_.ResetClient(clientIndex);
            (void)lagHistory_.ResetClient(clientIndex);
            const ClientRecord* const released = clients_.Get(clientIndex);
            const bool slotEmpty = released &&
                                   released->state == ClientLifecycleState::Empty &&
                                   released->edict == nullptr;
            char detail[128];
            std::snprintf(detail, sizeof(detail),
                          "index=%d remaining=%zu slot_empty=%d",
                          index, clients_.ConnectedCount(), slotEmpty ? 1 : 0);
            EngineProbe("ClientDisconnect_OK", detail);
        }
    }
}

void ServerGameClients004Adapter::ClientPutInServer(build4100::Edict* const entity,
                                                    const char* const playerName) {
    {
        char detail[160];
        std::snprintf(detail, sizeof(detail), "entity=%p name=%s", static_cast<void*>(entity), playerName ? playerName : "");
        EngineProbe("ClientPutInServer_ENTER", detail);
    }
    const int index = ResolveClientIndex(entity);
    if (index <= 0) {
        EngineProbe("ClientPutInServer_SKIP", "reason=resolve");
        return;
    }
    const bool seededFakeClient = playerName && std::strncmp(playerName, "cssv34_bot_", 11) == 0;
    const ClientRecord* const before = clients_.Get(static_cast<std::size_t>(index));
    if (seededFakeClient && before && before->state == ClientLifecycleState::Empty) {
        const bool adopted = clients_.AdoptFakeClient(static_cast<std::size_t>(index), entity, playerName);
        char detail[192];
        std::snprintf(detail, sizeof(detail), "index=%d name=%s adopted=%d",
                      index, playerName, adopted ? 1 : 0);
        EngineProbe(adopted ? "ClientPutInServer_FAKE_ADOPT" : "ClientPutInServer_FAKE_ADOPT_FAIL", detail);
        if (!adopted) return;
    }
    const bool inserted = clients_.PutInServer(static_cast<std::size_t>(index), entity, playerName);
    if (!inserted) {
        char detail[192];
        std::snprintf(detail, sizeof(detail), "index=%d name=%s reason=lifecycle",
                      index, playerName ? playerName : "");
        EngineProbe("ClientPutInServer_SKIP", detail);
        return;
    }

    // Feed the exact same ClientRecord used by reconstructed gameplay into the
    // engine-facing SendProp proxies. Fake clients are deterministically put on
    // the two playable teams so the first real snapshot contains genuine
    // gameplay-backed team/state differences instead of a synthetic net pulse.
    (void)weaponRuntime_.ResetClient(static_cast<std::size_t>(index));
    const bool gameplayInit = gameState_.OnClientPutInServer(static_cast<std::size_t>(index));
    if (gameplayInit && seededFakeClient) {
        const int team = (index & 1)
            ? static_cast<int>(build4100::CSTeam::Terrorist)
            : static_cast<int>(build4100::CSTeam::CounterTerrorist);
        (void)gameState_.JoinTeam(static_cast<std::size_t>(index), team);
        (void)gameState_.EnterGame(static_cast<std::size_t>(index));
    }
    if (const ClientRecord* const state = clients_.Get(static_cast<std::size_t>(index))) {
        char gameplayDetail[192];
        std::snprintf(gameplayDetail, sizeof(gameplayDetail),
                      "index=%d account=%d health=%d team=%d state=%d armor=%d buyzone=%d fake=%d",
                      index, state->account, state->health, state->teamIndex,
                      static_cast<int>(state->csState), state->armorValue,
                      state->inBuyZone ? 1 : 0, state->isFakeClient ? 1 : 0);
        EngineProbe("GAMEPLAY_SENDSTATE_READY", gameplayDetail);
    }

    bool bridgeOk = true;
    if (entityBridge_) {
        bridgeOk = entityBridge_->AttachClient(static_cast<std::size_t>(index), entity);
    }
    if (playerName && std::strncmp(playerName, "cssv34_bot_", 11) == 0) {
        (void)clients_.MarkFakeClient(static_cast<std::size_t>(index), entity);
    }

    // build4100 keeps packed entity state across CGameClient teardown when the
    // engine reuses the same client edict. ClientRecord is fresh here, but the
    // next full snapshot can otherwise replay the previous occupant's packed
    // team/origin/velocity/angles. Force this newly attached networkable to be
    // repacked from the gameplay SendProp proxies before signon reaches FULL.
    entity->stateFlags |= build4100::kEdictChanged;
    if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(index);
    {
        const ClientRecord* const refreshed = clients_.Get(static_cast<std::size_t>(index));
        char refreshDetail[224];
        std::snprintf(refreshDetail, sizeof(refreshDetail),
                      "index=%d team=%d state=%d origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f changed=1",
                      index, refreshed ? refreshed->teamIndex : -1,
                      refreshed ? static_cast<int>(refreshed->csState) : -1,
                      refreshed ? refreshed->absOrigin.x : 0.0f,
                      refreshed ? refreshed->absOrigin.y : 0.0f,
                      refreshed ? refreshed->absOrigin.z : 0.0f,
                      refreshed ? refreshed->absVelocity.x : 0.0f,
                      refreshed ? refreshed->absVelocity.y : 0.0f,
                      refreshed ? refreshed->absVelocity.z : 0.0f);
        EngineProbe("CLIENT_NETWORK_REFRESH", refreshDetail);
    }
    char detail[224];
    std::snprintf(detail, sizeof(detail), "index=%d name=%s bridge=%d",
                  index, playerName ? playerName : "", bridgeOk ? 1 : 0);
    EngineProbe(bridgeOk ? "ClientPutInServer_OK" : "ClientPutInServer_SKIP", detail);
}

void ServerGameClients004Adapter::ClientCommand(build4100::Edict* const entity) {
    const int index = ResolveClientIndex(entity);
    if (index <= 0 || !clients_.Command(static_cast<std::size_t>(index), entity)) return;
    if (!engine_ || !engine_->IsValid()) return;

    const int argc = engine_->CmdArgc();
    const char* const command = argc > 0 ? engine_->CmdArgv(0) : nullptr;
    if (!command) return;

    if (std::strcmp(command, "say") == 0 && argc >= 2) {
        const char* const text = engine_->CmdArgv(1);
        const int userId = engine_->GetPlayerUserId(entity);
        bool created = false;
        bool fired = false;
        if (gameEvents_ && gameEvents_->IsValid() && text && text[0]) {
            const GameEventClient event = gameEvents_->CreateEvent("player_say", true);
            created = event.IsValid();
            if (created) {
                event.SetInt("userid", userId);
                event.SetString("text", text);
                fired = gameEvents_->FireEvent(event, false);
                if (!fired) gameEvents_->FreeEvent(event);
            }
        }
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "index=%d cmd=say userid=%d text=%s created=%d fired=%d",
                      index, userId, text ? text : "", created ? 1 : 0, fired ? 1 : 0);
        EngineProbe(fired ? "GAME_EVENT_PLAYER_SAY" : "GAME_EVENT_PLAYER_SAY_FAIL", detail);
        return;
    }

    if (std::strcmp(command, "cssv34_usermsg") == 0 && argc >= 2) {
        constexpr int kSayTextMessageId = 3;
        const char* const text = engine_->CmdArgv(1);
        SingleClientRecipientFilter filter(index);
        void* const writer = text && text[0] ? engine_->UserMessageBegin(&filter, kSayTextMessageId) : nullptr;
        bool writeApi = false;
        bool stringOk = false;
        bool ended = false;
        if (writer) {
            BfWriteApi& api = EngineBfWriteApi();
            writeApi = api.Resolve(*engine_);
            if (writeApi) {
                api.writeByte(writer, index);
                stringOk = api.writeString(writer, text);
                api.writeByte(writer, 1);  // UTIL_SayTextFilter bChat=true
            }
            engine_->MessageEnd();
            ended = true;
        }
        char detail[320];
        std::snprintf(detail, sizeof(detail),
                      "index=%d cmd=cssv34_usermsg msg=3 name=SayText text=%s chat=1 begin=%d write_api=%d write_string=%d end=%d",
                      index, text ? text : "", writer ? 1 : 0, writeApi ? 1 : 0,
                      stringOk ? 1 : 0, ended ? 1 : 0);
        const bool sent = writer && writeApi && stringOk && ended;
        EngineProbe(sent ? "USER_MESSAGE_SAYTEXT" : "USER_MESSAGE_SAYTEXT_FAIL", detail);
        return;
    }

    if (std::strcmp(command, "cssv34_combat_setup") == 0) {
        const ClientRecord* const client = clients_.Get(static_cast<std::size_t>(index));
        const bool synthetic = client && client->name.rfind("cssv34_realprobe", 0) == 0;
        const bool ready = synthetic && SetupCombatProbe(static_cast<std::size_t>(index));
        EngineProbe(ready ? "GAMEPLAY_COMBAT_COMMAND" : "GAMEPLAY_COMBAT_COMMAND_FAIL",
                    ready ? "action=setup ready=1" : "action=setup ready=0");
        return;
    }
    if (std::strcmp(command, "cssv34_combat_reset") == 0) {
        const ClientRecord* const client = clients_.Get(static_cast<std::size_t>(index));
        const bool synthetic = client && client->name.rfind("cssv34_realprobe", 0) == 0;
        if (synthetic) ResetCombatProbe();
        EngineProbe(synthetic ? "GAMEPLAY_COMBAT_COMMAND" : "GAMEPLAY_COMBAT_COMMAND_FAIL",
                    synthetic ? "action=reset ready=1" : "action=reset ready=0");
        return;
    }

    if (std::strcmp(command, "cssv34_objective") == 0 && argc >= 2) {
        const char* const scenario = engine_->CmdArgv(1);
        bool started = false;
        if (scenario && std::strcmp(scenario, "bomb_rounds") == 0) {
            started = RunBombRoundsScenario(static_cast<std::size_t>(index));
        } else if (scenario && std::strcmp(scenario, "hostage_rescue") == 0) {
            started = RunHostageRescueScenario(static_cast<std::size_t>(index));
        }
        char detail[192];
        std::snprintf(detail, sizeof(detail), "index=%d scenario=%s started=%d", index, scenario ? scenario : "", started ? 1 : 0);
        EngineProbe(started ? "GAMEPLAY_OBJECTIVE_COMMAND" : "GAMEPLAY_OBJECTIVE_COMMAND_FAIL", detail);
        return;
    }

    if (std::strcmp(command, "jointeam") != 0 || argc < 2) return;

    const char* const teamText = engine_->CmdArgv(1);
    if (!teamText || !teamText[0]) return;
    char* end = nullptr;
    const long parsedTeam = std::strtol(teamText, &end, 10);
    if (!end || *end != '\0' || parsedTeam < static_cast<long>(build4100::CSTeam::Unassigned) ||
        parsedTeam > static_cast<long>(build4100::CSTeam::CounterTerrorist)) {
        return;
    }

    const std::size_t clientIndex = static_cast<std::size_t>(index);
    const ClientRecord* const before = clients_.Get(clientIndex);
    const int beforeTeam = before ? before->teamIndex : -1;
    const int beforeState = before ? static_cast<int>(before->csState) : -1;
    if (!gameState_.JoinTeam(clientIndex, static_cast<int>(parsedTeam))) return;

    // The reconstructed gameplay core has no separate joinclass UI state. Once
    // a real network client selects T/CT, complete its playable transition so
    // the next engine snapshot observes both inherited m_iTeamNum and direct
    // m_iPlayerState changes from the same gameplay event.
    if (build4100::IsPlayableTeam(static_cast<int>(parsedTeam))) {
        (void)gameState_.EnterGame(clientIndex);
        if (clientIndex < deferredTeamSpawn_.size()) {
            deferredTeamSpawn_[clientIndex] = static_cast<int>(parsedTeam);
        }
    }

    // Complete the command callback before entering EngineTrace. build4100 can
    // process the gameplay/team SendProp change immediately; the world-hull
    // placement runs once from the next GameFrame, outside ClientCommand.
    entity->stateFlags |= build4100::kEdictChanged;
    engine_->NotifyEdictFlagsChange(index);

    if (clientIndex < gameplayCommandProbeDone_.size() && !gameplayCommandProbeDone_[clientIndex]) {
        gameplayCommandProbeDone_[clientIndex] = true;
        const ClientRecord* const after = clients_.Get(clientIndex);
        char detail[224];
        std::snprintf(detail, sizeof(detail),
                      "index=%d cmd=jointeam arg=%ld before_team=%d after_team=%d before_state=%d after_state=%d changed=1",
                      index, parsedTeam, beforeTeam, after ? after->teamIndex : -1, beforeState,
                      after ? static_cast<int>(after->csState) : -1);
        EngineProbe("GAMEPLAY_COMMAND_EVENT", detail);
    }
}

void ServerGameClients004Adapter::SetCommandClient(const int index) {
    (void)clients_.SetCommandClient(index);
}

void ServerGameClients004Adapter::ClientSettingsChanged(build4100::Edict* const entity) {
    const int index = ResolveClientIndex(entity);
    if (index > 0) (void)clients_.SettingsChanged(static_cast<std::size_t>(index), entity);
}

void ServerGameClients004Adapter::ClientSetupVisibility(build4100::Edict* const viewEntity,
                                                        build4100::Edict* const client,
                                                        unsigned char* const pvs,
                                                        const int pvsSize) {
    (void)viewEntity;
    (void)pvs;
    (void)pvsSize;
    const int index = ResolveClientIndex(client);
    if (index > 0) {
        (void)clients_.SetupVisibility(static_cast<std::size_t>(index), client);
        if (index < static_cast<int>(visibilityProbeDone_.size()) && !visibilityProbeDone_[static_cast<std::size_t>(index)]) {
            visibilityProbeDone_[static_cast<std::size_t>(index)] = true;
            const ClientRecord* record = clients_.Get(static_cast<std::size_t>(index));
            char detail[192];
            std::snprintf(detail, sizeof(detail), "index=%d name=%s view=%p client=%p pvsSize=%d", index,
                          record ? record->name.c_str() : "", static_cast<void*>(viewEntity),
                          static_cast<void*>(client), pvsSize);
            EngineProbe("ClientSetupVisibility_OK", detail);
        }
    }
}

float ServerGameClients004Adapter::ProcessUsercmds(build4100::Edict* const player,
                                                   void* const buffer,
                                                   const int numCmds,
                                                   const int totalCmds,
                                                   const int droppedPackets,
                                                   const bool ignore,
                                                   const bool paused) {
    auto* const bits = static_cast<build4100::BitReadBuffer*>(buffer);
    if (!bits || numCmds < 0 || totalCmds < numCmds || totalCmds < 0 ||
        totalCmds > kBuild4100MaxWireUsercmds || droppedPackets < 0) {
        if (bits) bits->overflow = true;
        return 0.0f;
    }

    std::vector<build4100::UserCmd> commands;
    if (!DecodeUserCmdBatch(*bits, totalCmds, commands)) return 0.0f;

    const int index = ResolveClientIndex(player);
    if (index <= 0 || ignore) return 0.0f;

    const std::size_t clientIndex = static_cast<std::size_t>(index);
    const ClientRecord* const before = clients_.Get(clientIndex);
    const build4100::QAngle beforeAngles = before ? before->absAngles : build4100::QAngle{};
    const build4100::Vector beforeOrigin = before ? before->absOrigin : build4100::Vector{};
    const build4100::Vector beforeVelocity = before ? before->absVelocity : build4100::Vector{};
    const bool beforeOnGround = before ? before->movementOnGround : true;
    const std::int32_t beforeMovementButtons = before ? before->movementOldButtons : 0;
    // movementOldButtons intentionally tracks only movement-specific jump/duck
    // latch state. Objective/use edge detection needs the full previous wire
    // command button mask, otherwise held +use looks like a fresh press on every
    // packet and a defuse countdown is restarted forever.
    const std::int32_t beforeCommandButtons = before ? before->lastUserCommand.buttons : 0;

    if (!clients_.ProcessDecodedUsercmdBatch(clientIndex, player,
                                             commands.data(), commands.size(),
                                             numCmds, totalCmds, droppedPackets,
                                             false, paused)) {
        return 0.0f;
    }
    const float intervalPerTick = globals_ && globals_->interval_per_tick > 0.0f
        ? globals_->interval_per_tick : 0.015f;
    if (!clients_.ApplyUsercmdMovement(clientIndex, player, commands.data(), commands.size(),
                                       numCmds, intervalPerTick, paused)) {
        return 0.0f;
    }
    if (!commands.empty()) {
        HandleObjectiveButtons(clientIndex, beforeCommandButtons, commands.back().buttons);
        HandleHostageUse(clientIndex, beforeCommandButtons, commands.back().buttons);
        HandleWeaponButtons(clientIndex, beforeCommandButtons, commands.back());
    }

    const ClientRecord* const after = clients_.Get(clientIndex);
    const bool angleChanged = after &&
        (after->absAngles.x != beforeAngles.x || after->absAngles.y != beforeAngles.y ||
         after->absAngles.z != beforeAngles.z);
    const bool movementChanged = after &&
        (after->absOrigin.x != beforeOrigin.x || after->absOrigin.y != beforeOrigin.y ||
         after->absOrigin.z != beforeOrigin.z || after->absVelocity.x != beforeVelocity.x ||
         after->absVelocity.y != beforeVelocity.y || after->absVelocity.z != beforeVelocity.z);
    if (angleChanged || movementChanged) {
        // Reconstructed player eye angles live in ClientRecord rather than the
        // original CBasePlayer allocation. Tell the engine its edict changed so
        // m_angEyeAngles[0/1] gameplay proxies are sampled into the next delta.
        player->stateFlags |= build4100::kEdictChanged;
        if (engine_ && engine_->IsValid()) engine_->NotifyEdictFlagsChange(index);

        if (angleChanged && clientIndex < gameplayUsercmdProbeDone_.size() && !gameplayUsercmdProbeDone_[clientIndex]) {
            gameplayUsercmdProbeDone_[clientIndex] = true;
            char detail[256];
            const auto& cmd = commands.back();
            std::snprintf(detail, sizeof(detail),
                          "index=%d cmd=%d tick=%d pitch=%.3f yaw=%.3f roll=%.3f num=%d total=%d changed=1",
                          index, cmd.commandNumber, cmd.tickCount, after->absAngles.x, after->absAngles.y,
                          after->absAngles.z, numCmds, totalCmds);
            EngineProbe("GAMEPLAY_USERCMD_EVENT", detail);
        }
        if (movementChanged && clientIndex < gameplayMovementProbeDone_.size() &&
            !gameplayMovementProbeDone_[clientIndex]) {
            gameplayMovementProbeDone_[clientIndex] = true;
            char detail[448];
            const auto& cmd = commands.back();
            std::snprintf(detail, sizeof(detail),
                          "index=%d cmd=%d tick=%d origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f "
                          "forward=%.3f side=%.3f up=%.3f dt=%.3f onground=%d worldtrace=%d changed=1",
                          index, cmd.commandNumber, cmd.tickCount, after->absOrigin.x, after->absOrigin.y,
                          after->absOrigin.z, after->absVelocity.x, after->absVelocity.y, after->absVelocity.z,
                          cmd.forwardMove, cmd.sideMove, cmd.upMove, intervalPerTick, after->movementOnGround ? 1 : 0,
                          after->movementWorldTraceEnabled ? 1 : 0);
            EngineProbe("GAMEPLAY_MOVEMENT_EVENT", detail);
        }

        const auto& newestCmd = commands.back();
        const float beforeHorizontalSpeed =
            std::sqrt(beforeVelocity.x * beforeVelocity.x + beforeVelocity.y * beforeVelocity.y);
        const float afterHorizontalSpeed =
            std::sqrt(after->absVelocity.x * after->absVelocity.x + after->absVelocity.y * after->absVelocity.y);
        const bool noDirectionalInput = std::fabs(newestCmd.forwardMove) < 0.001f &&
                                        std::fabs(newestCmd.sideMove) < 0.001f &&
                                        std::fabs(newestCmd.upMove) < 0.001f;
        if (movementChanged && noDirectionalInput && beforeOnGround && after->movementOnGround &&
            afterHorizontalSpeed + 0.001f < beforeHorizontalSpeed &&
            clientIndex < gameplayFrictionProbeDone_.size() && !gameplayFrictionProbeDone_[clientIndex]) {
            gameplayFrictionProbeDone_[clientIndex] = true;
            char detail[384];
            std::snprintf(detail, sizeof(detail),
                          "index=%d cmd=%d tick=%d before_speed=%.3f after_speed=%.3f origin=%.3f,%.3f,%.3f "
                          "dt=%.3f onground=1 changed=1",
                          index, newestCmd.commandNumber, newestCmd.tickCount, beforeHorizontalSpeed,
                          afterHorizontalSpeed, after->absOrigin.x, after->absOrigin.y, after->absOrigin.z,
                          intervalPerTick);
            EngineProbe("GAMEPLAY_FRICTION_EVENT", detail);
        }

        constexpr std::int32_t kJumpButton = 1 << 1;
        const bool jumpPressed = (newestCmd.buttons & kJumpButton) != 0;
        if (movementChanged && jumpPressed && beforeOnGround && !after->movementOnGround &&
            (beforeMovementButtons & kJumpButton) == 0 &&
            clientIndex < gameplayJumpProbeDone_.size() && !gameplayJumpProbeDone_[clientIndex]) {
            gameplayJumpProbeDone_[clientIndex] = true;
            char detail[384];
            std::snprintf(detail, sizeof(detail),
                          "index=%d cmd=%d tick=%d origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f "
                          "buttons=0x%x dt=%.3f onground=0 changed=1",
                          index, newestCmd.commandNumber, newestCmd.tickCount, after->absOrigin.x, after->absOrigin.y,
                          after->absOrigin.z, after->absVelocity.x, after->absVelocity.y, after->absVelocity.z,
                          newestCmd.buttons, intervalPerTick);
            EngineProbe("GAMEPLAY_JUMP_EVENT", detail);
        } else if (movementChanged && jumpPressed && !beforeOnGround && !after->movementOnGround &&
                   (beforeMovementButtons & kJumpButton) != 0 &&
                   clientIndex < gameplayJumpHoldProbeDone_.size() && !gameplayJumpHoldProbeDone_[clientIndex]) {
            gameplayJumpHoldProbeDone_[clientIndex] = true;
            char detail[384];
            std::snprintf(detail, sizeof(detail),
                          "index=%d cmd=%d tick=%d origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f "
                          "buttons=0x%x dt=%.3f latch=held onground=0 changed=1",
                          index, newestCmd.commandNumber, newestCmd.tickCount, after->absOrigin.x, after->absOrigin.y,
                          after->absOrigin.z, after->absVelocity.x, after->absVelocity.y, after->absVelocity.z,
                          newestCmd.buttons, intervalPerTick);
            EngineProbe("GAMEPLAY_JUMP_HOLD_EVENT", detail);
        }
        const bool hasDirectionalInput = !noDirectionalInput;
        if (movementChanged && hasDirectionalInput && !beforeOnGround && !after->movementOnGround &&
            afterHorizontalSpeed > beforeHorizontalSpeed + 0.001f &&
            clientIndex < gameplayAirAccelProbeDone_.size() && !gameplayAirAccelProbeDone_[clientIndex]) {
            gameplayAirAccelProbeDone_[clientIndex] = true;
            char detail[448];
            std::snprintf(detail, sizeof(detail),
                          "index=%d cmd=%d tick=%d before_speed=%.3f after_speed=%.3f "
                          "origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f forward=%.3f side=%.3f "
                          "dt=%.3f accel=10 cap=30 onground=0 changed=1",
                          index, newestCmd.commandNumber, newestCmd.tickCount, beforeHorizontalSpeed,
                          afterHorizontalSpeed, after->absOrigin.x, after->absOrigin.y, after->absOrigin.z,
                          after->absVelocity.x, after->absVelocity.y, after->absVelocity.z,
                          newestCmd.forwardMove, newestCmd.sideMove, intervalPerTick);
            EngineProbe("GAMEPLAY_AIR_ACCEL_EVENT", detail);
        }
    }
    return globals_ ? globals_->interval_per_tick : 0.0f;
}

void ServerGameClients004Adapter::PostClientMessagesSent() {
    ++postClientMessagesCalls_;
}

void* ServerGameClients004Adapter::GetPlayerState(build4100::Edict* const player) {
    const int index = ResolveClientIndex(player);
    if (index <= 0) return nullptr;
    return clients_.PlayerStateFor(static_cast<std::size_t>(index), player);
}

void ServerGameClients004Adapter::ClientEarPosition(build4100::Edict* const entity,
                                                    build4100::Vector* const earOrigin) {
    if (!earOrigin) return;
    *earOrigin = build4100::Vector{};
    const int index = ResolveClientIndex(entity);
    if (index <= 0) return;
    const ClientRecord* const client = clients_.Get(static_cast<std::size_t>(index));
    if (!client) return;
    const float eyeHeight = client->movementDucked ? 46.0f : 64.0f;
    *earOrigin = build4100::Vector{client->absOrigin.x, client->absOrigin.y,
                                   client->absOrigin.z + eyeHeight};
}

int ServerGameClients004Adapter::GetReplayDelay(build4100::Edict* const player, int& entity) {
    (void)player;
    entity = -1;
    return 0;
}

void ServerGameClients004Adapter::GetBugReportInfo(char* const buffer, const int bufferLen) {
    if (buffer && bufferLen > 0) buffer[0] = '\0';
}

void ServerGameClients004Adapter::NetworkIDValidated(const char* const userName,
                                                     const char* const networkId) {
    lastValidatedUserName_ = userName ? userName : "";
    lastValidatedNetworkId_ = networkId ? networkId : "";
    (void)clients_.SetNetworkIdByName(userName, networkId);
}

}  // namespace cssv34::reconstructed::abi
