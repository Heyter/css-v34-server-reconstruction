#include "server/weapon_ballistics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cssv34::reconstructed {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float ToRadians(const float degrees) noexcept {
    return degrees * (kPi / 180.0f);
}

class SourceUniformRandomStream final {
public:
    explicit SourceUniformRandomStream(const std::int32_t seed = 0) noexcept { SetSeed(seed); }

    void SetSeed(const std::int32_t seed) noexcept {
        idum_ = seed > 0 ? -static_cast<std::int64_t>(seed) : static_cast<std::int64_t>(seed);
        iy_ = 0;
    }

    float RandomFloat(const float low, const float high) noexcept {
        constexpr double kAm = 1.0 / 2147483647.0;
        constexpr double kRnmx = 1.0 - 1.2e-7;
        const double unit = std::min(kAm * static_cast<double>(GenerateRandomNumber()), kRnmx);
        return low + static_cast<float>(unit) * (high - low);
    }

    int RandomInt(const int low, const int high) noexcept {
        if (high <= low) return low;
        const std::uint32_t range = static_cast<std::uint32_t>(high - low) + 1u;
        const std::uint64_t generatorRange = 0x80000000ull;
        const std::uint32_t maxAcceptable = static_cast<std::uint32_t>(
            0x7fffffffull - (generatorRange % range));
        std::uint32_t value = 0;
        do {
            value = static_cast<std::uint32_t>(GenerateRandomNumber());
        } while (value > maxAcceptable);
        return low + static_cast<int>(value % range);
    }

private:
    std::int32_t GenerateRandomNumber() noexcept {
        constexpr std::int64_t kIa = 16807;
        constexpr std::int64_t kIm = 2147483647;
        constexpr std::int64_t kIq = 127773;
        constexpr std::int64_t kIr = 2836;
        constexpr int kNtab = 32;
        constexpr std::int64_t kNdiv = 1 + (kIm - 1) / kNtab;

        if (idum_ <= 0 || iy_ == 0) {
            idum_ = (-idum_ < 1) ? 1 : -idum_;
            for (int j = kNtab + 7; j >= 0; --j) {
                const std::int64_t k = idum_ / kIq;
                idum_ = kIa * (idum_ - k * kIq) - kIr * k;
                if (idum_ < 0) idum_ += kIm;
                if (j < kNtab) iv_[j] = static_cast<std::int32_t>(idum_);
            }
            iy_ = iv_[0];
        }
        const std::int64_t k = idum_ / kIq;
        idum_ = kIa * (idum_ - k * kIq) - kIr * k;
        if (idum_ < 0) idum_ += kIm;
        const int j = static_cast<int>(iy_ / kNdiv);
        iy_ = iv_[j];
        iv_[j] = static_cast<std::int32_t>(idum_);
        return iy_;
    }

    std::int64_t idum_ = 0;
    std::int32_t iy_ = 0;
    std::int32_t iv_[32]{};
};

std::uint32_t SourceCrc32Byte(std::uint32_t crc, const std::uint8_t byte) noexcept {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
        crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}

std::uint32_t BuildSharedRandomSeed(const std::int32_t predictionRandomSeed,
                                    const char* sharedName,
                                    const std::int32_t additionalSeed) noexcept {
    std::uint32_t crc = 0xffffffffu;
    const auto processInt32Le = [&crc](const std::int32_t value) noexcept {
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        crc = SourceCrc32Byte(crc, static_cast<std::uint8_t>(bits & 0xffu));
        crc = SourceCrc32Byte(crc, static_cast<std::uint8_t>((bits >> 8) & 0xffu));
        crc = SourceCrc32Byte(crc, static_cast<std::uint8_t>((bits >> 16) & 0xffu));
        crc = SourceCrc32Byte(crc, static_cast<std::uint8_t>((bits >> 24) & 0xffu));
    };
    processInt32Le(predictionRandomSeed);
    processInt32Le(additionalSeed);
    if (sharedName) {
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(sharedName); *p; ++p) {
            crc = SourceCrc32Byte(crc, *p);
        }
    }
    return crc ^ 0xffffffffu;
}

std::uint32_t MixSeed(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float SignedUnit(std::uint32_t& state) noexcept {
    state = MixSeed(state + 0x9e3779b9u);
    constexpr float kInv = 1.0f / 16777215.0f;
    const float unit = static_cast<float>((state >> 8) & 0x00ffffffu) * kInv;
    return unit * 2.0f - 1.0f;
}

build4100::Vector Normalize(const build4100::Vector& v) noexcept {
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (!(lengthSq > 0.000001f)) return build4100::Vector{1.0f, 0.0f, 0.0f};
    const float invLength = 1.0f / std::sqrt(lengthSq);
    return build4100::Vector{v.x * invLength, v.y * invLength, v.z * invLength};
}

WeaponShotSolution SolveFromAngles(const WeaponRuntimeProfile& profile,
                                   const build4100::QAngle& shotAngles,
                                   const float spread,
                                   const float recoilPitch,
                                   const float recoilYaw,
                                   const std::int32_t randomSeed,
                                   const std::int32_t commandNumber,
                                   const std::size_t shotIndex,
                                   const bool sourcePredictionRandom) noexcept {
    WeaponShotSolution out;
    out.range = std::max(1.0f, profile.range);
    out.spread = std::max(0.0f, spread);
    out.recoilPitch = recoilPitch;
    out.recoilYaw = recoilYaw;

    std::uint32_t seed = MixSeed(static_cast<std::uint32_t>(randomSeed) ^
                                 (static_cast<std::uint32_t>(commandNumber) * 0x45d9f3bu) ^
                                 (static_cast<std::uint32_t>(shotIndex) * 0x27d4eb2du));

    const float pitch = ToRadians(shotAngles.x);
    const float yaw = ToRadians(shotAngles.y);
    const float sp = std::sin(pitch);
    const float cp = std::cos(pitch);
    const float sy = std::sin(yaw);
    const float cy = std::cos(yaw);

    const build4100::Vector forward{cp * cy, cp * sy, -sp};
    const build4100::Vector right{-sy, cy, 0.0f};
    const build4100::Vector up{sp * cy, sp * sy, cp};

    if (sourcePredictionRandom) {
        // Source 2005 FX_FireBullets seeds CUniformRandomStream with
        // (predictionRandomSeed & 255) + 1 for the first pellet, then consumes
        // two RandomFloat pairs.  The pair sums are multiplied by flSpread.
        const Build4100SpreadSample sample = Build4100FxSpreadSample(randomSeed, shotIndex);
        out.spreadX = sample.x * out.spread;
        out.spreadY = sample.y * out.spread;
    } else {
        // Compatibility path for weapon families whose exact fire function is
        // not reconstructed yet.
        out.spreadX = 0.5f * (SignedUnit(seed) + SignedUnit(seed)) * out.spread;
        out.spreadY = 0.5f * (SignedUnit(seed) + SignedUnit(seed)) * out.spread;
    }

    out.direction = Normalize(build4100::Vector{
        forward.x + right.x * out.spreadX + up.x * out.spreadY,
        forward.y + right.y * out.spreadX + up.y * out.spreadY,
        forward.z + right.z * out.spreadX + up.z * out.spreadY,
    });
    return out;
}

KickBackParameters SelectKickBack(const KickBackParameters& air,
                                  const KickBackParameters& moving,
                                  const KickBackParameters& ducked,
                                  const KickBackParameters& standing,
                                  const WeaponMovementState& state) noexcept {
    if (!state.onGround) return air;
    if (state.speed2D > 5.0f) return moving;
    if (state.ducked) return ducked;
    return standing;
}

}  // namespace

Build4100SpreadSample Build4100FxSpreadSample(const std::int32_t predictionRandomSeed,
                                               const std::size_t pelletIndex) noexcept {
    const std::uint32_t base = (static_cast<std::uint32_t>(predictionRandomSeed) & 0xffu) + 1u;
    const std::uint32_t rawSeed = base + static_cast<std::uint32_t>(pelletIndex);
    SourceUniformRandomStream stream(static_cast<std::int32_t>(rawSeed));
    const float x = stream.RandomFloat(-0.5f, 0.5f) + stream.RandomFloat(-0.5f, 0.5f);
    const float y = stream.RandomFloat(-0.5f, 0.5f) + stream.RandomFloat(-0.5f, 0.5f);
    return Build4100SpreadSample{x, y, static_cast<std::int32_t>(rawSeed)};
}

int Build4100SharedRandomInt(const std::int32_t predictionRandomSeed,
                             const char* const sharedName,
                             const int minValue,
                             const int maxValue,
                             const int additionalSeed) noexcept {
    const std::uint32_t seedBits = BuildSharedRandomSeed(
        predictionRandomSeed, sharedName, static_cast<std::int32_t>(additionalSeed));
    SourceUniformRandomStream stream(static_cast<std::int32_t>(seedBits));
    return stream.RandomInt(minValue, maxValue);
}

float Build4100SharedRandomFloat(const std::int32_t predictionRandomSeed,
                                 const char* const sharedName,
                                 const float minValue,
                                 const float maxValue,
                                 const int additionalSeed) noexcept {
    const std::uint32_t seedBits = BuildSharedRandomSeed(
        predictionRandomSeed, sharedName, static_cast<std::int32_t>(additionalSeed));
    SourceUniformRandomStream stream(static_cast<std::int32_t>(seedBits));
    return stream.RandomFloat(minValue, maxValue);
}

bool UsesBuild4100ClassicBallistics(const build4100::CSWeaponID weapon) noexcept {
    using W = build4100::CSWeaponID;
    switch (weapon) {
        case W::Ak47:
        case W::M4A1:
        case W::M249:
        case W::Ump45:
        case W::Aug:
        case W::Galil:
        case W::Famas:
        case W::Mp5Navy:
        case W::Mac10:
        case W::Tmp:
        case W::P90:
        case W::Sg552:
        case W::P228:
        case W::Glock:
        case W::Elite:
        case W::FiveSeven:
        case W::Usp:
        case W::Deagle:
        case W::Awp:
        case W::Scout:
        case W::G3Sg1:
        case W::Sg550:
        case W::M3:
        case W::XM1014:
            return true;
        default:
            return false;
    }
}

bool HasBuild4100RecoveredPistolSniperStateModel(
    const build4100::CSWeaponID weapon) noexcept {
    using W = build4100::CSWeaponID;
    switch (weapon) {
        case W::P228:
        case W::Glock:
        case W::Elite:
        case W::FiveSeven:
        case W::Usp:
        case W::Deagle:
        case W::Awp:
        case W::Scout:
        case W::G3Sg1:
        case W::Sg550:
            return true;
        default:
            return false;
    }
}

bool HasBuild4100RecoveredTimedAccuracy(const build4100::CSWeaponID weapon) noexcept {
    using W = build4100::CSWeaponID;
    switch (weapon) {
        case W::P228:
        case W::Glock:
        case W::Elite:
        case W::FiveSeven:
        case W::Usp:
        case W::Deagle:
        case W::G3Sg1:
        case W::Sg550:
            return true;
        default:
            return false;
    }
}

bool HasBuild4100RecoveredPistolSniperRecoil(
    const build4100::CSWeaponID weapon) noexcept {
    using W = build4100::CSWeaponID;
    switch (weapon) {
        case W::P228:
        case W::Glock:
        case W::Elite:
        case W::FiveSeven:
        case W::Usp:
        case W::Deagle:
        case W::Awp:
        case W::Scout:
        case W::G3Sg1:
        case W::Sg550:
            return true;
        default:
            return false;
    }
}

bool HasBuild4100RecoveredShotgunRecoil(
    const build4100::CSWeaponID weapon) noexcept {
    return weapon == build4100::CSWeaponID::M3 ||
           weapon == build4100::CSWeaponID::XM1014;
}

float AdvanceBuild4100TimedAccuracy(const build4100::CSWeaponID weapon,
                                    const float currentAccuracy,
                                    const float timeSinceLastShot) noexcept {
    using W = build4100::CSWeaponID;
    const float dt = std::max(0.0f, timeSinceLastShot);
    const auto pistol = [currentAccuracy, dt](const float window,
                                             const float decay,
                                             const float minimum,
                                             const float maximum) noexcept {
        return std::clamp(currentAccuracy - (window - dt) * decay, minimum, maximum);
    };
    switch (weapon) {
        // Exact m_flAccuracy constants recovered from original build4100 *Fire.
        case W::P228:      // CWeaponP228::P228Fire @ 0x974430.
            return pistol(0.325f, 0.300f, 0.600f, 0.900f);
        case W::FiveSeven: // CWeaponFiveSeven::FiveSevenFire @ 0x9658f0.
            return pistol(0.275f, 0.250f, 0.725f, 0.920f);
        case W::Elite:     // CWeaponElite::ELITEFire @ 0x962a90.
            return pistol(0.325f, 0.275f, 0.550f, 0.880f);
        case W::Deagle:    // CDEagle::DEAGLEFire @ 0x9614f0.
            return pistol(0.400f, 0.350f, 0.550f, 0.900f);
        case W::Usp:       // CWeaponUSP::USPFire @ 0x97afe0.
            return pistol(0.300f, 0.275f, 0.600f, 0.920f);
        case W::Glock:     // CWeaponGlock::GlockFire @ 0x9696c0.
            return pistol(0.325f, 0.275f, 0.600f, 0.900f);
        case W::G3Sg1:     // CWeaponG3SG1::G3SG1Fire @ 0x967310.
            return std::min(0.980f, dt * 0.300f + 0.550f);
        case W::Sg550:     // CWeaponSG550::SG550Fire @ 0x976b00.
            return std::min(0.980f, dt * 0.350f + 0.650f);
        default:
            return currentAccuracy;
    }
}

float AdvanceBuild4100Accuracy(const WeaponRuntimeProfile& profile,
                               const std::size_t shotsFired,
                               const float currentAccuracy) noexcept {
    // Original CWeaponCSBaseGun::CSBaseGunFire compares AccuracyDivisor to -1.
    if (profile.accuracyDivisor == -1.0f || !(profile.accuracyDivisor > 0.0f)) {
        return currentAccuracy;
    }
    const float shots = static_cast<float>(shotsFired);
    const float power = AccuracyQuadraticFlag(profile) != 0
        ? shots * shots
        : shots * shots * shots;
    float accuracy = power / profile.accuracyDivisor + profile.accuracyOffset;
    if (profile.maxInaccuracy > 0.0f) accuracy = std::min(accuracy, profile.maxInaccuracy);
    return accuracy;
}

float Build4100StateSpread(const build4100::CSWeaponID weapon,
                           const WeaponRuntimeProfile& profile,
                           const float accuracy,
                           const WeaponMovementState& movement) noexcept {
    using W = build4100::CSWeaponID;
    const float a = std::max(0.0f, accuracy);
    switch (weapon) {
        // Pistols: m_flAccuracy is a goodness value, hence (1 - accuracy).
        case W::P228:      // CWeaponP228::PrimaryAttack @ 0x974f20.
        case W::FiveSeven: { // CWeaponFiveSeven::PrimaryAttack @ 0x9663e0.
            const float miss = std::clamp(1.0f - a, 0.0f, 1.0f);
            if (!movement.onGround) return 1.500f * miss;
            if (movement.speed2D > 5.0f) return 0.255f * miss;
            if (movement.ducked) return 0.075f * miss;
            return 0.150f * miss;
        }
        case W::Elite: {   // CWeaponElite::PrimaryAttack @ 0x9637c0.
            const float miss = std::clamp(1.0f - a, 0.0f, 1.0f);
            if (!movement.onGround) return 1.300f * miss;
            if (movement.speed2D > 5.0f) return 0.175f * miss;
            if (movement.ducked) return 0.080f * miss;
            return 0.100f * miss;
        }
        case W::Deagle: {  // CDEagle::PrimaryAttack @ 0x961e20.
            const float miss = std::clamp(1.0f - a, 0.0f, 1.0f);
            if (!movement.onGround) return 1.500f * miss;
            if (movement.speed2D > 5.0f) return 0.250f * miss;
            if (movement.ducked) return 0.115f * miss;
            return 0.130f * miss;
        }
        case W::Usp: {     // CWeaponUSP::PrimaryAttack @ 0x97bb70.
            const float miss = std::clamp(1.0f - a, 0.0f, 1.0f);
            if (movement.silenced) {
                if (!movement.onGround) return 1.300f * miss;
                if (movement.speed2D > 5.0f) return 0.250f * miss;
                if (movement.ducked) return 0.125f * miss;
                return 0.150f * miss;
            }
            if (!movement.onGround) return 1.200f * miss;
            if (movement.speed2D > 5.0f) return 0.225f * miss;
            if (movement.ducked) return 0.080f * miss;
            return 0.100f * miss;
        }
        case W::Glock: {   // CWeaponGlock::PrimaryAttack @ 0x96a200.
            const float miss = std::clamp(1.0f - a, 0.0f, 1.0f);
            if (movement.burstMode) {
                if (!movement.onGround) return 1.200f * miss;
                if (movement.speed2D > 5.0f) return 0.185f * miss;
                if (movement.ducked) return 0.095f * miss;
                return 0.300f * miss;
            }
            if (!movement.onGround) return 1.000f * miss;
            if (movement.speed2D > 5.0f) return 0.165f * miss;
            if (movement.ducked) return 0.075f * miss;
            return 0.100f * miss;
        }
        // Snipers: *Fire adds an unzoomed FOV penalty after PrimaryAttack.
        case W::Awp: {     // PrimaryAttack @ 0x94eea0 / AWPFire @ 0x94efd0.
            float spread = 0.0f;
            if (!movement.onGround) spread = 0.850f;
            else if (movement.speed2D > 140.0f) spread = 0.250f;
            else if (movement.speed2D > 10.0f) spread = 0.100f;
            else if (!movement.ducked) spread = 0.001f;
            return spread + (movement.zoomed ? 0.0f : 0.080f);
        }
        case W::Scout: {   // PrimaryAttack @ 0x976050 / SCOUTFire @ 0x975f10.
            float spread = 0.0f;
            if (!movement.onGround) spread = 0.200f;
            else if (movement.speed2D > 170.0f) spread = 0.075f;
            else if (!movement.ducked) spread = 0.007f;
            return spread + (movement.zoomed ? 0.0f : 0.025f);
        }
        case W::G3Sg1: {  // PrimaryAttack @ 0x9674c0 / G3SG1Fire @ 0x967310.
            const float miss = std::clamp(1.0f - a, 0.0f, 1.0f);
            float spread = 0.0f;
            if (!movement.onGround) spread = 0.450f * miss;
            else if (movement.speed2D > 5.0f) spread = 0.150f;
            else if (movement.ducked) spread = 0.035f * miss;
            else spread = 0.055f * miss;
            return spread + (movement.zoomed ? 0.0f : 0.025f);
        }
        case W::Sg550: {  // PrimaryAttack @ 0x976cb0 / SG550Fire @ 0x976b00.
            const float miss = std::clamp(1.0f - a, 0.0f, 1.0f);
            float spread = 0.0f;
            if (!movement.onGround) spread = 0.450f * miss;
            else if (movement.speed2D > 5.0f) spread = 0.150f;
            else if (movement.ducked) spread = 0.040f * miss;
            else spread = 0.050f * miss;
            return spread + (movement.zoomed ? 0.0f : 0.025f);
        }
        case W::M3:
            // CWeaponM3::PrimaryAttack @ 0x96fe40 passes 0.0675 directly
            // to FX_FireBullets, independent of movement/accuracy state.
            return 0.0675000027f;
        case W::XM1014:
            // CWeaponXM1014::PrimaryAttack @ 0x97d300 passes 0.0725.
            return 0.0724999979f;
        case W::Ak47:
            // CAK47::PrimaryAttack @ 0x94dc30.
            if (!movement.onGround) return 0.04f + 0.40f * a;
            if (movement.speed2D > 140.0f) return 0.04f + 0.07f * a;
            return 0.0275f * a;
        case W::M4A1:
            // CWeaponM4A1::PrimaryAttack @ 0x9713c0.
            if (!movement.onGround) return 0.035f + 0.40f * a;
            if (movement.speed2D > 140.0f) return 0.035f + 0.07f * a;
            return (movement.silenced ? 0.025f : 0.020f) * a;
        case W::M249:
            // CWeaponM249::PrimaryAttack @ 0x96e760.
            if (!movement.onGround) return 0.045f + 0.50f * a;
            if (movement.speed2D > 140.0f) return 0.045f + 0.095f * a;
            return 0.030f * a;
        case W::Ump45:
            // CWeaponUMP45::PrimaryAttack @ 0x979540.
            return (movement.onGround ? 0.040f : 0.240f) * a;
        case W::Aug:
            // CWeaponAug::PrimaryAttack @ 0x94e500.
            if (!movement.onGround) return 0.400000006f * a;
            if (movement.speed2D > 140.0f) return 0.035f + 0.070f * a;
            return 0.020f * a;
        case W::Galil:
            // CWeaponGalil::PrimaryAttack @ 0x968150.
            if (!movement.onGround) return 0.300000012f * a;
            if (movement.speed2D > 140.0f) return 0.040f + 0.070f * a;
            return 0.0375f * a;
        case W::Famas:
            // CWeaponFamas::PrimaryAttack @ 0x9648d0.
            if (!movement.onGround) return 0.300000012f * a;
            if (movement.speed2D > 140.0f) return 0.030f + 0.070f * a;
            return 0.020f * a;
        case W::Mp5Navy:
            // CWeaponMP5Navy::PrimaryAttack @ 0x973c90.
            return (movement.onGround ? 0.040f : 0.200f) * a;
        case W::Mac10:
            // CWeaponMAC10::PrimaryAttack @ 0x9733c0.
            return (movement.onGround ? 0.030f : 0.375f) * a;
        case W::Tmp:
            // CWeaponTMP::PrimaryAttack @ 0x978c50.
            return (movement.onGround ? 0.030f : 0.250f) * a;
        case W::P90:
            // CWeaponP90::PrimaryAttack @ 0x975810.
            if (!movement.onGround) return 0.300000012f * a;
            if (movement.speed2D > 170.0f) return 0.045f + 0.115f * a;
            return 0.045f * a;
        case W::Sg552:
            // CWeaponSG552::PrimaryAttack @ 0x977920.
            if (!movement.onGround) return 0.449999988f * a;
            if (movement.speed2D > 140.0f) return 0.035f + 0.075f * a;
            return 0.020f * a;
        default:
            return std::max(0.0f, profile.baseSpread +
                                      std::clamp(a, 0.0f, 1.0f) * profile.spreadPenaltyScale);
    }
}

bool Build4100KickBackParameters(const build4100::CSWeaponID weapon,
                                 const WeaponMovementState& movement,
                                 KickBackParameters& out) noexcept {
    using W = build4100::CSWeaponID;
    switch (weapon) {
        case W::Ak47:
            // CAK47::AK47Fire @ 0x94da90.
            out = SelectKickBack(
                {2.0f, 1.0f, 0.5f, 0.35f, 9.0f, 6.0f, 5},
                {1.5f, 0.45f, 0.225f, 0.05f, 6.5f, 2.5f, 7},
                {0.9f, 0.35f, 0.15f, 0.025f, 5.5f, 1.5f, 9},
                {1.0f, 0.375f, 0.175f, 0.0375f, 5.75f, 1.75f, 8}, movement);
            return true;
        case W::M4A1:
            // CWeaponM4A1::M4A1Fire @ 0x9711d0.
            out = SelectKickBack(
                {1.2f, 0.5f, 0.23f, 0.15f, 5.5f, 3.5f, 6},
                {1.0f, 0.45f, 0.28f, 0.045f, 3.75f, 3.0f, 7},
                {0.6f, 0.3f, 0.2f, 0.0125f, 3.25f, 2.0f, 7},
                {0.65f, 0.35f, 0.25f, 0.015f, 3.5f, 2.25f, 7}, movement);
            return true;
        case W::M249:
            // CWeaponM249::M249Fire @ 0x96e5b0.
            out = SelectKickBack(
                {1.8f, 0.65f, 0.45f, 0.125f, 5.0f, 3.5f, 8},
                {1.1f, 0.5f, 0.3f, 0.06f, 4.0f, 3.0f, 8},
                {0.75f, 0.325f, 0.25f, 0.025f, 3.5f, 2.5f, 9},
                {0.8f, 0.35f, 0.3f, 0.03f, 3.75f, 3.0f, 9}, movement);
            return true;
        case W::Ump45:
            // CWeaponUMP45::UMP45Fire @ 0x979390.
            out = SelectKickBack(
                {0.125f, 0.65f, 0.55f, 0.0475f, 5.5f, 4.0f, 10},
                {0.55f, 0.3f, 0.225f, 0.03f, 3.5f, 2.5f, 10},
                {0.25f, 0.175f, 0.125f, 0.02f, 2.25f, 1.25f, 10},
                {0.275f, 0.2f, 0.15f, 0.0225f, 2.5f, 1.5f, 10}, movement);
            return true;
        case W::Aug:
            // CWeaponAug::AUGFire @ 0x94e350.
            out = SelectKickBack(
                {1.25f, 0.45f, 0.22f, 0.18f, 5.5f, 4.0f, 5},
                {1.0f, 0.45f, 0.275f, 0.05f, 4.0f, 2.5f, 7},
                {0.575f, 0.325f, 0.2f, 0.011f, 3.25f, 2.0f, 8},
                {0.625f, 0.375f, 0.25f, 0.0125f, 3.5f, 2.25f, 8}, movement);
            return true;
        case W::Galil:
            // CWeaponGalil::GalilFire @ 0x967fb0.
            out = SelectKickBack(
                {1.2f, 0.5f, 0.23f, 0.15f, 5.5f, 3.5f, 6},
                {1.0f, 0.45f, 0.28f, 0.045f, 3.75f, 3.0f, 7},
                {0.6f, 0.3f, 0.2f, 0.0125f, 3.25f, 2.0f, 7},
                {0.65f, 0.35f, 0.25f, 0.015f, 3.5f, 2.25f, 7}, movement);
            return true;
        case W::Famas:
            // CWeaponFamas::FamasFire @ 0x963f00.
            out = SelectKickBack(
                {1.25f, 0.45f, 0.22f, 0.18f, 5.5f, 4.0f, 5},
                {1.0f, 0.45f, 0.275f, 0.05f, 4.0f, 2.5f, 7},
                {0.575f, 0.325f, 0.2f, 0.011f, 3.25f, 2.0f, 8},
                {0.625f, 0.375f, 0.25f, 0.0125f, 3.5f, 2.25f, 8}, movement);
            return true;
        case W::Mp5Navy:
            // CWeaponMP5Navy::MP5NFire @ 0x973ae0.
            out = SelectKickBack(
                {0.9f, 0.475f, 0.35f, 0.0425f, 5.0f, 3.0f, 6},
                {0.5f, 0.275f, 0.2f, 0.03f, 3.0f, 2.0f, 10},
                {0.225f, 0.15f, 0.1f, 0.015f, 2.0f, 1.0f, 10},
                {0.25f, 0.175f, 0.125f, 0.02f, 2.25f, 1.25f, 10}, movement);
            return true;
        case W::Mac10:
            // CWeaponMAC10::MAC10Fire @ 0x973210.
            out = SelectKickBack(
                {1.3f, 0.55f, 0.4f, 0.05f, 4.75f, 3.75f, 5},
                {0.9f, 0.45f, 0.25f, 0.035f, 3.5f, 2.75f, 7},
                {0.75f, 0.4f, 0.175f, 0.03f, 2.75f, 2.5f, 10},
                {0.775f, 0.425f, 0.2f, 0.03f, 3.0f, 2.75f, 9}, movement);
            return true;
        case W::Tmp:
            // CWeaponTMP::TMPFire @ 0x978aa0.
            out = SelectKickBack(
                {1.1f, 0.5f, 0.35f, 0.045f, 4.5f, 3.5f, 6},
                {0.8f, 0.4f, 0.2f, 0.03f, 3.0f, 2.5f, 7},
                {0.7f, 0.35f, 0.125f, 0.025f, 2.5f, 2.0f, 10},
                {0.725f, 0.375f, 0.15f, 0.025f, 2.75f, 2.25f, 9}, movement);
            return true;
        case W::P90:
            // CWeaponP90::P90Fire @ 0x975660.
            out = SelectKickBack(
                {0.9f, 0.45f, 0.35f, 0.04f, 5.25f, 3.5f, 4},
                {0.45f, 0.3f, 0.2f, 0.0275f, 4.0f, 2.25f, 7},
                {0.275f, 0.2f, 0.125f, 0.02f, 3.0f, 1.0f, 9},
                {0.3f, 0.225f, 0.125f, 0.02f, 3.25f, 1.25f, 8}, movement);
            return true;
        case W::Sg552:
            // CWeaponSG552::SG552Fire @ 0x977770.
            out = SelectKickBack(
                {1.25f, 0.45f, 0.22f, 0.18f, 6.0f, 4.0f, 5},
                {1.0f, 0.45f, 0.28f, 0.04f, 4.25f, 2.5f, 7},
                {0.6f, 0.35f, 0.2f, 0.0125f, 3.7f, 2.0f, 10},
                {0.625f, 0.375f, 0.25f, 0.0125f, 4.0f, 2.25f, 9}, movement);
            return true;
        default:
            out = KickBackParameters{};
            return false;
    }
}

build4100::QAngle ApplyBuild4100KickBack(const build4100::QAngle& punch,
                                         const std::size_t shotsFired,
                                         int& kickDirection,
                                         const KickBackParameters& params,
                                         const std::int32_t predictionRandomSeed) noexcept {
    build4100::QAngle out = punch;
    if (shotsFired == 0) return out;

    const float shots = static_cast<float>(shotsFired);
    const float up = shotsFired == 1 ? params.upBase
                                     : params.upBase + shots * params.upModifier;
    const float lateral = shotsFired == 1 ? params.lateralBase
                                          : params.lateralBase + shots * params.lateralModifier;

    out.x = std::max(-params.upMax, out.x - up);
    if (kickDirection == 1) out.y = std::min(params.lateralMax, out.y + lateral);
    else out.y = std::max(-params.lateralMax, out.y - lateral);

    if (params.directionChange > 0 &&
        Build4100SharedRandomInt(predictionRandomSeed, "KickBack", 0,
                                 params.directionChange, 0) == 0) {
        kickDirection = 1 - kickDirection;
    }
    return out;
}

bool ApplyBuild4100PistolSniperRecoil(const build4100::CSWeaponID weapon,
                                        build4100::QAngle& punch,
                                        const std::int32_t predictionRandomSeed) noexcept {
    using W = build4100::CSWeaponID;
    switch (weapon) {
        // The original build4100 fire routines for these seven families use
        // CBasePlayer::GetPunchAngle/SetPunchAngle and subtract exactly 2.0
        // degrees from pitch after the shot.
        case W::P228:
        case W::Elite:
        case W::FiveSeven:
        case W::Usp:
        case W::Deagle:
        case W::Awp:
        case W::Scout:
            punch.x -= 2.0f;
            return true;

        // CWeaponGlock::GlockFire / FireRemaining do not write punch angle in
        // the original server binary. Keep this as an explicit recovered
        // no-op rather than inheriting the generic recoil approximation.
        case W::Glock:
            return true;

        // Autosniper fire uses SharedRandomFloat with named streams. The
        // pre-existing pitch punch is damped by 0.75, then the recovered
        // random pitch is subtracted and the random yaw is accumulated.
        case W::G3Sg1: {
            const float pitch = Build4100SharedRandomFloat(
                predictionRandomSeed, "G3SG1PunchAngleX", 0.75f, 1.75f, 0);
            const float yaw = Build4100SharedRandomFloat(
                predictionRandomSeed, "G3SG1PunchAngleY", -0.75f, 0.75f, 0);
            punch.x = punch.x * 0.75f - pitch;
            punch.y += yaw;
            return true;
        }
        case W::Sg550: {
            const float pitch = Build4100SharedRandomFloat(
                predictionRandomSeed, "SG550PunchAngleX", 0.75f, 1.25f, 0);
            const float yaw = Build4100SharedRandomFloat(
                predictionRandomSeed, "SG550PunchAngleY", -0.75f, 0.75f, 0);
            punch.x = punch.x * 0.75f - pitch;
            punch.y += yaw;
            return true;
        }
        default:
            return false;
    }
}

bool ApplyBuild4100ShotgunRecoil(const build4100::CSWeaponID weapon,
                                 const bool onGround,
                                 build4100::QAngle& punch,
                                 const std::int32_t predictionRandomSeed) noexcept {
    using W = build4100::CSWeaponID;
    int impulse = 0;
    switch (weapon) {
        case W::M3:
            impulse = onGround
                ? Build4100SharedRandomInt(predictionRandomSeed, "M3PunchAngleGround", 4, 6, 0)
                : Build4100SharedRandomInt(predictionRandomSeed, "M3PunchAngleAir", 8, 11, 0);
            break;
        case W::XM1014:
            impulse = onGround
                ? Build4100SharedRandomInt(predictionRandomSeed, "XM1014PunchAngleGround", 3, 5, 0)
                : Build4100SharedRandomInt(predictionRandomSeed, "XM1014PunchAngleAir", 7, 10, 0);
            break;
        default:
            return false;
    }
    punch.x -= static_cast<float>(impulse);
    return true;
}

void DecayBuild4100Punch(build4100::QAngle& punch,
                         build4100::QAngle& punchVelocity,
                         const float frameTime) noexcept {
    const float punchLengthSq = punch.x * punch.x + punch.y * punch.y + punch.z * punch.z;
    const float velocityLengthSq = punchVelocity.x * punchVelocity.x +
                                   punchVelocity.y * punchVelocity.y +
                                   punchVelocity.z * punchVelocity.z;
    if (!(punchLengthSq > 0.001f || velocityLengthSq > 0.001f)) {
        // Build 4100 leaves sub-threshold residual punch/velocity untouched.
        return;
    }

    const float dt = std::max(0.0f, frameTime);
    punch.x += punchVelocity.x * dt;
    punch.y += punchVelocity.y * dt;
    punch.z += punchVelocity.z * dt;

    const float damping = std::max(0.0f, 1.0f - 9.0f * dt);
    punchVelocity.x *= damping;
    punchVelocity.y *= damping;
    punchVelocity.z *= damping;

    const float spring = std::clamp(65.0f * dt, 0.0f, 2.0f);
    punchVelocity.x -= punch.x * spring;
    punchVelocity.y -= punch.y * spring;
    punchVelocity.z -= punch.z * spring;

    punch.x = std::clamp(punch.x, -89.0f, 89.0f);
    punch.y = std::clamp(punch.y, -179.0f, 179.0f);
    punch.z = std::clamp(punch.z, -89.0f, 89.0f);
}

WeaponShotSolution SolveBuild4100WeaponPellet(const build4100::CSWeaponID weapon,
                                              const WeaponRuntimeProfile& profile,
                                              const build4100::QAngle& viewAngles,
                                              const WeaponMovementState& movement,
                                              const float shotAccuracy,
                                              const build4100::QAngle& punchAngle,
                                              const std::int32_t randomSeed,
                                              const std::size_t pelletIndex,
                                              const float spreadOverride) noexcept {
    const float spread = spreadOverride >= 0.0f
        ? spreadOverride
        : Build4100StateSpread(weapon, profile, shotAccuracy, movement);
    const float recoilPitch = punchAngle.x * 2.0f;
    const float recoilYaw = punchAngle.y * 2.0f;
    const build4100::QAngle shotAngles{
        viewAngles.x + recoilPitch,
        viewAngles.y + recoilYaw,
        viewAngles.z + punchAngle.z * 2.0f,
    };
    return SolveFromAngles(profile, shotAngles, spread, recoilPitch, recoilYaw,
                           randomSeed, 0, pelletIndex, true);
}

WeaponShotSolution SolveBuild4100WeaponShot(const build4100::CSWeaponID weapon,
                                            const WeaponRuntimeProfile& profile,
                                            const build4100::QAngle& viewAngles,
                                            const WeaponMovementState& movement,
                                            const float shotAccuracy,
                                            const build4100::QAngle& punchAngle,
                                            const std::int32_t randomSeed,
                                            const std::int32_t commandNumber,
                                            const std::size_t shotIndex,
                                            const float spreadOverride) noexcept {
    (void)commandNumber;  // FX_FireBullets uses prediction random seed, not command number.
    (void)shotIndex;      // Shot ordinal is distinct from the per-shot pellet index.
    return SolveBuild4100WeaponPellet(weapon, profile, viewAngles, movement, shotAccuracy,
                                      punchAngle, randomSeed, 0u, spreadOverride);
}

WeaponShotSolution SolveWeaponShot(const WeaponRuntimeProfile& profile,
                                   const build4100::QAngle& viewAngles,
                                   const float accuracyPenalty,
                                   const float recoilIndex,
                                   const std::int32_t randomSeed,
                                   const std::int32_t commandNumber,
                                   const std::size_t shotIndex) noexcept {
    const float clampedPenalty = std::clamp(accuracyPenalty, 0.0f, 1.0f);
    const float clampedRecoil = std::clamp(recoilIndex, 0.0f, 12.0f);
    const float spread = std::max(0.0f, profile.baseSpread +
                                       clampedPenalty * profile.spreadPenaltyScale);

    std::uint32_t seed = MixSeed(static_cast<std::uint32_t>(randomSeed) ^
                                 (static_cast<std::uint32_t>(commandNumber) * 0x45d9f3bu) ^
                                 (static_cast<std::uint32_t>(shotIndex) * 0x27d4eb2du));
    const float recoilPitch = clampedRecoil * profile.recoilPitchPerIndex;
    const float recoilYaw = SignedUnit(seed) * clampedRecoil * profile.recoilYawPerIndex;
    const build4100::QAngle shotAngles{viewAngles.x + recoilPitch,
                                       viewAngles.y + recoilYaw,
                                       viewAngles.z};
    return SolveFromAngles(profile, shotAngles, spread, recoilPitch, recoilYaw,
                           randomSeed, commandNumber, shotIndex, false);
}

float BallisticDamageAtDistance(const WeaponRuntimeProfile& profile,
                                const float distance) noexcept {
    if (!(profile.baseDamage > 0.0f)) return 0.0f;
    const float clampedDistance = std::clamp(distance, 0.0f, std::max(1.0f, profile.range));
    const float modifier = std::clamp(profile.rangeModifier, 0.01f, 1.0f);
    return profile.baseDamage * std::pow(modifier, clampedDistance / 500.0f);
}

}  // namespace cssv34::reconstructed
