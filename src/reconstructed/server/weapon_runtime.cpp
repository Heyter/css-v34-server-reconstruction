#include "server/weapon_runtime.h"

#include "server/weapon_script_profile.h"
#include "server/weapon_ballistics.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace cssv34::reconstructed {
namespace {
constexpr WeaponRuntimeProfile kDefault{};
// Fallbacks preserve the already-accepted zt live path. v1.8.34zu overlays
// the original build4100 weapon script values when the real CSS content is
// available, removing grouped damage/range/cycle/armor approximations without
// making the rebuilt game DLL dependent on external downloaded tables.
constexpr WeaponRuntimeProfile kPistol{0.15f, 34.0f, 1.0f, 0.012f, 0.75f, 4096.0f, 0.82f, 0.035f, 0.08f, 0.030f};
constexpr WeaponRuntimeProfile kRifle{0.10f, 36.0f, 1.4f, 0.010f, 1.0f, 8192.0f, 0.96f, 0.045f, 0.11f, 0.040f};
constexpr WeaponRuntimeProfile kSniper{1.25f, 115.0f, 1.5f, 0.002f, 2.0f, 8192.0f, 0.99f, 0.020f, 0.16f, 0.025f};
constexpr WeaponRuntimeProfile kSmg{0.075f, 26.0f, 1.0f, 0.018f, 0.7f, 4096.0f, 0.84f, 0.055f, 0.07f, 0.035f};
constexpr WeaponRuntimeProfile kShotgun{0.85f, 22.0f, 1.0f, 0.055f, 1.4f, 2048.0f, 0.72f, 0.075f, 0.12f, 0.045f};

constexpr std::array<build4100::CSWeaponID, 24> kScriptedFirearms{{
    build4100::CSWeaponID::P228,
    build4100::CSWeaponID::Glock,
    build4100::CSWeaponID::Scout,
    build4100::CSWeaponID::XM1014,
    build4100::CSWeaponID::Mac10,
    build4100::CSWeaponID::Aug,
    build4100::CSWeaponID::Elite,
    build4100::CSWeaponID::FiveSeven,
    build4100::CSWeaponID::Ump45,
    build4100::CSWeaponID::Sg550,
    build4100::CSWeaponID::Galil,
    build4100::CSWeaponID::Famas,
    build4100::CSWeaponID::Usp,
    build4100::CSWeaponID::Awp,
    build4100::CSWeaponID::Mp5Navy,
    build4100::CSWeaponID::M249,
    build4100::CSWeaponID::M3,
    build4100::CSWeaponID::M4A1,
    build4100::CSWeaponID::Tmp,
    build4100::CSWeaponID::G3Sg1,
    build4100::CSWeaponID::Deagle,
    build4100::CSWeaponID::Sg552,
    build4100::CSWeaponID::Ak47,
    build4100::CSWeaponID::P90,
}};

std::array<WeaponRuntimeProfile, build4100::kBuild4100WeaponIdMax + 1> gProfiles{};
WeaponProfileLoadStatus gLoadStatus{};
std::once_flag gProfileInit;

float InitialBuild4100Accuracy(const build4100::CSWeaponID id) noexcept {
    using W = build4100::CSWeaponID;
    switch (id) {
        case W::P228: case W::Glock: case W::Deagle: return 0.900f;
        case W::Elite: return 0.880f;
        case W::FiveSeven: case W::Usp: return 0.920f;
        case W::G3Sg1: case W::Sg550: return 0.980f;
        default: return 0.200f;
    }
}

WeaponRuntimeProfile FallbackProfile(const build4100::CSWeaponID id) noexcept {
    using W = build4100::CSWeaponID;
    switch (id) {
        case W::P228: case W::Glock: case W::Elite: case W::FiveSeven: case W::Usp: case W::Deagle:
            return kPistol;
        case W::Ak47: case W::M4A1: case W::Aug: case W::Sg552: case W::Galil: case W::Famas: case W::M249:
            return kRifle;
        case W::Awp: case W::Scout: case W::Sg550: case W::G3Sg1:
            return kSniper;
        case W::Mac10: case W::Ump45: case W::Mp5Navy: case W::Tmp: case W::P90:
            return kSmg;
        case W::M3: case W::XM1014:
            return kShotgun;
        default:
            return kDefault;
    }
}

std::string JoinPath(const std::string& root, const char* const leaf, const char* const extension) {
    std::string path = root;
    if (!path.empty() && path.back() != '/') path.push_back('/');
    path += leaf;
    path += extension;
    return path;
}

std::vector<std::string> CandidateRoots() {
    std::vector<std::string> roots;
    if (const char* const env = std::getenv("CSSV34_WEAPON_SCRIPT_ROOT"); env && *env) {
        roots.emplace_back(env);
    }
    roots.emplace_back("cstrike/scripts");
    roots.emplace_back("./cstrike/scripts");
    roots.emplace_back("scripts");
    roots.emplace_back("./scripts");
    roots.emplace_back("/opt/css-v34/cstrike/scripts");
    return roots;
}

bool TryLoadProfile(const build4100::CSWeaponID id, WeaponRuntimeProfile& profile,
                    bool& encrypted, std::string& rootUsed) {
    const char* const classname = build4100::WeaponIDToClassname(id);
    if (!classname || std::strncmp(classname, "weapon_", 7) != 0) return false;
    for (const std::string& root : CandidateRoots()) {
        const std::string textPath = JoinPath(root, classname, ".txt");
        WeaponRuntimeProfile candidate = profile;
        if (LoadBuild4100WeaponScript(textPath.c_str(), false, candidate)) {
            profile = candidate;
            encrypted = false;
            rootUsed = root;
            return true;
        }
        const std::string ctxPath = JoinPath(root, classname, ".ctx");
        candidate = profile;
        if (LoadBuild4100WeaponScript(ctxPath.c_str(), true, candidate)) {
            profile = candidate;
            encrypted = true;
            rootUsed = root;
            return true;
        }
    }
    return false;
}

void InitializeProfilesOnce() noexcept {
    for (int value = 0; value <= build4100::kBuild4100WeaponIdMax; ++value) {
        const auto id = static_cast<build4100::CSWeaponID>(value);
        gProfiles[static_cast<std::size_t>(value)] = FallbackProfile(id);
    }

    gLoadStatus = WeaponProfileLoadStatus{};
    gLoadStatus.expectedProfiles = static_cast<int>(kScriptedFirearms.size());
    std::string firstRoot;
    for (const auto id : kScriptedFirearms) {
        const auto index = static_cast<std::size_t>(id);
        WeaponRuntimeProfile profile = gProfiles[index];
        bool encrypted = false;
        std::string root;
        if (TryLoadProfile(id, profile, encrypted, root)) {
            gProfiles[index] = profile;
            ++gLoadStatus.exactProfiles;
            if (encrypted) ++gLoadStatus.encryptedProfiles;
            else ++gLoadStatus.plaintextProfiles;
            gLoadStatus.exactMask |= (std::uint64_t{1} << static_cast<unsigned>(id));
            if (firstRoot.empty()) firstRoot = root;
        } else {
            ++gLoadStatus.failedProfiles;
        }
    }
    // Canonicalize the one-byte boolean contract after all fallback/script
    // overlays.  The zy i386 run surfaced a non-zero byte value of 205; any
    // non-zero source representation means "quadratic", but stored state and
    // telemetry must be exactly 0/1.
    for (auto& profile : gProfiles) {
        profile.accuracyQuadratic = profile.accuracyQuadratic != 0 ? std::uint8_t{1} : std::uint8_t{0};
    }

    if (firstRoot.empty()) firstRoot = "fallback_only";
    std::snprintf(gLoadStatus.sourceRoot, sizeof(gLoadStatus.sourceRoot), "%s", firstRoot.c_str());
}

}  // namespace

const WeaponProfileLoadStatus& InitializeWeaponProfilesFromDisk() noexcept {
    std::call_once(gProfileInit, InitializeProfilesOnce);
    return gLoadStatus;
}

const WeaponProfileLoadStatus& WeaponProfileLoadState() noexcept {
    return InitializeWeaponProfilesFromDisk();
}

const WeaponRuntimeProfile& WeaponProfile(const build4100::CSWeaponID id) noexcept {
    InitializeWeaponProfilesFromDisk();
    if (!build4100::IsValidWeaponID(id)) return gProfiles[0];
    return gProfiles[static_cast<std::size_t>(id)];
}

bool WeaponRuntimeController::Configure(const std::size_t maxClients) {
    players_.assign(maxClients + 1, PlayerWeaponRuntime{}); return true;
}
void WeaponRuntimeController::Reset() noexcept {
    for (auto& p : players_) p = PlayerWeaponRuntime{};
}
bool WeaponRuntimeController::ResetClient(const std::size_t clientIndex) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size()) return false;
    players_[clientIndex] = PlayerWeaponRuntime{};
    return true;
}
TimedWeaponResult WeaponRuntimeController::Fire(const std::size_t clientIndex, const float now,
                                                CsGameState& game,
                                                const build4100::CSWeaponID activeWeapon) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size()) return TimedWeaponResult::Rejected;
    PlayerWeaponRuntime& state = players_[clientIndex];
    if (now < state.nextAttackTime) return TimedWeaponResult::Cooldown;
    if (game.FireActiveWeapon(clientIndex) != WeaponActionResult::Fired) return TimedWeaponResult::Rejected;
    const auto& profile = WeaponProfile(activeWeapon);
    if (state.lastWeapon != activeWeapon) {
        state.silenced = false;
        state.burstMode = false;
        state.zoomLevel = 0;
        state.accuracy = InitialBuild4100Accuracy(activeWeapon);
        state.shotAccuracy = state.accuracy;
        state.shotSpreadOverride = -1.0f;
        state.shotsFired = 0;
        state.lastFireTime = 0.0f;
        state.nextBurstShotTime = 0.0f;
        state.burstShotsRemaining = 0;
        state.burstShotOrdinal = 0;
    }
    state.lastWeapon = activeWeapon;
    state.nextAttackTime = now + profile.cycleTime;
    state.classicBallistics = UsesBuild4100ClassicBallistics(activeWeapon);
    state.shotSpreadOverride = -1.0f;
    if (state.classicBallistics) {
        if (HasBuild4100RecoveredPistolSniperStateModel(activeWeapon)) {
            // Pistol/autosniper *Fire owns m_flAccuracy and consumes the newly
            // computed value for this shot. AWP/Scout state spread ignores it.
            if (HasBuild4100RecoveredTimedAccuracy(activeWeapon) && state.shotsFired != 0) {
                state.accuracy = AdvanceBuild4100TimedAccuracy(
                    activeWeapon, state.accuracy, std::max(0.0f, now - state.lastFireTime));
            }
            state.shotAccuracy = state.accuracy;
            ++state.shotsFired;
            state.lastFireTime = now;
            if (activeWeapon == build4100::CSWeaponID::Glock && state.burstMode) {
                state.burstShotsRemaining = 2;
                state.burstShotOrdinal = 1;
                state.nextBurstShotTime = now + 0.100f;
            }
        } else {
            // Automatic PrimaryAttack consumes the pre-fire accuracy and then
            // CSBaseGunFire advances it for the next round.
            state.shotAccuracy = state.accuracy;
            ++state.shotsFired;
            state.accuracy = AdvanceBuild4100Accuracy(profile, state.shotsFired, state.accuracy);
            state.lastFireTime = now;
        }
        state.accuracyPenalty = std::max(0.0f, state.accuracy - 0.2f);
        state.recoilIndex = std::min(12.0f, static_cast<float>(state.shotsFired));
    } else {
        state.recoilIndex = std::min(12.0f, state.recoilIndex + profile.recoilStep);
        state.accuracyPenalty = std::min(1.0f, state.accuracyPenalty + profile.baseSpread * 2.0f);
        ++state.shotsFired;
    }
    return TimedWeaponResult::Fired;
}
TimedWeaponResult WeaponRuntimeController::Melee(const std::size_t clientIndex, const float now,
                                                 const build4100::CSWeaponID activeWeapon) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size() || activeWeapon != build4100::CSWeaponID::Knife) {
        return TimedWeaponResult::Rejected;
    }
    PlayerWeaponRuntime& state = players_[clientIndex];
    if (now < state.nextAttackTime) return TimedWeaponResult::Cooldown;
    state.lastWeapon = activeWeapon;
    state.nextAttackTime = now + 0.4f;
    state.recoilIndex = 0.0f;
    state.accuracyPenalty = 0.0f;
    ++state.shotsFired;
    return TimedWeaponResult::Fired;
}

SecondaryWeaponResult WeaponRuntimeController::SecondaryAttack(
    const std::size_t clientIndex, const float now,
    const build4100::CSWeaponID activeWeapon) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size() ||
        !build4100::IsValidWeaponID(activeWeapon)) {
        return SecondaryWeaponResult::Rejected;
    }

    using W = build4100::CSWeaponID;
    const bool supported = activeWeapon == W::M4A1 || activeWeapon == W::Usp ||
                           activeWeapon == W::Glock || activeWeapon == W::Famas ||
                           activeWeapon == W::Awp || activeWeapon == W::Scout ||
                           activeWeapon == W::G3Sg1 || activeWeapon == W::Sg550;
    if (!supported) return SecondaryWeaponResult::Unsupported;

    PlayerWeaponRuntime& state = players_[clientIndex];
    if (state.lastWeapon != activeWeapon) {
        state.silenced = false;
        state.burstMode = false;
        state.zoomLevel = 0;
        state.nextSecondaryAttackTime = 0.0f;
        state.lastWeapon = activeWeapon;
    }
    if (now < state.nextSecondaryAttackTime) return SecondaryWeaponResult::Cooldown;

    switch (activeWeapon) {
        case W::M4A1:
        case W::Usp:
            state.silenced = !state.silenced;
            state.nextSecondaryAttackTime = now + 3.0f;
            state.nextAttackTime = std::max(state.nextAttackTime, now + 3.0f);
            return SecondaryWeaponResult::Toggled;

        case W::Glock:
        case W::Famas:
            state.burstMode = !state.burstMode;
            state.nextSecondaryAttackTime = now + 0.3f;
            return SecondaryWeaponResult::Toggled;

        case W::Awp:
        case W::Scout:
        case W::G3Sg1:
        case W::Sg550:
            state.zoomLevel = static_cast<std::uint8_t>((state.zoomLevel + 1u) % 3u);
            state.nextSecondaryAttackTime = now + 0.3f;
            return SecondaryWeaponResult::Toggled;

        default:
            return SecondaryWeaponResult::Unsupported;
    }
}

bool WeaponRuntimeController::FireScheduledGlockBurst(
    const std::size_t clientIndex, const float now, CsGameState& game,
    const build4100::CSWeaponID activeWeapon) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size() ||
        activeWeapon != build4100::CSWeaponID::Glock) return false;
    PlayerWeaponRuntime& state = players_[clientIndex];
    if (!state.burstMode || state.burstShotsRemaining <= 0 ||
        state.nextBurstShotTime <= 0.0f || now + 0.0001f < state.nextBurstShotTime) return false;
    if (game.FireActiveWeapon(clientIndex) != WeaponActionResult::Fired) {
        state.burstShotsRemaining = 0;
        state.nextBurstShotTime = 0.0f;
        return false;
    }
    state.shotAccuracy = state.accuracy;
    // CWeaponGlock::FireRemaining passes Vector(0.05,0.05,0.05) directly to
    // FireBullets rather than recomputing PrimaryAttack movement spread.
    state.shotSpreadOverride = 0.050f;
    ++state.shotsFired;
    ++state.burstShotOrdinal;
    --state.burstShotsRemaining;
    state.lastFireTime = now;
    if (state.burstShotsRemaining > 0) state.nextBurstShotTime = now + 0.100f;
    else state.nextBurstShotTime = 0.0f;
    state.accuracyPenalty = std::max(0.0f, state.accuracy - 0.2f);
    state.recoilIndex = std::min(12.0f, static_cast<float>(state.shotsFired));
    return true;
}

WeaponActionResult WeaponRuntimeController::Reload(const std::size_t clientIndex, const float now,
                                                   CsGameState& game,
                                                   const build4100::CSWeaponID activeWeapon) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size()) return WeaponActionResult::InvalidPlayer;
    PlayerWeaponRuntime& state = players_[clientIndex];
    if (now < state.nextAttackTime) return WeaponActionResult::NotActive;
    const WeaponActionResult result = game.ReloadActiveWeapon(clientIndex);
    if (result == WeaponActionResult::Reloaded) {
        state.lastWeapon = activeWeapon;
        state.nextAttackTime = now + 2.5f;
        state.classicBallistics = UsesBuild4100ClassicBallistics(activeWeapon);
        if (state.classicBallistics) {
            // CWeaponCSBaseGun::Reload resets burst accuracy/shots to the
            // build4100 deployment baseline. View punch is player-owned.
            state.accuracy = InitialBuild4100Accuracy(activeWeapon);
            state.shotAccuracy = state.accuracy;
            state.shotSpreadOverride = -1.0f;
            state.lastFireTime = 0.0f;
            state.nextBurstShotTime = 0.0f;
            state.burstShotsRemaining = 0;
            state.burstShotOrdinal = 0;
            state.shotsFired = 0;
            state.accuracyPenalty = 0.0f;
            state.recoilIndex = 0.0f;
        } else {
            state.accuracyPenalty = std::max(0.0f, state.accuracyPenalty - 0.15f);
        }
    }
    return result;
}

bool WeaponRuntimeController::Switch(const std::size_t clientIndex, const float now,
                                     CsGameState& game, const build4100::CSWeaponID weapon) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size()) return false;
    if (!game.SelectWeapon(clientIndex, weapon)) return false;
    PlayerWeaponRuntime& state = players_[clientIndex];
    state.lastWeapon = weapon;
    state.nextAttackTime = now + 0.5f;
    state.nextSecondaryAttackTime = now + 0.5f;
    state.silenced = false;
    state.burstMode = false;
    state.zoomLevel = 0;
    state.accuracyPenalty = 0.0f;
    state.recoilIndex = 0.0f;
    state.classicBallistics = UsesBuild4100ClassicBallistics(weapon);
    state.accuracy = InitialBuild4100Accuracy(weapon);
    state.shotAccuracy = state.accuracy;
    state.shotSpreadOverride = -1.0f;
    state.shotsFired = 0;
    state.lastFireTime = 0.0f;
    state.nextBurstShotTime = 0.0f;
    state.burstShotsRemaining = 0;
    state.burstShotOrdinal = 0;
    return true;
}

void WeaponRuntimeController::ApplyPostFireKickBack(
    const std::size_t clientIndex, const build4100::CSWeaponID weapon,
    const WeaponMovementState& movement, const std::int32_t randomSeed) noexcept {
    if (clientIndex == 0 || clientIndex >= players_.size()) return;
    PlayerWeaponRuntime& state = players_[clientIndex];
    if (state.lastWeapon != weapon || state.shotsFired == 0) return;
    KickBackParameters params{};
    if (Build4100KickBackParameters(weapon, movement, params)) {
        state.punchAngle = ApplyBuild4100KickBack(
            state.punchAngle, state.shotsFired, state.kickDirection, params, randomSeed);
        return;
    }
    if (ApplyBuild4100ShotgunRecoil(weapon, movement.onGround, state.punchAngle, randomSeed)) {
        return;
    }
    // Pistol/sniper post-shot punch is part of the live exact path.
    (void)ApplyBuild4100PistolSniperRecoil(weapon, state.punchAngle, randomSeed);
}

void WeaponRuntimeController::Tick(const float frameTime) noexcept {
    const float recovery = std::max(0.0f, frameTime) * 0.35f;
    for (auto& p : players_) {
        // Punch is player-owned in build4100 and decays regardless of which
        // weapon family produced it. This is now relevant to the recovered
        // pistol/sniper impulses as well as the automatic KickBack path.
        DecayBuild4100Punch(p.punchAngle, p.punchAngleVelocity, frameTime);
        if (!p.classicBallistics) {
            p.accuracyPenalty = std::max(0.0f, p.accuracyPenalty - recovery);
            p.recoilIndex = std::max(0.0f, p.recoilIndex - recovery * 4.0f);
        }
    }
}
const PlayerWeaponRuntime* WeaponRuntimeController::State(const std::size_t clientIndex) const noexcept {
    return clientIndex < players_.size() ? &players_[clientIndex] : nullptr;
}
} // namespace cssv34::reconstructed
