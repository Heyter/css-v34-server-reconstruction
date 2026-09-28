#pragma once

#include <cstdint>

#include "abi/vtable_client.h"
#include "public/build4100/engine_trace.h"
#include "public/build4100/player_info.h"

namespace cssv34::reconstructed::abi {

class EngineTraceServer003Client final {
public:
    EngineTraceServer003Client() noexcept = default;
    explicit EngineTraceServer003Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept;
    std::int32_t PointContentsWorld(const build4100::Vector& point) const noexcept;

    // EngineTraceServer003 build4100 address-point slots recovered from
    // engine_i486.so: GetPointContents=0, GetPointContents_Collideable=1,
    // ClipRayToEntity=2, ClipRayToCollideable=3, TraceRay=4,
    // GetWorldCollideable=19.
    void TraceRay(const build4100::EngineTraceRay& ray, std::uint32_t mask,
                  void* traceFilter, build4100::EngineGameTrace& trace) const noexcept;

    void* GetWorldCollideable() const noexcept;
    void ClipRayToCollideable(const build4100::EngineTraceRay& ray, std::uint32_t mask,
                              void* collideable, build4100::EngineGameTrace& trace) const noexcept;

    bool TraceWorldHull(const build4100::Vector& start, const build4100::Vector& end,
                        const build4100::Vector& mins, const build4100::Vector& maxs,
                        std::uint32_t mask, build4100::EngineGameTrace& trace) const noexcept;

    // Exact build4100 BSP submodel path for moving brush entities.  modelName
    // is the entity-lump inline-model token (for example "*3").
    bool TraceInlineModelHull(const char* modelName,
                              const build4100::Vector& modelOrigin,
                              const build4100::QAngle& modelAngles,
                              const build4100::Vector& start,
                              const build4100::Vector& end,
                              const build4100::Vector& mins,
                              const build4100::Vector& maxs,
                              std::uint32_t mask,
                              build4100::EngineGameTrace& trace) const noexcept;

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
