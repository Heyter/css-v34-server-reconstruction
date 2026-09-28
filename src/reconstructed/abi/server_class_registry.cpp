#include "abi/server_class_registry.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

namespace cssv34::reconstructed::abi {
namespace {

struct ClassDefinition {
    const char* networkName;
    const char* tableName;
    const char* cppType;
};

ClientRegistry* gGameplayClients = nullptr;
CsGameState* gGameplayGameState = nullptr;
int gGameplayPlantedC4EntityIndex = -1;
constexpr ClassDefinition kDefinitions[] = {
#include "abi/serverclass_build4100.inc"
};
static_assert(std::size(kDefinitions) == 177, "build4100 ServerClass root count drift");

#include "abi/sendprop_build4100.inc"

struct DataTableLinkDescriptor {
    const char* ownerTable;
    const char* propName;
    const char* targetTable;
};

#include "abi/sendtable_links_build4100.inc"

struct FixedArrayDescriptor {
    const char* ownerTable;
    const char* propName;
    std::int32_t stride;
    std::int32_t elementCount;
    OriginalSendPropKind elementKind;
    std::int32_t elementBits;
    std::int32_t elementFlags;
};

#include "abi/sendarray_build4100.inc"

struct UtlVectorDescriptor {
    const char* ownerTable;
    const char* propName;
    std::int32_t vectorOffset;
    std::int32_t elementStride;
    std::int32_t maxElements;
    OriginalSendPropKind elementKind;
    std::int32_t elementBits;
    std::int32_t elementFlags;
    const char* lengthTableName;
    const char* mainTableName;
    const char* lengthPropName;
    const char* elementTableName;
};

#include "abi/sendutlvector_build4100.inc"

const UtlVectorDescriptor* FindUtlVectorDescriptor(const char* ownerTable, const char* propName) noexcept {
    if (!ownerTable || !propName) return nullptr;
    for (const auto& descriptor : kUtlVectorDescriptors) {
        if (std::strcmp(descriptor.ownerTable, ownerTable) == 0 &&
            std::strcmp(descriptor.propName, propName) == 0) {
            return &descriptor;
        }
    }
    return nullptr;
}

const FixedArrayDescriptor* FindFixedArrayDescriptor(const char* ownerTable, const char* propName) noexcept {
    if (!ownerTable || !propName) return nullptr;
    for (const auto& descriptor : kFixedArrayDescriptors) {
        if (std::strcmp(descriptor.ownerTable, ownerTable) == 0 &&
            std::strcmp(descriptor.propName, propName) == 0) {
            return &descriptor;
        }
    }
    return nullptr;
}

const char* ArrayElementName(const std::size_t index) noexcept {
    static const auto names = [] {
        std::array<std::array<char, 4>, 256> value{};
        for (std::size_t i = 0; i < value.size(); ++i) {
            value[i][0] = static_cast<char>('0' + ((i / 100) % 10));
            value[i][1] = static_cast<char>('0' + ((i / 10) % 10));
            value[i][2] = static_cast<char>('0' + (i % 10));
            value[i][3] = '\0';
        }
        return value;
    }();
    return index < names.size() ? names[index].data() : nullptr;
}

constexpr bool IsLiveKind(const OriginalSendPropKind) noexcept {
    return true;
}

build4100::SendPropType LiveType(const OriginalSendPropKind kind) noexcept {
    switch (kind) {
        case OriginalSendPropKind::Time:
        case OriginalSendPropKind::Angle:
        case OriginalSendPropKind::Float:
            return build4100::SendPropType::Float;
        case OriginalSendPropKind::Vector:
        case OriginalSendPropKind::QAngles:
            return build4100::SendPropType::Vector;
        case OriginalSendPropKind::String:
            return build4100::SendPropType::String;
        case OriginalSendPropKind::InternalArray:
            return build4100::SendPropType::Array;
        case OriginalSendPropKind::Array3:
        case OriginalSendPropKind::DataTable:
        case OriginalSendPropKind::UtlVector:
            return build4100::SendPropType::DataTable;
        default:
            return build4100::SendPropType::Int;
    }
}

int LiveBits(const OriginalSendPropDescriptor& descriptor) noexcept {
    // EHandles are always encoded as the build4100 networked handle width.
    // One original helper call (m_hGroundEntity) passes SPROP_CHANGES_OFTEN
    // through the parameter slot our static oracle records as `bits`; normalize
    // that helper-specific call shape before exposing the live SendProp.
    if (descriptor.kind == OriginalSendPropKind::EHandle) return 21;
    if (descriptor.bits >= 0) return descriptor.bits;
    switch (descriptor.kind) {
        case OriginalSendPropKind::Bool: return 1;
        case OriginalSendPropKind::StringT: return 32;
        case OriginalSendPropKind::PredictableId: return 32;
        default: return 32;
    }
}

int LiveFlags(const OriginalSendPropDescriptor& descriptor) noexcept {
    if (descriptor.kind == OriginalSendPropKind::Exclude) return 0x40;
    if (descriptor.kind == OriginalSendPropKind::EHandle && descriptor.bits > 32) {
        return descriptor.bits;  // helper-specific flags slot; currently 0x400.
    }
    // SendPropBool is a helper around a one-bit *unsigned* DPT_Int.  The
    // static constructor oracle records no explicit flags argument for that
    // helper, so descriptor.flags is -1.  Treating it as flags=0 creates a
    // signed one-bit integer whose representable positive range ends at zero;
    // true then quantizes to the same wire value as false and disappears from
    // entity deltas (observed on CPlantedC4::m_bBombTicking in build4100).
    if (descriptor.kind == OriginalSendPropKind::Bool) {
        return build4100::kSendPropUnsigned |
               (descriptor.flags >= 0 ? descriptor.flags : 0);
    }
    return descriptor.flags >= 0 ? descriptor.flags : 0;
}

void ProxyZeroInt(const void*, const void*, const void*, void* out, int, int) {
    if (!out) return;
    const std::int32_t value = 0;
    std::memcpy(out, &value, sizeof(value));
}

const ClientRecord* GameplayClient(const int objectId) noexcept {
    if (!gGameplayClients || objectId <= 0) return nullptr;
    return gGameplayClients->Get(static_cast<std::size_t>(objectId));
}

void WriteProxyInt(void* const out, const std::int32_t value) noexcept {
    if (out) std::memcpy(out, &value, sizeof(value));
}

void WriteProxyFloat(void* const out, const float value) noexcept {
    if (out) std::memcpy(out, &value, sizeof(value));
}

void WriteProxyVector(void* const out, const build4100::Vector& value) noexcept {
    if (out) std::memcpy(out, &value, sizeof(value));
}

float NormalizeNetworkAngle(float value) noexcept {
    while (value < 0.0f) value += 360.0f;
    while (value >= 360.0f) value -= 360.0f;
    return value;
}

float ScaledFloatHighLowMul(const int bits, const float low, const float high) noexcept {
    if (bits <= 0 || bits >= 32 || !(high > low)) return 0.0f;
    const std::uint32_t maxCode = (std::uint32_t{1} << static_cast<unsigned>(bits)) - 1u;
    return static_cast<float>(maxCode) / (high - low);
}

void ProxyGameplayAccount(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client ? client->account : 0);
}

void ProxyGameplayHealth(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client ? client->health : 0);
}

void ProxyGameplayFov(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client ? client->currentFov : 0);
}

void ProxyGameplayDefaultFov(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client ? client->defaultFov : 90);
}

void ProxyGameplayTeam(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client ? client->teamIndex : 0);
}

void ProxyGameplayPlayerState(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client ? static_cast<std::int32_t>(client->csState) : 0);
}

void ProxyGameplayArmor(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client ? client->armorValue : 0);
}

void ProxyGameplayBuyZone(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client && client->inBuyZone ? 1 : 0);
}

void ProxyGameplayDefuser(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client && client->hasDefuser ? 1 : 0);
}

void ProxyGameplayHelmet(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyInt(out, client && client->hasHelmet ? 1 : 0);
}

void ProxyGameplayEyePitch(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyFloat(out, client ? NormalizeNetworkAngle(client->absAngles.x) : 0.0f);
}

void ProxyGameplayEyeYaw(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyFloat(out, client ? NormalizeNetworkAngle(client->absAngles.y) : 0.0f);
}

void ProxyGameplayOrigin(const void*, const void*, const void*, void* out, int, const int objectId) {
    if (gGameplayGameState && objectId == gGameplayPlantedC4EntityIndex) {
        WriteProxyVector(out, gGameplayGameState->Bomb().PlantOrigin());
        return;
    }
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyVector(out, client ? client->absOrigin : build4100::Vector{});
}

void ProxyBombTicking(const void*, const void*, const void*, void* out, int, const int objectId) {
    const bool planted = gGameplayGameState && objectId == gGameplayPlantedC4EntityIndex &&
        gGameplayGameState->Bomb().State() == BombObjectiveState::Planted;
    WriteProxyInt(out, planted ? 1 : 0);
}

void ProxyBombBlowTime(const void*, const void*, const void*, void* out, int, const int objectId) {
    WriteProxyFloat(out, gGameplayGameState && objectId == gGameplayPlantedC4EntityIndex
        ? gGameplayGameState->Bomb().ExplodeTime() : 0.0f);
}

void ProxyBombTimerLength(const void*, const void*, const void*, void* out, int, const int objectId) {
    WriteProxyFloat(out, gGameplayGameState && objectId == gGameplayPlantedC4EntityIndex
        ? gGameplayGameState->Bomb().TimerSeconds() : 0.0f);
}

void ProxyBombDefuseLength(const void*, const void*, const void*, void* out, int, const int objectId) {
    WriteProxyFloat(out, gGameplayGameState && objectId == gGameplayPlantedC4EntityIndex
        ? gGameplayGameState->Bomb().DefuseLength() : 0.0f);
}

void ProxyBombDefuseCountdown(const void*, const void*, const void*, void* out, int, const int objectId) {
    WriteProxyFloat(out, gGameplayGameState && objectId == gGameplayPlantedC4EntityIndex
        ? gGameplayGameState->Bomb().DefuseCompleteTime() : 0.0f);
}

void ProxyGameplayVelocityX(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyFloat(out, client ? client->absVelocity.x : 0.0f);
}

void ProxyGameplayVelocityY(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyFloat(out, client ? client->absVelocity.y : 0.0f);
}

void ProxyGameplayVelocityZ(const void*, const void*, const void*, void* out, int, const int objectId) {
    const ClientRecord* const client = GameplayClient(objectId);
    WriteProxyFloat(out, client ? client->absVelocity.z : 0.0f);
}

build4100::SendVarProxyFn GameplayProxy(const char* const tableName,
                                        const char* const propName) noexcept {
    if (!tableName || !propName) return nullptr;
    if (std::strcmp(tableName, "DT_BaseEntity") == 0) {
        if (std::strcmp(propName, "m_iTeamNum") == 0) return &ProxyGameplayTeam;
        if (std::strcmp(propName, "m_vecOrigin") == 0) return &ProxyGameplayOrigin;
    }
    // CCSPlayer excludes DT_BaseEntity::m_vecOrigin and re-sends origin through
    // local/non-local exclusive tables. Bind both copies to the gameplay state;
    // the datatable recipient proxies below decide which copy reaches a client.
    if ((std::strcmp(tableName, "DT_CSLocalPlayerExclusive") == 0 ||
         std::strcmp(tableName, "DT_CSNonLocalPlayerExclusive") == 0) &&
        std::strcmp(propName, "m_vecOrigin") == 0) {
        return &ProxyGameplayOrigin;
    }
    if (std::strcmp(tableName, "DT_BasePlayer") == 0) {
        if (std::strcmp(propName, "m_iHealth") == 0) return &ProxyGameplayHealth;
        if (std::strcmp(propName, "m_iFOV") == 0) return &ProxyGameplayFov;
        if (std::strcmp(propName, "m_iDefaultFOV") == 0) return &ProxyGameplayDefaultFov;
    }
    if (std::strcmp(tableName, "DT_PlantedC4") == 0) {
        if (std::strcmp(propName, "m_bBombTicking") == 0) return &ProxyBombTicking;
        if (std::strcmp(propName, "m_flC4Blow") == 0) return &ProxyBombBlowTime;
        if (std::strcmp(propName, "m_flTimerLength") == 0) return &ProxyBombTimerLength;
        if (std::strcmp(propName, "m_flDefuseLength") == 0) return &ProxyBombDefuseLength;
        if (std::strcmp(propName, "m_flDefuseCountDown") == 0) return &ProxyBombDefuseCountdown;
        return nullptr;
    }
    if (std::strcmp(tableName, "DT_LocalPlayerExclusive") == 0) {
        if (std::strcmp(propName, "m_vecVelocity[0]") == 0) return &ProxyGameplayVelocityX;
        if (std::strcmp(propName, "m_vecVelocity[1]") == 0) return &ProxyGameplayVelocityY;
        if (std::strcmp(propName, "m_vecVelocity[2]") == 0) return &ProxyGameplayVelocityZ;
        return nullptr;
    }
    if (std::strcmp(tableName, "DT_CSPlayer") != 0) return nullptr;
    if (std::strcmp(propName, "m_iAccount") == 0) return &ProxyGameplayAccount;
    if (std::strcmp(propName, "m_iPlayerState") == 0) return &ProxyGameplayPlayerState;
    if (std::strcmp(propName, "m_ArmorValue") == 0) return &ProxyGameplayArmor;
    if (std::strcmp(propName, "m_bInBuyZone") == 0) return &ProxyGameplayBuyZone;
    if (std::strcmp(propName, "m_bHasDefuser") == 0) return &ProxyGameplayDefuser;
    if (std::strcmp(propName, "m_bHasHelmet") == 0) return &ProxyGameplayHelmet;
    if (std::strcmp(propName, "m_angEyeAngles[0]") == 0) return &ProxyGameplayEyePitch;
    if (std::strcmp(propName, "m_angEyeAngles[1]") == 0) return &ProxyGameplayEyeYaw;
    return nullptr;
}

void ProxyZeroFloat(const void*, const void*, const void*, void* out, int, int) {
    if (!out) return;
    const float value = 0.0f;
    std::memcpy(out, &value, sizeof(value));
}

void ProxyZeroVector(const void*, const void*, const void*, void* out, int, int) {
    if (!out) return;
    const float value[3] = {0.0f, 0.0f, 0.0f};
    std::memcpy(out, value, sizeof(value));
}

void ProxyEmptyString(const void*, const void*, const void*, void* out, int, int) {
    if (!out) return;
    static const char kEmpty[] = "";
    const char* value = kEmpty;
    std::memcpy(out, &value, sizeof(value));
}

void ProxyInt8(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    const std::int32_t value = *static_cast<const std::int8_t*>(data);
    std::memcpy(out, &value, sizeof(value));
}
void ProxyInt16(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    const std::int32_t value = *static_cast<const std::int16_t*>(data);
    std::memcpy(out, &value, sizeof(value));
}
void ProxyInt32(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    std::memcpy(out, data, sizeof(std::int32_t));
}
void ProxyUInt8(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    const std::int32_t value = *static_cast<const std::uint8_t*>(data);
    std::memcpy(out, &value, sizeof(value));
}
void ProxyUInt16(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    const std::int32_t value = *static_cast<const std::uint16_t*>(data);
    std::memcpy(out, &value, sizeof(value));
}
void ProxyUInt32(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    std::memcpy(out, data, sizeof(std::uint32_t));
}
void ProxyFloat(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    std::memcpy(out, data, sizeof(float));
}
void ProxyVector(const void*, const void*, const void* data, void* out, int, int) {
    if (!data || !out) return;
    std::memcpy(out, data, sizeof(float) * 3);
}

// v1.8.35g: DT_TEFireBullets is not an entity snapshot backed by the gameplay
// registry. VEngineServer021::PlaybackTempEntity serializes a short-lived
// CTEFireBullets-compatible data object directly, so these properties must
// read the sender memory instead of the safe-zero fallback used by otherwise
// unmaterialized network classes.
build4100::SendVarProxyFn TempEntityProxy(const char* const tableName,
                                          const OriginalSendPropKind kind) noexcept {
    if (!tableName || std::strcmp(tableName, "DT_TEFireBullets") != 0) return nullptr;
    switch (kind) {
        case OriginalSendPropKind::Vector: return &ProxyVector;
        case OriginalSendPropKind::Angle:
        case OriginalSendPropKind::Float: return &ProxyFloat;
        case OriginalSendPropKind::Int: return &ProxyInt32;
        default: return nullptr;
    }
}

int LiveOffset(const char* const tableName, const OriginalSendPropDescriptor& descriptor) noexcept {
    if (tableName && std::strcmp(tableName, "DT_TEFireBullets") == 0) {
        // The static constructor oracle preserves the helper's -offsetof form
        // for SendPropAngle. The live CTEFireBullets object itself stores the
        // two scalar angles at +0x1c/+0x20 (TE_FireBullets @ 0x9f8b40).
        if (std::strcmp(descriptor.name, "m_vecAngles[0]") == 0) return 28;
        if (std::strcmp(descriptor.name, "m_vecAngles[1]") == 0) return 32;
    }
    return descriptor.rawOffset >= 0 ? descriptor.rawOffset : 0;
}

void ProxyZeroVectorLength(const void*, const void*, const void*, void* out, int, int) {
    if (!out) return;
    const std::int32_t value = 0;
    std::memcpy(out, &value, sizeof(value));
}

void* ProxyDataTable(const void*, const void* structBase, const void* data, void*, int) {
    return const_cast<void*>(data ? data : structBase);
}

void* ProxySendLocalDataTable(const void*, const void* structBase, const void* data,
                              void* recipients, const int objectId) {
    // Original build4100 SendProxy_SendLocalDataTable (server_i486.so
    // 0x009a5500): CSendProxyRecipients is eight uint32 words. Clear all
    // recipients and select only the player whose entity index is objectId.
    if (recipients) {
        auto* words = static_cast<std::uint32_t*>(recipients);
        for (int i = 0; i < 8; ++i) words[i] = 0u;
        if (objectId >= 1 && objectId <= 256) {
            const unsigned bit = static_cast<unsigned>(objectId - 1);
            words[bit >> 5] |= (std::uint32_t{1} << (bit & 31u));
        }
    }
    return const_cast<void*>(data ? data : structBase);
}

void* ProxySendNonLocalDataTable(const void*, const void* structBase, const void* data,
                                 void* recipients, const int objectId) {
    // Original build4100 SendProxy_SendNonLocalDataTable (server_i486.so
    // 0x009c41a0): start with all recipients and clear the owning player.
    if (recipients) {
        auto* words = static_cast<std::uint32_t*>(recipients);
        for (int i = 0; i < 8; ++i) words[i] = 0xffffffffu;
        if (objectId >= 1 && objectId <= 256) {
            const unsigned bit = static_cast<unsigned>(objectId - 1);
            words[bit >> 5] &= ~(std::uint32_t{1} << (bit & 31u));
        }
    }
    return const_cast<void*>(data ? data : structBase);
}

build4100::SendTableProxyFn DataTableProxy(const char* const ownerTable,
                                            const char* const propName,
                                            const OriginalSendPropKind kind) noexcept {
    if (kind != OriginalSendPropKind::DataTable &&
        kind != OriginalSendPropKind::Array3 &&
        kind != OriginalSendPropKind::UtlVector) {
        return nullptr;
    }
    if (ownerTable && propName) {
        if (std::strcmp(ownerTable, "DT_BasePlayer") == 0 &&
            std::strcmp(propName, "localdata") == 0) {
            return &ProxySendLocalDataTable;
        }
        if (std::strcmp(ownerTable, "DT_CSPlayer") == 0) {
            if (std::strcmp(propName, "cslocaldata") == 0) return &ProxySendLocalDataTable;
            if (std::strcmp(propName, "csnonlocaldata") == 0) return &ProxySendNonLocalDataTable;
        }
    }
    return &ProxyDataTable;
}

void ProxyEmptySend(const void*, const void*, const void*, void*, int, int) {
}

int ProxyEmptyArrayLength(const void*, int) {
    // Only DT_Team::player_array has a dynamic length proxy in the recovered
    // build4100 tables. Reconstructed team entities do not expose the original
    // CTeam player vector, so fail closed to an empty dynamic array.
    return 0;
}

build4100::SendVarProxyFn LiveProxy(const OriginalSendPropKind kind) noexcept {
    switch (kind) {
        case OriginalSendPropKind::Time:
        case OriginalSendPropKind::Angle:
        case OriginalSendPropKind::Float:
            return &ProxyZeroFloat;
        case OriginalSendPropKind::Vector:
        case OriginalSendPropKind::QAngles:
            return &ProxyZeroVector;
        case OriginalSendPropKind::String:
            return &ProxyEmptyString;
        case OriginalSendPropKind::InternalArray:
            // Exact InternalSendPropArray constructor uses SendProxy_Empty.
            return &ProxyEmptySend;
        case OriginalSendPropKind::Array3:
        case OriginalSendPropKind::DataTable:
        case OriginalSendPropKind::UtlVector:
        case OriginalSendPropKind::Exclude:
            return nullptr;
        default:
            return &ProxyZeroInt;
    }
}

const char* DataTableTargetName(const char* ownerTable, const char* propName) noexcept {
    if (!ownerTable || !propName) return nullptr;
    for (const auto& link : kLiveDataTableLinks) {
        if (std::strcmp(link.ownerTable, ownerTable) == 0 && std::strcmp(link.propName, propName) == 0) {
            return link.targetTable;
        }
    }
    return nullptr;
}

}  // namespace

ServerClassRegistry::ServerClassRegistry() noexcept {
    classes_.resize(std::size(kDefinitions));
    const std::size_t tableCount = sizeof(kOriginalSendTableMetadata) / sizeof(kOriginalSendTableMetadata[0]);
    liveTables_.resize(tableCount);
    liveProps_.resize(tableCount);
    generatedArrays_.reserve(sizeof(kFixedArrayDescriptors) / sizeof(kFixedArrayDescriptors[0]));
    generatedUtlVectors_.reserve(sizeof(kUtlVectorDescriptors) / sizeof(kUtlVectorDescriptors[0]));

    for (std::size_t tableIndex = 0; tableIndex < tableCount; ++tableIndex) {
        const auto& metadata = kOriginalSendTableMetadata[tableIndex];
        auto& table = liveTables_[tableIndex];
        auto& props = liveProps_[tableIndex];

        table.netTableName = metadata.tableName;
        table.precalc = nullptr;
        table.initialized = false;
        table.hasPropsEncodedAgainstTickCount = false;

        props.reserve(metadata.propCount);
        for (std::size_t propIndex = 0; propIndex < metadata.propCount; ++propIndex) {
            const auto& descriptor = metadata.props[propIndex];
            if (!IsLiveKind(descriptor.kind)) continue;

            props.emplace_back();
            auto& prop = props.back();
            prop.matchingRecvProp = nullptr;
            prop.type = LiveType(descriptor.kind);
            prop.bits = descriptor.kind == OriginalSendPropKind::Array3 ? 0 : LiveBits(descriptor);
            // SendPropAngle/SendPropQAngles in the build4100 SDK use the
            // canonical [0,360] quantization range.  The static oracle records
            // helper arguments (bits/flags) but not constructor low/high values,
            // so restore the angle range explicitly for the live engine schema.
            const bool angleRange = descriptor.kind == OriginalSendPropKind::Angle ||
                                    descriptor.kind == OriginalSendPropKind::QAngles;
            const bool localVelocityRange =
                std::strcmp(metadata.tableName, "DT_LocalPlayerExclusive") == 0 &&
                (std::strcmp(descriptor.name, "m_vecVelocity[0]") == 0 ||
                 std::strcmp(descriptor.name, "m_vecVelocity[1]") == 0 ||
                 std::strcmp(descriptor.name, "m_vecVelocity[2]") == 0);
            // Original DT_LocalPlayerExclusive initializer passes -2048/+2048
            // for all three velocity components (x/y 20 bits, z 16 bits).
            const bool teFireBulletsSpread =
                std::strcmp(metadata.tableName, "DT_TEFireBullets") == 0 &&
                std::strcmp(descriptor.name, "m_flSpread") == 0;
            // Original CTEFireBullets SendPropFloat constructor passes
            // low=0/high=1/bits=10 for m_flSpread (server_i486.so 0x9f8591).
            prop.lowValue = localVelocityRange ? -2048.0f : 0.0f;
            prop.highValue = teFireBulletsSpread ? 1.0f
                : (angleRange ? 360.0f : (localVelocityRange ? 2048.0f : 0.0f));
            prop.arrayProp = nullptr;
            prop.arrayLengthProxy = nullptr;
            prop.elements = 1;
            prop.elementStride = -1;
            prop.excludeDataTableName = descriptor.kind == OriginalSendPropKind::Exclude
                ? descriptor.excludeTable : nullptr;
            prop.varName = descriptor.name;
            // The original SendPropAngle helper precomputes m_fHighLowMul in
            // the SendProp object.  Engine Float_Encode consumes this field
            // directly; leaving it at zero quantizes every scaled angle to
            // wire value 0 even though the gameplay proxy returns a nonzero
            // float. Restore the exact scaled-range multiplier for angles.
            prop.highLowMul = (angleRange || localVelocityRange || teFireBulletsSpread)
                ? ScaledFloatHighLowMul(prop.bits, prop.lowValue, prop.highValue)
                : 0.0f;
            prop.flags = LiveFlags(descriptor);
            if (const auto tempEntityProxy = TempEntityProxy(metadata.tableName, descriptor.kind)) {
                prop.proxyFn = tempEntityProxy;
            } else if (const auto gameplayProxy = GameplayProxy(metadata.tableName, descriptor.name)) {
                prop.proxyFn = gameplayProxy;
            } else {
                prop.proxyFn = LiveProxy(descriptor.kind);
            }
            prop.dataTableProxyFn = DataTableProxy(metadata.tableName, descriptor.name, descriptor.kind);
            prop.dataTable = nullptr;
            prop.offset = LiveOffset(metadata.tableName, descriptor);
            prop.extraData = nullptr;

            if (descriptor.kind == OriginalSendPropKind::InternalArray) {
                // SetupArrayProps_R in engine_i486.so pairs DPT_Array with the
                // immediately preceding SendProp and ORs SPROP_INSIDEARRAY on
                // that element. Mirror the post-init relationship here as well
                // so static schema tests can inspect the exact array contract.
                if (descriptor.arrayElements > 0 && !props.empty() && props.size() >= 2) {
                    auto& elementProp = props[props.size() - 2];
                    elementProp.flags |= build4100::kSendPropInsideArray;
                    prop.arrayProp = &elementProp;
                    prop.arrayLengthProxy = descriptor.hasArrayLengthProxy
                        ? &ProxyEmptyArrayLength : nullptr;
                    prop.elements = descriptor.arrayElements;
                    prop.elementStride = descriptor.arrayStride;
                    prop.bits = 0;
                    prop.flags = 0;
                    prop.offset = 0;
                    ++liveInternalArrayCount_;
                    liveInternalArrayElementCapacity_ += static_cast<std::size_t>(descriptor.arrayElements);
                }
            }

            if (descriptor.kind == OriginalSendPropKind::Array3) {
                const auto* fixed = FindFixedArrayDescriptor(metadata.tableName, descriptor.name);
                if (fixed && fixed->elementCount > 0 && fixed->elementCount <= 256 && fixed->stride > 0) {
                    generatedArrays_.emplace_back();
                    auto& generated = generatedArrays_.back();
                    generated.props.resize(static_cast<std::size_t>(fixed->elementCount));

                    for (std::int32_t element = 0; element < fixed->elementCount; ++element) {
                        auto& child = generated.props[static_cast<std::size_t>(element)];
                        child.matchingRecvProp = nullptr;
                        child.type = LiveType(fixed->elementKind);
                        child.bits = fixed->elementBits >= 0
                            ? fixed->elementBits
                            : (fixed->elementKind == OriginalSendPropKind::EHandle ? 21 : 32);
                        child.lowValue = 0.0f;
                        child.highValue = 0.0f;
                        child.arrayProp = nullptr;
                        child.arrayLengthProxy = nullptr;
                        child.elements = 1;
                        child.elementStride = -1;
                        child.excludeDataTableName = descriptor.name;
                        child.varName = ArrayElementName(static_cast<std::size_t>(element));
                        child.highLowMul = 0.0f;
                        child.flags = fixed->elementFlags >= 0 ? fixed->elementFlags : 0;
                        child.proxyFn = LiveProxy(fixed->elementKind);
                        child.dataTableProxyFn = nullptr;
                        child.dataTable = nullptr;
                        child.offset = element * fixed->stride;
                        child.extraData = nullptr;
                    }

                    generated.table.props = generated.props.data();
                    generated.table.propCount = fixed->elementCount;
                    generated.table.netTableName = descriptor.name;
                    generated.table.precalc = nullptr;
                    generated.table.initialized = false;
                    generated.table.hasPropsEncodedAgainstTickCount = false;
                    prop.dataTable = &generated.table;
                    ++liveFixedArrayCount_;
                    liveFixedArrayElementCount_ += static_cast<std::size_t>(fixed->elementCount);
                    ++liveDataTableLinkCount_;
                }
            } else if (descriptor.kind == OriginalSendPropKind::UtlVector) {
                const auto* vectorDescriptor = FindUtlVectorDescriptor(metadata.tableName, descriptor.name);
                if (vectorDescriptor && vectorDescriptor->maxElements > 0 &&
                    vectorDescriptor->maxElements <= 256 && vectorDescriptor->elementStride > 0) {
                    generatedUtlVectors_.emplace_back();
                    auto& generated = generatedUtlVectors_.back();
                    generated.vectorOffset = vectorDescriptor->vectorOffset;
                    generated.elementStride = vectorDescriptor->elementStride;
                    generated.maxElements = vectorDescriptor->maxElements;

                    int lengthBits = 0;
                    for (int value = vectorDescriptor->maxElements; value > 0; value >>= 1) ++lengthBits;
                    generated.lengthProp.type = build4100::SendPropType::Int;
                    generated.lengthProp.bits = lengthBits;
                    generated.lengthProp.varName = vectorDescriptor->lengthPropName;
                    generated.lengthProp.flags = 1;
                    generated.lengthProp.proxyFn = &ProxyZeroVectorLength;
                    generated.lengthProp.offset = 0;
                    generated.lengthProp.extraData = &generated;

                    generated.lengthTable.props = &generated.lengthProp;
                    generated.lengthTable.propCount = 1;
                    generated.lengthTable.netTableName = vectorDescriptor->lengthTableName;

                    generated.mainProps.resize(static_cast<std::size_t>(vectorDescriptor->maxElements) + 1);
                    auto& lengthProxy = generated.mainProps[0];
                    lengthProxy.type = build4100::SendPropType::DataTable;
                    lengthProxy.varName = "lengthproxy";
                    lengthProxy.flags = 0;
                    lengthProxy.dataTableProxyFn = &ProxyDataTable;
                    lengthProxy.dataTable = &generated.lengthTable;
                    lengthProxy.offset = 0;
                    lengthProxy.extraData = &generated;

                    for (std::int32_t element = 0; element < vectorDescriptor->maxElements; ++element) {
                        auto& child = generated.mainProps[static_cast<std::size_t>(element) + 1];
                        child.type = LiveType(vectorDescriptor->elementKind);
                        child.bits = vectorDescriptor->elementBits >= 0
                            ? vectorDescriptor->elementBits
                            : LiveBits(descriptor);
                        child.varName = ArrayElementName(static_cast<std::size_t>(element));
                        child.elements = 1;
                        // Exact build4100 SendPropUtlVector geometry: +0x24 is repurposed
                        // as the vector element index and +0x44 remains zero. The engine's
                        // UtlVector proxy reads the index from elementStride.
                        child.elementStride = element;
                        child.offset = 0;
                        child.extraData = &generated;
                        child.flags = vectorDescriptor->elementFlags;
                        child.proxyFn = nullptr;
                        child.dataTableProxyFn = nullptr;
                        child.dataTable = nullptr;
                        if (vectorDescriptor->elementKind == OriginalSendPropKind::DataTable) {
                            // Original build4100 keeps the source element SendTable pointer on
                            // every generated UtlVector<DataTable> element. It only clears the
                            // datatable proxy and marks the generated element internal (0x200).
                            // Leaving dataTable null makes SendTable precalc dereference null.
                            child.flags = 0x200;
                            child.dataTable = FindLiveTableByName(vectorDescriptor->elementTableName);
                            if (child.dataTable) ++liveDataTableLinkCount_;
                        }
                    }

                    generated.mainTable.props = generated.mainProps.data();
                    generated.mainTable.propCount = vectorDescriptor->maxElements + 1;
                    generated.mainTable.netTableName = vectorDescriptor->mainTableName;
                    prop.dataTable = &generated.mainTable;
                    prop.offset = 0;
                    ++liveUtlVectorCount_;
                    liveUtlVectorElementCapacity_ += static_cast<std::size_t>(vectorDescriptor->maxElements);
                    liveUtlVectorGeneratedPropCount_ += static_cast<std::size_t>(vectorDescriptor->maxElements) + 2;
                    liveDataTableLinkCount_ += 2;  // top-level vector + generated lengthproxy.
                }
            }
            ++livePropTotal_;
        }

        table.props = props.empty() ? nullptr : props.data();
        table.propCount = static_cast<std::int32_t>(props.size());
    }

    for (std::size_t tableIndex = 0; tableIndex < tableCount; ++tableIndex) {
        auto& table = liveTables_[tableIndex];
        for (int propIndex = 0; propIndex < table.propCount; ++propIndex) {
            auto& prop = table.props[propIndex];
            if (prop.type != build4100::SendPropType::DataTable || !prop.varName || prop.dataTable) continue;
            const char* targetName = DataTableTargetName(table.netTableName, prop.varName);
            if (!targetName) continue;
            prop.dataTable = FindLiveTableByName(targetName);
            if (prop.dataTable) ++liveDataTableLinkCount_;
        }
    }

    for (std::size_t i = 0; i < classes_.size(); ++i) {
        classes_[i].networkName = kDefinitions[i].networkName;
        classes_[i].table = FindLiveTableByName(kDefinitions[i].tableName);
        classes_[i].next = i + 1 < classes_.size() ? &classes_[i + 1] : nullptr;
        classes_[i].classId = static_cast<std::int32_t>(i);
        classes_[i].instanceBaselineIndex = 0xffff;
    }
}

build4100::ServerClass* ServerClassRegistry::FindByNetworkName(const char* const name) noexcept {
    if (!name) return nullptr;
    for (auto& value : classes_) {
        if (value.networkName && std::strcmp(value.networkName, name) == 0) return &value;
    }
    return nullptr;
}

build4100::SendTable* ServerClassRegistry::FindLiveTableByName(const char* const tableName) noexcept {
    if (!tableName) return nullptr;
    for (auto& table : liveTables_) {
        if (table.netTableName && std::strcmp(table.netTableName, tableName) == 0) return &table;
    }
    return nullptr;
}

const build4100::SendTable* ServerClassRegistry::FindLiveTableByName(const char* const tableName) const noexcept {
    if (!tableName) return nullptr;
    for (const auto& table : liveTables_) {
        if (table.netTableName && std::strcmp(table.netTableName, tableName) == 0) return &table;
    }
    return nullptr;
}

const OriginalSendTableMetadata* ServerClassRegistry::FindOriginalMetadataByTableName(
    const char* const tableName) const noexcept {
    if (!tableName) return nullptr;
    for (const auto& metadata : kOriginalSendTableMetadata) {
        if (metadata.tableName && std::strcmp(metadata.tableName, tableName) == 0) return &metadata;
    }
    return nullptr;
}

const OriginalSendTableMetadata* ServerClassRegistry::OriginalMetadata(
    const std::size_t index) const noexcept {
    if (index >= std::size(kDefinitions)) return nullptr;
    return FindOriginalMetadataByTableName(kDefinitions[index].tableName);
}

int ServerClassRegistry::OriginalPropCount(const std::size_t index) const noexcept {
    const auto* const metadata = OriginalMetadata(index);
    return metadata ? static_cast<int>(metadata->propCount) : -1;
}

std::size_t ServerClassRegistry::OriginalMetadataTableCount() const noexcept {
    return sizeof(kOriginalSendTableMetadata) / sizeof(kOriginalSendTableMetadata[0]);
}

ServerClassRegistry& ServerClasses() noexcept {
    static ServerClassRegistry value;
    return value;
}

void BindGameplayClientRegistry(ClientRegistry* const clients) noexcept {
    gGameplayClients = clients;
}

void BindGameplayGameState(CsGameState* const gameState) noexcept {
    gGameplayGameState = gameState;
}

void SetGameplayPlantedC4EntityIndex(const int entityIndex) noexcept {
    gGameplayPlantedC4EntityIndex = entityIndex;
}

int DumpRuntimeInstanceBaselines(const char* const path) noexcept {
    if (!path || !path[0]) return -1;
    auto& registry = ServerClasses();

    std::vector<const build4100::ServerClass*> classes;
    classes.reserve(registry.Count());
    int expectedId = 0;
    int ready = 0;
    for (const auto* serverClass = registry.Head(); serverClass; serverClass = serverClass->next) {
        if (!serverClass->networkName || !serverClass->table || !serverClass->table->netTableName ||
            serverClass->classId != expectedId) {
            return -2;
        }
        const int baseline = serverClass->instanceBaselineIndex;
        if (baseline != 0xffff) {
            if (baseline < 0) return -3;
            for (const auto* prior : classes) {
                if (prior->instanceBaselineIndex != 0xffff &&
                    prior->instanceBaselineIndex == baseline) {
                    return -4;
                }
            }
            ++ready;
        }
        classes.push_back(serverClass);
        ++expectedId;
    }
    if (classes.size() != registry.Count()) return -5;

    const std::string temp = std::string(path) + ".tmp";
    FILE* const file = std::fopen(temp.c_str(), "w");
    if (!file) return -6;
    std::fprintf(file, "#build4100-instancebaseline-v1\n");
    std::fprintf(file, "#class_id\tclass_name\troot_table\tbaseline_index\tready\n");
    for (const auto* serverClass : classes) {
        const int baseline = serverClass->instanceBaselineIndex;
        std::fprintf(file, "%d\t%s\t%s\t%d\t%d\n",
                     serverClass->classId, serverClass->networkName,
                     serverClass->table->netTableName, baseline, baseline != 0xffff ? 1 : 0);
    }
    if (std::fclose(file) != 0) {
        std::remove(temp.c_str());
        return -7;
    }
    if (std::rename(temp.c_str(), path) != 0) {
        std::remove(temp.c_str());
        return -8;
    }
    return ready;
}

int DumpRuntimeSendPropSchema(const char* const path) noexcept {
#if UINTPTR_MAX != 0xffffffffu
    (void)path;
    return -1;
#else
    if (!path || !path[0]) return -1;
    auto& registry = ServerClasses();

    struct FlatClass {
        const build4100::ServerClass* serverClass = nullptr;
        build4100::SendProp** props = nullptr;
        int count = 0;
    };
    std::vector<FlatClass> flat(registry.Count());
    int classIndex = 0;
    int total = 0;
    for (const auto* serverClass = registry.Head(); serverClass; serverClass = serverClass->next) {
        if (classIndex >= static_cast<int>(flat.size()) || !serverClass->table || !serverClass->table->precalc) {
            return -2;
        }
        const auto* const base = static_cast<const std::byte*>(serverClass->table->precalc);
        build4100::SendProp** props = nullptr;
        int count = 0;
        std::memcpy(&props, base + 0x2c, sizeof(props));
        std::memcpy(&count, base + 0x38, sizeof(count));
        // build4100 legitimately produces empty flattened precalcs for root
        // classes such as DT_BaseTempEntity and DT_GameRulesProxy.  An empty
        // vector may expose a null data pointer; only non-empty lists require
        // storage.  Treat negative/absurd counts as corruption, not count==0.
        if (count < 0 || count > 8192 || (count > 0 && !props)) return -3;
        flat[static_cast<std::size_t>(classIndex)] = {serverClass, props, count};
        total += count;
        ++classIndex;
    }
    if (classIndex != static_cast<int>(registry.Count())) return -4;

    // ABI sanity oracle recovered from the real v1.8.11 packet and engine
    // CSendTablePrecalc layout: CCSPlayer flattened field 347 is m_iAccount.
    const FlatClass* player = nullptr;
    for (const auto& item : flat) {
        if (item.serverClass && item.serverClass->networkName &&
            std::strcmp(item.serverClass->networkName, "CCSPlayer") == 0) {
            player = &item;
            break;
        }
    }
    if (!player || player->count <= 347 || !player->props[347] || !player->props[347]->varName ||
        std::strcmp(player->props[347]->varName, "m_iAccount") != 0) {
        return -5;
    }

    const std::string temp = std::string(path) + ".tmp";
    FILE* const file = std::fopen(temp.c_str(), "w");
    if (!file) return -6;
    std::fprintf(file, "#build4100-sendprop-schema-v2\n");
    std::fprintf(file, "#class_id\tclass_name\troot_table\tfield_index\tprop_name\ttype\tbits\tflags\tlow\thigh\tarray_elements\tarray_stride\telement_type\telement_bits\telement_flags\telement_low\telement_high\n");
    for (const auto& item : flat) {
        for (int field = 0; field < item.count; ++field) {
            const build4100::SendProp* const prop = item.props[field];
            if (!prop || !prop->varName) {
                std::fclose(file);
                std::remove(temp.c_str());
                return -7;
            }
            const build4100::SendProp* const element =
                prop->type == build4100::SendPropType::Array ? prop->arrayProp : nullptr;
            std::fprintf(file, "%d\t%s\t%s\t%d\t%s\t%d\t%d\t%d\t%.9g\t%.9g\t%d\t%d\t%d\t%d\t%d\t%.9g\t%.9g\n",
                         item.serverClass->classId, item.serverClass->networkName,
                         item.serverClass->table->netTableName, field, prop->varName,
                         static_cast<int>(prop->type), prop->bits, prop->flags,
                         static_cast<double>(prop->lowValue), static_cast<double>(prop->highValue),
                         element ? prop->elements : 0, element ? prop->elementStride : 0,
                         element ? static_cast<int>(element->type) : -1,
                         element ? element->bits : 0, element ? element->flags : 0,
                         element ? static_cast<double>(element->lowValue) : 0.0,
                         element ? static_cast<double>(element->highValue) : 0.0);
        }
    }
    if (std::fclose(file) != 0) {
        std::remove(temp.c_str());
        return -8;
    }
    if (std::rename(temp.c_str(), path) != 0) {
        std::remove(temp.c_str());
        return -9;
    }
    return total;
#endif
}

CStandardSendProxiesV1* StandardSendProxies() noexcept {
    static CStandardSendProxiesV1 value{
        &ProxyInt8, &ProxyInt16, &ProxyInt32, &ProxyUInt8,
        &ProxyUInt16, &ProxyUInt32, &ProxyFloat, &ProxyVector
    };
    return &value;
}

}  // namespace cssv34::reconstructed::abi
