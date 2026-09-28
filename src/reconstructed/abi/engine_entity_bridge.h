#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "abi/engine_networkable_shim.h"
#include "abi/engine_server021_client.h"
#include "public/build4100/edict.h"
#include "server/entity_world.h"

namespace cssv34::reconstructed::abi {

struct EngineEntityBinding {
    build4100::Edict* edict = nullptr;
    void* baseEntity = nullptr;
    EntityHandle entity{};
    std::size_t clientIndex = 0;
    std::unique_ptr<EngineNetworkableShim> networkable;
    std::uint32_t previousStateFlags = build4100::kEdictFree;
    std::uint32_t previousNetworkable08 = 0;
    std::uint32_t previousUnknown0c = 0;
    std::uint32_t desiredTransmitState = build4100::kEdictTransmitFullCheck;
};

class EngineEntityBridge final {
public:
    void Bind(const EngineServer021Client* engine, std::size_t maxEntities = 2048);
    void Unbind() noexcept;
    void ServerActivate(build4100::Edict* base, int count) noexcept;
    bool AttachClient(std::size_t clientIndex, build4100::Edict* edict);
    EntityHandle AttachGeneric(build4100::Edict* edict, const char* classname,
                               std::uint32_t transmitState = build4100::kEdictTransmitFullCheck);
    bool Detach(build4100::Edict* edict, bool removeEngineEdict) noexcept;
    void ObserveBaseEntity(build4100::Edict* edict, void* baseEntity) noexcept;
    void Tick(float now) noexcept;

    build4100::Edict* EdictForBaseEntity(void* baseEntity) const noexcept;
    void* BaseEntityForEdict(build4100::Edict* edict) const noexcept;
    build4100::IServerNetworkable* NetworkableForEdict(build4100::Edict* edict) const noexcept;
    build4100::IServerUnknown* UnknownForEdict(build4100::Edict* edict) const noexcept;
    const EngineEntityBinding* BindingForEdict(build4100::Edict* edict) const noexcept;
    int EngineIndexForEdict(const build4100::Edict* edict) const noexcept;
    build4100::Edict* BoundEdictForEngineIndex(unsigned int index) const noexcept;
    std::size_t CollectForcedTransmitIndices(unsigned short* out, std::size_t capacity) const noexcept;
    bool SetGenericTransmitState(build4100::Edict* edict, std::uint32_t transmitState) noexcept;
    bool GenericTransmitStateForEngineIndex(unsigned int index, std::uint32_t& transmitState) const noexcept;
    std::size_t BoundCount() const noexcept { return bindings_.size(); }
    build4100::Edict* EngineEdictBase() const noexcept { return edictBase_; }
    int EngineEdictCount() const noexcept { return edictCount_; }

private:
    EngineEntityBinding* MutableBinding(build4100::Edict* edict) noexcept;
    std::uint32_t MakeHandle(build4100::Edict* edict, std::size_t fallbackIndex) const noexcept;
    const EngineServer021Client* engine_ = nullptr;
    EntityWorld world_{2048};
    std::vector<EngineEntityBinding> bindings_;
    build4100::Edict* edictBase_ = nullptr;
    int edictCount_ = 0;
};

}  // namespace cssv34::reconstructed::abi
