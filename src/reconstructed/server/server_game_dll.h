#pragma once

#include <cstddef>
#include <string>

#include "engine/engine_server.h"

namespace cssv34::reconstructed {

enum class ServerLifecycleState {
    Cold,
    DllReady,
    GameReady,
    LevelLoaded,
    ServerActive,
    GameStopped,
    DllStopped,
};

// Source-level lifecycle shell for the runtime-proven ServerGameDLL006 order.
// Gameplay classes are deliberately not fabricated here; they will be moved in
// subsystem-by-subsystem behind this stable lifecycle boundary.
class ServerGameDll {
public:
    explicit ServerGameDll(EngineServer& engine) noexcept : engine_(engine) {}

    bool DLLInit() noexcept;
    bool GameInit() noexcept;
    bool LevelInit(std::string mapName, std::string entityLump);
    bool ServerActivate() noexcept;
    bool GameFrame(bool simulating) noexcept;
    bool PreClientUpdate(bool simulating) noexcept;
    bool LevelShutdown() noexcept;
    bool GameShutdown() noexcept;
    bool DLLShutdown() noexcept;

    ServerLifecycleState State() const noexcept { return state_; }
    std::size_t GameFrameCalls() const noexcept { return gameFrameCalls_; }
    std::size_t PreClientUpdateCalls() const noexcept { return preClientUpdateCalls_; }

private:
    EngineServer& engine_;
    ServerLifecycleState state_ = ServerLifecycleState::Cold;
    std::size_t gameFrameCalls_ = 0;
    std::size_t preClientUpdateCalls_ = 0;
};

}  // namespace cssv34::reconstructed
