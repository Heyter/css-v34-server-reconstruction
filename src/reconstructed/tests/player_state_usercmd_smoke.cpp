#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

#include "public/build4100/player_state.h"
#include "public/build4100/usercmd.h"
#include "server/client_registry.h"
#include "server/cs_game_state.h"

using cssv34::build4100::CSTeam;
using cssv34::build4100::Edict;
using cssv34::build4100::PlayerState;
using cssv34::build4100::UserCmd;
using cssv34::reconstructed::ClientRegistry;
using cssv34::reconstructed::CsGameState;

int main() {
    ClientRegistry clients;
    assert(clients.Configure(4));

    Edict player{};
    Edict enemy{};
    char reject[64]{};
    assert(clients.Connect(1, &player, "alpha", "10.0.0.1", reject, sizeof(reject)));
    assert(clients.Connect(2, &enemy, "bravo", "10.0.0.2", reject, sizeof(reject)));
    assert(clients.PutInServer(1, &player, "alpha"));
    assert(clients.PutInServer(2, &enemy, "bravo"));
    assert(clients.Activate(1, &player, false));
    assert(clients.Activate(2, &enemy, false));

    PlayerState* state = clients.PlayerStateFor(1, &player);
    assert(state);
    assert(std::strcmp(state->_GetClassName(), "CPlayerState") == 0);
    state->NetworkStateChanged();
    state->NetworkStateChanged(nullptr);
    assert(!state->deadflag && state->frags == 0 && state->deaths == 0);

    UserCmd cmds[2]{};
    cmds[0].commandNumber = 100;
    cmds[0].tickCount = 200;
    cmds[0].viewAngles = {10.0f, 20.0f, 0.0f};
    cmds[0].forwardMove = 250.0f;
    cmds[0].buttons = 1;
    cmds[1].commandNumber = 101;
    cmds[1].tickCount = 201;
    cmds[1].viewAngles = {15.0f, 30.0f, 0.0f};
    cmds[1].sideMove = -125.0f;
    cmds[1].buttons = 4;
    cmds[1].weaponSelect = 7;
    cmds[1].randomSeed = 12345;
    cmds[1].mouseDx = 12;
    cmds[1].mouseDy = -8;

    assert(clients.ProcessDecodedUsercmds(1, &player, cmds, 2, 3, 1, false, false));
    auto* record = clients.Get(1);
    assert(record);
    assert(record->usercmdCalls == 1 && record->decodedUsercmds == 2);
    assert(record->lastNumCmds == 2 && record->lastTotalCmds == 3 && record->lastDroppedPackets == 1);
    assert(record->lastUserCommand.commandNumber == 101);
    assert(record->lastUserCommand.tickCount == 201);
    assert(record->lastUserCommand.buttons == 4);
    assert(record->lastUserCommand.weaponSelect == 7);
    assert(record->lastUserCommand.randomSeed == 12345);
    assert(record->lastUserCommand.mouseDx == 12 && record->lastUserCommand.mouseDy == -8);
    state = clients.PlayerStateFor(1, &player);
    assert(state && state->vAngle.x == 15.0f && state->vAngle.y == 30.0f);

    UserCmd movement{};
    movement.commandNumber = 102;
    movement.tickCount = 202;
    movement.viewAngles = {0.0f, 90.0f, 0.0f};
    movement.forwardMove = 200.0f;
    assert(clients.ApplyUsercmdMovement(1, &player, &movement, 1, 1, 0.015f, false));
    record = clients.Get(1);
    assert(record);
    assert(record->movementOnGround);
    assert(std::fabs(record->absVelocity.x) < 0.001f);
    assert(std::fabs(record->absVelocity.y - 15.0f) < 0.001f);
    assert(std::fabs(record->absVelocity.z) < 0.001f);
    assert(std::fabs(record->absOrigin.x) < 0.001f);
    assert(std::fabs(record->absOrigin.y - 0.225f) < 0.001f);
    assert(std::fabs(record->absOrigin.z) < 0.001f);

    // A zero-input ground command applies sv_friction/stopspeed before movement.
    UserCmd friction{};
    friction.commandNumber = 103;
    friction.tickCount = 203;
    friction.viewAngles = {0.0f, 90.0f, 0.0f};
    assert(clients.ApplyUsercmdMovement(1, &player, &friction, 1, 1, 0.015f, false));
    record = clients.Get(1);
    assert(record && record->movementOnGround);
    assert(std::fabs(record->absVelocity.y - 10.5f) < 0.001f);
    assert(std::fabs(record->absOrigin.y - 0.3825f) < 0.001f);

    // build4100 IN_JUMP is bit 1. Split gravity + the 45-unit jump impulse
    // leaves 250.328... u/s after the first airborne command. Holding jump
    // must not apply the impulse again until the button is released.
    UserCmd jump{};
    jump.commandNumber = 104;
    jump.tickCount = 204;
    jump.viewAngles = {0.0f, 90.0f, 0.0f};
    jump.buttons = 1 << 1;
    assert(clients.ApplyUsercmdMovement(1, &player, &jump, 1, 1, 0.015f, false));
    record = clients.Get(1);
    assert(record && !record->movementOnGround);
    assert((record->movementOldButtons & (1 << 1)) != 0);
    assert(std::fabs(record->absOrigin.y - 0.54f) < 0.001f);
    assert(std::fabs(record->absOrigin.z - 3.844922f) < 0.001f);
    assert(std::fabs(record->absVelocity.y - 10.5f) < 0.001f);
    assert(std::fabs(record->absVelocity.z - 250.32816f) < 0.002f);

    jump.commandNumber = 105;
    jump.tickCount = 205;
    assert(clients.ApplyUsercmdMovement(1, &player, &jump, 1, 1, 0.015f, false));
    record = clients.Get(1);
    assert(record && !record->movementOnGround);
    assert(std::fabs(record->absOrigin.y - 0.6975f) < 0.001f);
    assert(std::fabs(record->absOrigin.z - 7.509845f) < 0.002f);
    assert(std::fabs(record->absVelocity.z - 238.32816f) < 0.002f);

    // Release jump and accelerate forward while airborne. Source caps the
    // air-current/add speed at 30 u/s while the acceleration term still uses
    // the original 200 u/s wish speed, taking horizontal speed to exactly 30.
    UserCmd air{};
    air.commandNumber = 106;
    air.tickCount = 206;
    air.viewAngles = {0.0f, 90.0f, 0.0f};
    air.forwardMove = 200.0f;
    assert(clients.ApplyUsercmdMovement(1, &player, &air, 1, 1, 0.015f, false));
    record = clients.Get(1);
    assert(record && !record->movementOnGround);
    assert((record->movementOldButtons & (1 << 1)) == 0);
    assert(std::fabs(record->absOrigin.y - 1.1475f) < 0.001f);
    assert(std::fabs(record->absOrigin.z - 10.994767f) < 0.002f);
    assert(std::fabs(record->absVelocity.y - 30.0f) < 0.001f);
    assert(std::fabs(record->absVelocity.z - 226.32816f) < 0.002f);

    // Positive sideMove follows Source AngleVectors' right vector. At yaw=0
    // that is negative Y; reset the synthetic movement state to isolate the sign.
    record->absOrigin = {};
    record->absVelocity = {};
    record->movementOnGround = true;
    record->movementOldButtons = 0;
    UserCmd side{};
    side.commandNumber = 107;
    side.tickCount = 207;
    side.viewAngles = {0.0f, 0.0f, 0.0f};
    side.sideMove = 200.0f;
    assert(clients.ApplyUsercmdMovement(1, &player, &side, 1, 1, 0.015f, false));
    record = clients.Get(1);
    assert(record);
    assert(std::fabs(record->absVelocity.x) < 0.001f);
    assert(std::fabs(record->absVelocity.y + 15.0f) < 0.001f);
    assert(std::fabs(record->absOrigin.y + 0.225f) < 0.001f);

    UserCmd ignored{};
    ignored.commandNumber = 999;
    ignored.viewAngles = {90.0f, 90.0f, 90.0f};
    assert(clients.ProcessDecodedUsercmds(1, &player, &ignored, 1, 1, 0, true, true));
    record = clients.Get(1);
    assert(record->lastUserCommand.commandNumber == 101);
    assert(record->lastUsercmdIgnore && record->lastUsercmdPaused);

    CsGameState game(clients);
    game.Configure();
    assert(game.OnClientPutInServer(1));
    assert(game.OnClientPutInServer(2));
    assert(game.JoinTeam(1, static_cast<int>(CSTeam::Terrorist)));
    assert(game.JoinTeam(2, static_cast<int>(CSTeam::CounterTerrorist)));
    assert(game.EnterGame(1));
    assert(game.EnterGame(2));
    assert(game.Kill(1, 2));

    state = clients.PlayerStateFor(1, &player);
    assert(state && state->deadflag && state->deaths == 1);
    PlayerState* enemyState = clients.PlayerStateFor(2, &enemy);
    assert(enemyState && !enemyState->deadflag && enemyState->frags == 1);
    assert(game.Respawn(1));
    assert(!clients.PlayerStateFor(1, &player)->deadflag);

    std::cout << "PLAYER_STATE_USERCMD_V0_7_0=PASS\n";
    std::cout << "CPlayerState=0x34_i386 slots=0..4 usercmd=0x40_i386 decoded_batch=PASS source_walk_core=accel+friction+split_gravity+jump_latch+side_sign\n";
    return 0;
}
