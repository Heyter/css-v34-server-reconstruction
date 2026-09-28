#include "server/client_registry.h"

#include "public/build4100/engine_trace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

namespace cssv34::reconstructed {

bool ClientRegistry::Configure(const std::size_t maxClients) {
    maxClients_ = maxClients;
    clients_.assign(maxClients_ + 1, ClientRecord{});
    commandClient_ = -1;
    return true;
}

void ClientRegistry::Reset() noexcept {
    for (auto& client : clients_) client = ClientRecord{};
    commandClient_ = -1;
}

void ClientRegistry::BindMovementTrace(void* const context,
                                       const PlayerHullTraceFn traceFn,
                                       const PlayerPointContentsFn pointContentsFn) noexcept {
    movementTraceContext_ = context;
    movementTraceFn_ = traceFn;
    movementPointContentsFn_ = pointContentsFn;
}

void ClientRegistry::SetMovementLadders(std::vector<MovementLadderVolume> ladders) {
    movementLadders_ = std::move(ladders);
}

void ClientRegistry::WriteReject(char* const reject, const int maxRejectLen,
                                 const char* const text) noexcept {
    if (!reject || maxRejectLen <= 0) return;
    const char* source = text ? text : "rejected";
    const std::size_t capacity = static_cast<std::size_t>(maxRejectLen);
    const std::size_t count = std::min(capacity - 1, std::strlen(source));
    std::memcpy(reject, source, count);
    reject[count] = '\0';
}

ClientRecord* ClientRegistry::Get(const std::size_t clientIndex) noexcept {
    if (clientIndex == 0 || clientIndex > maxClients_ || clientIndex >= clients_.size()) return nullptr;
    return &clients_[clientIndex];
}

const ClientRecord* ClientRegistry::Get(const std::size_t clientIndex) const noexcept {
    if (clientIndex == 0 || clientIndex > maxClients_ || clientIndex >= clients_.size()) return nullptr;
    return &clients_[clientIndex];
}

bool ClientRegistry::Matches(const std::size_t clientIndex,
                             const build4100::Edict* const edict) const noexcept {
    const ClientRecord* client = Get(clientIndex);
    return client && client->state != ClientLifecycleState::Empty && client->edict == edict;
}

bool ClientRegistry::Connect(const std::size_t clientIndex, build4100::Edict* const edict,
                             const char* const name, const char* const address,
                             char* const reject, const int maxRejectLen) {
    ClientRecord* client = Get(clientIndex);
    if (!client || !edict || !name || !address) {
        WriteReject(reject, maxRejectLen, "invalid client");
        return false;
    }
    if (client->state != ClientLifecycleState::Empty) {
        WriteReject(reject, maxRejectLen, "client slot already connected");
        return false;
    }
    client->edict = edict;
    client->state = ClientLifecycleState::Connected;
    client->name = name;
    client->address = address;
    (void)SyncPlayerState(clientIndex);
    if (reject && maxRejectLen > 0) reject[0] = '\0';
    return true;
}

bool ClientRegistry::AdoptFakeClient(const std::size_t clientIndex,
                                     build4100::Edict* const edict,
                                     const char* const playerName) {
    ClientRecord* client = Get(clientIndex);
    if (!client || !edict || !playerName || client->state != ClientLifecycleState::Empty) return false;
    client->edict = edict;
    client->state = ClientLifecycleState::Connected;
    client->name = playerName;
    client->address = "BOT";
    client->networkId = "BOT";
    client->isFakeClient = true;
    (void)SyncPlayerState(clientIndex);
    return true;
}

bool ClientRegistry::PutInServer(const std::size_t clientIndex, build4100::Edict* const edict,
                                 const char* const playerName) {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict) || !playerName ||
        client->state != ClientLifecycleState::Connected) return false;
    client->name = playerName;
    client->state = ClientLifecycleState::InGame;
    (void)SyncPlayerState(clientIndex);
    return true;
}

bool ClientRegistry::Activate(const std::size_t clientIndex, build4100::Edict* const edict,
                              const bool loadGame) {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict)) return false;
    if (client->state != ClientLifecycleState::Connected &&
        client->state != ClientLifecycleState::InGame) return false;
    client->loadGame = loadGame;
    client->state = ClientLifecycleState::Active;
    (void)SyncPlayerState(clientIndex);
    return true;
}

bool ClientRegistry::Disconnect(const std::size_t clientIndex,
                                build4100::Edict* const edict) noexcept {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict)) return false;
    *client = ClientRecord{};
    if (commandClient_ == static_cast<int>(clientIndex - 1)) commandClient_ = -1;
    return true;
}

bool ClientRegistry::Command(const std::size_t clientIndex, build4100::Edict* const edict) noexcept {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict)) return false;
    ++client->commandCalls;
    return true;
}

bool ClientRegistry::SettingsChanged(const std::size_t clientIndex,
                                     build4100::Edict* const edict) noexcept {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict)) return false;
    ++client->settingsChangedCalls;
    return true;
}

bool ClientRegistry::SetupVisibility(const std::size_t clientIndex,
                                     build4100::Edict* const edict) noexcept {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict)) return false;
    ++client->visibilityCalls;
    return true;
}

bool ClientRegistry::ProcessUsercmds(const std::size_t clientIndex,
                                     build4100::Edict* const edict,
                                     const int numCmds, const int totalCmds,
                                     const int droppedPackets, const bool ignore,
                                     const bool paused) noexcept {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict) || numCmds < 0 || totalCmds < numCmds ||
        droppedPackets < 0) return false;
    ++client->usercmdCalls;
    client->lastNumCmds = numCmds;
    client->lastTotalCmds = totalCmds;
    client->lastDroppedPackets = droppedPackets;
    client->lastUsercmdIgnore = ignore;
    client->lastUsercmdPaused = paused;
    return true;
}

bool ClientRegistry::ProcessDecodedUsercmds(const std::size_t clientIndex,
                                            build4100::Edict* const edict,
                                            const build4100::UserCmd* const cmds,
                                            const std::size_t cmdCount,
                                            const int totalCmds,
                                            const int droppedPackets,
                                            const bool ignore,
                                            const bool paused) noexcept {
    return ProcessDecodedUsercmdBatch(clientIndex, edict, cmds, cmdCount,
                                      static_cast<int>(cmdCount), totalCmds,
                                      droppedPackets, ignore, paused);
}

bool ClientRegistry::ProcessDecodedUsercmdBatch(const std::size_t clientIndex,
                                                build4100::Edict* const edict,
                                                const build4100::UserCmd* const cmds,
                                                const std::size_t cmdCount,
                                                const int numCmds,
                                                const int totalCmds,
                                                const int droppedPackets,
                                                const bool ignore,
                                                const bool paused) noexcept {
    if (cmdCount > static_cast<std::size_t>(INT32_MAX)) return false;
    if (cmdCount != 0 && !cmds) return false;
    if (numCmds < 0 || totalCmds < numCmds || cmdCount < static_cast<std::size_t>(numCmds) ||
        cmdCount > static_cast<std::size_t>(totalCmds)) return false;
    if (!ProcessUsercmds(clientIndex, edict, numCmds, totalCmds,
                         droppedPackets, ignore, paused)) return false;

    ClientRecord* const client = Get(clientIndex);
    if (!client) return false;
    client->decodedUsercmds += cmdCount;
    if (ignore || cmdCount == 0) return true;

    // DecodeUserCmdBatch stores commands oldest -> newest. The last command is
    // therefore the one Source would leave as the current usercmd after the batch.
    const build4100::UserCmd& cmd = cmds[cmdCount - 1];
    client->lastUserCommand.commandNumber = cmd.commandNumber;
    client->lastUserCommand.tickCount = cmd.tickCount;
    client->lastUserCommand.viewAngles = cmd.viewAngles;
    client->lastUserCommand.forwardMove = cmd.forwardMove;
    client->lastUserCommand.sideMove = cmd.sideMove;
    client->lastUserCommand.upMove = cmd.upMove;
    client->lastUserCommand.buttons = cmd.buttons;
    client->lastUserCommand.impulse = cmd.impulse;
    client->lastUserCommand.weaponSelect = cmd.weaponSelect;
    client->lastUserCommand.weaponSubtype = cmd.weaponSubtype;
    client->lastUserCommand.randomSeed = cmd.randomSeed;
    client->lastUserCommand.mouseDx = cmd.mouseDx;
    client->lastUserCommand.mouseDy = cmd.mouseDy;
    client->lastUserCommand.hasBeenPredicted = cmd.hasBeenPredicted;
    client->absAngles = cmd.viewAngles;
    (void)SyncPlayerState(clientIndex);
    return true;
}


namespace {
constexpr float kMovementDegreesToRadians = 0.01745329251994329577f;
constexpr float kMovementAccelerate = 5.0f;
constexpr float kMovementAirAccelerate = 10.0f;
constexpr float kMovementFriction = 4.0f;
constexpr float kMovementStopSpeed = 75.0f;
constexpr float kMovementGravity = 800.0f;
constexpr float kMovementMaxSpeed = 250.0f;
constexpr float kMovementAirSpeedCap = 30.0f;
constexpr float kMovementJumpImpulse = 268.3281572999747f;
constexpr float kMovementStopEpsilon = 0.1f;
constexpr float kMovementStepSize = 18.0f;
constexpr float kMovementDistEpsilon = 0.03125f;
constexpr float kMovementWalkableNormalZ = 0.7f;
constexpr float kMovementStayOnGroundEpsilon = 0.015625f;
constexpr int kMovementMaxBumps = 4;
constexpr int kMovementMaxClipPlanes = 5;
constexpr std::int32_t kMovementButtonJump = 1 << 1;
constexpr std::int32_t kMovementButtonDuck = 1 << 2;
constexpr float kMovementDuckHullHeight = 36.0f;
constexpr float kMovementStandHullHeight = 72.0f;
constexpr float kMovementDuckSpeedScale = 0.34f;
constexpr float kMovementLadderAttachPadding = 2.0f;
constexpr float kMovementLadderJumpSpeed = 270.0f;
constexpr float kMovementWaterSpeedScale = 0.8f;
constexpr float kMovementWaterSinkSpeed = 60.0f;
constexpr float kMovementWaterJumpSpeed = 100.0f;
constexpr float kMovementSlimeJumpSpeed = 80.0f;
constexpr float kMovementStandingViewOffsetZ = 64.0f;
constexpr float kMovementDuckedViewOffsetZ = 28.0f;

float HorizontalSpeed(const build4100::Vector& velocity) noexcept {
    return std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
}

void ApplyGroundFriction(build4100::Vector& velocity, const float frameTime) noexcept {
    const float speed = HorizontalSpeed(velocity);
    if (speed < kMovementStopEpsilon) {
        velocity.x = 0.0f;
        velocity.y = 0.0f;
        return;
    }
    const float control = std::max(speed, kMovementStopSpeed);
    const float drop = control * kMovementFriction * frameTime;
    const float newSpeed = std::max(speed - drop, 0.0f);
    if (newSpeed == speed) return;
    const float scale = newSpeed / speed;
    velocity.x *= scale;
    velocity.y *= scale;
}

void BuildAngleVectors(const build4100::QAngle& angles,
                       build4100::Vector& forward,
                       build4100::Vector& right) noexcept {
    const float pitch = angles.x * kMovementDegreesToRadians;
    const float yaw = angles.y * kMovementDegreesToRadians;
    const float roll = angles.z * kMovementDegreesToRadians;
    const float sp = std::sin(pitch), cp = std::cos(pitch);
    const float sy = std::sin(yaw), cy = std::cos(yaw);
    const float sr = std::sin(roll), cr = std::cos(roll);
    forward = build4100::Vector{cp * cy, cp * sy, -sp};
    right = build4100::Vector{-sr * sp * cy + cr * sy,
                              -sr * sp * sy - cr * cy,
                              -sr * cp};
}

void BuildWishDirection(const build4100::UserCmd& cmd,
                        build4100::Vector& wishDirection,
                        float& wishSpeed) noexcept {
    const float yaw = cmd.viewAngles.y * kMovementDegreesToRadians;
    float cosine = std::cos(yaw);
    float sine = std::sin(yaw);
    if (std::fabs(cosine) < 1.0e-6f) cosine = 0.0f;
    if (std::fabs(sine) < 1.0e-6f) sine = 0.0f;

    // Source AngleVectors at pitch/roll zero: forward=(cos,sin,0),
    // right=(sin,-cos,0). Positive sideMove therefore points right.
    const float wishX = cosine * cmd.forwardMove + sine * cmd.sideMove;
    const float wishY = sine * cmd.forwardMove - cosine * cmd.sideMove;
    wishSpeed = std::sqrt(wishX * wishX + wishY * wishY);
    if (wishSpeed <= 0.0f) {
        wishDirection = build4100::Vector{};
        return;
    }
    const float invSpeed = 1.0f / wishSpeed;
    wishDirection = build4100::Vector{wishX * invSpeed, wishY * invSpeed, 0.0f};
    wishSpeed = std::min(wishSpeed, kMovementMaxSpeed);
}

void Accelerate(build4100::Vector& velocity, const build4100::Vector& wishDirection,
                const float wishSpeed, const float acceleration,
                const float frameTime, const float speedCap) noexcept {
    if (wishSpeed <= 0.0f) return;
    const float cappedWishSpeed = std::min(wishSpeed, speedCap);
    const float currentSpeed = velocity.x * wishDirection.x + velocity.y * wishDirection.y;
    const float addSpeed = cappedWishSpeed - currentSpeed;
    if (addSpeed <= 0.0f) return;

    // AirAccelerate caps only addSpeed/currentSpeed while retaining the original
    // wish speed in the acceleration term, matching the build4100 branch.
    float accelerateSpeed = acceleration * frameTime * wishSpeed;
    accelerateSpeed = std::min(accelerateSpeed, addSpeed);
    velocity.x += accelerateSpeed * wishDirection.x;
    velocity.y += accelerateSpeed * wishDirection.y;
}

float Dot(const build4100::Vector& a, const build4100::Vector& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

build4100::Vector Cross(const build4100::Vector& a, const build4100::Vector& b) noexcept {
    return build4100::Vector{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float Length(const build4100::Vector& value) noexcept {
    return std::sqrt(Dot(value, value));
}

build4100::Vector Normalize(const build4100::Vector& value) noexcept {
    const float length = Length(value);
    if (length <= 1.0e-8f) return build4100::Vector{};
    const float inv = 1.0f / length;
    return build4100::Vector{value.x * inv, value.y * inv, value.z * inv};
}

// CGameMovement::ClipVelocity from build4100: overbounce is applied to the
// incoming component, then a negative residual into the plane is removed.
int ClipVelocity(const build4100::Vector& input, const build4100::Vector& normal,
                 build4100::Vector& output, const float overbounce = 1.0f) noexcept {
    int blocked = 0;
    if (normal.z > 0.0f) blocked |= 1;
    if (normal.z == 0.0f) blocked |= 2;

    const float backoff = Dot(input, normal) * overbounce;
    output.x = input.x - normal.x * backoff;
    output.y = input.y - normal.y * backoff;
    output.z = input.z - normal.z * backoff;

    const float adjust = Dot(output, normal);
    if (adjust < 0.0f) {
        output.x -= normal.x * adjust;
        output.y -= normal.y * adjust;
        output.z -= normal.z * adjust;
    }
    return blocked;
}
}  // namespace

bool ClientRegistry::ApplyUsercmdMovement(const std::size_t clientIndex,
                                           build4100::Edict* const edict,
                                           const build4100::UserCmd* const cmds,
                                           const std::size_t cmdCount,
                                           const int numCmds,
                                           const float intervalPerTick,
                                           const bool paused) noexcept {
    ClientRecord* const client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict) || numCmds < 0 ||
        cmdCount < static_cast<std::size_t>(numCmds) ||
        (cmdCount != 0 && !cmds) || !(intervalPerTick > 0.0f)) {
        return false;
    }
    if (paused || numCmds == 0) return true;

    auto traceDynamicPlayer = [&](const ClientRecord& other,
                                  const std::size_t otherIndex,
                                  const build4100::Vector& from,
                                  const build4100::Vector& to,
                                  PlayerHullTraceResult& trace) noexcept -> bool {
        constexpr float kSweepEpsilon = 1.0e-4f;
        const build4100::Vector expandedMins{
            other.absOrigin.x + other.playerMins.x - client->playerMaxs.x,
            other.absOrigin.y + other.playerMins.y - client->playerMaxs.y,
            other.absOrigin.z + other.playerMins.z - client->playerMaxs.z,
        };
        const build4100::Vector expandedMaxs{
            other.absOrigin.x + other.playerMaxs.x - client->playerMins.x,
            other.absOrigin.y + other.playerMaxs.y - client->playerMins.y,
            other.absOrigin.z + other.playerMaxs.z - client->playerMins.z,
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
            trace.contents = static_cast<std::int32_t>(build4100::kContentsMonster);
            trace.startSolid = true;
            trace.allSolid = strictInside(to);
            trace.hitClientIndex = otherIndex;
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
                // Exact contact while moving parallel to a face must remain
                // slideable instead of becoming a false start-solid hit.
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
        trace.contents = static_cast<std::int32_t>(build4100::kContentsMonster);
        trace.hitClientIndex = otherIndex;
        return true;
    };

    auto traceHull = [&](const build4100::Vector& from, const build4100::Vector& to,
                         PlayerHullTraceResult& trace) noexcept -> bool {
        if (!client->movementWorldTraceEnabled || !movementTraceFn_) return false;

        PlayerHullTraceResult best{};
        best.endPos = to;
        best.fraction = 1.0f;
        bool traced = movementTraceFn_(movementTraceContext_, from, to,
                                      client->playerMins, client->playerMaxs,
                                      build4100::kMaskPlayerSolid, best);
        best.hitClientIndex = 0;

        // build4100 TryPlayerMove constructs CTraceFilterSimple(self, 8), where
        // 8 is COLLISION_GROUP_PLAYER_MOVEMENT, then calls TraceRay with
        // MASK_PLAYERSOLID. Reconstruct the player-entity portion locally and
        // merge it with the already-validated BSP CM_BoxTrace result.
        if ((build4100::kMaskPlayerSolid & build4100::kContentsMonster) != 0u) {
            for (std::size_t otherIndex = 1; otherIndex < clients_.size(); ++otherIndex) {
                if (otherIndex == clientIndex) continue;
                const ClientRecord& other = clients_[otherIndex];
                if (other.state != ClientLifecycleState::Active || other.isDead || other.isObserver) continue;
                if (!(other.playerMaxs.x > other.playerMins.x) ||
                    !(other.playerMaxs.y > other.playerMins.y) ||
                    !(other.playerMaxs.z > other.playerMins.z)) continue;

                PlayerHullTraceResult dynamicTrace{};
                if (!traceDynamicPlayer(other, otherIndex, from, to, dynamicTrace)) continue;
                traced = true;
                const bool replace = dynamicTrace.startSolid ||
                    (!best.startSolid && dynamicTrace.fraction < best.fraction);
                if (replace) best = dynamicTrace;
            }
        }

        trace = best;
        return traced;
    };

    auto updateDuckState = [&](const build4100::UserCmd& cmd) noexcept {
        const bool wantsDuck = (cmd.buttons & kMovementButtonDuck) != 0;
        if (wantsDuck) {
            client->movementDucked = true;
            client->playerMaxs.z = kMovementDuckHullHeight;
            client->movementOldButtons |= kMovementButtonDuck;
            return;
        }
        if (!client->movementDucked) {
            client->playerMaxs.z = kMovementStandHullHeight;
            client->movementOldButtons &= ~kMovementButtonDuck;
            return;
        }

        // Source refuses to unduck into solid geometry. Probe the standing hull
        // at the current feet position, restoring the duck hull when blocked.
        const float duckHeight = client->playerMaxs.z;
        client->playerMaxs.z = kMovementStandHullHeight;
        bool blocked = false;
        if (client->movementWorldTraceEnabled && movementTraceFn_) {
            PlayerHullTraceResult trace{};
            if (traceHull(client->absOrigin, client->absOrigin, trace)) {
                blocked = trace.startSolid || trace.allSolid;
            }
        }
        if (blocked) {
            client->playerMaxs.z = duckHeight;
            client->movementDucked = true;
        } else {
            client->movementDucked = false;
        }
        client->movementOldButtons &= ~kMovementButtonDuck;
    };

    auto findLadder = [&]() noexcept -> std::size_t {
        if (movementLadders_.empty()) return 0;
        const build4100::Vector playerMins{
            client->absOrigin.x + client->playerMins.x,
            client->absOrigin.y + client->playerMins.y,
            client->absOrigin.z + client->playerMins.z,
        };
        const build4100::Vector playerMaxs{
            client->absOrigin.x + client->playerMaxs.x,
            client->absOrigin.y + client->playerMaxs.y,
            client->absOrigin.z + client->playerMaxs.z,
        };
        for (std::size_t i = 0; i < movementLadders_.size(); ++i) {
            const MovementLadderVolume& ladder = movementLadders_[i];
            if (playerMaxs.x < ladder.mins.x - kMovementLadderAttachPadding ||
                playerMins.x > ladder.maxs.x + kMovementLadderAttachPadding ||
                playerMaxs.y < ladder.mins.y - kMovementLadderAttachPadding ||
                playerMins.y > ladder.maxs.y + kMovementLadderAttachPadding ||
                playerMaxs.z < ladder.mins.z - kMovementLadderAttachPadding ||
                playerMins.z > ladder.maxs.z + kMovementLadderAttachPadding) {
                continue;
            }
            return i + 1;
        }
        return 0;
    };

    auto checkWater = [&]() noexcept -> bool {
        client->movementWaterLevel = 0;
        client->movementWaterType = 0;
        client->movementBaseVelocity = build4100::Vector{};
        if (!client->movementWorldTraceEnabled || !movementPointContentsFn_) return false;
        const float centerX = client->absOrigin.x + (client->playerMins.x + client->playerMaxs.x) * 0.5f;
        const float centerY = client->absOrigin.y + (client->playerMins.y + client->playerMaxs.y) * 0.5f;
        build4100::Vector point{centerX, centerY, client->absOrigin.z + client->playerMins.z + 1.0f};
        const std::int32_t feet = movementPointContentsFn_(movementTraceContext_, point);
        if ((static_cast<std::uint32_t>(feet) & build4100::kMaskWater) == 0u) return false;
        client->movementWaterLevel = 1;
        client->movementWaterType = feet & static_cast<std::int32_t>(build4100::kContentsWater | build4100::kContentsSlime);
        point.z = client->absOrigin.z + (client->playerMins.z + client->playerMaxs.z) * 0.5f;
        const std::int32_t waist = movementPointContentsFn_(movementTraceContext_, point);
        if ((static_cast<std::uint32_t>(waist) & build4100::kMaskWater) != 0u) {
            client->movementWaterLevel = 2;
            point.z = client->absOrigin.z +
                (client->movementDucked ? kMovementDuckedViewOffsetZ : kMovementStandingViewOffsetZ);
            const std::int32_t eyes = movementPointContentsFn_(movementTraceContext_, point);
            if ((static_cast<std::uint32_t>(eyes) & build4100::kMaskWater) != 0u) client->movementWaterLevel = 3;
        }
        const std::uint32_t current = static_cast<std::uint32_t>(feet) & build4100::kMaskCurrent;
        build4100::Vector dir{};
        if (current & build4100::kContentsCurrent0) dir.x += 1.0f;
        if (current & build4100::kContentsCurrent90) dir.y += 1.0f;
        if (current & build4100::kContentsCurrent180) dir.x -= 1.0f;
        if (current & build4100::kContentsCurrent270) dir.y -= 1.0f;
        if (current & build4100::kContentsCurrentUp) dir.z += 1.0f;
        if (current & build4100::kContentsCurrentDown) dir.z -= 1.0f;
        const float scale = 50.0f * static_cast<float>(client->movementWaterLevel);
        client->movementBaseVelocity = build4100::Vector{dir.x * scale, dir.y * scale, dir.z * scale};
        return client->movementWaterLevel > 1;
    };

    auto categorizeGround = [&]() noexcept {
        if (!client->movementWorldTraceEnabled || !movementTraceFn_) return;
        if (client->absVelocity.z > 140.0f) {
            client->movementOnGround = false;
            return;
        }
        build4100::Vector from = client->absOrigin;
        build4100::Vector to = client->absOrigin;
        from.z += 2.0f;
        to.z -= 2.0f;
        PlayerHullTraceResult trace{};
        if (!traceHull(from, to, trace) || trace.allSolid || trace.startSolid ||
            trace.fraction >= 1.0f || trace.planeNormal.z < kMovementWalkableNormalZ) {
            client->movementOnGround = false;
            return;
        }
        client->movementOnGround = true;
        client->absOrigin = trace.endPos;
        if (client->absVelocity.z < 0.0f) client->absVelocity.z = 0.0f;
    };

    auto tryPlayerMove = [&](const float frameTime,
                             const build4100::Vector* const firstDestination,
                             const PlayerHullTraceResult* const firstTrace) noexcept -> int {
        if (!client->movementWorldTraceEnabled || !movementTraceFn_) {
            client->absOrigin.x += client->absVelocity.x * frameTime;
            client->absOrigin.y += client->absVelocity.y * frameTime;
            if (!client->movementOnGround) client->absOrigin.z += client->absVelocity.z * frameTime;
            return 0;
        }

        build4100::Vector planes[kMovementMaxClipPlanes]{};
        int numPlanes = 0;
        int blocked = 0;
        float timeLeft = frameTime;
        float allFraction = 0.0f;
        build4100::Vector originalVelocity = client->absVelocity;
        const build4100::Vector primalVelocity = client->absVelocity;

        for (int bump = 0; bump < kMovementMaxBumps; ++bump) {
            if (Length(client->absVelocity) <= 1.0e-8f || timeLeft <= 0.0f) break;

            build4100::Vector end{
                client->absOrigin.x + client->absVelocity.x * timeLeft,
                client->absOrigin.y + client->absVelocity.y * timeLeft,
                client->absOrigin.z + client->absVelocity.z * timeLeft,
            };
            PlayerHullTraceResult trace{};
            bool traced = false;
            if (bump == 0 && firstDestination && firstTrace &&
                std::fabs(end.x - firstDestination->x) < 1.0e-5f &&
                std::fabs(end.y - firstDestination->y) < 1.0e-5f &&
                std::fabs(end.z - firstDestination->z) < 1.0e-5f) {
                trace = *firstTrace;
                traced = true;
            } else {
                traced = traceHull(client->absOrigin, end, trace);
            }
            if (!traced) {
                client->absOrigin = end;
                break;
            }

            allFraction += trace.fraction;
            if (trace.allSolid) {
                client->absVelocity = build4100::Vector{};
                return 4;
            }

            if (trace.fraction > 0.0f) {
                client->absOrigin = trace.endPos;
                originalVelocity = client->absVelocity;
                numPlanes = 0;
            }
            if (trace.fraction >= 1.0f) break;

            if (trace.planeNormal.z > kMovementWalkableNormalZ) blocked |= 1;
            if (trace.planeNormal.z == 0.0f) blocked |= 2;

            timeLeft -= timeLeft * trace.fraction;
            if (numPlanes >= kMovementMaxClipPlanes) {
                client->absVelocity = build4100::Vector{};
                break;
            }
            planes[numPlanes++] = trace.planeNormal;

            build4100::Vector newVelocity{};
            int planeIndex = 0;
            for (; planeIndex < numPlanes; ++planeIndex) {
                ClipVelocity(originalVelocity, planes[planeIndex], newVelocity, 1.0f);
                int otherPlane = 0;
                for (; otherPlane < numPlanes; ++otherPlane) {
                    if (otherPlane == planeIndex) continue;
                    if (Dot(newVelocity, planes[otherPlane]) < 0.0f) break;
                }
                if (otherPlane == numPlanes) break;
            }

            if (planeIndex == numPlanes) {
                if (numPlanes != 2) {
                    client->absVelocity = build4100::Vector{};
                    break;
                }
                const build4100::Vector direction = Normalize(Cross(planes[0], planes[1]));
                const float distance = Dot(direction, client->absVelocity);
                newVelocity = build4100::Vector{
                    direction.x * distance,
                    direction.y * distance,
                    direction.z * distance,
                };
            }

            client->absVelocity = newVelocity;
            if (Dot(client->absVelocity, primalVelocity) <= 0.0f) {
                client->absVelocity = build4100::Vector{};
                break;
            }
        }

        if (allFraction == 0.0f) client->absVelocity = build4100::Vector{};
        return blocked;
    };

    auto stayOnGround = [&]() noexcept {
        if (!client->movementWorldTraceEnabled || !movementTraceFn_) return;

        build4100::Vector start = client->absOrigin;
        build4100::Vector end = client->absOrigin;
        start.z += 2.0f;
        end.z -= kMovementStepSize;

        // build4100 first traces upward to a safe start if the +2 point itself
        // is obstructed, then traces down by m_flStepSize.
        PlayerHullTraceResult upTrace{};
        build4100::Vector safeStart = client->absOrigin;
        if (traceHull(client->absOrigin, start, upTrace) &&
            !upTrace.startSolid && !upTrace.allSolid) {
            safeStart = upTrace.endPos;
        }
        end = safeStart;
        end.z -= kMovementStepSize + 2.0f;

        PlayerHullTraceResult downTrace{};
        if (!traceHull(safeStart, end, downTrace) || downTrace.startSolid || downTrace.allSolid ||
            downTrace.fraction <= 0.0f || downTrace.fraction >= 1.0f ||
            downTrace.planeNormal.z < kMovementWalkableNormalZ) {
            return;
        }
        if (std::fabs(client->absOrigin.z - downTrace.endPos.z) > kMovementStayOnGroundEpsilon) {
            client->absOrigin = downTrace.endPos;
        }
        client->movementOnGround = true;
        if (client->absVelocity.z < 0.0f) client->absVelocity.z = 0.0f;
    };

    auto stepMove = [&](const float frameTime,
                        const build4100::Vector& destination,
                        const PlayerHullTraceResult& firstTrace) noexcept {
        const build4100::Vector startOrigin = client->absOrigin;
        const build4100::Vector startVelocity = client->absVelocity;

        // First try the normal (non-step) movement path.
        (void)tryPlayerMove(frameTime, &destination, &firstTrace);
        const build4100::Vector downOrigin = client->absOrigin;
        const build4100::Vector downVelocity = client->absVelocity;

        // Restore and try moving up by one player step plus the original
        // DIST_EPSILON before replaying the horizontal movement.
        client->absOrigin = startOrigin;
        client->absVelocity = startVelocity;
        build4100::Vector up = startOrigin;
        up.z += kMovementStepSize + kMovementDistEpsilon;
        PlayerHullTraceResult upTrace{};
        if (!traceHull(startOrigin, up, upTrace) || upTrace.startSolid || upTrace.allSolid) {
            client->absOrigin = downOrigin;
            client->absVelocity = downVelocity;
            return;
        }
        client->absOrigin = upTrace.endPos;
        (void)tryPlayerMove(frameTime, nullptr, nullptr);

        build4100::Vector down = client->absOrigin;
        down.z -= kMovementStepSize + kMovementDistEpsilon;
        PlayerHullTraceResult downTrace{};
        if (!traceHull(client->absOrigin, down, downTrace)) {
            client->absOrigin = downOrigin;
            client->absVelocity = downVelocity;
            return;
        }
        if (downTrace.planeNormal.z < kMovementWalkableNormalZ) {
            client->absOrigin = downOrigin;
            client->absVelocity = downVelocity;
            return;
        }
        if (!downTrace.startSolid && !downTrace.allSolid) client->absOrigin = downTrace.endPos;

        const float downDx = downOrigin.x - startOrigin.x;
        const float downDy = downOrigin.y - startOrigin.y;
        const float upDx = client->absOrigin.x - startOrigin.x;
        const float upDy = client->absOrigin.y - startOrigin.y;
        const float downDistance = downDx * downDx + downDy * downDy;
        const float upDistance = upDx * upDx + upDy * upDy;
        if (downDistance > upDistance) {
            client->absOrigin = downOrigin;
            client->absVelocity = downVelocity;
        } else {
            client->absVelocity.z = downVelocity.z;
        }
    };

    auto walkMove = [&](const float frameTime) noexcept {
        client->absVelocity.z = 0.0f;
        if (HorizontalSpeed(client->absVelocity) < 1.0f) {
            client->absVelocity.x = 0.0f;
            client->absVelocity.y = 0.0f;
            stayOnGround();
            return;
        }

        build4100::Vector destination{
            client->absOrigin.x + client->absVelocity.x * frameTime,
            client->absOrigin.y + client->absVelocity.y * frameTime,
            client->absOrigin.z,
        };
        PlayerHullTraceResult trace{};
        if (!traceHull(client->absOrigin, destination, trace)) {
            client->absOrigin = destination;
            return;
        }
        if (trace.fraction >= 1.0f) {
            client->absOrigin = trace.endPos;
            stayOnGround();
            return;
        }

        // Build4100 only attempts StepMove while grounded; callers enter here
        // through the ground-movement branch, so the condition is explicit.
        if (client->movementOnGround) stepMove(frameTime, destination, trace);
        else (void)tryPlayerMove(frameTime, &destination, &trace);
        stayOnGround();
    };

    auto waterMove = [&](const build4100::UserCmd& cmd, const float frameTime) noexcept {
        build4100::Vector forward{}, right{};
        BuildAngleVectors(cmd.viewAngles, forward, right);
        build4100::Vector wishVelocity{forward.x * cmd.forwardMove + right.x * cmd.sideMove,
                                       forward.y * cmd.forwardMove + right.y * cmd.sideMove,
                                       forward.z * cmd.forwardMove + right.z * cmd.sideMove};
        if ((cmd.buttons & kMovementButtonJump) != 0) {
            wishVelocity.z += kMovementMaxSpeed;
        } else if (cmd.forwardMove == 0.0f && cmd.sideMove == 0.0f && cmd.upMove == 0.0f) {
            wishVelocity.z -= kMovementWaterSinkSpeed;
        } else {
            wishVelocity.z += cmd.upMove + std::clamp(cmd.forwardMove * forward.z * 2.0f, 0.0f, kMovementMaxSpeed);
        }
        build4100::Vector wishDirection = Normalize(wishVelocity);
        float wishSpeed = std::min(Length(wishVelocity), kMovementMaxSpeed) * kMovementWaterSpeedScale;
        float speed = Length(client->absVelocity);
        if (speed > 0.0f) {
            const float newSpeed = speed - frameTime * speed * kMovementFriction;
            if (newSpeed < kMovementStopEpsilon) client->absVelocity = build4100::Vector{};
            else {
                const float scale = newSpeed / speed;
                client->absVelocity.x *= scale; client->absVelocity.y *= scale; client->absVelocity.z *= scale;
            }
        }
        if (wishSpeed > 0.0f) {
            const float addSpeed = wishSpeed - Dot(client->absVelocity, wishDirection);
            if (addSpeed > 0.0f) {
                const float accelSpeed = std::min(kMovementAccelerate * wishSpeed * frameTime, addSpeed);
                client->absVelocity.x += accelSpeed * wishDirection.x;
                client->absVelocity.y += accelSpeed * wishDirection.y;
                client->absVelocity.z += accelSpeed * wishDirection.z;
            }
        }
        client->absVelocity.x += client->movementBaseVelocity.x;
        client->absVelocity.y += client->movementBaseVelocity.y;
        client->absVelocity.z += client->movementBaseVelocity.z;
        build4100::Vector destination{client->absOrigin.x + client->absVelocity.x * frameTime,
                                      client->absOrigin.y + client->absVelocity.y * frameTime,
                                      client->absOrigin.z + client->absVelocity.z * frameTime};
        PlayerHullTraceResult trace{};
        if (!traceHull(client->absOrigin, destination, trace)) client->absOrigin = destination;
        else if (trace.fraction >= 1.0f) { client->absOrigin = trace.endPos; if (client->movementOnGround) stayOnGround(); }
        else if (client->movementOnGround) stepMove(frameTime, destination, trace);
        else (void)tryPlayerMove(frameTime, &destination, &trace);
        client->absVelocity.x -= client->movementBaseVelocity.x;
        client->absVelocity.y -= client->movementBaseVelocity.y;
        client->absVelocity.z -= client->movementBaseVelocity.z;
    };

    auto ladderMove = [&](const build4100::UserCmd& cmd, const std::size_t ladderIndex,
                          const float frameTime) noexcept -> bool {
        if (ladderIndex == 0 || ladderIndex > movementLadders_.size()) return false;
        const MovementLadderVolume& ladder = movementLadders_[ladderIndex - 1];
        const float widthX = ladder.maxs.x - ladder.mins.x;
        const float widthY = ladder.maxs.y - ladder.mins.y;
        const float centerX = (ladder.mins.x + ladder.maxs.x) * 0.5f;
        const float centerY = (ladder.mins.y + ladder.maxs.y) * 0.5f;
        build4100::Vector normal{};
        if (widthX <= widthY) {
            normal.x = client->absOrigin.x >= centerX ? 1.0f : -1.0f;
        } else {
            normal.y = client->absOrigin.y >= centerY ? 1.0f : -1.0f;
        }

        build4100::Vector forward{}, right{};
        BuildAngleVectors(cmd.viewAngles, forward, right);
        const float facingDot = forward.x * normal.x + forward.y * normal.y;
        const bool facingIntoLadder = facingDot < -0.10f;
        const bool explicitVertical = std::fabs(cmd.upMove) > 0.01f;
        if (!client->movementOnLadder && !explicitVertical &&
            std::fabs(cmd.forwardMove) > 0.01f && !facingIntoLadder) {
            return false;
        }

        client->movementOnLadder = true;
        client->movementLadderIndex = ladderIndex;
        client->movementOnGround = false;

        if ((cmd.buttons & kMovementButtonJump) != 0) {
            client->movementOnLadder = false;
            client->movementLadderIndex = 0;
            client->absVelocity = build4100::Vector{
                normal.x * kMovementLadderJumpSpeed,
                normal.y * kMovementLadderJumpSpeed,
                50.0f,
            };
            client->movementOldButtons |= kMovementButtonJump;
            return true;
        }

        const build4100::Vector tangent{-normal.y, normal.x, 0.0f};
        float climbSpeed = cmd.upMove;
        if (facingIntoLadder || client->movementLadderIndex == ladderIndex) climbSpeed += cmd.forwardMove;
        climbSpeed = std::clamp(climbSpeed, -kMovementMaxSpeed, kMovementMaxSpeed);
        const float tangentSpeed = std::clamp(cmd.sideMove, -kMovementMaxSpeed, kMovementMaxSpeed);
        client->absVelocity = build4100::Vector{
            tangent.x * tangentSpeed,
            tangent.y * tangentSpeed,
            climbSpeed,
        };

        const build4100::Vector destination{
            client->absOrigin.x + client->absVelocity.x * frameTime,
            client->absOrigin.y + client->absVelocity.y * frameTime,
            client->absOrigin.z + client->absVelocity.z * frameTime,
        };
        PlayerHullTraceResult trace{};
        if (!traceHull(client->absOrigin, destination, trace) || trace.fraction >= 1.0f) {
            client->absOrigin = trace.fraction >= 1.0f ? trace.endPos : destination;
        } else if (!trace.startSolid && !trace.allSolid) {
            client->absOrigin = trace.endPos;
            build4100::Vector clipped{};
            (void)ClipVelocity(client->absVelocity, trace.planeNormal, clipped);
            client->absVelocity = clipped;
        } else {
            client->absVelocity = build4100::Vector{};
        }
        client->movementOldButtons &= ~kMovementButtonJump;
        return true;
    };

    const std::size_t firstNew = cmdCount - static_cast<std::size_t>(numCmds);
    for (std::size_t i = firstNew; i < cmdCount; ++i) {
        const build4100::UserCmd& cmd = cmds[i];
        updateDuckState(cmd);
        build4100::UserCmd effectiveCmd = cmd;
        if (client->movementDucked) {
            effectiveCmd.forwardMove *= kMovementDuckSpeedScale;
            effectiveCmd.sideMove *= kMovementDuckSpeedScale;
            effectiveCmd.upMove *= kMovementDuckSpeedScale;
        }
        if (client->movementWorldTraceEnabled) categorizeGround();
        const bool inWater = checkWater();
        const std::size_t ladderIndex = inWater ? 0 : findLadder();
        if (!inWater && ladderIndex != 0 && ladderMove(effectiveCmd, ladderIndex, intervalPerTick)) {
            (void)checkWater();
            continue;
        }
        client->movementOnLadder = false;
        client->movementLadderIndex = 0;
        if (!inWater) client->absVelocity.z -= kMovementGravity * 0.5f * intervalPerTick;

        const bool jumpPressed = (cmd.buttons & kMovementButtonJump) != 0;
        if (inWater && jumpPressed) {
            client->movementOnGround = false;
            if (client->movementWaterType & static_cast<std::int32_t>(build4100::kContentsWater)) client->absVelocity.z = kMovementWaterJumpSpeed;
            else if (client->movementWaterType & static_cast<std::int32_t>(build4100::kContentsSlime)) client->absVelocity.z = kMovementSlimeJumpSpeed;
            client->movementOldButtons &= ~kMovementButtonJump;
        } else if (jumpPressed) {
            if (client->movementOnGround && (client->movementOldButtons & kMovementButtonJump) == 0) {
                client->movementOnGround = false;
                client->absVelocity.z += kMovementJumpImpulse;
                client->absVelocity.z -= kMovementGravity * 0.5f * intervalPerTick;
            }
            client->movementOldButtons |= kMovementButtonJump;
        } else client->movementOldButtons &= ~kMovementButtonJump;

        if (inWater) {
            waterMove(effectiveCmd, intervalPerTick);
            if (client->movementWorldTraceEnabled) categorizeGround();
            if (client->movementOnGround) client->absVelocity.z = 0.0f;
        } else {
            build4100::Vector wishDirection{}; float wishSpeed = 0.0f;
            BuildWishDirection(effectiveCmd, wishDirection, wishSpeed);
            if (client->movementOnGround) {
                client->absVelocity.z = 0.0f;
                ApplyGroundFriction(client->absVelocity, intervalPerTick);
                Accelerate(client->absVelocity, wishDirection, wishSpeed, kMovementAccelerate, intervalPerTick, kMovementMaxSpeed);
                client->absVelocity.z = 0.0f;
                if (client->movementWorldTraceEnabled) walkMove(intervalPerTick);
                else { client->absOrigin.x += client->absVelocity.x * intervalPerTick; client->absOrigin.y += client->absVelocity.y * intervalPerTick; }
            } else {
                Accelerate(client->absVelocity, wishDirection, wishSpeed, kMovementAirAccelerate, intervalPerTick, kMovementAirSpeedCap);
                (void)tryPlayerMove(intervalPerTick, nullptr, nullptr);
            }
            if (!client->movementWorldTraceEnabled) {
                if (client->absOrigin.z <= 0.0f && client->absVelocity.z <= 0.0f) {
                    client->absOrigin.z = 0.0f; client->absVelocity.z = 0.0f; client->movementOnGround = true;
                }
            } else categorizeGround();
        }
        const bool stillInWater = checkWater();
        if (!stillInWater) client->absVelocity.z -= kMovementGravity * 0.5f * intervalPerTick;
        if (client->movementOnGround) client->absVelocity.z = 0.0f;
    }
    return true;
}

bool ClientRegistry::SyncPlayerState(const std::size_t clientIndex) noexcept {
    ClientRecord* const client = Get(clientIndex);
    if (!client || client->state == ClientLifecycleState::Empty) return false;
    client->playerState.deadflag = client->isDead;
    client->playerState.vAngle = client->absAngles;
    client->playerState.netname = 0;  // string_t pool reconstruction is a later engine milestone.
    client->playerState.fixangle = 0;
    client->playerState.anglechange = build4100::Vector{};
    client->playerState.hltv = client->isHLTV;
    client->playerState.frags = client->fragCount;
    client->playerState.deaths = client->deathCount;
    return true;
}

build4100::PlayerState* ClientRegistry::PlayerStateFor(
    const std::size_t clientIndex, build4100::Edict* const edict) noexcept {
    ClientRecord* const client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict)) return nullptr;
    (void)SyncPlayerState(clientIndex);
    return &client->playerState;
}

bool ClientRegistry::SetNetworkIdByName(const char* const name, const char* const networkId) {
    if (!name || !networkId) return false;
    for (std::size_t index = 1; index < clients_.size(); ++index) {
        ClientRecord& client = clients_[index];
        if (client.state != ClientLifecycleState::Empty && client.name == name) {
            client.networkId = networkId;
            return true;
        }
    }
    return false;
}

bool ClientRegistry::MarkFakeClient(const std::size_t clientIndex,
                                    build4100::Edict* const edict,
                                    const bool isFake) noexcept {
    ClientRecord* client = Get(clientIndex);
    if (!client || !Matches(clientIndex, edict)) return false;
    client->isFakeClient = isFake;
    return true;
}

bool ClientRegistry::SetCommandClient(const int zeroBasedClientIndex) noexcept {
    if (zeroBasedClientIndex < 0) {
        commandClient_ = -1;
        return true;
    }
    const std::size_t clientIndex = static_cast<std::size_t>(zeroBasedClientIndex) + 1;
    const ClientRecord* client = Get(clientIndex);
    if (!client || client->state == ClientLifecycleState::Empty) return false;
    commandClient_ = zeroBasedClientIndex;
    return true;
}

std::size_t ClientRegistry::ConnectedCount() const noexcept {
    std::size_t count = 0;
    for (std::size_t index = 1; index < clients_.size(); ++index) {
        if (clients_[index].state != ClientLifecycleState::Empty) ++count;
    }
    return count;
}

}  // namespace cssv34::reconstructed
