#include "abi/player_info_manager002_adapter.h"

namespace cssv34::reconstructed::abi {

PlayerInfoManager002Adapter& PlayerInfoManager002Instance() noexcept {
    static PlayerInfoManager002Adapter instance;
    return instance;
}

void PlayerInfoManager002Adapter::Bind(const EngineServer021Client* const engine,
                                       ClientRegistry* const clients,
                                       build4100::GlobalVarsBase* const globals) noexcept {
    engine_ = engine;
    clients_ = clients;
    globals_ = globals;
    const std::size_t maxClients = globals_ && globals_->maxClients > 0
        ? static_cast<std::size_t>(globals_->maxClients) : 0;
    for (std::size_t index = 0; index < playerInfo_.size(); ++index) {
        if (index > 0 && index <= maxClients) playerInfo_[index].Bind(engine_, clients_, index);
        else playerInfo_[index].Unbind();
    }
}

void PlayerInfoManager002Adapter::Unbind() noexcept {
    for (auto& info : playerInfo_) info.Unbind();
    engine_ = nullptr;
    clients_ = nullptr;
    globals_ = nullptr;
}

void* PlayerInfoManager002Adapter::GetPlayerInfo(build4100::Edict* const entity) {
    if (!entity || !engine_ || !engine_->IsValid() || !clients_ || !globals_) return nullptr;
    const int index = engine_->IndexOfEdict(entity);
    if (index <= 0 || index > globals_->maxClients || index >= static_cast<int>(playerInfo_.size())) {
        return nullptr;
    }
    const ClientRecord* const record = clients_->Get(static_cast<std::size_t>(index));
    if (!record || record->state == ClientLifecycleState::Empty || record->edict != entity) return nullptr;
    return &playerInfo_[static_cast<std::size_t>(index)];
}

build4100::GlobalVarsBase* PlayerInfoManager002Adapter::GetGlobalVars() {
    return globals_;
}

}  // namespace cssv34::reconstructed::abi
