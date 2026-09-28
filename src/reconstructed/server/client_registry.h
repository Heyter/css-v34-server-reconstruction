#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "public/build4100/cstrike_defs.h"
#include "public/build4100/damage.h"
#include "public/build4100/edict.h"
#include "public/build4100/player_info.h"
#include "public/build4100/player_state.h"
#include "public/build4100/usercmd.h"
#include "server/weapon_inventory.h"

namespace cssv34::reconstructed {

enum class ClientLifecycleState {
    Empty,
    Connected,
    InGame,
    Active,
};

struct ClientRecord {
    build4100::Edict* edict = nullptr;
    ClientLifecycleState state = ClientLifecycleState::Empty;
    std::string name;
    std::string address;
    std::string networkId;
    int userId = -1;
    int teamIndex = 0;
    build4100::CSPlayerState csState = build4100::CSPlayerState::Welcome;
    int account = 0;
    int fragCount = 0;
    int deathCount = 0;
    int teamKillCount = 0;
    int armorValue = 0;
    // build4100 DT_BasePlayer wire state: m_iFOV==0 means default FOV.
    int currentFov = 0;
    int defaultFov = 90;
    bool isHLTV = false;
    bool isFakeClient = false;
    bool isDead = false;
    bool isInVehicle = false;
    bool isObserver = false;
    bool hasDefuser = false;
    bool hasHelmet = false;
    bool hasNightVision = false;
    bool hasC4 = false;
    bool inBuyZone = false;
    bool receivesRoundStartMoney = true;
    build4100::Vector absOrigin{};
    build4100::Vector absVelocity{};
    build4100::QAngle absAngles{};
    bool movementOnGround = true;
    bool movementWorldTraceEnabled = false;
    std::int32_t movementOldButtons = 0;
    bool movementDucked = false;
    bool movementOnLadder = false;
    std::size_t movementLadderIndex = 0;
    std::uint8_t movementWaterLevel = 0;
    std::int32_t movementWaterType = 0;
    build4100::Vector movementBaseVelocity{};
    build4100::Vector playerMins{};
    build4100::Vector playerMaxs{};
    WeaponInventory weapons;
    std::string modelName;
    int health = 0;
    int maxHealth = 0;
    build4100::HitGroup lastHitGroup = build4100::HitGroup::Invalid;
    std::size_t lastAttackerIndex = 0;
    int lastDamageTaken = 0;
    bool lastDeathWasHeadshot = false;
    float flashBlindUntil = 0.0f;
    build4100::BotCmd lastUserCommand{};
    build4100::PlayerState playerState{};
    int lastNumCmds = 0;
    int lastTotalCmds = 0;
    int lastDroppedPackets = 0;
    bool lastUsercmdIgnore = false;
    bool lastUsercmdPaused = false;
    std::size_t decodedUsercmds = 0;
    bool loadGame = false;
    std::size_t commandCalls = 0;
    std::size_t settingsChangedCalls = 0;
    std::size_t visibilityCalls = 0;
    std::size_t usercmdCalls = 0;
};


struct MovementLadderVolume {
    build4100::Vector mins{};
    build4100::Vector maxs{};
};

struct PlayerHullTraceResult {
    build4100::Vector endPos{};
    build4100::Vector planeNormal{};
    float fraction = 1.0f;
    std::int32_t contents = 0;
    bool allSolid = false;
    bool startSolid = false;
    // 0 = BSP/world. Non-zero identifies a source-owned player slot hit by
    // the build4100 COLLISION_GROUP_PLAYER_MOVEMENT reconstruction.
    std::size_t hitClientIndex = 0;
    // 0 = no source-owned non-player solid. Otherwise one-based index into
    // the map movement-solid descriptor list owned by ServerGameClients004.
    std::size_t hitDynamicSolidIndex = 0;
};

using PlayerPointContentsFn = std::int32_t (*)(void* context,
                                                 const build4100::Vector& point) noexcept;

using PlayerHullTraceFn = bool (*)(void* context,
                                   const build4100::Vector& start,
                                   const build4100::Vector& end,
                                   const build4100::Vector& mins,
                                   const build4100::Vector& maxs,
                                   std::uint32_t mask,
                                   PlayerHullTraceResult& trace) noexcept;

class ClientRegistry final {
public:
    bool Configure(std::size_t maxClients);
    void Reset() noexcept;
    void BindMovementTrace(void* context, PlayerHullTraceFn traceFn,
                           PlayerPointContentsFn pointContentsFn = nullptr) noexcept;
    void SetMovementLadders(std::vector<MovementLadderVolume> ladders);

    bool Connect(std::size_t clientIndex, build4100::Edict* edict,
                 const char* name, const char* address,
                 char* reject, int maxRejectLen);
    // build4100 fake clients bypass ServerGameClients004::ClientConnect and
    // enter the game DLL at ClientPutInServer. Adopt that engine-owned slot
    // into the source-owned lifecycle before the normal PutInServer/Active path.
    bool AdoptFakeClient(std::size_t clientIndex, build4100::Edict* edict,
                         const char* playerName);
    bool PutInServer(std::size_t clientIndex, build4100::Edict* edict,
                     const char* playerName);
    bool Activate(std::size_t clientIndex, build4100::Edict* edict, bool loadGame);
    bool Disconnect(std::size_t clientIndex, build4100::Edict* edict) noexcept;
    bool Command(std::size_t clientIndex, build4100::Edict* edict) noexcept;
    bool SettingsChanged(std::size_t clientIndex, build4100::Edict* edict) noexcept;
    bool SetupVisibility(std::size_t clientIndex, build4100::Edict* edict) noexcept;
    bool ProcessUsercmds(std::size_t clientIndex, build4100::Edict* edict,
                         int numCmds, int totalCmds, int droppedPackets,
                         bool ignore, bool paused) noexcept;
    bool ProcessDecodedUsercmds(std::size_t clientIndex, build4100::Edict* edict,
                                const build4100::UserCmd* cmds, std::size_t cmdCount,
                                int totalCmds, int droppedPackets,
                                bool ignore, bool paused) noexcept;
    bool ProcessDecodedUsercmdBatch(std::size_t clientIndex, build4100::Edict* edict,
                                    const build4100::UserCmd* cmds, std::size_t cmdCount,
                                    int numCmds, int totalCmds, int droppedPackets,
                                    bool ignore, bool paused) noexcept;
    bool ApplyUsercmdMovement(std::size_t clientIndex, build4100::Edict* edict,
                              const build4100::UserCmd* cmds, std::size_t cmdCount,
                              int numCmds, float intervalPerTick, bool paused) noexcept;
    bool SyncPlayerState(std::size_t clientIndex) noexcept;
    build4100::PlayerState* PlayerStateFor(std::size_t clientIndex,
                                           build4100::Edict* edict) noexcept;
    bool SetNetworkIdByName(const char* name, const char* networkId);
    bool MarkFakeClient(std::size_t clientIndex, build4100::Edict* edict, bool isFake = true) noexcept;

    bool SetCommandClient(int zeroBasedClientIndex) noexcept;
    int CommandClient() const noexcept { return commandClient_; }

    std::size_t MaxClients() const noexcept { return maxClients_; }
    std::size_t ConnectedCount() const noexcept;
    const ClientRecord* Get(std::size_t clientIndex) const noexcept;
    ClientRecord* Get(std::size_t clientIndex) noexcept;

private:
    bool Matches(std::size_t clientIndex, const build4100::Edict* edict) const noexcept;
    static void WriteReject(char* reject, int maxRejectLen, const char* text) noexcept;

    std::size_t maxClients_ = 0;
    std::vector<ClientRecord> clients_;
    int commandClient_ = -1;
    void* movementTraceContext_ = nullptr;
    PlayerHullTraceFn movementTraceFn_ = nullptr;
    PlayerPointContentsFn movementPointContentsFn_ = nullptr;
    std::vector<MovementLadderVolume> movementLadders_;
};

}  // namespace cssv34::reconstructed
