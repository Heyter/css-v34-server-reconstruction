#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "public/build4100/cs_weapon.h"
#include "server/lag_compensation.h"
#include "server/weapon_ballistics.h"
#include "server/weapon_runtime.h"
#include "server/weapon_script_profile.h"

using cssv34::build4100::CSWeaponID;
using cssv34::build4100::QAngle;
using cssv34::build4100::Vector;
using cssv34::reconstructed::AdvanceBuild4100Accuracy;
using cssv34::reconstructed::AccuracyQuadraticFlag;
using cssv34::reconstructed::ApplyBuild4100KickBack;
using cssv34::reconstructed::ApplyBuild4100PistolSniperRecoil;
using cssv34::reconstructed::ApplyBuild4100ShotgunRecoil;
using cssv34::reconstructed::Build4100FxSpreadSample;
using cssv34::reconstructed::Build4100SharedRandomInt;
using cssv34::reconstructed::Build4100SharedRandomFloat;
using cssv34::reconstructed::DecayBuild4100Punch;
using cssv34::reconstructed::BallisticDamageAtDistance;
using cssv34::reconstructed::Build4100KickBackParameters;
using cssv34::reconstructed::Build4100StateSpread;
using cssv34::reconstructed::AdvanceBuild4100TimedAccuracy;
using cssv34::reconstructed::HasBuild4100RecoveredPistolSniperStateModel;
using cssv34::reconstructed::HasBuild4100RecoveredPistolSniperRecoil;
using cssv34::reconstructed::HasBuild4100RecoveredShotgunRecoil;
using cssv34::reconstructed::HasBuild4100RecoveredTimedAccuracy;
using cssv34::reconstructed::KickBackParameters;
using cssv34::reconstructed::LagCompensationHistory;
using cssv34::reconstructed::LagCompensationRecord;
using cssv34::reconstructed::SolveBuild4100WeaponShot;
using cssv34::reconstructed::SolveBuild4100WeaponPellet;
using cssv34::reconstructed::SolveWeaponShot;
using cssv34::reconstructed::WeaponMovementState;
using cssv34::reconstructed::WeaponProfile;
using cssv34::reconstructed::DecodeBuild4100Ice;
using cssv34::reconstructed::ParseBuild4100WeaponScript;
using cssv34::reconstructed::WeaponRuntimeProfile;
using cssv34::reconstructed::UsesBuild4100ClassicBallistics;

namespace {
bool Near(float a, float b, float eps = 0.00001f) {
    return std::fabs(a - b) <= eps;
}

void AssertKick(const CSWeaponID weapon,
                const WeaponMovementState& state,
                const KickBackParameters& expected) {
    KickBackParameters actual{};
    assert(Build4100KickBackParameters(weapon, state, actual));
    assert(Near(actual.upBase, expected.upBase));
    assert(Near(actual.lateralBase, expected.lateralBase));
    assert(Near(actual.upModifier, expected.upModifier));
    assert(Near(actual.lateralModifier, expected.lateralModifier));
    assert(Near(actual.upMax, expected.upMax));
    assert(Near(actual.lateralMax, expected.lateralMax));
    assert(actual.directionChange == expected.directionChange);
}
}

int main() {
    // Reference Thin-ICE vector generated from Matthew Kwan's level-0 ICE
    // algorithm with the build4100 CSS key. It proves the source-owned decoder
    // independently of filesystem/runtime integration.
    std::vector<unsigned char> iceBlock{0x5b, 0xbe, 0xa4, 0x9a, 0xec, 0x16, 0x82, 0xf2};
    assert(DecodeBuild4100Ice(iceBlock));
    assert(std::string(reinterpret_cast<const char*>(iceBlock.data()), iceBlock.size()) == "WeaponDa");

    WeaponRuntimeProfile parsed{};
    constexpr const char kWeaponScript[] = R"KV(WeaponData
{
    "Damage" "41"
    "Range" "7777"
    "RangeModifier" "0.91"
    "WeaponArmorRatio" "1.25"
    "CycleTime" "0.123"
    "Bullets" "7"
    "Penetration" "3"
    "AccuracyDivisor" "222"
    "AccuracyQuadratic" "1"
    "AccuracyOffset" "0.31"
    "MaxInaccuracy" "1.11"
    "SoundData" { "single_shot" "Weapon_Test.Single" }
}
)KV";
    assert(ParseBuild4100WeaponScript(kWeaponScript, parsed));
    assert(Near(parsed.baseDamage, 41.0f));
    assert(Near(parsed.range, 7777.0f));
    assert(Near(parsed.rangeModifier, 0.91f));
    assert(Near(parsed.armorRatio, 1.25f));
    assert(Near(parsed.cycleTime, 0.123f));
    assert(parsed.bulletsPerShot == 7);
    assert(parsed.penetration == 3);
    assert(Near(parsed.accuracyDivisor, 222.0f));
    assert(parsed.accuracyQuadratic == 1);
    assert(AccuracyQuadraticFlag(parsed) == 1);
    assert(Near(parsed.accuracyOffset, 0.31f));
    assert(Near(parsed.maxInaccuracy, 1.11f));

    // CWeaponCSBaseGun::CSBaseGunFire accuracy progression: cube by
    // default, square when AccuracyQuadratic is set, capped by MaxInaccuracy.
    WeaponRuntimeProfile cubic{};
    cubic.accuracyDivisor = 200.0f;
    cubic.accuracyOffset = 0.2f;
    cubic.maxInaccuracy = 0.9f;
    cubic.accuracyQuadratic = 0;
    assert(Near(AdvanceBuild4100Accuracy(cubic, 2, 0.2f), 0.24f));
    assert(Near(AdvanceBuild4100Accuracy(cubic, 20, 0.2f), 0.9f));
    cubic.accuracyQuadratic = 1;
    assert(Near(AdvanceBuild4100Accuracy(cubic, 2, 0.2f), 0.22f));
    cubic.accuracyDivisor = -1.0f;
    assert(Near(AdvanceBuild4100Accuracy(cubic, 7, 0.37f), 0.37f));

    // v1.8.34zy live i386 regression: the old bool-backed field surfaced the
    // non-canonical byte value 205 in startup telemetry.  The source-owned
    // profile now treats any non-zero byte as the boolean "quadratic" state
    // and emits a strict normalized 0/1 flag.
    cubic.accuracyDivisor = 200.0f;
    cubic.accuracyQuadratic = 205;
    assert(AccuracyQuadraticFlag(cubic) == 1);
    assert(Near(AdvanceBuild4100Accuracy(cubic, 2, 0.2f), 0.22f));

    // Exact state-dependent spread recovered from build4100 PrimaryAttack.
    // v1.8.35b expands the accepted four-family path to twelve automatic
    // rifle/SMG families in one batch.
    for (const CSWeaponID weapon : {
             CSWeaponID::Ak47, CSWeaponID::M4A1, CSWeaponID::M249, CSWeaponID::Ump45,
             CSWeaponID::Aug, CSWeaponID::Galil, CSWeaponID::Famas, CSWeaponID::Mp5Navy,
             CSWeaponID::Mac10, CSWeaponID::Tmp, CSWeaponID::P90, CSWeaponID::Sg552}) {
        assert(UsesBuild4100ClassicBallistics(weapon));
    }
    for (const CSWeaponID weapon : {
             CSWeaponID::P228, CSWeaponID::Glock, CSWeaponID::Elite, CSWeaponID::FiveSeven,
             CSWeaponID::Usp, CSWeaponID::Deagle, CSWeaponID::Awp, CSWeaponID::Scout,
             CSWeaponID::G3Sg1, CSWeaponID::Sg550}) {
        assert(UsesBuild4100ClassicBallistics(weapon));
    }
    assert(UsesBuild4100ClassicBallistics(CSWeaponID::M3));
    assert(UsesBuild4100ClassicBallistics(CSWeaponID::XM1014));
    assert(HasBuild4100RecoveredShotgunRecoil(CSWeaponID::M3));
    assert(HasBuild4100RecoveredShotgunRecoil(CSWeaponID::XM1014));

    WeaponMovementState state{};
    state.onGround = false;
    assert(Near(Build4100StateSpread(CSWeaponID::Ak47, cubic, 0.2f, state), 0.12f));
    assert(Near(Build4100StateSpread(CSWeaponID::M4A1, cubic, 0.2f, state), 0.115f));
    assert(Near(Build4100StateSpread(CSWeaponID::M249, cubic, 0.2f, state), 0.145f));
    assert(Near(Build4100StateSpread(CSWeaponID::Ump45, cubic, 0.2f, state), 0.048f));
    assert(Near(Build4100StateSpread(CSWeaponID::Aug, cubic, 0.2f, state), 0.08f));
    assert(Near(Build4100StateSpread(CSWeaponID::Galil, cubic, 0.2f, state), 0.06f));
    assert(Near(Build4100StateSpread(CSWeaponID::Famas, cubic, 0.2f, state), 0.06f));
    assert(Near(Build4100StateSpread(CSWeaponID::Mp5Navy, cubic, 0.2f, state), 0.04f));
    assert(Near(Build4100StateSpread(CSWeaponID::Mac10, cubic, 0.2f, state), 0.075f));
    assert(Near(Build4100StateSpread(CSWeaponID::Tmp, cubic, 0.2f, state), 0.05f));
    assert(Near(Build4100StateSpread(CSWeaponID::P90, cubic, 0.2f, state), 0.06f));
    assert(Near(Build4100StateSpread(CSWeaponID::Sg552, cubic, 0.2f, state), 0.09f));
    assert(Near(Build4100StateSpread(CSWeaponID::M3, cubic, 0.2f, state), 0.0675000027f));
    assert(Near(Build4100StateSpread(CSWeaponID::XM1014, cubic, 0.2f, state), 0.0724999979f));

    state.onGround = true;
    state.speed2D = 200.0f;
    assert(Near(Build4100StateSpread(CSWeaponID::Ak47, cubic, 0.2f, state), 0.054f));
    assert(Near(Build4100StateSpread(CSWeaponID::M4A1, cubic, 0.2f, state), 0.049f));
    assert(Near(Build4100StateSpread(CSWeaponID::M249, cubic, 0.2f, state), 0.064f));
    assert(Near(Build4100StateSpread(CSWeaponID::Aug, cubic, 0.2f, state), 0.049f));
    assert(Near(Build4100StateSpread(CSWeaponID::Galil, cubic, 0.2f, state), 0.054f));
    assert(Near(Build4100StateSpread(CSWeaponID::Famas, cubic, 0.2f, state), 0.044f));
    assert(Near(Build4100StateSpread(CSWeaponID::P90, cubic, 0.2f, state), 0.068f));
    assert(Near(Build4100StateSpread(CSWeaponID::Sg552, cubic, 0.2f, state), 0.05f));

    state.speed2D = 0.0f;
    assert(Near(Build4100StateSpread(CSWeaponID::Ak47, cubic, 0.2f, state), 0.0055f));
    assert(Near(Build4100StateSpread(CSWeaponID::M4A1, cubic, 0.2f, state), 0.004f));
    assert(Near(Build4100StateSpread(CSWeaponID::M249, cubic, 0.2f, state), 0.006f));
    assert(Near(Build4100StateSpread(CSWeaponID::Ump45, cubic, 0.2f, state), 0.008f));
    assert(Near(Build4100StateSpread(CSWeaponID::Aug, cubic, 0.2f, state), 0.004f));
    assert(Near(Build4100StateSpread(CSWeaponID::Galil, cubic, 0.2f, state), 0.0075f));
    assert(Near(Build4100StateSpread(CSWeaponID::Famas, cubic, 0.2f, state), 0.004f));
    assert(Near(Build4100StateSpread(CSWeaponID::Mp5Navy, cubic, 0.2f, state), 0.008f));
    assert(Near(Build4100StateSpread(CSWeaponID::Mac10, cubic, 0.2f, state), 0.006f));
    assert(Near(Build4100StateSpread(CSWeaponID::Tmp, cubic, 0.2f, state), 0.006f));
    assert(Near(Build4100StateSpread(CSWeaponID::P90, cubic, 0.2f, state), 0.009f));
    assert(Near(Build4100StateSpread(CSWeaponID::Sg552, cubic, 0.2f, state), 0.004f));
    state.silenced = true;
    assert(Near(Build4100StateSpread(CSWeaponID::M4A1, cubic, 0.2f, state), 0.005f));

    // v1.8.35e: recovered pistol/sniper state models are now selected by the
    // live exact path after secondary/FOV/burst state recovery.
    for (const auto weapon : {CSWeaponID::P228, CSWeaponID::Glock, CSWeaponID::Elite,
                              CSWeaponID::FiveSeven, CSWeaponID::Usp, CSWeaponID::Deagle,
                              CSWeaponID::Awp, CSWeaponID::Scout, CSWeaponID::G3Sg1,
                              CSWeaponID::Sg550}) {
        assert(HasBuild4100RecoveredPistolSniperStateModel(weapon));
    }
    assert(!HasBuild4100RecoveredPistolSniperStateModel(CSWeaponID::Ak47));
    for (const auto weapon : {CSWeaponID::P228, CSWeaponID::Glock, CSWeaponID::Elite,
                              CSWeaponID::FiveSeven, CSWeaponID::Usp, CSWeaponID::Deagle,
                              CSWeaponID::G3Sg1, CSWeaponID::Sg550}) {
        assert(HasBuild4100RecoveredTimedAccuracy(weapon));
    }
    assert(!HasBuild4100RecoveredTimedAccuracy(CSWeaponID::Awp));
    assert(!HasBuild4100RecoveredTimedAccuracy(CSWeaponID::Scout));

    // All ten pistol/sniper families use the recovered post-shot recoil path.
    for (const auto weapon : {CSWeaponID::P228, CSWeaponID::Glock, CSWeaponID::Elite,
                              CSWeaponID::FiveSeven, CSWeaponID::Usp, CSWeaponID::Deagle,
                              CSWeaponID::Awp, CSWeaponID::Scout, CSWeaponID::G3Sg1,
                              CSWeaponID::Sg550}) {
        assert(HasBuild4100RecoveredPistolSniperRecoil(weapon));
    }
    assert(!HasBuild4100RecoveredPistolSniperRecoil(CSWeaponID::Ak47));

    WeaponMovementState pistolState{};
    const float pistolAccuracy = 0.8f;
    pistolState.onGround = false;
    assert(Near(Build4100StateSpread(CSWeaponID::P228, cubic, pistolAccuracy, pistolState), 0.300f));
    assert(Near(Build4100StateSpread(CSWeaponID::Elite, cubic, pistolAccuracy, pistolState), 0.260f));
    assert(Near(Build4100StateSpread(CSWeaponID::Deagle, cubic, pistolAccuracy, pistolState), 0.300f));
    assert(Near(Build4100StateSpread(CSWeaponID::Usp, cubic, pistolAccuracy, pistolState), 0.240f));
    assert(Near(Build4100StateSpread(CSWeaponID::Glock, cubic, pistolAccuracy, pistolState), 0.200f));
    pistolState.silenced = true;
    assert(Near(Build4100StateSpread(CSWeaponID::Usp, cubic, pistolAccuracy, pistolState), 0.260f));
    pistolState.silenced = false;
    pistolState.burstMode = true;
    assert(Near(Build4100StateSpread(CSWeaponID::Glock, cubic, pistolAccuracy, pistolState), 0.240f));

    pistolState = WeaponMovementState{};
    pistolState.speed2D = 6.0f;
    assert(Near(Build4100StateSpread(CSWeaponID::P228, cubic, pistolAccuracy, pistolState), 0.051f));
    assert(Near(Build4100StateSpread(CSWeaponID::Elite, cubic, pistolAccuracy, pistolState), 0.035f));
    assert(Near(Build4100StateSpread(CSWeaponID::Deagle, cubic, pistolAccuracy, pistolState), 0.050f));
    assert(Near(Build4100StateSpread(CSWeaponID::Usp, cubic, pistolAccuracy, pistolState), 0.045f));
    assert(Near(Build4100StateSpread(CSWeaponID::Glock, cubic, pistolAccuracy, pistolState), 0.033f));
    pistolState = WeaponMovementState{};
    pistolState.ducked = true;
    assert(Near(Build4100StateSpread(CSWeaponID::P228, cubic, pistolAccuracy, pistolState), 0.015f));
    assert(Near(Build4100StateSpread(CSWeaponID::Elite, cubic, pistolAccuracy, pistolState), 0.016f));
    assert(Near(Build4100StateSpread(CSWeaponID::Deagle, cubic, pistolAccuracy, pistolState), 0.023f));
    assert(Near(Build4100StateSpread(CSWeaponID::Usp, cubic, pistolAccuracy, pistolState), 0.016f));
    assert(Near(Build4100StateSpread(CSWeaponID::Glock, cubic, pistolAccuracy, pistolState), 0.015f));
    pistolState = WeaponMovementState{};
    assert(Near(Build4100StateSpread(CSWeaponID::P228, cubic, pistolAccuracy, pistolState), 0.030f));
    assert(Near(Build4100StateSpread(CSWeaponID::FiveSeven, cubic, pistolAccuracy, pistolState), 0.030f));
    assert(Near(Build4100StateSpread(CSWeaponID::Elite, cubic, pistolAccuracy, pistolState), 0.020f));
    assert(Near(Build4100StateSpread(CSWeaponID::Deagle, cubic, pistolAccuracy, pistolState), 0.026f));
    assert(Near(Build4100StateSpread(CSWeaponID::Usp, cubic, pistolAccuracy, pistolState), 0.020f));
    assert(Near(Build4100StateSpread(CSWeaponID::Glock, cubic, pistolAccuracy, pistolState), 0.020f));
    pistolState.silenced = true;
    assert(Near(Build4100StateSpread(CSWeaponID::Usp, cubic, pistolAccuracy, pistolState), 0.030f));
    pistolState = WeaponMovementState{};
    pistolState.burstMode = true;
    assert(Near(Build4100StateSpread(CSWeaponID::Glock, cubic, pistolAccuracy, pistolState), 0.060f));

    WeaponMovementState sniperState{};
    const float sniperAccuracy = 0.8f;
    assert(Near(Build4100StateSpread(CSWeaponID::Awp, cubic, sniperAccuracy, sniperState), 0.081f));
    assert(Near(Build4100StateSpread(CSWeaponID::Scout, cubic, sniperAccuracy, sniperState), 0.032f));
    assert(Near(Build4100StateSpread(CSWeaponID::G3Sg1, cubic, sniperAccuracy, sniperState), 0.036f));
    assert(Near(Build4100StateSpread(CSWeaponID::Sg550, cubic, sniperAccuracy, sniperState), 0.035f));
    sniperState.zoomed = true;
    assert(Near(Build4100StateSpread(CSWeaponID::Awp, cubic, sniperAccuracy, sniperState), 0.001f));
    assert(Near(Build4100StateSpread(CSWeaponID::Scout, cubic, sniperAccuracy, sniperState), 0.007f));
    sniperState = WeaponMovementState{};
    sniperState.speed2D = 180.0f;
    assert(Near(Build4100StateSpread(CSWeaponID::Awp, cubic, sniperAccuracy, sniperState), 0.330f));
    assert(Near(Build4100StateSpread(CSWeaponID::Scout, cubic, sniperAccuracy, sniperState), 0.100f));
    assert(Near(Build4100StateSpread(CSWeaponID::G3Sg1, cubic, sniperAccuracy, sniperState), 0.175f));
    assert(Near(Build4100StateSpread(CSWeaponID::Sg550, cubic, sniperAccuracy, sniperState), 0.175f));
    sniperState = WeaponMovementState{};
    sniperState.onGround = false;
    assert(Near(Build4100StateSpread(CSWeaponID::Awp, cubic, sniperAccuracy, sniperState), 0.930f));
    assert(Near(Build4100StateSpread(CSWeaponID::Scout, cubic, sniperAccuracy, sniperState), 0.225f));
    assert(Near(Build4100StateSpread(CSWeaponID::G3Sg1, cubic, sniperAccuracy, sniperState), 0.115f));
    assert(Near(Build4100StateSpread(CSWeaponID::Sg550, cubic, sniperAccuracy, sniperState), 0.115f));

    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::P228, 0.900f, 0.100f), 0.8325f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::FiveSeven, 0.920f, 0.100f), 0.87625f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::Elite, 0.880f, 0.100f), 0.818125f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::Deagle, 0.900f, 0.100f), 0.795f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::Usp, 0.920f, 0.100f), 0.865f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::Glock, 0.900f, 0.100f), 0.838125f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::P228, 0.900f, 1.000f), 0.900f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::G3Sg1, 0.980f, 0.500f), 0.700f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::Sg550, 0.980f, 0.500f), 0.825f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::G3Sg1, 0.980f, 2.000f), 0.980f));
    assert(Near(AdvanceBuild4100TimedAccuracy(CSWeaponID::Sg550, 0.980f, 2.000f), 0.980f));

    // Exact CCSPlayer::KickBack state tables recovered from each build4100 fire
    // function. Pin all four selectors for every newly-added family.
    const WeaponMovementState air{false, false, false, 0.0f};
    const WeaponMovementState moving{true, false, false, 10.0f};
    const WeaponMovementState ducked{true, true, false, 0.0f};
    const WeaponMovementState standing{true, false, false, 0.0f};

    AssertKick(CSWeaponID::Ak47, air, {2.0f, 1.0f, 0.5f, 0.35f, 9.0f, 6.0f, 5});
    AssertKick(CSWeaponID::M4A1, ducked, {0.6f, 0.3f, 0.2f, 0.0125f, 3.25f, 2.0f, 7});
    AssertKick(CSWeaponID::M249, moving, {1.1f, 0.5f, 0.3f, 0.06f, 4.0f, 3.0f, 8});
    AssertKick(CSWeaponID::Ump45, standing, {0.275f, 0.2f, 0.15f, 0.0225f, 2.5f, 1.5f, 10});

    AssertKick(CSWeaponID::Aug, air, {1.25f, 0.45f, 0.22f, 0.18f, 5.5f, 4.0f, 5});
    AssertKick(CSWeaponID::Aug, moving, {1.0f, 0.45f, 0.275f, 0.05f, 4.0f, 2.5f, 7});
    AssertKick(CSWeaponID::Aug, ducked, {0.575f, 0.325f, 0.2f, 0.011f, 3.25f, 2.0f, 8});
    AssertKick(CSWeaponID::Aug, standing, {0.625f, 0.375f, 0.25f, 0.0125f, 3.5f, 2.25f, 8});

    AssertKick(CSWeaponID::Galil, air, {1.2f, 0.5f, 0.23f, 0.15f, 5.5f, 3.5f, 6});
    AssertKick(CSWeaponID::Galil, moving, {1.0f, 0.45f, 0.28f, 0.045f, 3.75f, 3.0f, 7});
    AssertKick(CSWeaponID::Galil, ducked, {0.6f, 0.3f, 0.2f, 0.0125f, 3.25f, 2.0f, 7});
    AssertKick(CSWeaponID::Galil, standing, {0.65f, 0.35f, 0.25f, 0.015f, 3.5f, 2.25f, 7});

    AssertKick(CSWeaponID::Famas, air, {1.25f, 0.45f, 0.22f, 0.18f, 5.5f, 4.0f, 5});
    AssertKick(CSWeaponID::Famas, moving, {1.0f, 0.45f, 0.275f, 0.05f, 4.0f, 2.5f, 7});
    AssertKick(CSWeaponID::Famas, ducked, {0.575f, 0.325f, 0.2f, 0.011f, 3.25f, 2.0f, 8});
    AssertKick(CSWeaponID::Famas, standing, {0.625f, 0.375f, 0.25f, 0.0125f, 3.5f, 2.25f, 8});

    AssertKick(CSWeaponID::Mp5Navy, air, {0.9f, 0.475f, 0.35f, 0.0425f, 5.0f, 3.0f, 6});
    AssertKick(CSWeaponID::Mp5Navy, moving, {0.5f, 0.275f, 0.2f, 0.03f, 3.0f, 2.0f, 10});
    AssertKick(CSWeaponID::Mp5Navy, ducked, {0.225f, 0.15f, 0.1f, 0.015f, 2.0f, 1.0f, 10});
    AssertKick(CSWeaponID::Mp5Navy, standing, {0.25f, 0.175f, 0.125f, 0.02f, 2.25f, 1.25f, 10});

    AssertKick(CSWeaponID::Mac10, air, {1.3f, 0.55f, 0.4f, 0.05f, 4.75f, 3.75f, 5});
    AssertKick(CSWeaponID::Mac10, moving, {0.9f, 0.45f, 0.25f, 0.035f, 3.5f, 2.75f, 7});
    AssertKick(CSWeaponID::Mac10, ducked, {0.75f, 0.4f, 0.175f, 0.03f, 2.75f, 2.5f, 10});
    AssertKick(CSWeaponID::Mac10, standing, {0.775f, 0.425f, 0.2f, 0.03f, 3.0f, 2.75f, 9});

    AssertKick(CSWeaponID::Tmp, air, {1.1f, 0.5f, 0.35f, 0.045f, 4.5f, 3.5f, 6});
    AssertKick(CSWeaponID::Tmp, moving, {0.8f, 0.4f, 0.2f, 0.03f, 3.0f, 2.5f, 7});
    AssertKick(CSWeaponID::Tmp, ducked, {0.7f, 0.35f, 0.125f, 0.025f, 2.5f, 2.0f, 10});
    AssertKick(CSWeaponID::Tmp, standing, {0.725f, 0.375f, 0.15f, 0.025f, 2.75f, 2.25f, 9});

    AssertKick(CSWeaponID::P90, air, {0.9f, 0.45f, 0.35f, 0.04f, 5.25f, 3.5f, 4});
    AssertKick(CSWeaponID::P90, moving, {0.45f, 0.3f, 0.2f, 0.0275f, 4.0f, 2.25f, 7});
    AssertKick(CSWeaponID::P90, ducked, {0.275f, 0.2f, 0.125f, 0.02f, 3.0f, 1.0f, 9});
    AssertKick(CSWeaponID::P90, standing, {0.3f, 0.225f, 0.125f, 0.02f, 3.25f, 1.25f, 8});

    AssertKick(CSWeaponID::Sg552, air, {1.25f, 0.45f, 0.22f, 0.18f, 6.0f, 4.0f, 5});
    AssertKick(CSWeaponID::Sg552, moving, {1.0f, 0.45f, 0.28f, 0.04f, 4.25f, 2.5f, 7});
    AssertKick(CSWeaponID::Sg552, ducked, {0.6f, 0.35f, 0.2f, 0.0125f, 3.7f, 2.0f, 10});
    AssertKick(CSWeaponID::Sg552, standing, {0.625f, 0.375f, 0.25f, 0.0125f, 4.0f, 2.25f, 9});

    // Source CUniformRandomStream / FX_FireBullets deterministic vectors.
    const auto spread17 = Build4100FxSpreadSample(17, 0);
    assert(spread17.seed == 18);
    assert(Near(spread17.x, -0.15010276f, 0.000002f));
    assert(Near(spread17.y, -0.04484209f, 0.000002f));
    const auto spreadWrapped = Build4100FxSpreadSample(17 + 256, 0);
    assert(Near(spread17.x, spreadWrapped.x, 0.0000001f));
    assert(Near(spread17.y, spreadWrapped.y, 0.0000001f));
    const auto spreadPellet1 = Build4100FxSpreadSample(17, 1);
    assert(spreadPellet1.seed == 19);
    assert(!Near(spread17.x, spreadPellet1.x) || !Near(spread17.y, spreadPellet1.y));

    // Isolate the KickBack impulse from direction RNG by disabling flips.
    KickBackParameters deterministicKick{1.0f, 0.375f, 0.175f, 0.0375f, 5.75f, 1.75f, 0};
    int direction = 1;
    QAngle punch{};
    punch = ApplyBuild4100KickBack(punch, 1, direction, deterministicKick, 111);
    assert(Near(punch.x, -1.0f) && Near(punch.y, 0.375f));
    punch = ApplyBuild4100KickBack(punch, 2, direction, deterministicKick, 111);
    assert(Near(punch.x, -2.35f) && Near(punch.y, 0.825f));

    // build4100 CCSPlayer::KickBack calls SharedRandomInt("KickBack",0,N,0).
    // These vectors pin the Source CRC32 seed hash + CUniformRandomStream.
    assert(Build4100SharedRandomInt(17, "KickBack", 0, 8, 0) == 4);
    assert(Build4100SharedRandomInt(3, "KickBack", 0, 8, 0) == 0);
    KickBackParameters randomKick = deterministicKick;
    randomKick.directionChange = 8;
    direction = 1;
    (void)ApplyBuild4100KickBack(QAngle{}, 1, direction, randomKick, 17);
    assert(direction == 1);
    direction = 1;
    (void)ApplyBuild4100KickBack(QAngle{}, 1, direction, randomKick, 3);
    assert(direction == 0);  // SharedRandomInt roll 0 -> flip.

    // Pistol/AWP/Scout build4100 routines subtract an exact 2 degree pitch
    // impulse. Glock is an explicitly recovered no-op. Autosnipers use named
    // SharedRandomFloat streams with the original ranges.
    QAngle pistolPunch{1.0f, 0.5f, 0.0f};
    assert(ApplyBuild4100PistolSniperRecoil(CSWeaponID::P228, pistolPunch, 17));
    assert(Near(pistolPunch.x, -1.0f) && Near(pistolPunch.y, 0.5f));
    QAngle awpPunch{0.25f, -0.5f, 0.0f};
    assert(ApplyBuild4100PistolSniperRecoil(CSWeaponID::Awp, awpPunch, 17));
    assert(Near(awpPunch.x, -1.75f) && Near(awpPunch.y, -0.5f));
    QAngle glockPunch{0.25f, -0.5f, 0.0f};
    assert(ApplyBuild4100PistolSniperRecoil(CSWeaponID::Glock, glockPunch, 17));
    assert(Near(glockPunch.x, 0.25f) && Near(glockPunch.y, -0.5f));

    const float g3Pitch = Build4100SharedRandomFloat(17, "G3SG1PunchAngleX", 0.75f, 1.75f, 0);
    const float g3Yaw = Build4100SharedRandomFloat(17, "G3SG1PunchAngleY", -0.75f, 0.75f, 0);
    assert(g3Pitch >= 0.75f && g3Pitch <= 1.75f);
    assert(g3Yaw >= -0.75f && g3Yaw <= 0.75f);
    QAngle g3Punch{1.0f, 0.25f, 0.0f};
    assert(ApplyBuild4100PistolSniperRecoil(CSWeaponID::G3Sg1, g3Punch, 17));
    assert(Near(g3Punch.x, 0.75f - g3Pitch, 0.000002f));
    assert(Near(g3Punch.y, 0.25f + g3Yaw, 0.000002f));

    const float sgPitch = Build4100SharedRandomFloat(31, "SG550PunchAngleX", 0.75f, 1.25f, 0);
    const float sgYaw = Build4100SharedRandomFloat(31, "SG550PunchAngleY", -0.75f, 0.75f, 0);
    QAngle sgPunch{-0.5f, -0.25f, 0.0f};
    assert(ApplyBuild4100PistolSniperRecoil(CSWeaponID::Sg550, sgPunch, 31));
    assert(Near(sgPunch.x, -0.375f - sgPitch, 0.000002f));
    assert(Near(sgPunch.y, -0.25f + sgYaw, 0.000002f));

    // Exact CGameMovement::DecayPunchAngle damped spring.
    QAngle decayPunch{-2.0f, 0.5f, 0.0f};
    QAngle decayVelocity{};
    DecayBuild4100Punch(decayPunch, decayVelocity, 0.015f);
    assert(Near(decayPunch.x, -2.0f));
    assert(Near(decayPunch.y, 0.5f));
    assert(Near(decayVelocity.x, 1.95f, 0.00001f));
    assert(Near(decayVelocity.y, -0.4875f, 0.00001f));
    DecayBuild4100Punch(decayPunch, decayVelocity, 0.015f);
    assert(Near(decayPunch.x, -1.97075f, 0.00001f));
    assert(Near(decayPunch.y, 0.4926875f, 0.00001f));
    assert(Near(decayVelocity.x, 3.608231f, 0.00002f));
    assert(Near(decayVelocity.y, -0.9020578f, 0.00002f));
    QAngle tinyPunch{0.01f, 0.0f, 0.0f};
    QAngle tinyVelocity{};
    DecayBuild4100Punch(tinyPunch, tinyVelocity, 0.015f);
    assert(Near(tinyPunch.x, 0.01f));
    assert(Near(tinyVelocity.x, 0.0f));

    // Shotgun PrimaryAttack uses fixed spread and FX_FireBullets reseeds once
    // per pellet. M3 has 9 script pellets, XM1014 has 6; parser/live profile
    // assertions pin those counts when original content is available.
    WeaponRuntimeProfile m3Profile{};
    m3Profile.range = 3000.0f;
    m3Profile.rangeModifier = 0.7f;
    m3Profile.baseDamage = 26.0f;
    m3Profile.bulletsPerShot = 9;
    WeaponMovementState shotgunState{};
    const auto m3Pellet0 = SolveBuild4100WeaponPellet(
        CSWeaponID::M3, m3Profile, QAngle{}, shotgunState, 0.2f, QAngle{}, 17, 0);
    const auto m3Pellet1 = SolveBuild4100WeaponPellet(
        CSWeaponID::M3, m3Profile, QAngle{}, shotgunState, 0.2f, QAngle{}, 17, 1);
    assert(Near(m3Pellet0.spread, 0.0675000027f));
    assert(Near(m3Pellet1.spread, 0.0675000027f));
    assert(!Near(m3Pellet0.spreadX, m3Pellet1.spreadX) ||
           !Near(m3Pellet0.spreadY, m3Pellet1.spreadY));
    assert(Near(m3Pellet0.spreadX, spread17.x * 0.0675000027f, 0.0000002f));
    assert(Near(m3Pellet0.spreadY, spread17.y * 0.0675000027f, 0.0000002f));
    assert(Near(m3Pellet1.spreadX, spreadPellet1.x * 0.0675000027f, 0.0000002f));
    assert(Near(m3Pellet1.spreadY, spreadPellet1.y * 0.0675000027f, 0.0000002f));

    QAngle m3GroundPunch{};
    assert(ApplyBuild4100ShotgunRecoil(CSWeaponID::M3, true, m3GroundPunch, 17));
    const int m3GroundImpulse = Build4100SharedRandomInt(17, "M3PunchAngleGround", 4, 6, 0);
    assert(m3GroundImpulse >= 4 && m3GroundImpulse <= 6);
    assert(Near(m3GroundPunch.x, -static_cast<float>(m3GroundImpulse)));
    QAngle m3AirPunch{};
    assert(ApplyBuild4100ShotgunRecoil(CSWeaponID::M3, false, m3AirPunch, 17));
    const int m3AirImpulse = Build4100SharedRandomInt(17, "M3PunchAngleAir", 8, 11, 0);
    assert(m3AirImpulse >= 8 && m3AirImpulse <= 11);
    assert(Near(m3AirPunch.x, -static_cast<float>(m3AirImpulse)));
    QAngle xmGroundPunch{};
    assert(ApplyBuild4100ShotgunRecoil(CSWeaponID::XM1014, true, xmGroundPunch, 31));
    const int xmGroundImpulse = Build4100SharedRandomInt(31, "XM1014PunchAngleGround", 3, 5, 0);
    assert(xmGroundImpulse >= 3 && xmGroundImpulse <= 5);
    assert(Near(xmGroundPunch.x, -static_cast<float>(xmGroundImpulse)));
    QAngle xmAirPunch{};
    assert(ApplyBuild4100ShotgunRecoil(CSWeaponID::XM1014, false, xmAirPunch, 31));
    const int xmAirImpulse = Build4100SharedRandomInt(31, "XM1014PunchAngleAir", 7, 10, 0);
    assert(xmAirImpulse >= 7 && xmAirImpulse <= 10);
    assert(Near(xmAirPunch.x, -static_cast<float>(xmAirImpulse)));

    // CSBaseGunFire fires with EyeAngles + 2*old PunchAngle. New KickBack is
    // deliberately not part of this solution and therefore affects next shot.
    state = WeaponMovementState{};
    const QAngle oldPunch{-1.0f, 0.25f, 0.0f};
    const auto classicShot = SolveBuild4100WeaponShot(
        CSWeaponID::Ak47, cubic, QAngle{}, state, 0.2f, oldPunch, 17, 23, 4);
    assert(Near(classicShot.recoilPitch, -2.0f));
    assert(Near(classicShot.recoilYaw, 0.5f));
    assert(Near(classicShot.spread, 0.0055f));
    assert(Near(classicShot.spreadX, -0.0008255652f, 0.00000002f));
    assert(Near(classicShot.spreadY, -0.0002466315f, 0.00000002f));

    const auto& pistol = WeaponProfile(CSWeaponID::Usp);
    const auto& rifle = WeaponProfile(CSWeaponID::Ak47);
    assert(pistol.range >= 4096.0f);
    assert(rifle.range >= pistol.range);
    assert(pistol.rangeModifier > 0.0f && pistol.rangeModifier <= 1.0f);

    const QAngle aim{0.0f, 90.0f, 0.0f};
    const auto shotA = SolveWeaponShot(pistol, aim, 0.10f, 1.5f, 12345, 77, 3);
    const auto shotB = SolveWeaponShot(pistol, aim, 0.10f, 1.5f, 12345, 77, 3);
    const auto shotC = SolveWeaponShot(pistol, aim, 0.10f, 1.5f, 12346, 77, 3);
    assert(Near(shotA.direction.x, shotB.direction.x));
    assert(Near(shotA.direction.y, shotB.direction.y));
    assert(Near(shotA.direction.z, shotB.direction.z));
    assert(Near(shotA.spreadX, shotB.spreadX));
    assert(Near(shotA.spreadY, shotB.spreadY));
    assert(!Near(shotA.spreadX, shotC.spreadX) || !Near(shotA.spreadY, shotC.spreadY));
    const float length = std::sqrt(shotA.direction.x * shotA.direction.x +
                                   shotA.direction.y * shotA.direction.y +
                                   shotA.direction.z * shotA.direction.z);
    assert(std::fabs(length - 1.0f) < 0.0001f);
    assert(shotA.recoilPitch > 0.0f);

    const float nearDamage = BallisticDamageAtDistance(pistol, 0.0f);
    const float midDamage = BallisticDamageAtDistance(pistol, 1000.0f);
    const float farDamage = BallisticDamageAtDistance(pistol, pistol.range);
    assert(Near(nearDamage, pistol.baseDamage));
    assert(midDamage < nearDamage);
    assert(farDamage <= midDamage);
    assert(farDamage > 0.0f);

    LagCompensationHistory history;
    assert(history.Configure(2, 4));
    LagCompensationRecord r{};
    r.alive = true;
    r.mins = Vector{-16.0f, -16.0f, 0.0f};
    r.maxs = Vector{16.0f, 16.0f, 72.0f};
    r.tick = 100;
    r.simulationTime = 1.50f;
    r.origin = Vector{10.0f, 0.0f, 0.0f};
    assert(history.Record(1, r));
    r.tick = 105;
    r.simulationTime = 1.575f;
    r.origin = Vector{20.0f, 0.0f, 0.0f};
    assert(history.Record(1, r));
    // Same-tick samples replace rather than consume history depth.
    r.origin = Vector{21.0f, 0.0f, 0.0f};
    assert(history.Record(1, r));
    assert(history.Count(1) == 2);

    LagCompensationRecord selected{};
    assert(history.Select(1, 102, 106, 13, selected));
    assert(selected.tick == 100);
    assert(Near(selected.origin.x, 10.0f));
    assert(history.Select(1, 105, 106, 13, selected));
    assert(selected.tick == 105);
    assert(Near(selected.origin.x, 21.0f));
    // Future usercmd ticks are clamped to the current server tick/newest sample.
    assert(history.Select(1, 999, 106, 13, selected));
    assert(selected.tick == 105);
    // Stale history beyond the rewind window is rejected.
    assert(!history.Select(1, 100, 130, 13, selected));

    std::cout << "COMBAT_BALLISTICS=PASS\n";
    std::cout << "weapon_ctx=thin_ice+kv_parser exact_fields=damage,range,range_modifier,armor,cycle,bullets,penetration,accuracy_divisor,accuracy_offset,max_inaccuracy,accuracy_quadratic accuracy=build4100_cube_or_square legacy_spread=auto12_state_exact spread=auto12+pistol6+sniper4_live_exact shotgun_spread=m3_0.0675+xm1014_0.0725 rng=source_uniform_fx_seed_exact legacy_recoil=KickBack_auto12_state_exact legacy_model_recoil=KickBack_auto12_state_exact+pistol6_sniper4_model_exact recoil=KickBack_auto12+pistol6_sniper4_live_exact shotgun_recoil=m3_ground4_6_air8_11+xm_ground3_5_air7_10 legacy_timed_accuracy=pistol6+autosniper2_model_exact timed_accuracy=pistol6+autosniper2_live_exact punch_decay=source_spring_exact glock_followup_spread=0.05_exact live_exact_families=24 shotgun_pellets=fx_seed_per_pellet lagcomp=200ms_bounded_history\n";
    return 0;
}
