#pragma once

#include <cstddef>
#include <cstdint>

// Exact callable signature used by build4100's first eight standard value
// send-proxy entries. DVariant is kept opaque here; these proxies only write
// its leading scalar/vector payload, exactly as the original tiny functions do.
using Build4100StandardSendProxyFn = void (*)(const void* sendProp,
                                               const void* structBase,
                                               const void* data,
                                               void* outVariant,
                                               int element,
                                               int objectId);

struct CStandardSendProxiesV1 {
    Build4100StandardSendProxyFn int8ToInt32 = nullptr;    // +0x00
    Build4100StandardSendProxyFn int16ToInt32 = nullptr;   // +0x04
    Build4100StandardSendProxyFn int32ToInt32 = nullptr;   // +0x08
    Build4100StandardSendProxyFn uint8ToInt32 = nullptr;   // +0x0c
    Build4100StandardSendProxyFn uint16ToInt32 = nullptr;  // +0x10
    Build4100StandardSendProxyFn uint32ToInt32 = nullptr;  // +0x14
    Build4100StandardSendProxyFn floatToFloat = nullptr;   // +0x18
    Build4100StandardSendProxyFn vectorToVector = nullptr; // +0x1c
};

#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(CStandardSendProxiesV1) == 0x20,
              "build4100 CStandardSendProxiesV1 must be 0x20 bytes");
static_assert(offsetof(CStandardSendProxiesV1, int8ToInt32) == 0x00);
static_assert(offsetof(CStandardSendProxiesV1, int16ToInt32) == 0x04);
static_assert(offsetof(CStandardSendProxiesV1, int32ToInt32) == 0x08);
static_assert(offsetof(CStandardSendProxiesV1, uint8ToInt32) == 0x0c);
static_assert(offsetof(CStandardSendProxiesV1, uint16ToInt32) == 0x10);
static_assert(offsetof(CStandardSendProxiesV1, uint32ToInt32) == 0x14);
static_assert(offsetof(CStandardSendProxiesV1, floatToFloat) == 0x18);
static_assert(offsetof(CStandardSendProxiesV1, vectorToVector) == 0x1c);
#endif
