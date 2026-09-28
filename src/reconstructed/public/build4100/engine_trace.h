#pragma once

#include <cstddef>
#include <cstdint>

#include "public/build4100/vector.h"

namespace cssv34::build4100 {

// CSS build4100 server engine trace ABI recovered from engine_i486.so.
// Ray_t is the pre-VectorAligned layout used by this branch: four 12-byte
// vectors followed by m_IsRay/m_IsSwept. CEngineTrace::TraceRay reads exactly
// through byte 0x31 and its vtable address point exposes TraceRay at slot 4.
struct EngineTraceRay {
    Vector start{};          // 0x00: centered start
    Vector delta{};          // 0x0c: end - start
    Vector startOffset{};    // 0x18: add to centered start to recover caller start
    Vector extents{};        // 0x24: half-size of swept AABB
    bool isRay = true;       // 0x30
    bool isSwept = false;    // 0x31
    std::uint8_t pad[2]{};   // 0x32

    void Init(const Vector& from, const Vector& to) noexcept;
    void Init(const Vector& from, const Vector& to,
              const Vector& mins, const Vector& maxs) noexcept;
};

struct EngineTracePlane {
    Vector normal{};         // 0x00
    float distance = 0.0f;   // 0x0c
    std::uint8_t type = 0;   // 0x10
    std::uint8_t signBits = 0; // 0x11
    std::uint8_t pad[2]{};   // 0x12
};

struct EngineTraceSurface {
    const char* name = nullptr;
    std::int16_t surfaceProps = 0;
    std::uint16_t flags = 0;
};

// CBaseTrace + CGameTrace build4100 layout. The fields through startSolid are
// directly evidenced by CEngineTrace::TraceRay accesses (fraction @ 0x2c,
// contents @ 0x30, allSolid @ 0x36, startSolid @ 0x37, fractionLeftSolid @ 0x38).
struct EngineGameTrace {
    Vector startPos{};                // 0x00
    Vector endPos{};                  // 0x0c
    EngineTracePlane plane{};         // 0x18
    float fraction = 1.0f;            // 0x2c
    std::int32_t contents = 0;        // 0x30
    std::uint16_t dispFlags = 0;      // 0x34
    bool allSolid = false;            // 0x36
    bool startSolid = false;          // 0x37
    float fractionLeftSolid = 0.0f;   // 0x38
    EngineTraceSurface surface{};     // 0x3c on i386
    std::int32_t hitGroup = 0;        // 0x44 on i386
    std::int16_t physicsBone = 0;     // 0x48 on i386
    std::uint16_t worldSurfaceIndex = 0; // 0x4a on i386
    void* entity = nullptr;           // 0x4c on i386
    std::int32_t hitbox = 0;          // 0x50 on i386
};

// Contents values used by the Source 2005/2006 player-solid masks.
constexpr std::uint32_t kContentsSolid = 0x00000001u;
constexpr std::uint32_t kContentsWindow = 0x00000002u;
constexpr std::uint32_t kContentsGrate = 0x00000008u;
constexpr std::uint32_t kContentsSlime = 0x00000010u;
constexpr std::uint32_t kContentsWater = 0x00000020u;
constexpr std::uint32_t kContentsMoveable = 0x00004000u;
constexpr std::uint32_t kContentsPlayerClip = 0x00010000u;
constexpr std::uint32_t kContentsCurrent0 = 0x00040000u;
constexpr std::uint32_t kContentsCurrent90 = 0x00080000u;
constexpr std::uint32_t kContentsCurrent180 = 0x00100000u;
constexpr std::uint32_t kContentsCurrent270 = 0x00200000u;
constexpr std::uint32_t kContentsCurrentUp = 0x00400000u;
constexpr std::uint32_t kContentsCurrentDown = 0x00800000u;
constexpr std::uint32_t kContentsMonster = 0x02000000u;
constexpr std::uint32_t kMaskWater = kContentsWater | kContentsMoveable | kContentsSlime;
constexpr std::uint32_t kMaskCurrent =
    kContentsCurrent0 | kContentsCurrent90 | kContentsCurrent180 |
    kContentsCurrent270 | kContentsCurrentUp | kContentsCurrentDown;
constexpr std::uint32_t kMaskPlayerSolid =
    kContentsSolid | kContentsMoveable | kContentsPlayerClip |
    kContentsWindow | kContentsMonster | kContentsGrate;
constexpr std::uint32_t kMaskPlayerSolidBrushOnly =
    kContentsSolid | kContentsMoveable | kContentsPlayerClip |
    kContentsWindow | kContentsGrate;

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(EngineTraceRay) == 0x34, "build4100 Ray_t size");
static_assert(offsetof(EngineTraceRay, startOffset) == 0x18, "build4100 Ray_t startOffset");
static_assert(offsetof(EngineTraceRay, extents) == 0x24, "build4100 Ray_t extents");
static_assert(offsetof(EngineTraceRay, isRay) == 0x30, "build4100 Ray_t isRay");
static_assert(sizeof(EngineTracePlane) == 0x14, "build4100 cplane_t size");
static_assert(offsetof(EngineGameTrace, fraction) == 0x2c, "build4100 trace fraction");
static_assert(offsetof(EngineGameTrace, contents) == 0x30, "build4100 trace contents");
static_assert(offsetof(EngineGameTrace, allSolid) == 0x36, "build4100 trace allsolid");
static_assert(offsetof(EngineGameTrace, startSolid) == 0x37, "build4100 trace startsolid");
static_assert(offsetof(EngineGameTrace, fractionLeftSolid) == 0x38, "build4100 trace fractionleftsolid");
static_assert(offsetof(EngineGameTrace, entity) == 0x4c, "build4100 trace entity");
static_assert(sizeof(EngineGameTrace) == 0x54, "build4100 CGameTrace size");
#endif

}  // namespace cssv34::reconstructed::build4100
