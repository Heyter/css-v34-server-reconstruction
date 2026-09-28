#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "public/build4100/sendtable.h"
#include "public/build4100/sendproxies.h"
#include "server/client_registry.h"
#include "server/cs_game_state.h"

namespace cssv34::reconstructed::abi {

enum class OriginalSendPropKind : std::uint8_t {
    Int, Bool, Time, Angle, Float, Array3, Vector, EHandle, Exclude, StringT, DataTable,
    String, QAngles, IntMinusOne, UtlVector, PredictableId, InternalArray,
};

struct OriginalSendPropDescriptor {
    const char* name;
    OriginalSendPropKind kind;
    std::int32_t rawOffset;
    std::int32_t absoluteOffset;
    std::int32_t bits;
    std::int32_t flags;
    const char* excludeTable;
    // InternalSendPropArray-only metadata. Other recovered helper kinds leave
    // these at their aggregate defaults. build4100 pairs a DPT_Array with the
    // immediately preceding element SendProp during SendTable initialization.
    std::int32_t arrayElements = 0;
    std::int32_t arrayStride = 0;
    bool hasArrayLengthProxy = false;
};

struct OriginalSendTableMetadata {
    const char* tableName;
    const OriginalSendPropDescriptor* props;
    std::size_t propCount;
};

class ServerClassRegistry final {
public:
    ServerClassRegistry() noexcept;

    build4100::ServerClass* Head() noexcept { return classes_.data(); }
    const build4100::ServerClass* Head() const noexcept { return classes_.data(); }
    std::size_t Count() const noexcept { return classes_.size(); }
    build4100::ServerClass* FindByNetworkName(const char* name) noexcept;
    build4100::SendTable* FindLiveTableByName(const char* tableName) noexcept;
    const build4100::SendTable* FindLiveTableByName(const char* tableName) const noexcept;
    int OriginalPropCount(std::size_t index) const noexcept;
    const OriginalSendTableMetadata* OriginalMetadata(std::size_t index) const noexcept;
    const OriginalSendTableMetadata* FindOriginalMetadataByTableName(const char* tableName) const noexcept;
    std::size_t OriginalMetadataTableCount() const noexcept;
    std::size_t LiveTableCount() const noexcept { return liveTables_.size(); }
    std::size_t LivePropTotal() const noexcept { return livePropTotal_; }
    std::size_t LiveDataTableLinkCount() const noexcept { return liveDataTableLinkCount_; }
    std::size_t LiveFixedArrayCount() const noexcept { return liveFixedArrayCount_; }
    std::size_t LiveFixedArrayElementCount() const noexcept { return liveFixedArrayElementCount_; }
    std::size_t LiveUtlVectorCount() const noexcept { return liveUtlVectorCount_; }
    std::size_t LiveUtlVectorElementCapacity() const noexcept { return liveUtlVectorElementCapacity_; }
    std::size_t LiveUtlVectorGeneratedPropCount() const noexcept { return liveUtlVectorGeneratedPropCount_; }
    std::size_t LiveInternalArrayCount() const noexcept { return liveInternalArrayCount_; }
    std::size_t LiveInternalArrayElementCapacity() const noexcept { return liveInternalArrayElementCapacity_; }

private:
    struct GeneratedArrayStorage {
        std::vector<build4100::SendProp> props{};
        build4100::SendTable table{};
    };

    struct GeneratedUtlVectorStorage {
        // Keep the leading fields compatible with build4100's private
        // SendPropUtlVector extra-data contract for diagnostics/future proxies.
        build4100::SendTableProxyFn elementDataTableProxy = nullptr;
        build4100::SendVarProxyFn elementVarProxy = nullptr;
        void* resizeFn = nullptr;
        std::int32_t elementStride = 0;
        std::int32_t vectorOffset = 0;
        std::int32_t maxElements = 0;
        build4100::SendProp lengthProp{};
        build4100::SendTable lengthTable{};
        std::vector<build4100::SendProp> mainProps{};
        build4100::SendTable mainTable{};
    };

    std::vector<build4100::SendTable> liveTables_{};
    std::vector<std::vector<build4100::SendProp>> liveProps_{};
    std::vector<GeneratedArrayStorage> generatedArrays_{};
    std::vector<GeneratedUtlVectorStorage> generatedUtlVectors_{};
    std::vector<build4100::ServerClass> classes_{};
    std::size_t livePropTotal_ = 0;
    std::size_t liveDataTableLinkCount_ = 0;
    std::size_t liveFixedArrayCount_ = 0;
    std::size_t liveFixedArrayElementCount_ = 0;
    std::size_t liveUtlVectorCount_ = 0;
    std::size_t liveUtlVectorElementCapacity_ = 0;
    std::size_t liveUtlVectorGeneratedPropCount_ = 0;
    std::size_t liveInternalArrayCount_ = 0;
    std::size_t liveInternalArrayElementCapacity_ = 0;
};

ServerClassRegistry& ServerClasses() noexcept;
CStandardSendProxiesV1* StandardSendProxies() noexcept;

// Bind the engine-facing SendProp proxies to the authoritative reconstructed
// client/gameplay registry. The binding is dynamic because ServerClasses is
// constructed before DLLInit, while ClientRegistry is configured during DLLInit.
void BindGameplayClientRegistry(ClientRegistry* clients) noexcept;
void BindGameplayGameState(CsGameState* gameState) noexcept;
void SetGameplayPlantedC4EntityIndex(int entityIndex) noexcept;

// Dump the exact engine-generated flattened SendProp schema. build4100 stores
// CSendTablePrecalc::m_Props at +0x2c and the flattened count at +0x38.
// Returns the total number of dumped flat fields, or <=0 when unavailable.
int DumpRuntimeSendPropSchema(const char* path) noexcept;

// Dump the live ServerClass -> instancebaseline string-table index mapping.
// build4100 writes ServerClass::instanceBaselineIndex after baseline creation;
// 0xffff remains the sentinel for classes with no materialized baseline.
// Returns the number of ready classes or a negative structural/file error.
int DumpRuntimeInstanceBaselines(const char* path) noexcept;

}  // namespace cssv34::reconstructed::abi
