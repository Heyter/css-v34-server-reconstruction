#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "abi/engine_server021_client.h"
#include "abi/engine_trace_server003_client.h"
#include "public/build4100/edict.h"
#include "public/build4100/globalvars.h"
#include "public/build4100/vector.h"
#include "server/client_registry.h"
#include "server/cs_game_state.h"
#include "server/lag_compensation.h"
#include "server/weapon_ballistics.h"
#include "server/weapon_runtime.h"

namespace cssv34::reconstructed::abi {

class EngineEntityBridge;
class GameEventManager002Client;
class VoiceServer001Client;

struct MapMovementSolid {
    enum class Kind { StudioPhysics, InlineBrush, HostageBBox, NpcBBox };
    Kind kind = Kind::StudioPhysics;
    std::string classname;
    std::string model;
    build4100::Vector origin{};
    build4100::QAngle angles{};
    mutable const void* modelHandle = nullptr;
    mutable const void* vcollide = nullptr;
    mutable build4100::Vector bboxMins{};
    mutable build4100::Vector bboxMaxs{};
    mutable bool resolveAttempted = false;
    mutable bool boundsResolved = false;
    std::size_t actorOrdinal = 0;
    std::size_t followClientIndex = 0;
};

struct MapObjectiveZone {
    enum class Kind { BombTarget, HostageRescue };
    Kind kind = Kind::BombTarget;
    std::string classname;
    std::string model;
    build4100::Vector origin{};
    mutable const void* modelHandle = nullptr;
    mutable build4100::Vector mins{};
    mutable build4100::Vector maxs{};
    mutable bool resolveAttempted = false;
    mutable bool boundsResolved = false;
    int siteIndex = -1;
};

// Exact build4100 CServerGameClients vtable geometry was recovered directly
// from runtime/original/cstrike/bin/server_i486.so. Address-point slots 0..15:
// GetPlayerLimits, ClientConnect, ClientActive, ClientDisconnect,
// ClientPutInServer, ClientCommand, SetCommandClient, ClientSettingsChanged,
// ClientSetupVisibility, ProcessUsercmds, PostClientMessagesSent,
// GetPlayerState, ClientEarPosition, GetReplayDelay, GetBugReportInfo,
// NetworkIDValidated.
class ServerGameClients004Adapter final {
public:
    void Bind(const EngineServer021Client* engine, build4100::GlobalVarsBase* globals,
              EngineEntityBridge* entityBridge = nullptr,
              const EngineTraceServer003Client* engineTrace = nullptr,
              void* modelInfo = nullptr, void* physicsCollision = nullptr,
              const GameEventManager002Client* gameEvents = nullptr,
              const VoiceServer001Client* voiceServer = nullptr);
    void Unbind() noexcept;
    bool SyncMaxClients(std::size_t maxClients);
    void SetEngineEdictBase(build4100::Edict* base, int count) noexcept;
    // Must run from IServerGameDLL::ServerActivate before returning to the
    // engine. build4100 calls SV_CreateBaseline immediately after that callback,
    // so baseline-requiring shim entities have to exist at this boundary.
    void PrepareLevelNetworkEntities(const char* phase) noexcept;
    // Called from the first post-activation GameFrame. By then the engine has
    // completed SV_CreateBaseline and written instanceBaselineIndex into the
    // ServerClass objects returned by GetAllServerClasses().
    void VerifyLevelNetworkEntityBaselines() noexcept;
    void SetMapPlayerSpawns(std::vector<build4100::Vector> terroristSpawns,
                            std::vector<build4100::Vector> counterTerroristSpawns);
    void SetMapMovementSolids(std::vector<MapMovementSolid> solids);
    void SetMapObjectiveZones(std::vector<MapObjectiveZone> zones);
    void SetMapLadders(std::vector<MovementLadderVolume> ladders);
    void SetMapName(const char* mapName);
    void ProcessDeferredTeamSpawns() noexcept;
    void UpdateDynamicNonPlayerOrigins() noexcept;
    void TickGameplayRound() noexcept;
    void ProcessDeferredWeaponBursts() noexcept;
    // Temp entities are captured at usercmd fire time, then emitted from the
    // next IServerGameDLL::GameFrame. build4100 snapshots unreliable temp
    // events from the game-frame path; emitting directly from ProcessUsercmds
    // can miss CGameServer::CopyTempEntities for observer snapshots.
    void ProcessDeferredTempEntities() noexcept;
    void OnLevelShutdown() noexcept;
    bool ProbeMapWorldTrace() const noexcept;

    virtual void GetPlayerLimits(int& minPlayers, int& maxPlayers, int& defaultMaxPlayers) const; // 0
    virtual bool ClientConnect(build4100::Edict* entity, const char* name, const char* address,
                               char* reject, int maxRejectLen);                                    // 1
    virtual void ClientActive(build4100::Edict* entity, bool loadGame);                            // 2
    virtual void ClientDisconnect(build4100::Edict* entity);                                      // 3
    virtual void ClientPutInServer(build4100::Edict* entity, const char* playerName);              // 4
    virtual void ClientCommand(build4100::Edict* entity);                                         // 5
    virtual void SetCommandClient(int index);                                                      // 6
    virtual void ClientSettingsChanged(build4100::Edict* entity);                                 // 7
    virtual void ClientSetupVisibility(build4100::Edict* viewEntity, build4100::Edict* client,
                                       unsigned char* pvs, int pvsSize);                            // 8
    virtual float ProcessUsercmds(build4100::Edict* player, void* buffer, int numCmds,
                                  int totalCmds, int droppedPackets, bool ignore, bool paused);     // 9
    virtual void PostClientMessagesSent();                                                         // 10
    virtual void* GetPlayerState(build4100::Edict* player);                                       // 11
    virtual void ClientEarPosition(build4100::Edict* entity, build4100::Vector* earOrigin);        // 12
    virtual int GetReplayDelay(build4100::Edict* player, int& entity);                             // 13
    virtual void GetBugReportInfo(char* buffer, int bufferLen);                                   // 14
    virtual void NetworkIDValidated(const char* userName, const char* networkId);                  // 15

    const ClientRegistry& Clients() const noexcept { return clients_; }
    ClientRegistry& ClientsMutable() noexcept { return clients_; }
    std::size_t PostClientMessagesCalls() const noexcept { return postClientMessagesCalls_; }
    const std::string& LastValidatedUserName() const noexcept { return lastValidatedUserName_; }
    const std::string& LastValidatedNetworkId() const noexcept { return lastValidatedNetworkId_; }

private:
    bool PlaceClientAtTeamSpawn(std::size_t clientIndex, int team, const char* reason) noexcept;
    void ProbeAdvancedMovementGeometry(std::size_t clientIndex) noexcept;
    void ProbeDynamicPlayerCollision(std::size_t clientIndex) noexcept;
    void ProbeDynamicNonPlayerCollision(std::size_t clientIndex) noexcept;
    void ProbeHostageCollision(std::size_t clientIndex) noexcept;
    void ProbeHostageFollowing(std::size_t clientIndex) noexcept;
    void ProbeGenericNpcMovement(std::size_t clientIndex) noexcept;
    void ProbeWaterMovement(std::size_t clientIndex) noexcept;
    void ProbeDuckMovement(std::size_t clientIndex) noexcept;
    void ProbeLadderMovement(std::size_t clientIndex) noexcept;
    bool TraceMovementHull(const build4100::Vector& start, const build4100::Vector& end,
                           const build4100::Vector& mins, const build4100::Vector& maxs,
                           std::uint32_t mask, PlayerHullTraceResult& out) noexcept;
    bool ResolveStudioCollision(MapMovementSolid& solid) noexcept;
    bool ResolveActorBounds(MapMovementSolid& solid) noexcept;
    bool AdvanceDynamicActor(MapMovementSolid& solid, float frameTime) noexcept;
    bool ResolveObjectiveZoneBounds(MapObjectiveZone& zone) noexcept;
    int FindObjectiveZone(const build4100::Vector& point, MapObjectiveZone::Kind kind) noexcept;
    build4100::Vector ObjectiveZoneCenter(int zoneIndex) noexcept;
    bool FireObjectiveEvent(const char* name, int userid = 0, int site = -1, int hostage = -1,
                            int winner = -1, int reason = -1, bool hasKit = false) noexcept;
    bool FireWeaponGameEvent(const char* name, std::size_t clientIndex,
                             build4100::CSWeaponID weapon, bool includeWeapon) noexcept;
    bool FirePlayerHurtEvent(std::size_t attackerIndex, std::size_t victimIndex,
                             build4100::CSWeaponID weapon) noexcept;
    bool FirePlayerDeathEvent(std::size_t attackerIndex, std::size_t victimIndex,
                              build4100::CSWeaponID weapon) noexcept;
    bool ApplyHitscanAttack(std::size_t attackerIndex, build4100::CSWeaponID weapon,
                            const build4100::UserCmd& command) noexcept;
    bool EmitFireBulletsTempEntity(std::size_t clientIndex, build4100::CSWeaponID weapon,
                                   const build4100::UserCmd& command) noexcept;
    bool ApplyShotgunAttack(std::size_t attackerIndex, build4100::CSWeaponID weapon,
                            const build4100::UserCmd& command) noexcept;
    void RecordLagCompensationFrame() noexcept;
    bool SetupCombatProbe(std::size_t attackerIndex) noexcept;
    void ResetCombatProbe() noexcept;
    void HandleWeaponButtons(std::size_t clientIndex, std::int32_t beforeButtons,
                             const build4100::UserCmd& command) noexcept;
    bool ReservePlantedC4Edict() noexcept;
    bool SpawnPlantedC4() noexcept;
    bool MaterializePlantedC4() noexcept;
    void MarkPlantedC4Changed() noexcept;
    void RemovePlantedC4() noexcept;
    void ReleasePlantedC4Reservation() noexcept;
    bool RunBombRoundsScenario(std::size_t commandClient) noexcept;
    bool RunHostageRescueScenario(std::size_t commandClient) noexcept;
    bool RunRoundCycleScenario() noexcept;
    void HandleObjectiveButtons(std::size_t clientIndex, std::int32_t beforeButtons,
                                std::int32_t afterButtons) noexcept;
    void HandleHostageUse(std::size_t clientIndex, std::int32_t beforeButtons,
                          std::int32_t afterButtons) noexcept;
    int ResolveClientIndex(build4100::Edict* entity) const noexcept;

    struct PendingFireBulletsTempEntity {
        std::size_t shooterIndex = 0;
        std::array<int, 64> recipients{};
        std::size_t recipientCount = 0;
        std::int32_t player = 0;
        build4100::Vector origin{};
        build4100::QAngle angles{};
        std::int32_t weaponId = 0;
        std::int32_t mode = 0;
        std::int32_t seed = 0;
        float spread = 0.0f;
    };

    const EngineServer021Client* engine_ = nullptr;
    build4100::GlobalVarsBase* globals_ = nullptr;
    EngineEntityBridge* entityBridge_ = nullptr;
    const EngineTraceServer003Client* engineTrace_ = nullptr;
    void* modelInfo_ = nullptr;
    void* physicsCollision_ = nullptr;
    const GameEventManager002Client* gameEvents_ = nullptr;
    const VoiceServer001Client* voiceServer_ = nullptr;
    ClientRegistry clients_;
    CsGameState gameState_{clients_};
    WeaponRuntimeController weaponRuntime_;
    LagCompensationHistory lagHistory_;
    std::size_t postClientMessagesCalls_ = 0;
    std::string lastValidatedUserName_;
    std::string lastValidatedNetworkId_;
    build4100::Edict* edictBase_ = nullptr;
    int edictCount_ = 0;
    std::vector<build4100::Vector> terroristSpawns_;
    std::vector<build4100::Vector> counterTerroristSpawns_;
    std::vector<MapMovementSolid> mapMovementSolids_;
    std::vector<MapObjectiveZone> mapObjectiveZones_;
    std::vector<MovementLadderVolume> mapLadders_;
    build4100::Edict* plantedC4Edict_ = nullptr;
    int plantedC4EntityIndex_ = -1;
    bool plantedC4SpawnPending_ = false;
    bool plantedC4Active_ = false;
    int plantedC4ReservationAttempts_ = 0;
    bool plantedC4PreBaselineProbeDone_ = false;
    bool plantedC4BaselineProbeDone_ = false;
    std::string objectiveScenario_;
    int objectiveScenarioStep_ = 0;
    float objectiveScenarioNextTime_ = 0.0f;
    std::size_t objectiveScenarioClient_ = 0;
    std::string mapName_;
    std::array<bool, 65> visibilityProbeDone_{};
    std::array<bool, 65> gameplayCommandProbeDone_{};
    std::array<bool, 65> gameplayUsercmdProbeDone_{};
    std::array<bool, 65> gameplayMovementProbeDone_{};
    std::array<bool, 65> gameplayFrictionProbeDone_{};
    std::array<bool, 65> gameplayJumpProbeDone_{};
    std::array<bool, 65> gameplayJumpHoldProbeDone_{};
    std::array<bool, 65> gameplayAirAccelProbeDone_{};
    std::array<bool, 65> gameplayWeaponFireProbeDone_{};
    std::array<bool, 65> gameplayFireBulletsTempEntityProbeDone_{};
    std::array<bool, 65> gameplayWeaponReloadProbeDone_{};
    std::array<bool, 65> gameplayGlockBurstProbeDone_{};
    std::array<bool, 65> gameplayDamageProbeDone_{};
    std::array<int, 65> deferredTeamSpawn_{};
    std::vector<PendingFireBulletsTempEntity> pendingFireBulletsTempEntities_;
    bool combatProbeActive_ = false;
    std::size_t combatProbeAttacker_ = 0;
    std::size_t combatProbeTarget_ = 0;
    bool combatProbeSavedFriendlyFire_ = false;
    build4100::Vector combatProbeSavedTargetOrigin_{};
    build4100::Vector combatProbeSavedTargetVelocity_{};
    build4100::Vector combatProbeSavedTargetMins_{};
    build4100::Vector combatProbeSavedTargetMaxs_{};
    int combatProbeSavedTargetHealth_ = 0;
    int combatProbeSavedTargetArmor_ = 0;
    int combatProbeSavedTargetDeaths_ = 0;
    int combatProbeSavedTargetTeam_ = 0;
    build4100::CSPlayerState combatProbeSavedTargetState_ = build4100::CSPlayerState::Welcome;
    bool combatProbeSavedTargetDead_ = false;
    bool combatProbeSavedTargetObserver_ = false;
    build4100::HitGroup combatProbeSavedTargetLastHitGroup_ = build4100::HitGroup::Invalid;
    std::size_t combatProbeSavedTargetLastAttacker_ = 0;
    int combatProbeSavedTargetLastDamage_ = 0;
    bool combatProbeSavedTargetLastHeadshot_ = false;
    build4100::QAngle combatProbeSavedAttackerAngles_{};
    int combatProbeSavedAttackerAccount_ = 0;
    int combatProbeSavedAttackerFrags_ = 0;
    int combatProbeSavedAttackerTeamKills_ = 0;
    bool engineTraceWorldProbeDone_ = false;
    bool advancedMovementGeometryProbeDone_ = false;
    bool dynamicPlayerCollisionProbeDone_ = false;
    bool dynamicNonPlayerCollisionProbeDone_ = false;
    bool hostageCollisionProbeDone_ = false;
    bool hostageFollowingProbeDone_ = false;
    bool genericNpcProbeDone_ = false;
    bool waterMovementProbeDone_ = false;
    bool duckMovementProbeDone_ = false;
    bool ladderMovementProbeDone_ = false;
    bool waterProbeSyntheticContents_ = false;
    build4100::Vector waterProbeSyntheticOrigin_{};
};

ServerGameClients004Adapter& ServerGameClients004Instance() noexcept;

}  // namespace cssv34::reconstructed::abi
