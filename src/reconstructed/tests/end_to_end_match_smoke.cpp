#include <cassert>
#include <iostream>
#include "server/game_runtime.h"
using namespace cssv34::reconstructed;
using namespace cssv34::build4100;
int main() {
    GameRuntime runtime; assert(runtime.Initialize(10));
    for (std::size_t i = 1; i <= 10; ++i) {
        assert(runtime.ConnectPlayer(i, "p" + std::to_string(i)));
        assert(runtime.JoinTeam(i, i <= 5 ? 2 : 3));
        assert(runtime.EnterGame(i));
        assert(runtime.Game().AddAccount(i, 15200));
    }
    runtime.ConfigureObjective(CsMapObjective::Bomb);
    runtime.StartRound(0.0f);
    assert(runtime.Tick(6.1f, 0.015f) == CsRoundTickResult::FreezeEnded);
    for (std::size_t i = 1; i <= 10; ++i) assert(runtime.Game().SetInBuyZone(i, true));
    assert(runtime.ClientCommand(1, "buy awp", 6.2f) == CommandResult::Executed);
    assert(runtime.Game().SelectWeapon(1, CSWeaponID::Awp));
    assert(runtime.Game().GiveWeapon(2, CSWeaponID::SmokeGrenade, false));
    assert(runtime.ThrowGrenade(2, CSWeaponID::SmokeGrenade, 6.3f));
    runtime.Tick(8.0f, 1.7f);
    for (std::size_t victim = 6; victim <= 10; ++victim) {
        const float shotTime = 8.0f + static_cast<float>(victim - 5) * 1.3f;
        const auto result = runtime.FireAt(1, victim, HitGroup::Head, shotTime);
        assert(result == DamageApplyResult::Killed);
    }
    assert(runtime.Game().RoundPhase() == CsRoundPhase::Ended);
    assert(runtime.Game().TerroristScore() == 1);
    assert(runtime.Events().Count("player_death") == 5);
    const float restartAt = runtime.Game().ScheduledRestartTime();
    assert(runtime.Tick(restartAt + 0.01f, 5.0f) == CsRoundTickResult::Restarted);
    const float liveAt = runtime.Game().FreezeEndTime() + 0.01f;
    assert(runtime.Tick(liveAt, 6.1f) == CsRoundTickResult::FreezeEnded);
    int carrier = runtime.Game().BombCarrierIndex(); assert(carrier >= 1 && carrier <= 5);
    assert(runtime.PlantBomb(static_cast<std::size_t>(carrier), liveAt + 0.1f));
    assert(runtime.Game().GiveDefuser(6, true));
    assert(runtime.BeginDefuse(6, liveAt + 0.2f));
    assert(runtime.Tick(liveAt + 5.3f, 5.1f) == CsRoundTickResult::ObjectiveResolved);
    assert(runtime.Game().RoundEndReason() == CSRoundEndReason::BombDefused);
    assert(runtime.Game().CounterTerroristScore() == 1);
    assert(runtime.NetworkState().players.size() == 10);
    assert(runtime.Events().Count("round_start") >= 2);
    assert(runtime.Events().Count("weapon_fire") == 5);
    std::cout << "END_TO_END_MATCH=PASS\n";
}
