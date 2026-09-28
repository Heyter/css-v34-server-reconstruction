#include "server/server_game_dll.h"

#include <utility>

namespace cssv34::reconstructed {

bool ServerGameDll::DLLInit() noexcept {
    if (state_ != ServerLifecycleState::Cold) {
        return false;
    }
    state_ = ServerLifecycleState::DllReady;
    return true;
}

bool ServerGameDll::GameInit() noexcept {
    if (state_ != ServerLifecycleState::DllReady) {
        return false;
    }
    state_ = ServerLifecycleState::GameReady;
    return true;
}

bool ServerGameDll::LevelInit(std::string mapName, std::string entityLump) {
    if (state_ != ServerLifecycleState::GameReady) {
        return false;
    }

    engine_.BeginLevel(std::move(mapName), std::move(entityLump));
    state_ = ServerLifecycleState::LevelLoaded;
    return true;
}

bool ServerGameDll::ServerActivate() noexcept {
    if (state_ != ServerLifecycleState::LevelLoaded) {
        return false;
    }
    state_ = ServerLifecycleState::ServerActive;
    return true;
}

bool ServerGameDll::GameFrame(const bool simulating) noexcept {
    if (state_ != ServerLifecycleState::ServerActive) {
        return false;
    }
    if (simulating) {
        ++gameFrameCalls_;
    }
    return true;
}

bool ServerGameDll::PreClientUpdate(const bool simulating) noexcept {
    if (state_ != ServerLifecycleState::ServerActive) {
        return false;
    }
    if (simulating) {
        ++preClientUpdateCalls_;
    }
    return true;
}

bool ServerGameDll::LevelShutdown() noexcept {
    if (state_ != ServerLifecycleState::ServerActive &&
        state_ != ServerLifecycleState::LevelLoaded) {
        return false;
    }

    engine_.EndLevel();
    state_ = ServerLifecycleState::GameReady;
    return true;
}

bool ServerGameDll::GameShutdown() noexcept {
    if (state_ != ServerLifecycleState::GameReady) {
        return false;
    }
    state_ = ServerLifecycleState::GameStopped;
    return true;
}

bool ServerGameDll::DLLShutdown() noexcept {
    if (state_ != ServerLifecycleState::GameStopped) {
        return false;
    }
    state_ = ServerLifecycleState::DllStopped;
    return true;
}

}  // namespace cssv34::reconstructed
