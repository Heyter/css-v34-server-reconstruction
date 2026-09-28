#include <cassert>
#include <iostream>
#include "server/game_runtime.h"
#include "server/weapon_ballistics.h"
using namespace cssv34::reconstructed;
using namespace cssv34::build4100;
int main() {
    GameRuntime runtime; assert(runtime.Initialize(3));
    for (std::size_t i=1;i<=3;++i) {
        assert(runtime.ConnectPlayer(i, "p"+std::to_string(i)));
        assert(runtime.JoinTeam(i, i==1 ? 2 : 3));
        assert(runtime.EnterGame(i));
    }
    runtime.StartRound(0.0f); assert(runtime.Tick(6.1f,0.015f)==CsRoundTickResult::FreezeEnded);
    auto* p1=runtime.Clients().Get(1); auto* p2=runtime.Clients().Get(2); auto* p3=runtime.Clients().Get(3);
    assert(p1&&p2&&p3);
    p1->absOrigin={0,0,0}; p2->absOrigin={100,0,0}; p3->absOrigin={1000,0,0};

    assert(runtime.Game().GiveWeapon(1, CSWeaponID::Ak47, false));
    p1->weapons.SetAmmo(AmmoTypeForWeapon(CSWeaponID::Ak47), 90);
    assert(p1->weapons.SetClip(CSWeaponID::Ak47,1));
    assert(runtime.SelectWeapon(1,CSWeaponID::Ak47,6.2f));
    assert(runtime.FireAt(1,2,HitGroup::Chest,6.8f)==DamageApplyResult::Applied);
    const auto* wr=runtime.WeaponRuntime().State(1);
    assert(wr && wr->shotsFired == 1 && wr->classicBallistics && wr->shotAccuracy == 0.2f);
    assert(runtime.Reload(1,7.0f)==WeaponActionResult::Reloaded);
    assert(p1->weapons.Clip(CSWeaponID::Ak47)>0);

    const EntityHandle dropped=runtime.DropWeapon(1,CSWeaponID::Ak47,10.0f);
    assert(dropped.IsValid()&&!p1->weapons.Has(CSWeaponID::Ak47));
    assert(runtime.PickupWeapon(2,dropped,10.1f)); assert(p2->weapons.Has(CSWeaponID::Ak47));

    assert(runtime.SelectWeapon(1,CSWeaponID::Knife,11.0f));
    const int beforeKnife=p3->health;
    assert(runtime.FireAt(1,3,HitGroup::Chest,11.6f)==DamageApplyResult::Applied);
    assert(p3->health<beforeKnife);
    assert(runtime.FireAt(1,3,HitGroup::Chest,11.7f)==DamageApplyResult::NoDamage); // knife cooldown

    assert(runtime.Game().GiveWeapon(1,CSWeaponID::HEGrenade,false));
    assert(runtime.ThrowGrenade(1,CSWeaponID::HEGrenade,20.0f));
    const int p2Before=p2->health, p3Before=p3->health;
    runtime.Tick(21.6f,1.6f);
    assert(runtime.Events().Count("hegrenade_detonate")==1);
    assert(p2->health<p2Before); assert(p3->health==p3Before);
    if (p1->isDead) assert(runtime.Game().Respawn(1));
    if (p2->isDead) assert(runtime.Game().Respawn(2));
    p1->absOrigin={0,0,0}; p2->absOrigin={100,0,0}; p3->absOrigin={1000,0,0};

    assert(runtime.Game().GiveWeapon(1,CSWeaponID::Flashbang,false));
    assert(runtime.ThrowGrenade(1,CSWeaponID::Flashbang,22.0f));
    runtime.Tick(23.6f,2.0f);
    assert(runtime.Events().Count("flashbang_detonate")==1);
    assert(p2->flashBlindUntil>23.6f && p3->flashBlindUntil>23.6f);

    assert(runtime.Game().GiveWeapon(1,CSWeaponID::SmokeGrenade,false));
    assert(runtime.ThrowGrenade(1,CSWeaponID::SmokeGrenade,24.0f));
    runtime.Tick(27.1f,3.5f);
    assert(runtime.Events().Count("smokegrenade_detonate")==1);
    assert(runtime.SmokeCloudCount()==1);
    runtime.Tick(45.2f,18.1f); assert(runtime.SmokeCloudCount()==0);

    // v1.8.35d: live secondary-action state for the eight build4100 families
    // whose PrimaryAttack/Fire behavior depends on IN_ATTACK2. Switching the
    // modeled weapon clears incompatible mode bits exactly once.
    auto& weapons = runtime.WeaponRuntime();
    assert(weapons.SecondaryAttack(1,46.0f,CSWeaponID::M4A1)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->silenced && weapons.State(1)->zoomLevel==0);
    assert(weapons.SecondaryAttack(1,46.1f,CSWeaponID::M4A1)==SecondaryWeaponResult::Cooldown);
    assert(weapons.SecondaryAttack(1,49.1f,CSWeaponID::Usp)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->silenced && !weapons.State(1)->burstMode);
    assert(weapons.SecondaryAttack(1,52.2f,CSWeaponID::Glock)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->burstMode && !weapons.State(1)->silenced);
    assert(weapons.SecondaryAttack(1,52.51f,CSWeaponID::Famas)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->burstMode);
    assert(weapons.SecondaryAttack(1,52.82f,CSWeaponID::Awp)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->zoomLevel==1 && !weapons.State(1)->burstMode);
    assert(weapons.SecondaryAttack(1,53.13f,CSWeaponID::Scout)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->zoomLevel==1);
    assert(weapons.SecondaryAttack(1,53.44f,CSWeaponID::G3Sg1)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->zoomLevel==1);
    assert(weapons.SecondaryAttack(1,53.75f,CSWeaponID::Sg550)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->zoomLevel==1);
    assert(weapons.SecondaryAttack(1,54.06f,CSWeaponID::Sg550)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->zoomLevel==2);
    assert(weapons.SecondaryAttack(1,54.37f,CSWeaponID::Sg550)==SecondaryWeaponResult::Toggled);
    assert(weapons.State(1)->zoomLevel==0);

    // v1.8.35e: recovered pistol6/sniper4 are selected by the live exact path.
    for (const auto weapon : {CSWeaponID::P228, CSWeaponID::Glock, CSWeaponID::Elite,
                              CSWeaponID::FiveSeven, CSWeaponID::Usp, CSWeaponID::Deagle,
                              CSWeaponID::Awp, CSWeaponID::Scout, CSWeaponID::G3Sg1,
                              CSWeaponID::Sg550}) {
        assert(UsesBuild4100ClassicBallistics(weapon));
    }

    // Glock burst = PrimaryAttack + two FireRemaining rounds. Original follow-up
    // scheduling is currenttime+0.100 and FireRemaining uses fixed spread 0.05.
    assert(p1->weapons.Has(CSWeaponID::Glock));
    assert(runtime.SelectWeapon(1,CSWeaponID::Glock,55.0f));
    assert(weapons.SecondaryAttack(1,55.51f,CSWeaponID::Glock)==SecondaryWeaponResult::Toggled);
    const int glockClipBefore = p1->weapons.Clip(CSWeaponID::Glock);
    assert(glockClipBefore >= 3);
    assert(weapons.Fire(1,55.82f,runtime.Game(),CSWeaponID::Glock)==TimedWeaponResult::Fired);
    assert(weapons.State(1)->burstShotsRemaining==2);
    assert(weapons.State(1)->burstShotOrdinal==1);
    assert(weapons.State(1)->nextBurstShotTime > 55.919f && weapons.State(1)->nextBurstShotTime < 55.921f);
    assert(!weapons.FireScheduledGlockBurst(1,55.90f,runtime.Game(),CSWeaponID::Glock));
    assert(weapons.FireScheduledGlockBurst(1,55.92f,runtime.Game(),CSWeaponID::Glock));
    assert(weapons.State(1)->burstShotsRemaining==1 && weapons.State(1)->burstShotOrdinal==2);
    assert(weapons.State(1)->shotSpreadOverride==0.050f);
    assert(weapons.State(1)->nextBurstShotTime > 56.019f && weapons.State(1)->nextBurstShotTime < 56.021f);
    assert(weapons.FireScheduledGlockBurst(1,56.02f,runtime.Game(),CSWeaponID::Glock));
    assert(weapons.State(1)->burstShotsRemaining==0 && weapons.State(1)->burstShotOrdinal==3);
    assert(weapons.State(1)->nextBurstShotTime==0.0f);
    assert(p1->weapons.Clip(CSWeaponID::Glock)==glockClipBefore-3);

    // Slot reuse must not inherit attack cooldown/recoil/shots from the previous
    // occupant. The live adapter follows the same ResetClient contract.
    const auto* beforeReconnect = runtime.WeaponRuntime().State(1);
    assert(beforeReconnect && beforeReconnect->shotsFired > 0);
    assert(runtime.DisconnectPlayer(1));
    const auto* afterDisconnect = runtime.WeaponRuntime().State(1);
    assert(afterDisconnect && afterDisconnect->shotsFired == 0);
    assert(afterDisconnect->nextAttackTime == 0.0f && afterDisconnect->nextSecondaryAttackTime == 0.0f);
    assert(afterDisconnect->recoilIndex == 0.0f && !afterDisconnect->silenced &&
           !afterDisconnect->burstMode && afterDisconnect->zoomLevel == 0);
    assert(runtime.ConnectPlayer(1, "p1_reconnect"));
    const auto* afterReconnect = runtime.WeaponRuntime().State(1);
    assert(afterReconnect && afterReconnect->shotsFired == 0);
    assert(afterReconnect->nextAttackTime == 0.0f && afterReconnect->accuracyPenalty == 0.0f);

    std::cout << "WEAPON_GRENADE_RUNTIME=PASS secondary_modes=8_live_state glock_burst=3shot_100ms_fixed_spread live_exact_families=22\n";
}
