#pragma once

#include <array>
#include <cstddef>

#include "abi/engine_server021_client.h"
#include "abi/player_info_adapter.h"
#include "public/build4100/edict.h"
#include "public/build4100/globalvars.h"
#include "server/client_registry.h"

namespace cssv34::reconstructed::abi {

class PlayerInfoManager002Adapter final {
public:
    void Bind(const EngineServer021Client* engine, ClientRegistry* clients,
              build4100::GlobalVarsBase* globals) noexcept;
    void Unbind() noexcept;

    virtual void* GetPlayerInfo(build4100::Edict* entity); // slot 0
    virtual build4100::GlobalVarsBase* GetGlobalVars();    // slot 1

private:
    const EngineServer021Client* engine_ = nullptr;
    ClientRegistry* clients_ = nullptr;
    build4100::GlobalVarsBase* globals_ = nullptr;
    std::array<PlayerInfoAdapter, 65> playerInfo_{};
};

PlayerInfoManager002Adapter& PlayerInfoManager002Instance() noexcept;

}  // namespace cssv34::reconstructed::abi
