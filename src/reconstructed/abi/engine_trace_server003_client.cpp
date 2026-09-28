#include "abi/engine_trace_server003_client.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(__GNUC__) && defined(__linux__)
#include <dlfcn.h>
#include <link.h>
#endif

namespace cssv34::build4100 {

void EngineTraceRay::Init(const Vector& from, const Vector& to) noexcept {
    start = from;
    delta = Vector{to.x - from.x, to.y - from.y, to.z - from.z};
    startOffset = Vector{};
    extents = Vector{};
    isRay = true;
    isSwept = (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) != 0.0f;
}

void EngineTraceRay::Init(const Vector& from, const Vector& to,
                          const Vector& mins, const Vector& maxs) noexcept {
    delta = Vector{to.x - from.x, to.y - from.y, to.z - from.z};
    isSwept = (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) != 0.0f;
    extents = Vector{(maxs.x - mins.x) * 0.5f,
                     (maxs.y - mins.y) * 0.5f,
                     (maxs.z - mins.z) * 0.5f};
    isRay = (extents.x * extents.x + extents.y * extents.y + extents.z * extents.z) < 1.0e-6f;
    startOffset = Vector{(mins.x + maxs.x) * 0.5f,
                         (mins.y + maxs.y) * 0.5f,
                         (mins.z + maxs.z) * 0.5f};
    start = Vector{from.x + startOffset.x, from.y + startOffset.y, from.z + startOffset.z};
    startOffset.x = -startOffset.x;
    startOffset.y = -startOffset.y;
    startOffset.z = -startOffset.z;
}

}  // namespace cssv34::reconstructed::build4100

namespace cssv34::reconstructed::abi {
namespace {

#if defined(__GNUC__) && defined(__linux__)
struct LoadedEnginePath {
    char path[1024]{};
};

int FindLoadedEngine(dl_phdr_info* const info, const std::size_t, void* const opaque) {
    if (!info || !info->dlpi_name || !info->dlpi_name[0] || !opaque) return 0;
    const char* const slash = std::strrchr(info->dlpi_name, '/');
    const char* const base = slash ? slash + 1 : info->dlpi_name;
    if (std::strcmp(base, "engine_i486.so") != 0) return 0;
    auto* const out = static_cast<LoadedEnginePath*>(opaque);
    std::snprintf(out->path, sizeof(out->path), "%s", info->dlpi_name);
    return 1;
}

using CmBoxTraceFn = void (*)(const build4100::EngineTraceRay&, int, int, bool,
                              build4100::EngineGameTrace&);
using CmPointContentsFn = int (*)(const build4100::Vector&, int);
using CmInlineModelFn = void* (*)(const char*);
using CmTransformedBoxTraceFn = void (*)(const build4100::EngineTraceRay&, int, int,
                                         const build4100::Vector&,
                                         const build4100::QAngle&,
                                         build4100::EngineGameTrace&);

struct CollisionModel4100 {
    build4100::Vector mins{};
    build4100::Vector maxs{};
    build4100::Vector origin{};
    int headnode = 0;
    std::uint8_t vcollide[12]{};
};
static_assert(sizeof(CollisionModel4100) == 52, "build4100 cmodel_t size");

template <typename Fn>
Fn ResolveEngineSymbol(const char* symbol) noexcept {
    LoadedEnginePath engine{};
    (void)dl_iterate_phdr(&FindLoadedEngine, &engine);
    if (!engine.path[0]) return nullptr;
    void* const handle = dlopen(engine.path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    if (!handle) return nullptr;
    void* const raw = dlsym(handle, symbol);
    Fn fn = nullptr;
    static_assert(sizeof(fn) == sizeof(raw), "build4100 function-pointer ABI");
    std::memcpy(&fn, &raw, sizeof(fn));
    dlclose(handle);
    return fn;
}

CmBoxTraceFn ResolveCmBoxTrace() noexcept {
    static CmBoxTraceFn cached =
        ResolveEngineSymbol<CmBoxTraceFn>("_Z11CM_BoxTraceRK5Ray_tiibR10CGameTrace");
    return cached;
}

CmPointContentsFn ResolveCmPointContents() noexcept {
    static CmPointContentsFn cached =
        ResolveEngineSymbol<CmPointContentsFn>("_Z16CM_PointContentsRK6Vectori");
    return cached;
}

CmInlineModelFn ResolveCmInlineModel() noexcept {
    static CmInlineModelFn cached =
        ResolveEngineSymbol<CmInlineModelFn>("_Z14CM_InlineModelPKc");
    return cached;
}

CmTransformedBoxTraceFn ResolveCmTransformedBoxTrace() noexcept {
    static CmTransformedBoxTraceFn cached = ResolveEngineSymbol<CmTransformedBoxTraceFn>(
        "_Z22CM_TransformedBoxTraceRK5Ray_tiiRK6VectorRK6QAngleR10CGameTrace");
    return cached;
}
#else
using CmBoxTraceFn = void (*)(const build4100::EngineTraceRay&, int, int, bool,
                              build4100::EngineGameTrace&);
using CmPointContentsFn = int (*)(const build4100::Vector&, int);
using CmInlineModelFn = void* (*)(const char*);
using CmTransformedBoxTraceFn = void (*)(const build4100::EngineTraceRay&, int, int,
                                         const build4100::Vector&,
                                         const build4100::QAngle&,
                                         build4100::EngineGameTrace&);

struct CollisionModel4100 {
    build4100::Vector mins{};
    build4100::Vector maxs{};
    build4100::Vector origin{};
    int headnode = 0;
    std::uint8_t vcollide[12]{};
};
static_assert(sizeof(CollisionModel4100) == 52, "build4100 cmodel_t size");
CmBoxTraceFn ResolveCmBoxTrace() noexcept { return nullptr; }
CmPointContentsFn ResolveCmPointContents() noexcept { return nullptr; }
CmInlineModelFn ResolveCmInlineModel() noexcept { return nullptr; }
CmTransformedBoxTraceFn ResolveCmTransformedBoxTrace() noexcept { return nullptr; }
#endif

}  // namespace
bool EngineTraceServer003Client::IsValid() const noexcept {
    return detail::HasVTable(object_);
}

std::int32_t EngineTraceServer003Client::PointContentsWorld(
    const build4100::Vector& point) const noexcept {
    if (const CmPointContentsFn pointContents = ResolveCmPointContents()) {
        return pointContents(point, 0);
    }
    using Fn = std::int32_t (*)(void*, const build4100::Vector*, void**);
    const Fn fn = detail::VTableSlot<Fn>(object_, 0);
    return fn ? fn(object_, &point, nullptr) : 0;
}

void EngineTraceServer003Client::TraceRay(const build4100::EngineTraceRay& ray,
                                          const std::uint32_t mask,
                                          void* const traceFilter,
                                          build4100::EngineGameTrace& trace) const noexcept {
    using Fn = void (*)(void*, const build4100::EngineTraceRay*, std::uint32_t,
                        void*, build4100::EngineGameTrace*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 4);
    if (!fn) {
        trace = build4100::EngineGameTrace{};
        trace.fraction = 1.0f;
        trace.startPos = build4100::Vector{};
        trace.endPos = build4100::Vector{};
        return;
    }
    fn(object_, &ray, mask, traceFilter, &trace);
}

void* EngineTraceServer003Client::GetWorldCollideable() const noexcept {
    using Fn = void* (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 19);
    return fn ? fn(object_) : nullptr;
}

void EngineTraceServer003Client::ClipRayToCollideable(
    const build4100::EngineTraceRay& ray, const std::uint32_t mask,
    void* const collideable, build4100::EngineGameTrace& trace) const noexcept {
    using Fn = void (*)(void*, const build4100::EngineTraceRay*, std::uint32_t,
                        void*, build4100::EngineGameTrace*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 3);
    if (!fn || !collideable) {
        trace = build4100::EngineGameTrace{};
        trace.fraction = 1.0f;
        return;
    }
    fn(object_, &ray, mask, collideable, &trace);
}

bool EngineTraceServer003Client::TraceWorldHull(const build4100::Vector& start,
                                                const build4100::Vector& end,
                                                const build4100::Vector& mins,
                                                const build4100::Vector& maxs,
                                                const std::uint32_t mask,
                                                build4100::EngineGameTrace& trace) const noexcept {
    build4100::EngineTraceRay ray{};
    ray.Init(start, end, mins, maxs);
    trace = build4100::EngineGameTrace{};
    trace.fraction = 1.0f;

    // The reconstructed world entity intentionally has no ICollideable yet, so
    // CEngineTraceServer::GetWorldCollideable() returns null.  For world-only BSP
    // movement use the exact exported collision-model entry point used by this
    // engine branch.  headnode 0 is the loaded world BSP root; computeEndPos=true
    // fills the complete CGameTrace result.  This avoids entity/filter traversal
    // while keeping the original build4100 Ray_t/CGameTrace ABI.
    if (const CmBoxTraceFn cmBoxTrace = ResolveCmBoxTrace()) {
        cmBoxTrace(ray, 0, static_cast<int>(mask), true, trace);
        return true;
    }

    // Unit/smoke binaries do not load engine_i486.so. Keep the vtable fallback
    // for their mock EngineTraceServer003 object.
    if (!IsValid()) return false;
    void* const worldCollideable = GetWorldCollideable();
    if (!worldCollideable) return false;
    ClipRayToCollideable(ray, mask, worldCollideable, trace);
    return true;
}

bool EngineTraceServer003Client::TraceInlineModelHull(
    const char* const modelName,
    const build4100::Vector& modelOrigin,
    const build4100::QAngle& modelAngles,
    const build4100::Vector& start,
    const build4100::Vector& end,
    const build4100::Vector& mins,
    const build4100::Vector& maxs,
    const std::uint32_t mask,
    build4100::EngineGameTrace& trace) const noexcept {
    trace = build4100::EngineGameTrace{};
    trace.fraction = 1.0f;
    if (!modelName || modelName[0] != '*') return false;

    const CmInlineModelFn inlineModel = ResolveCmInlineModel();
    const CmTransformedBoxTraceFn transformedTrace = ResolveCmTransformedBoxTrace();
    if (!inlineModel || !transformedTrace) return false;
    auto* const model = static_cast<CollisionModel4100*>(inlineModel(modelName));
    if (!model) return false;

    build4100::EngineTraceRay ray{};
    ray.Init(start, end, mins, maxs);
    transformedTrace(ray, model->headnode, static_cast<int>(mask),
                     modelOrigin, modelAngles, trace);
    return true;
}

}  // namespace cssv34::reconstructed::abi
