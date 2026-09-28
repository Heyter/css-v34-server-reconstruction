#pragma once

#include <cstddef>
#include <cstdint>

namespace cssv34::build4100 {

// build4100 bf_read storage layout recovered directly from the original
// server_i486.so constructors (bf_read::bf_read / StartReading).
// The class has no vptr. Only the storage ABI is modeled here; decoding lives
// in reconstructed/usercmd_codec so the replacement DLL does not depend on
// Valve tier1 at runtime.
struct BitReadBuffer {
    const std::uint8_t* data = nullptr;  // +0x00
    std::int32_t dataBytes = 0;          // +0x04
    std::int32_t dataBits = -1;          // +0x08
    std::int32_t curBit = 0;             // +0x0c
    bool overflow = false;               // +0x10
    bool assertOnOverflow = true;         // +0x11
    std::uint8_t pad12[2]{};             // +0x12
    const char* debugName = nullptr;      // +0x14 (i386)
};

#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(BitReadBuffer) == 0x18, "build4100 bf_read must be 0x18 bytes on i386");
static_assert(offsetof(BitReadBuffer, data) == 0x00);
static_assert(offsetof(BitReadBuffer, dataBytes) == 0x04);
static_assert(offsetof(BitReadBuffer, dataBits) == 0x08);
static_assert(offsetof(BitReadBuffer, curBit) == 0x0c);
static_assert(offsetof(BitReadBuffer, overflow) == 0x10);
static_assert(offsetof(BitReadBuffer, assertOnOverflow) == 0x11);
static_assert(offsetof(BitReadBuffer, debugName) == 0x14);
#endif

inline void InitBitReadBuffer(BitReadBuffer& buffer, const void* data,
                              const std::int32_t bytes, const std::int32_t bits = -1) noexcept {
    buffer.data = static_cast<const std::uint8_t*>(data);
    buffer.dataBytes = bytes;
    buffer.dataBits = bits >= 0 ? bits : (bytes >= 0 ? bytes * 8 : 0);
    buffer.curBit = 0;
    buffer.overflow = false;
    buffer.assertOnOverflow = true;
    buffer.debugName = nullptr;
}

}  // namespace cssv34::build4100
