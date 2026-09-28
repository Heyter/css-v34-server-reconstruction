#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "public/build4100/cs_weapon.h"
#include "public/build4100/vector.h"
#include "server/cs_game_state.h"
#include "server/weapon_action.h"

namespace cssv34::reconstructed {

struct WeaponRuntimeProfile {
    float cycleTime = 0.2f;
    float baseDamage = 30.0f;
    float armorRatio = 1.0f;
    float baseSpread = 0.02f;
    float recoilStep = 1.0f;
    float range = 8192.0f;
    float rangeModifier = 0.98f;
    float spreadPenaltyScale = 0.04f;
    float recoilPitchPerIndex = 0.10f;
    float recoilYawPerIndex = 0.035f;
    int bulletsPerShot = 1;
    int penetration = 1;
    // build4100 CCSWeaponInfo fields. AccuracyDivisor == -1 means the weapon
    // owns its accuracy update instead of CWeaponCSBaseGun::CSBaseGunFire.
    float accuracyDivisor = -1.0f;
    float accuracyOffset = 0.2f;
    float maxInaccuracy = 1.0f;
    // Store as an explicit byte instead of C++ bool.  The user-live i386
    // build exposed a non-canonical bool representation (205) in telemetry;
    // byte storage plus explicit normalization keeps the reconstructed path
    // deterministic even if external/packed state writes a non-zero byte.
    std::uint8_t accuracyQuadratic = 0;
};

constexpr int AccuracyQuadraticFlag(const WeaponRuntimeProfile& profile) noexcept {
    return profile.accuracyQuadratic != 0 ? 1 : 0;
}

struct WeaponProfileLoadStatus {
    int exactProfiles = 0;
    int expectedProfiles = 0;
    int encryptedProfiles = 0;
    int plaintextProfiles = 0;
    int failedProfiles = 0;
    std::uint64_t exactMask = 0;
    char sourceRoot[256]{};
};

struct WeaponMovementState {
    bool onGround = true;
    bool ducked = false;
    bool silenced = false;
    float speed2D = 0.0f;
    // Recovered PrimaryAttack/Fire formulas consume the live IN_ATTACK2 mode
    // state. They are appended so accepted four-field aggregate initializers
    // keep their exact meaning.
    bool zoomed = false;
    bool burstMode = false;
};

struct PlayerWeaponRuntime {
    float nextAttackTime = 0.0f;
    float nextSecondaryAttackTime = 0.0f;

    // Legacy diagnostics are retained for historical smoke/gate compatibility;
    // classic automatic-weapon ballistics use shotAccuracy/accuracy/punchAngle.
    float accuracyPenalty = 0.0f;
    float recoilIndex = 0.0f;

    float accuracy = 0.2f;
    float shotAccuracy = 0.2f;
    // <0 => use state-derived spread. Glock FireRemaining uses an exact
    // fixed 0.05 spread for the two scheduled follow-up bullets.
    float shotSpreadOverride = -1.0f;
    build4100::QAngle punchAngle{};
    build4100::QAngle punchAngleVelocity{};
    int kickDirection = 1;
    bool classicBallistics = false;

    // v1.8.35d: live usercmd IN_ATTACK2 state. These flags intentionally live
    // beside the existing per-player reconstructed weapon state for now. The
    // original DLL stores them on individual weapon entities; retaining mode
    // across weapon switches is the next inventory-state refinement, not a
    // prerequisite for decoding and exercising the secondary-action path.
    bool silenced = false;
    bool burstMode = false;
    std::uint8_t zoomLevel = 0;  // 0=unzoomed, 1/2=zoom stages.

    // v1.8.35e exact pistol/sniper live state. Glock schedules two follow-up
    // shots at 100ms intervals (original FireRemaining cadence).
    float lastFireTime = 0.0f;
    float nextBurstShotTime = 0.0f;
    int burstShotsRemaining = 0;
    int burstShotOrdinal = 0;

    build4100::CSWeaponID lastWeapon = build4100::CSWeaponID::None;
    std::size_t shotsFired = 0;
};

enum class TimedWeaponResult {
    Fired,
    Cooldown,
    Rejected,
};

enum class SecondaryWeaponResult {
    Toggled,
    Cooldown,
    Unsupported,
    Rejected,
};

const WeaponProfileLoadStatus& InitializeWeaponProfilesFromDisk() noexcept;
const WeaponProfileLoadStatus& WeaponProfileLoadState() noexcept;
const WeaponRuntimeProfile& WeaponProfile(build4100::CSWeaponID id) noexcept;

class WeaponRuntimeController final {
public:
    bool Configure(std::size_t maxClients);
    void Reset() noexcept;
    bool ResetClient(std::size_t clientIndex) noexcept;
    TimedWeaponResult Fire(std::size_t clientIndex, float now, CsGameState& game,
                           build4100::CSWeaponID activeWeapon) noexcept;
    TimedWeaponResult Melee(std::size_t clientIndex, float now,
                            build4100::CSWeaponID activeWeapon) noexcept;
    SecondaryWeaponResult SecondaryAttack(std::size_t clientIndex, float now,
                                          build4100::CSWeaponID activeWeapon) noexcept;
    bool FireScheduledGlockBurst(std::size_t clientIndex, float now, CsGameState& game,
                                 build4100::CSWeaponID activeWeapon) noexcept;
    WeaponActionResult Reload(std::size_t clientIndex, float now, CsGameState& game,
                              build4100::CSWeaponID activeWeapon) noexcept;
    bool Switch(std::size_t clientIndex, float now, CsGameState& game,
                build4100::CSWeaponID weapon) noexcept;
    void ApplyPostFireKickBack(std::size_t clientIndex, build4100::CSWeaponID weapon,
                               const WeaponMovementState& movement,
                               std::int32_t randomSeed) noexcept;
    void Tick(float frameTime) noexcept;
    const PlayerWeaponRuntime* State(std::size_t clientIndex) const noexcept;
private:
    std::vector<PlayerWeaponRuntime> players_;
};

} // namespace cssv34::reconstructed
