#pragma once

#include <cstddef>
#include <cstdint>

namespace cssv34::build4100 {

struct SendTable;

// build4100 predates DPT_VectorXY / DPT_Int64.  The exact enum values are
// proven by the original SendProp* helpers (DataTable writes type 5).
enum class SendPropType : std::int32_t {
    Int = 0,
    Float = 1,
    Vector = 2,
    String = 3,
    Array = 4,
    DataTable = 5,
    Count = 6,
};

// build4100 SendProp flags. Keep these explicit because protocol 7 differs
// from later Source branches: SPROP_XYZE is defined at 0x80, but the original
// engine Vector_Encode/Vector_Decode paths do not branch on it.
constexpr std::int32_t kSendPropUnsigned = 1 << 0;
constexpr std::int32_t kSendPropCoord = 1 << 1;
constexpr std::int32_t kSendPropNoScale = 1 << 2;
constexpr std::int32_t kSendPropRoundDown = 1 << 3;
constexpr std::int32_t kSendPropRoundUp = 1 << 4;
constexpr std::int32_t kSendPropNormal = 1 << 5;
constexpr std::int32_t kSendPropExclude = 1 << 6;
constexpr std::int32_t kSendPropXYZE = 1 << 7;
constexpr std::int32_t kSendPropInsideArray = 1 << 8;
constexpr std::int32_t kSendPropProxyAlwaysYes = 1 << 9;
constexpr std::int32_t kSendPropChangesOften = 1 << 10;

using SendVarProxyFn = void (*)(const void* prop, const void* structBase,
                                const void* data, void* outVariant,
                                int element, int objectId);
using SendTableProxyFn = void* (*)(const void* prop, const void* structBase,
                                   const void* data, void* recipients,
                                   int objectId);
using ArrayLengthSendProxyFn = int (*)(const void* structBase, int objectId);

// Exact build4100 SendProp memory geometry recovered from server_i486.so:
// constructor 0x009a6800, SendPropInt 0x009a58a0, SendPropDataTable
// 0x009a6650, plus g_SendProps arrays whose stride is 0x4c.
class SendProp {
public:
    SendProp() noexcept = default;
    virtual ~SendProp() = default; // original vtable has the two destructor entries only.

    void* matchingRecvProp = nullptr;             // +0x04
    SendPropType type = SendPropType::Count;      // +0x08
    std::int32_t bits = 0;                        // +0x0c
    float lowValue = 0.0f;                        // +0x10
    float highValue = 0.0f;                       // +0x14
    SendProp* arrayProp = nullptr;                 // +0x18
    ArrayLengthSendProxyFn arrayLengthProxy = nullptr; // +0x1c
    std::int32_t elements = 1;                    // +0x20
    std::int32_t elementStride = -1;              // +0x24
    const char* excludeDataTableName = nullptr;    // +0x28
    const char* varName = nullptr;                 // +0x2c
    float highLowMul = 0.0f;                      // +0x30
    std::int32_t flags = 0;                       // +0x34
    SendVarProxyFn proxyFn = nullptr;              // +0x38
    SendTableProxyFn dataTableProxyFn = nullptr;   // +0x3c
    SendTable* dataTable = nullptr;                // +0x40
    std::int32_t offset = 0;                      // +0x44
    const void* extraData = nullptr;               // +0x48
};

// Exact leading build4100 SendTable geometry recovered from the original
// SendTable constructors (server_i486.so 0x009a68e0/0x009a6f70).
struct SendTable {
    SendProp* props = nullptr;                    // +0x00
    std::int32_t propCount = 0;                  // +0x04
    const char* netTableName = nullptr;           // +0x08
    void* precalc = nullptr;                      // +0x0c
    bool initialized = false;                     // +0x10
    bool hasPropsEncodedAgainstTickCount = false; // +0x11
    std::uint16_t padding12 = 0;                  // +0x12
};

// Exact build4100 ServerClass field order recovered from
// ServerClass::ServerClass(char*, SendTable*) at 0x00791bf0.
struct ServerClass {
    const char* networkName = nullptr;            // +0x00
    SendTable* table = nullptr;                   // +0x04
    ServerClass* next = nullptr;                  // +0x08
    std::int32_t classId = -1;                   // +0x0c
    std::int32_t instanceBaselineIndex = 0xffff; // +0x10
};

#if UINTPTR_MAX == 0xffffffffu
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(sizeof(SendProp) == 0x4c, "build4100 SendProp must be 0x4c bytes");
static_assert(offsetof(SendProp, matchingRecvProp) == 0x04);
static_assert(offsetof(SendProp, type) == 0x08);
static_assert(offsetof(SendProp, bits) == 0x0c);
static_assert(offsetof(SendProp, lowValue) == 0x10);
static_assert(offsetof(SendProp, highValue) == 0x14);
static_assert(offsetof(SendProp, arrayProp) == 0x18);
static_assert(offsetof(SendProp, arrayLengthProxy) == 0x1c);
static_assert(offsetof(SendProp, elements) == 0x20);
static_assert(offsetof(SendProp, elementStride) == 0x24);
static_assert(offsetof(SendProp, excludeDataTableName) == 0x28);
static_assert(offsetof(SendProp, varName) == 0x2c);
static_assert(offsetof(SendProp, highLowMul) == 0x30);
static_assert(offsetof(SendProp, flags) == 0x34);
static_assert(offsetof(SendProp, proxyFn) == 0x38);
static_assert(offsetof(SendProp, dataTableProxyFn) == 0x3c);
static_assert(offsetof(SendProp, dataTable) == 0x40);
static_assert(offsetof(SendProp, offset) == 0x44);
static_assert(offsetof(SendProp, extraData) == 0x48);
static_assert(sizeof(SendTable) == 0x14, "build4100 SendTable must be 0x14 bytes");
static_assert(offsetof(SendTable, props) == 0x00);
static_assert(offsetof(SendTable, propCount) == 0x04);
static_assert(offsetof(SendTable, netTableName) == 0x08);
static_assert(offsetof(SendTable, precalc) == 0x0c);
static_assert(offsetof(SendTable, initialized) == 0x10);
static_assert(offsetof(SendTable, hasPropsEncodedAgainstTickCount) == 0x11);
static_assert(sizeof(ServerClass) == 0x14, "build4100 ServerClass must be 0x14 bytes");
static_assert(offsetof(ServerClass, networkName) == 0x00);
static_assert(offsetof(ServerClass, table) == 0x04);
static_assert(offsetof(ServerClass, next) == 0x08);
static_assert(offsetof(ServerClass, classId) == 0x0c);
static_assert(offsetof(ServerClass, instanceBaselineIndex) == 0x10);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

}  // namespace cssv34::build4100
