#include "abi/engine_unknown_shim.h"

namespace cssv34::reconstructed::abi {

void EngineServerUnknownShim::Rebind(build4100::Edict* const newEdict,
                                     build4100::IServerNetworkable* const networkable,
                                     const std::uint32_t entityHandle) noexcept {
    edict = newEdict;
    networkable_ = networkable;
    refHandle.value = entityHandle;
    collideable_ = nullptr;
}

void EngineServerUnknownShim::SetRefEHandle(const build4100::BaseHandle& handle) {
    refHandle = handle;
}

const build4100::BaseHandle& EngineServerUnknownShim::GetRefEHandle() const {
    return refHandle;
}

void* EngineServerUnknownShim::GetCollideable() { return collideable_; }
build4100::IServerNetworkable* EngineServerUnknownShim::GetNetworkable() { return networkable_; }
void* EngineServerUnknownShim::GetBaseEntity() { return this; }

} // namespace cssv34::reconstructed::abi
