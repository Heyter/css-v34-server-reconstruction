#pragma once

#include <cstddef>
#include <cstdint>

#include "public/build4100/vector.h"
#include "server/weapon_runtime.h"

namespace cssv34::reconstructed {

struct WeaponShotSolution {
    build4100::Vector direction{};
    float spreadX = 0.0f;
    float spreadY = 0.0f;
    float spread = 0.0f;
    float recoilPitch = 0.0f;
    float recoilYaw = 0.0f;
    float range = 8192.0f;
};

struct KickBackParameters {
    float upBase = 0.0f;
    float lateralBase = 0.0f;
    float upModifier = 0.0f;
    float lateralModifier = 0.0f;
    float upMax = 0.0f;
    float lateralMax = 0.0f;
    int directionChange = 0;
};

bool UsesBuild4100ClassicBallistics(build4100::CSWeaponID weapon) noexcept;
// Binary-recovered v1.8.35c pistol/sniper models are intentionally separate
// from the currently live-wired twelve-family automatic selector.
bool HasBuild4100RecoveredPistolSniperStateModel(build4100::CSWeaponID weapon) noexcept;
bool HasBuild4100RecoveredTimedAccuracy(build4100::CSWeaponID weapon) noexcept;
bool HasBuild4100RecoveredPistolSniperRecoil(build4100::CSWeaponID weapon) noexcept;
bool HasBuild4100RecoveredShotgunRecoil(build4100::CSWeaponID weapon) noexcept;
float AdvanceBuild4100TimedAccuracy(build4100::CSWeaponID weapon,
                                    float currentAccuracy,
                                    float timeSinceLastShot) noexcept;

// Exact CWeaponCSBaseGun::CSBaseGunFire accuracy progression recovered from
// build4100 server_i486.so. The caller passes the already-incremented shot
// count. AccuracyDivisor == -1 keeps the weapon-owned accuracy unchanged.
float AdvanceBuild4100Accuracy(const WeaponRuntimeProfile& profile,
                               std::size_t shotsFired,
                               float currentAccuracy) noexcept;

// State-dependent spread recovered directly from build4100 PrimaryAttack.
// v1.8.35f selects 12 automatic + 6 pistol + 4 sniper + 2 shotgun families live.
float Build4100StateSpread(build4100::CSWeaponID weapon,
                           const WeaponRuntimeProfile& profile,
                           float accuracy,
                           const WeaponMovementState& movement) noexcept;

// Exact build4100 KickBack parameter sets for the same twelve weapon families.
// Returns false for weapons still using the compatibility approximation path.
bool Build4100KickBackParameters(build4100::CSWeaponID weapon,
                                 const WeaponMovementState& movement,
                                 KickBackParameters& out) noexcept;

struct Build4100SpreadSample {
    float x = 0.0f;
    float y = 0.0f;
    std::int32_t seed = 0;
};

// Source 2005/build4100 prediction RNG path used by FX_FireBullets: the
// prediction seed is reduced to 8 bits, incremented once, and each pellet
// consumes four RandomFloat(-0.5, 0.5) values from CUniformRandomStream.
Build4100SpreadSample Build4100FxSpreadSample(std::int32_t predictionRandomSeed,
                                               std::size_t pelletIndex) noexcept;

// Exact Source prediction SharedRandomInt path (CRC32 of prediction seed,
// additional seed and shared name, then CUniformRandomStream::RandomInt).
int Build4100SharedRandomInt(std::int32_t predictionRandomSeed,
                             const char* sharedName,
                             int minValue,
                             int maxValue,
                             int additionalSeed) noexcept;

// Same Source prediction SharedRandom seed path as SharedRandomInt, but with
// CUniformRandomStream::RandomFloat. G3SG1/SG550 use this for their exact
// build4100 punch-angle impulses.
float Build4100SharedRandomFloat(std::int32_t predictionRandomSeed,
                                 const char* sharedName,
                                 float minValue,
                                 float maxValue,
                                 int additionalSeed) noexcept;

// Binary-recovered post-fire recoil for the six pistols and four snipers from
// v1.8.35c. Glock's build4100 GlockFire path has no post-shot SetPunchAngle
// write; it is represented as a successful no-op rather than an approximation.
bool ApplyBuild4100PistolSniperRecoil(build4100::CSWeaponID weapon,
                                      build4100::QAngle& punch,
                                      std::int32_t predictionRandomSeed) noexcept;

// Exact M3/XM1014 post-shot pitch impulses recovered from build4100
// PrimaryAttack. Ground/air ranges use distinct SharedRandomInt stream names.
bool ApplyBuild4100ShotgunRecoil(build4100::CSWeaponID weapon,
                                 bool onGround,
                                 build4100::QAngle& punch,
                                 std::int32_t predictionRandomSeed) noexcept;

// Implements the CCSPlayer::KickBack impulse/clamps. build4100 calls
// SharedRandomInt("KickBack", 0, directionChange, 0) for direction changes.
build4100::QAngle ApplyBuild4100KickBack(const build4100::QAngle& punch,
                                         std::size_t shotsFired,
                                         int& kickDirection,
                                         const KickBackParameters& params,
                                         std::int32_t predictionRandomSeed) noexcept;

// Exact Source CGameMovement::DecayPunchAngle spring/damping step.
void DecayBuild4100Punch(build4100::QAngle& punch,
                         build4100::QAngle& punchVelocity,
                         float frameTime) noexcept;

// Classic build4100 path: PrimaryAttack computes spread from pre-fire accuracy
// and CSBaseGunFire uses EyeAngles + 2*PunchAngle. The new KickBack impulse is
// intentionally applied only after this solution has been consumed.
WeaponShotSolution SolveBuild4100WeaponShot(build4100::CSWeaponID weapon,
                                            const WeaponRuntimeProfile& profile,
                                            const build4100::QAngle& viewAngles,
                                            const WeaponMovementState& movement,
                                            float shotAccuracy,
                                            const build4100::QAngle& punchAngle,
                                            std::int32_t randomSeed,
                                            std::int32_t commandNumber,
                                            std::size_t shotIndex,
                                            float spreadOverride = -1.0f) noexcept;

// Same build4100 solver but with an explicit FX_FireBullets pellet index.
// Shotguns reseed CUniformRandomStream with ((predictionSeed & 255)+1+pellet).
WeaponShotSolution SolveBuild4100WeaponPellet(build4100::CSWeaponID weapon,
                                              const WeaponRuntimeProfile& profile,
                                              const build4100::QAngle& viewAngles,
                                              const WeaponMovementState& movement,
                                              float shotAccuracy,
                                              const build4100::QAngle& punchAngle,
                                              std::int32_t randomSeed,
                                              std::size_t pelletIndex,
                                              float spreadOverride = -1.0f) noexcept;

// Compatibility solver for weapons whose exact state model has not yet been
// recovered. This preserves the accepted zv behavior for those weapons.
WeaponShotSolution SolveWeaponShot(const WeaponRuntimeProfile& profile,
                                   const build4100::QAngle& viewAngles,
                                   float accuracyPenalty,
                                   float recoilIndex,
                                   std::int32_t randomSeed,
                                   std::int32_t commandNumber,
                                   std::size_t shotIndex) noexcept;

float BallisticDamageAtDistance(const WeaponRuntimeProfile& profile,
                                float distance) noexcept;

}  // namespace cssv34::reconstructed
