#include <cassert>
#include <iostream>
#include "server/game_runtime.h"
using namespace cssv34::reconstructed;
int main() {
    GameRuntime runtime; assert(runtime.Initialize(4));
    assert(runtime.ConnectPlayer(1, "t")); assert(runtime.ConnectPlayer(2, "ct"));
    assert(runtime.JoinTeam(1, 2)); assert(runtime.JoinTeam(2, 3));
    assert(runtime.EnterGame(1)); assert(runtime.EnterGame(2));
    runtime.Game().AddAccount(1, 15200); runtime.Game().AddAccount(2, 15200);
    runtime.StartRound(0.0f); assert(runtime.Tick(6.1f, 0.015f) == CsRoundTickResult::FreezeEnded);
    assert(runtime.Game().SetInBuyZone(1, true)); assert(runtime.Game().SetInBuyZone(2, true));
    assert(runtime.ClientCommand(1, "buy ak47", 6.2f) == CommandResult::Executed);
    assert(runtime.ClientCommand(2, "buy m4a1", 6.2f) == CommandResult::Executed);
    assert(runtime.ClientCommand(1, "say hello", 6.3f) == CommandResult::Executed);
    assert(runtime.ServerCommand("mp_friendlyfire 1", 6.4f) == CommandResult::Executed);
    assert(runtime.Game().FriendlyFire());
    const auto& snapshot = runtime.NetworkState();
    assert(snapshot.players.size() == 2 && snapshot.round == 1);
    assert(runtime.Events().Count("player_connect") == 2);
    assert(runtime.Events().Count("player_say") == 1);
    assert(runtime.Commands().CVars().Count() >= 8);
    std::cout << "COMMANDS_NETWORK=PASS\n";
}
