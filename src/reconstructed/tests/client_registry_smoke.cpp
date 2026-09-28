#include <cassert>
#include <cstring>
#include <cmath>
#include <vector>
#include <iostream>

#include "server/client_registry.h"
#include "public/build4100/engine_trace.h"

using cssv34::build4100::Edict;
using cssv34::reconstructed::ClientLifecycleState;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::PlayerHullTraceResult;


namespace {

bool TraceFloorAndWall(void*, const cssv34::build4100::Vector& start,
                       const cssv34::build4100::Vector& end,
                       const cssv34::build4100::Vector& mins,
                       const cssv34::build4100::Vector& maxs,
                       std::uint32_t, PlayerHullTraceResult& trace) noexcept {
    trace = PlayerHullTraceResult{};
    trace.endPos = end;
    trace.fraction = 1.0f;

    float best = 1.0f;
    cssv34::build4100::Vector normal{};
    // Ground plane z=0 for the player's bottom (mins.z == 0 in standing hull).
    const float startBottom = start.z + mins.z;
    const float endBottom = end.z + mins.z;
    if (startBottom >= 0.0f && endBottom < 0.0f && startBottom != endBottom) {
        const float f = startBottom / (startBottom - endBottom);
        if (f < best) {
            best = f;
            normal = cssv34::build4100::Vector{0.0f, 0.0f, 1.0f};
        }
    }

    // Vertical wall at world y=16.1. Standing maxs.y=16 means the player's
    // origin can advance only to y=0.1 before the hull touches the wall.
    constexpr float kWallY = 16.1f;
    const float startSide = start.y + maxs.y;
    const float endSide = end.y + maxs.y;
    if (startSide <= kWallY && endSide > kWallY && startSide != endSide) {
        const float f = (kWallY - startSide) / (endSide - startSide);
        if (f < best) {
            best = f;
            normal = cssv34::build4100::Vector{0.0f, -1.0f, 0.0f};
        }
    }

    if (best < 1.0f) {
        trace.fraction = best;
        trace.endPos = cssv34::build4100::Vector{
            start.x + (end.x - start.x) * best,
            start.y + (end.y - start.y) * best,
            start.z + (end.z - start.z) * best,
        };
        trace.planeNormal = normal;
    }
    return true;
}


bool TraceFlatFloor(void*, const cssv34::build4100::Vector& start,
                    const cssv34::build4100::Vector& end,
                    const cssv34::build4100::Vector& mins,
                    const cssv34::build4100::Vector&,
                    const std::uint32_t mask,
                    PlayerHullTraceResult& trace) noexcept {
    assert(mask == cssv34::build4100::kMaskPlayerSolid);
    trace = PlayerHullTraceResult{};
    trace.endPos = end;
    trace.fraction = 1.0f;
    const float startBottom = start.z + mins.z;
    const float endBottom = end.z + mins.z;
    if (startBottom >= 0.0f && endBottom < 0.0f && startBottom != endBottom) {
        const float f = startBottom / (startBottom - endBottom);
        trace.fraction = f;
        trace.endPos = cssv34::build4100::Vector{
            start.x + (end.x - start.x) * f,
            start.y + (end.y - start.y) * f,
            start.z + (end.z - start.z) * f,
        };
        trace.planeNormal = cssv34::build4100::Vector{0.0f, 0.0f, 1.0f};
    }
    return true;
}


bool TraceDuckCeiling(void* context, const cssv34::build4100::Vector& start,
                      const cssv34::build4100::Vector& end,
                      const cssv34::build4100::Vector& mins,
                      const cssv34::build4100::Vector& maxs,
                      const std::uint32_t mask,
                      PlayerHullTraceResult& trace) noexcept {
    if (start.x < 50.0f && maxs.z > 40.0f && start.z + maxs.z > 48.0f) {
        assert(mask == cssv34::build4100::kMaskPlayerSolid);
        trace = PlayerHullTraceResult{};
        trace.endPos = start;
        trace.fraction = 0.0f;
        trace.startSolid = true;
        trace.allSolid = true;
        return true;
    }
    return TraceFlatFloor(context, start, end, mins, maxs, mask, trace);
}

bool TraceFloorAndStep(void*, const cssv34::build4100::Vector& start,
                       const cssv34::build4100::Vector& end,
                       const cssv34::build4100::Vector& mins,
                       const cssv34::build4100::Vector& maxs,
                       const std::uint32_t mask,
                       PlayerHullTraceResult& trace) noexcept {
    assert(mask == cssv34::build4100::kMaskPlayerSolid);
    trace = PlayerHullTraceResult{};
    trace.endPos = end;
    trace.fraction = 1.0f;

    constexpr float kStepFront = 18.0f;
    constexpr float kStepTop = 8.0f;
    const float startBottom = start.z + mins.z;
    const float endBottom = end.z + mins.z;
    const bool startOverStep = start.y + maxs.y >= kStepFront;
    const bool endOverStep = end.y + maxs.y >= kStepFront;

    float best = 1.0f;
    cssv34::build4100::Vector normal{};

    // Ground/top surface for vertical sweeps.
    const float groundHeight = endOverStep ? kStepTop : 0.0f;
    if (startBottom >= groundHeight && endBottom < groundHeight && startBottom != endBottom) {
        const float f = (startBottom - groundHeight) / (startBottom - endBottom);
        if (f < best) {
            best = f;
            normal = cssv34::build4100::Vector{0.0f, 0.0f, 1.0f};
        }
    }

    // Vertical step face. Once the player bottom is above the 8-unit step,
    // the horizontal sweep can pass over it.
    const float startFront = start.y + maxs.y;
    const float endFront = end.y + maxs.y;
    if (!startOverStep && endOverStep && startBottom < kStepTop && startFront != endFront) {
        const float f = (kStepFront - startFront) / (endFront - startFront);
        if (f < best) {
            best = f;
            normal = cssv34::build4100::Vector{0.0f, -1.0f, 0.0f};
        }
    }

    if (best < 1.0f) {
        trace.fraction = best;
        trace.endPos = cssv34::build4100::Vector{
            start.x + (end.x - start.x) * best,
            start.y + (end.y - start.y) * best,
            start.z + (end.z - start.z) * best,
        };
        trace.planeNormal = normal;
    }
    return true;
}

struct SlopeTraceContext {
    float slope = 0.0f;  // ground z = slope * y
};

bool TraceSlope(void* rawContext, const cssv34::build4100::Vector& start,
                const cssv34::build4100::Vector& end,
                const cssv34::build4100::Vector& mins,
                const cssv34::build4100::Vector&,
                const std::uint32_t mask,
                PlayerHullTraceResult& trace) noexcept {
    assert(mask == cssv34::build4100::kMaskPlayerSolid);
    auto* context = static_cast<SlopeTraceContext*>(rawContext);
    assert(context);
    const float slope = context->slope;
    trace = PlayerHullTraceResult{};
    trace.endPos = end;
    trace.fraction = 1.0f;

    const auto signedDistance = [&](const cssv34::build4100::Vector& point) noexcept {
        return point.z + mins.z - slope * point.y;
    };
    const float startDistance = signedDistance(start);
    const float endDistance = signedDistance(end);
    if (startDistance >= 0.0f && endDistance < 0.0f && startDistance != endDistance) {
        const float f = startDistance / (startDistance - endDistance);
        trace.fraction = f;
        trace.endPos = cssv34::build4100::Vector{
            start.x + (end.x - start.x) * f,
            start.y + (end.y - start.y) * f,
            start.z + (end.z - start.z) * f,
        };
        const float inv = 1.0f / std::sqrt(1.0f + slope * slope);
        trace.planeNormal = cssv34::build4100::Vector{0.0f, -slope * inv, inv};
    }
    return true;
}

bool TraceTwoWalls(void*, const cssv34::build4100::Vector& start,
                   const cssv34::build4100::Vector& end,
                   const cssv34::build4100::Vector&,
                   const cssv34::build4100::Vector&,
                   const std::uint32_t mask,
                   PlayerHullTraceResult& trace) noexcept {
    assert(mask == cssv34::build4100::kMaskPlayerSolid);
    trace = PlayerHullTraceResult{};
    trace.endPos = end;
    trace.fraction = 1.0f;
    float best = 1.0f;
    cssv34::build4100::Vector normal{};

    if (start.x <= 1.0f && end.x > 1.0f && start.x != end.x) {
        const float f = (1.0f - start.x) / (end.x - start.x);
        if (f < best) {
            best = f;
            normal = cssv34::build4100::Vector{-1.0f, 0.0f, 0.0f};
        }
    }
    if (start.y <= 1.0f && end.y > 1.0f && start.y != end.y) {
        const float f = (1.0f - start.y) / (end.y - start.y);
        if (f < best || (std::fabs(f - best) < 1.0e-6f && normal.x == 0.0f)) {
            best = f;
            normal = cssv34::build4100::Vector{0.0f, -1.0f, 0.0f};
        }
    }
    if (best < 1.0f) {
        trace.fraction = best;
        trace.endPos = cssv34::build4100::Vector{
            start.x + (end.x - start.x) * best,
            start.y + (end.y - start.y) * best,
            start.z + (end.z - start.z) * best,
        };
        trace.planeNormal = normal;
    }
    return true;
}

bool TraceOpenWater(void*, const cssv34::build4100::Vector&, const cssv34::build4100::Vector& end,
                    const cssv34::build4100::Vector&, const cssv34::build4100::Vector&,
                    std::uint32_t mask, PlayerHullTraceResult& trace) noexcept {
    assert(mask == cssv34::build4100::kMaskPlayerSolid); trace={}; trace.endPos=end; trace.fraction=1.0f; return true;
}
std::int32_t PointContentsWater(void*, const cssv34::build4100::Vector& point) noexcept {
    return point.z < 100.0f ? static_cast<std::int32_t>(cssv34::build4100::kContentsWater) : 0;
}

}  // namespace

int main() {
    ClientRegistry clients;
    assert(clients.Configure(4));
    assert(clients.MaxClients() == 4);

    Edict edict1{};
    Edict edict2{};
    char reject[64] = {};

    assert(!clients.Connect(0, &edict1, "zero", "127.0.0.1", reject, sizeof(reject)));
    assert(std::strlen(reject) > 0);

    std::memset(reject, 0, sizeof(reject));
    assert(clients.Connect(1, &edict1, "alpha", "10.0.0.1", reject, sizeof(reject)));
    assert(clients.ConnectedCount() == 1);
    assert(clients.Get(1)->state == ClientLifecycleState::Connected);
    assert(!clients.Connect(1, &edict1, "duplicate", "10.0.0.2", reject, sizeof(reject)));

    assert(clients.MarkFakeClient(1, &edict1));
    assert(clients.Get(1)->isFakeClient);
    assert(clients.PutInServer(1, &edict1, "alpha-renamed"));
    assert(clients.Activate(1, &edict1, false));
    assert(clients.Get(1)->state == ClientLifecycleState::Active);
    assert(clients.Command(1, &edict1));
    assert(clients.SettingsChanged(1, &edict1));
    assert(clients.SetupVisibility(1, &edict1));
    assert(clients.ProcessUsercmds(1, &edict1, 1, 1, 0, false, false));
    assert(clients.Get(1)->commandCalls == 1);
    assert(clients.Get(1)->settingsChangedCalls == 1);
    assert(clients.Get(1)->visibilityCalls == 1);
    assert(clients.Get(1)->usercmdCalls == 1);

    assert(clients.SetCommandClient(0));
    assert(clients.CommandClient() == 0);
    assert(!clients.SetCommandClient(1));

    assert(!clients.Disconnect(1, &edict2));
    assert(clients.Disconnect(1, &edict1));
    assert(clients.ConnectedCount() == 0);
    assert(clients.CommandClient() == -1);

    // build4100 CreateFakeClient bypasses ClientConnect. The game DLL must
    // adopt the engine-owned edict at ClientPutInServer and continue normally.
    assert(clients.AdoptFakeClient(2, &edict2, "cssv34_bot_01"));
    assert(clients.Get(2)->state == ClientLifecycleState::Connected);
    assert(clients.Get(2)->isFakeClient);
    assert(clients.PutInServer(2, &edict2, "cssv34_bot_01"));
    assert(clients.Activate(2, &edict2, false));
    assert(clients.Get(2)->state == ClientLifecycleState::Active);
    assert(clients.Disconnect(2, &edict2));

    assert(clients.Connect(1, &edict1, "reconnect", "10.0.0.3", reject, sizeof(reject)));
    assert(clients.Connect(2, &edict2, "beta", "10.0.0.4", reject, sizeof(reject)));
    assert(clients.ConnectedCount() == 2);

    // World-hull movement path: floor categorization, swept wall clip and a
    // complete jump -> gravity -> landing cycle through the trace callback.
    ClientRegistry traced;
    Edict traceEdict{};
    assert(traced.Configure(1));
    traced.BindMovementTrace(nullptr, &TraceFloorAndWall);
    assert(traced.Connect(1, &traceEdict, "trace", "local", reject, sizeof(reject)));
    assert(traced.PutInServer(1, &traceEdict, "trace"));
    auto* traceClient = traced.Get(1);
    assert(traceClient);
    traceClient->playerMins = cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f};
    traceClient->playerMaxs = cssv34::build4100::Vector{16.0f, 16.0f, 72.0f};
    traceClient->movementWorldTraceEnabled = true;
    traceClient->movementOnGround = true;

    cssv34::build4100::UserCmd wallCmd{};
    wallCmd.commandNumber = 1;
    wallCmd.tickCount = 1;
    wallCmd.viewAngles.y = 90.0f;
    wallCmd.forwardMove = 200.0f;
    assert(traced.ApplyUsercmdMovement(1, &traceEdict, &wallCmd, 1, 1, 0.015f, false));
    assert(traceClient->absOrigin.y > 0.09f && traceClient->absOrigin.y < 0.11f);
    assert(std::fabs(traceClient->absVelocity.y) < 0.001f);
    assert(traceClient->movementOnGround);

    // Move away from the synthetic wall before jumping so only the floor is hit.
    traceClient->absOrigin.y = -8.0f;
    traceClient->absVelocity = cssv34::build4100::Vector{};
    cssv34::build4100::UserCmd jump{};
    jump.commandNumber = 2;
    jump.tickCount = 2;
    jump.buttons = 1 << 1;
    assert(traced.ApplyUsercmdMovement(1, &traceEdict, &jump, 1, 1, 0.015f, false));
    assert(!traceClient->movementOnGround && traceClient->absOrigin.z > 3.0f);

    std::vector<cssv34::build4100::UserCmd> fall(80);
    for (std::size_t i = 0; i < fall.size(); ++i) {
        fall[i].commandNumber = 3 + static_cast<int>(i);
        fall[i].tickCount = 3 + static_cast<int>(i);
    }
    assert(traced.ApplyUsercmdMovement(1, &traceEdict, fall.data(), fall.size(),
                                       static_cast<int>(fall.size()), 0.015f, false));
    assert(traceClient->movementOnGround);
    assert(std::fabs(traceClient->absOrigin.z) < 0.001f);
    assert(std::fabs(traceClient->absVelocity.z) < 0.001f);

    // Preserve the v1.8.16/v1.8.17 flat-floor network oracle exactly while
    // introducing StepMove/slope handling.
    ClientRegistry flat;
    Edict flatEdict{};
    assert(flat.Configure(1));
    flat.BindMovementTrace(nullptr, &TraceFlatFloor);
    assert(flat.Connect(1, &flatEdict, "flat", "local", reject, sizeof(reject)));
    assert(flat.PutInServer(1, &flatEdict, "flat"));
    auto* flatClient = flat.Get(1);
    assert(flatClient);
    flatClient->playerMins = cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f};
    flatClient->playerMaxs = cssv34::build4100::Vector{16.0f, 16.0f, 72.0f};
    flatClient->movementWorldTraceEnabled = true;
    flatClient->movementOnGround = true;
    cssv34::build4100::UserCmd flatCmds[5]{};
    flatCmds[0].commandNumber = 1; flatCmds[0].tickCount = 1;
    flatCmds[0].viewAngles.y = 90.0f; flatCmds[0].forwardMove = 200.0f;
    flatCmds[1].commandNumber = 2; flatCmds[1].tickCount = 2;
    flatCmds[1].viewAngles.y = 90.0f;
    flatCmds[2].commandNumber = 3; flatCmds[2].tickCount = 3;
    flatCmds[2].viewAngles.y = 90.0f; flatCmds[2].buttons = 1 << 1;
    flatCmds[3].commandNumber = 4; flatCmds[3].tickCount = 4;
    flatCmds[3].viewAngles.y = 90.0f; flatCmds[3].buttons = 1 << 1;
    flatCmds[4].commandNumber = 5; flatCmds[4].tickCount = 5;
    flatCmds[4].viewAngles.y = 90.0f; flatCmds[4].forwardMove = 200.0f;
    for (const auto& cmd : flatCmds) {
        assert(flat.ApplyUsercmdMovement(1, &flatEdict, &cmd, 1, 1, 0.015f, false));
    }
    assert(std::fabs(flatClient->absOrigin.x) < 0.001f);
    assert(std::fabs(flatClient->absOrigin.y - 1.1475f) < 0.002f);
    assert(std::fabs(flatClient->absOrigin.z - 10.994767f) < 0.002f);
    assert(std::fabs(flatClient->absVelocity.x) < 0.001f);
    assert(std::fabs(flatClient->absVelocity.y - 30.0f) < 0.002f);
    assert(std::fabs(flatClient->absVelocity.z - 226.32816f) < 0.002f);

    // StepMove: the 8-unit obstacle blocks a direct walk sweep, but the
    // build4100 18-unit step path must advance over it and settle on top.
    ClientRegistry stepped;
    Edict stepEdict{};
    assert(stepped.Configure(1));
    stepped.BindMovementTrace(nullptr, &TraceFloorAndStep);
    assert(stepped.Connect(1, &stepEdict, "step", "local", reject, sizeof(reject)));
    assert(stepped.PutInServer(1, &stepEdict, "step"));
    auto* stepClient = stepped.Get(1);
    assert(stepClient);
    stepClient->playerMins = cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f};
    stepClient->playerMaxs = cssv34::build4100::Vector{16.0f, 16.0f, 72.0f};
    stepClient->movementWorldTraceEnabled = true;
    stepClient->movementOnGround = true;
    stepClient->absVelocity = cssv34::build4100::Vector{0.0f, 250.0f, 0.0f};
    cssv34::build4100::UserCmd stepCmd{};
    stepCmd.commandNumber = 1;
    stepCmd.tickCount = 1;
    assert(stepped.ApplyUsercmdMovement(1, &stepEdict, &stepCmd, 1, 1, 0.015f, false));
    assert(stepClient->absOrigin.y > 3.0f);
    assert(std::fabs(stepClient->absOrigin.z - 8.0f) < 0.01f);
    assert(stepClient->movementOnGround);

    // Walkable slope: downhill movement stays grounded and StayOnGround snaps
    // the hull to the sloped plane. A >45-degree plane is rejected by the
    // exact build4100 normal.z >= 0.7 rule.
    SlopeTraceContext walkableSlope{0.5f};
    ClientRegistry sloped;
    Edict slopeEdict{};
    assert(sloped.Configure(1));
    sloped.BindMovementTrace(&walkableSlope, &TraceSlope);
    assert(sloped.Connect(1, &slopeEdict, "slope", "local", reject, sizeof(reject)));
    assert(sloped.PutInServer(1, &slopeEdict, "slope"));
    auto* slopeClient = sloped.Get(1);
    assert(slopeClient);
    slopeClient->playerMins = cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f};
    slopeClient->playerMaxs = cssv34::build4100::Vector{16.0f, 16.0f, 72.0f};
    slopeClient->movementWorldTraceEnabled = true;
    slopeClient->movementOnGround = true;
    slopeClient->absVelocity = cssv34::build4100::Vector{0.0f, -250.0f, 0.0f};
    cssv34::build4100::UserCmd slopeCmd{};
    slopeCmd.commandNumber = 1;
    slopeCmd.tickCount = 1;
    assert(sloped.ApplyUsercmdMovement(1, &slopeEdict, &slopeCmd, 1, 1, 0.015f, false));
    assert(slopeClient->absOrigin.y < -3.0f);
    assert(slopeClient->absOrigin.z < -1.0f);
    assert(slopeClient->movementOnGround);

    SlopeTraceContext steepSlope{2.0f};
    sloped.BindMovementTrace(&steepSlope, &TraceSlope);
    slopeClient->absOrigin = cssv34::build4100::Vector{};
    slopeClient->absVelocity = cssv34::build4100::Vector{};
    slopeClient->movementOnGround = true;
    assert(sloped.ApplyUsercmdMovement(1, &slopeEdict, &slopeCmd, 1, 1, 0.015f, false));
    assert(!slopeClient->movementOnGround);

    // Multi-plane TryPlayerMove: hit two perpendicular walls in one air move.
    // The two-plane resolution must keep only the legal crease (vertical) motion.
    ClientRegistry creased;
    Edict creaseEdict{};
    assert(creased.Configure(1));
    creased.BindMovementTrace(nullptr, &TraceTwoWalls);
    assert(creased.Connect(1, &creaseEdict, "crease", "local", reject, sizeof(reject)));
    assert(creased.PutInServer(1, &creaseEdict, "crease"));
    auto* creaseClient = creased.Get(1);
    assert(creaseClient);
    creaseClient->playerMins = cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f};
    creaseClient->playerMaxs = cssv34::build4100::Vector{16.0f, 16.0f, 72.0f};
    creaseClient->movementWorldTraceEnabled = true;
    creaseClient->movementOnGround = false;
    creaseClient->absOrigin = cssv34::build4100::Vector{0.0f, 0.0f, 10.0f};
    creaseClient->absVelocity = cssv34::build4100::Vector{100.0f, 100.0f, 100.0f};
    cssv34::build4100::UserCmd creaseCmd{};
    creaseCmd.commandNumber = 1;
    creaseCmd.tickCount = 1;
    assert(creased.ApplyUsercmdMovement(1, &creaseEdict, &creaseCmd, 1, 1, 0.015f, false));
    assert(creaseClient->absOrigin.x > 0.99f && creaseClient->absOrigin.x < 1.01f);
    assert(creaseClient->absOrigin.y > 0.99f && creaseClient->absOrigin.y < 1.01f);
    assert(creaseClient->absOrigin.z > 10.5f);
    assert(std::fabs(creaseClient->absVelocity.x) < 0.001f);
    assert(std::fabs(creaseClient->absVelocity.y) < 0.001f);
    assert(creaseClient->absVelocity.z > 70.0f);

    // Dynamic player collision: build4100 TryPlayerMove traces with
    // CTraceFilterSimple(self, COLLISION_GROUP_PLAYER_MOVEMENT=8). The
    // source-owned reconstruction merges the BSP floor with another active
    // player's standing hull and must stop exactly at hull contact.
    ClientRegistry dynamicPlayers;
    Edict moverEdict{};
    Edict blockerEdict{};
    assert(dynamicPlayers.Configure(2));
    dynamicPlayers.BindMovementTrace(nullptr, &TraceFlatFloor);
    assert(dynamicPlayers.Connect(1, &moverEdict, "mover", "local", reject, sizeof(reject)));
    assert(dynamicPlayers.PutInServer(1, &moverEdict, "mover"));
    assert(dynamicPlayers.Activate(1, &moverEdict, false));
    assert(dynamicPlayers.Connect(2, &blockerEdict, "blocker", "local", reject, sizeof(reject)));
    assert(dynamicPlayers.PutInServer(2, &blockerEdict, "blocker"));
    assert(dynamicPlayers.Activate(2, &blockerEdict, false));
    auto* mover = dynamicPlayers.Get(1);
    auto* blocker = dynamicPlayers.Get(2);
    assert(mover && blocker);
    mover->playerMins = cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f};
    mover->playerMaxs = cssv34::build4100::Vector{16.0f, 16.0f, 72.0f};
    blocker->playerMins = cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f};
    blocker->playerMaxs = cssv34::build4100::Vector{16.0f, 16.0f, 72.0f};
    mover->absOrigin = cssv34::build4100::Vector{0.0f, 0.0f, 0.0f};
    mover->absVelocity = cssv34::build4100::Vector{200.0f, 0.0f, 0.0f};
    mover->movementWorldTraceEnabled = true;
    mover->movementOnGround = true;
    blocker->absOrigin = cssv34::build4100::Vector{34.0f, 0.0f, 0.0f};
    blocker->absVelocity = cssv34::build4100::Vector{};
    blocker->movementWorldTraceEnabled = true;
    blocker->movementOnGround = true;
    cssv34::build4100::UserCmd dynamicCmd{};
    dynamicCmd.commandNumber = 1;
    dynamicCmd.tickCount = 1;
    assert(dynamicPlayers.ApplyUsercmdMovement(1, &moverEdict, &dynamicCmd, 1, 1, 0.015f, false));
    assert(mover->absOrigin.x > 1.99f && mover->absOrigin.x < 2.01f);
    assert(std::fabs(mover->absVelocity.x) < 0.001f);
    assert(mover->movementOnGround);

    // Contact is not start-solid: moving away from the touching player remains
    // possible, matching TraceRay sliding semantics.
    mover->absVelocity = cssv34::build4100::Vector{-100.0f, 0.0f, 0.0f};
    dynamicCmd.commandNumber = 2;
    dynamicCmd.tickCount = 2;
    assert(dynamicPlayers.ApplyUsercmdMovement(1, &moverEdict, &dynamicCmd, 1, 1, 0.015f, false));
    assert(mover->absOrigin.x < 1.0f);

    // Duck/crouch: IN_DUCK swaps to the 36-unit hull, crops ground speed,
    // refuses to stand into a low ceiling, and restores the 72-unit standing
    // hull once clear.
    ClientRegistry duckClients;
    Edict duckEdict{};
    assert(duckClients.Configure(1));
    duckClients.BindMovementTrace(nullptr, &TraceDuckCeiling);
    assert(duckClients.Connect(1, &duckEdict, "ducker", "local", reject, sizeof(reject)));
    assert(duckClients.PutInServer(1, &duckEdict, "ducker"));
    assert(duckClients.Activate(1, &duckEdict, false));
    auto* ducker = duckClients.Get(1);
    assert(ducker);
    ducker->playerMins = {-16.0f, -16.0f, 0.0f};
    ducker->playerMaxs = {16.0f, 16.0f, 72.0f};
    ducker->movementWorldTraceEnabled = true;
    ducker->movementOnGround = true;
    cssv34::build4100::UserCmd duckCmd{};
    duckCmd.commandNumber = 1;
    duckCmd.tickCount = 1;
    duckCmd.buttons = 1 << 2;
    duckCmd.forwardMove = 200.0f;
    assert(duckClients.ApplyUsercmdMovement(1, &duckEdict, &duckCmd, 1, 1, 0.015f, false));
    assert(ducker->movementDucked);
    assert(std::fabs(ducker->playerMaxs.z - 36.0f) < 0.001f);
    assert(ducker->absVelocity.x > 5.09f && ducker->absVelocity.x < 5.11f);

    cssv34::build4100::UserCmd standCmd{};
    standCmd.commandNumber = 2;
    standCmd.tickCount = 2;
    assert(duckClients.ApplyUsercmdMovement(1, &duckEdict, &standCmd, 1, 1, 0.015f, false));
    assert(ducker->movementDucked);
    assert(std::fabs(ducker->playerMaxs.z - 36.0f) < 0.001f);
    ducker->absOrigin.x = 100.0f;
    ducker->absVelocity = {};
    standCmd.commandNumber = 3;
    standCmd.tickCount = 3;
    assert(duckClients.ApplyUsercmdMovement(1, &duckEdict, &standCmd, 1, 1, 0.015f, false));
    assert(!ducker->movementDucked);
    assert(std::fabs(ducker->playerMaxs.z - 72.0f) < 0.001f);

    // Ladder: a real info_ladder-like thin AABB suppresses gravity and maps
    // explicit vertical input to climb velocity; jump detaches away from it.
    ClientRegistry ladderClients;
    Edict ladderEdict{};
    assert(ladderClients.Configure(1));
    ladderClients.BindMovementTrace(nullptr, &TraceFlatFloor);
    ladderClients.SetMovementLadders({cssv34::reconstructed::MovementLadderVolume{
        {-32.0f, -1.0f, 0.0f}, {32.0f, 1.0f, 128.0f}}});
    assert(ladderClients.Connect(1, &ladderEdict, "climber", "local", reject, sizeof(reject)));
    assert(ladderClients.PutInServer(1, &ladderEdict, "climber"));
    assert(ladderClients.Activate(1, &ladderEdict, false));
    auto* climber = ladderClients.Get(1);
    assert(climber);
    climber->playerMins = {-16.0f, -16.0f, 0.0f};
    climber->playerMaxs = {16.0f, 16.0f, 72.0f};
    climber->movementWorldTraceEnabled = true;
    climber->movementOnGround = false;
    climber->absOrigin = {0.0f, 18.0f, 10.0f};
    cssv34::build4100::UserCmd climbCmd{};
    climbCmd.commandNumber = 1;
    climbCmd.tickCount = 1;
    climbCmd.upMove = 200.0f;
    assert(ladderClients.ApplyUsercmdMovement(1, &ladderEdict, &climbCmd, 1, 1, 0.015f, false));
    assert(climber->movementOnLadder);
    assert(climber->movementLadderIndex == 1);
    assert(climber->absOrigin.z > 12.99f && climber->absOrigin.z < 13.01f);
    assert(climber->absVelocity.z > 199.9f && climber->absVelocity.z < 200.1f);
    cssv34::build4100::UserCmd ladderJump{};
    ladderJump.commandNumber = 2;
    ladderJump.tickCount = 2;
    ladderJump.buttons = 1 << 1;
    assert(ladderClients.ApplyUsercmdMovement(1, &ladderEdict, &ladderJump, 1, 1, 0.015f, false));
    assert(!climber->movementOnLadder);
    assert(climber->movementLadderIndex == 0);
    assert(std::fabs(climber->absVelocity.y) > 269.0f);

    ClientRegistry waterClients; Edict waterEdict{}; assert(waterClients.Configure(1));
    waterClients.BindMovementTrace(nullptr, &TraceOpenWater, &PointContentsWater);
    assert(waterClients.Connect(1,&waterEdict,"swimmer","local",reject,sizeof(reject))); assert(waterClients.PutInServer(1,&waterEdict,"swimmer")); assert(waterClients.Activate(1,&waterEdict,false));
    auto* swimmer=waterClients.Get(1); assert(swimmer); swimmer->playerMins={-16,-16,0}; swimmer->playerMaxs={16,16,72}; swimmer->movementWorldTraceEnabled=true; swimmer->movementOnGround=false;
    cssv34::build4100::UserCmd wc{}; wc.commandNumber=1; wc.tickCount=1; assert(waterClients.ApplyUsercmdMovement(1,&waterEdict,&wc,1,1,0.015f,false)); assert(swimmer->movementWaterLevel==3); assert(swimmer->absVelocity.z < -3.59f && swimmer->absVelocity.z > -3.61f);
    swimmer->absOrigin={}; swimmer->absVelocity={}; swimmer->movementOnGround=false; wc={}; wc.commandNumber=2; wc.tickCount=2; wc.forwardMove=200; assert(waterClients.ApplyUsercmdMovement(1,&waterEdict,&wc,1,1,0.015f,false)); assert(swimmer->absVelocity.x>11.99f&&swimmer->absVelocity.x<12.01f);
    swimmer->absOrigin={}; swimmer->absVelocity={}; swimmer->movementOnGround=false; wc={}; wc.commandNumber=3; wc.tickCount=3; wc.buttons=1<<1; assert(waterClients.ApplyUsercmdMovement(1,&waterEdict,&wc,1,1,0.015f,false)); assert(swimmer->absVelocity.z>108.9f&&swimmer->absVelocity.z<109.1f);

    clients.Reset();
    assert(clients.ConnectedCount() == 0);

    std::cout << "CLIENT_REGISTRY_V0_4_2=PASS\n";
    std::cout << "lifecycle=connect,fakeclient,connectless_fake_adopt,putinserver,active,command,settings,visibility,usercmd,disconnect,reconnect world_hull=floor+wall_clip+jump_landing+step18+walkable_slope+steep_reject+multiplane_crease dynamic_player=swept_aabb+self_skip+contact_slide duck=hull36+speed_crop+blocked_unduck ladder=volume_attach+climb+jump_detach water=levels+sink+friction+accel+jump\n";
    return 0;
}
