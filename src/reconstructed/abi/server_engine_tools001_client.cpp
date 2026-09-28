#include "abi/server_engine_tools001_client.h"

namespace cssv34::reconstructed::abi {

void ServerEngineTools001Client::CallVoid(const std::size_t slot) const noexcept {
    using Fn = void (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, slot);
    if (fn) {
        fn(object_);
    }
}

void ServerEngineTools001Client::LevelInitPreEntityAllTools() const noexcept { CallVoid(2); }
void ServerEngineTools001Client::LevelInitPostEntityAllTools() const noexcept { CallVoid(3); }
void ServerEngineTools001Client::LevelShutdownPreEntityAllTools() const noexcept { CallVoid(4); }
void ServerEngineTools001Client::LevelShutdownPostEntityAllTools() const noexcept { CallVoid(5); }
void ServerEngineTools001Client::FrameUpdatePreEntityThink() const noexcept { CallVoid(6); }
void ServerEngineTools001Client::FrameUpdatePostEntityThink() const noexcept { CallVoid(7); }
void ServerEngineTools001Client::PreClientUpdate() const noexcept { CallVoid(8); }

}  // namespace cssv34::reconstructed::abi
