#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "public/build4100/edict.h"
#include "public/build4100/server_unknown.h"

namespace cssv34::reconstructed::abi {

// The original CServerGameEnts::BaseEntityToEdict reads *(baseEntity + 0x168).
// Keep the field at the proven build4100 offset while also presenting the exact
// IServerUnknown/IHandleEntity vtable chain to the engine.
class EngineServerUnknownShim final : public build4100::IServerUnknown {
public:
    EngineServerUnknownShim() = default;

    void Rebind(build4100::Edict* edict,
                build4100::IServerNetworkable* networkable,
                std::uint32_t entityHandle) noexcept;

    void SetRefEHandle(const build4100::BaseHandle& handle) override;
    const build4100::BaseHandle& GetRefEHandle() const override;
    void* GetCollideable() override;
    build4100::IServerNetworkable* GetNetworkable() override;
    void* GetBaseEntity() override;

    build4100::Edict* Edict() const noexcept { return edict; }

public:
    // Layout-critical prefix.  Public only so offsetof can validate the i386
    // ABI in the build; callers should use the virtual interface/accessors.
    // The object begins with a 4-byte vptr.  0x160 opaque bytes + the 4-byte
    // handle place edict exactly at +0x168.
    std::array<std::byte, 0x160> opaque{};
    build4100::BaseHandle refHandle{};
    build4100::Edict* edict = nullptr;

private:
    build4100::IServerNetworkable* networkable_ = nullptr;
    void* collideable_ = nullptr;

    // Live build4100 SendProps reference offsets up to ~0x1458 in the selected
    // CS entity graph.  The reconstructed entity shim does not yet mirror every
    // original C++ field, so keep a zero-initialized backing tail large enough
    // for engine-side pointer arithmetic and safe zero-value proxies.
    std::array<std::byte, 0x1800> networkStateTail_{};
};

#if UINTPTR_MAX == 0xffffffffu
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(offsetof(EngineServerUnknownShim, edict) == 0x168,
              "build4100 CBaseEntity edict field must stay at +0x168");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

} // namespace cssv34::reconstructed::abi
