#include "engine/engine_server.h"

#include <utility>

namespace cssv34::reconstructed {

EngineServer::EngineServer(const std::size_t maxEdicts) : edicts_(maxEdicts) {}

bool EngineServer::ConfigureClientSlots(const std::size_t maxClients) noexcept {
    return edicts_.ConfigureClientSlots(maxClients);
}

build4100::Edict* EngineServer::AcquireClientEdict(const std::size_t clientIndex) noexcept {
    return edicts_.AcquireClient(clientIndex);
}

bool EngineServer::ReleaseClientEdict(build4100::Edict* const edict) noexcept {
    return edicts_.ReleaseClient(edict);
}

build4100::Edict* EngineServer::CreateEdict(const int forcedIndex) noexcept {
    return edicts_.Create(forcedIndex);
}

bool EngineServer::RemoveEdict(build4100::Edict* const edict) noexcept {
    return edicts_.Remove(edict);
}

int EngineServer::IndexOfEdict(const build4100::Edict* const edict) const noexcept {
    return edicts_.IndexOf(edict);
}

void* EngineServer::PvAllocEntPrivateData(const std::size_t bytes) noexcept {
    return privateData_.Allocate(bytes);
}

bool EngineServer::FreeEntPrivateData(void* const pointer) noexcept {
    return privateData_.Free(pointer);
}

void EngineServer::BeginLevel(std::string mapName, std::string entityLump) {
    if (levelActive_) {
        EndLevel();
    }

    mapName_ = std::move(mapName);
    mapEntities_ = std::move(entityLump);
    levelActive_ = true;
    ++levelGeneration_;
}

void EngineServer::EndLevel() noexcept {
    edicts_.ResetLevel();
    levelActive_ = false;
}

const char* EngineServer::GetMapEntitiesString() const noexcept {
    return mapEntities_.c_str();
}

bool EngineServer::ServerCommand(const std::string_view command) {
    if (command.empty()) {
        return false;
    }

    pendingCommands_.emplace_back(command);
    return true;
}

std::size_t EngineServer::ServerExecute() {
    const std::size_t count = pendingCommands_.size();
    while (!pendingCommands_.empty()) {
        executedCommands_.push_back(std::move(pendingCommands_.front()));
        pendingCommands_.pop_front();
    }
    return count;
}

}  // namespace cssv34::reconstructed
