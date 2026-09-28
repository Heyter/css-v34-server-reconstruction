#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "engine/edict_pool.h"
#include "engine/private_data_allocator.h"

namespace cssv34::reconstructed {

// Source-level reconstruction of the proven VEngineServer021 semantics used by
// build4100 server_i486.so. Binary-facing calls stay in abi/ adapters.
class EngineServer {
public:
    explicit EngineServer(std::size_t maxEdicts);

    // Engine-owned client edicts occupy indices [1..maxClients]. Configure the
    // range at a clean level boundary before allocating dynamic entities.
    bool ConfigureClientSlots(std::size_t maxClients) noexcept;
    build4100::Edict* AcquireClientEdict(std::size_t clientIndex) noexcept;
    bool ReleaseClientEdict(build4100::Edict* edict) noexcept;
    std::size_t MaxClients() const noexcept { return edicts_.ReservedClientCount(); }

    build4100::Edict* CreateEdict(int forcedIndex = -1) noexcept;
    bool RemoveEdict(build4100::Edict* edict) noexcept;
    int IndexOfEdict(const build4100::Edict* edict) const noexcept;

    void* PvAllocEntPrivateData(std::size_t bytes) noexcept;
    bool FreeEntPrivateData(void* pointer) noexcept;

    void BeginLevel(std::string mapName, std::string entityLump);
    void EndLevel() noexcept;

    const char* GetMapEntitiesString() const noexcept;
    const std::string& MapName() const noexcept { return mapName_; }
    bool LevelActive() const noexcept { return levelActive_; }
    std::size_t LevelGeneration() const noexcept { return levelGeneration_; }

    // CVEngineServer::ServerCommand queues text. ServerExecute consumes the
    // current queue. Repeated commands are valid semantics; no call-number or
    // filename allowlist is used here.
    bool ServerCommand(std::string_view command);
    std::size_t ServerExecute();

    std::size_t PendingCommandCount() const noexcept { return pendingCommands_.size(); }
    const std::vector<std::string>& ExecutedCommands() const noexcept {
        return executedCommands_;
    }

    EdictPool& Edicts() noexcept { return edicts_; }
    const EdictPool& Edicts() const noexcept { return edicts_; }
    PrivateDataAllocator& PrivateData() noexcept { return privateData_; }
    const PrivateDataAllocator& PrivateData() const noexcept { return privateData_; }

private:
    EdictPool edicts_;
    PrivateDataAllocator privateData_;
    std::string mapName_;
    std::string mapEntities_;
    bool levelActive_ = false;
    std::size_t levelGeneration_ = 0;
    std::deque<std::string> pendingCommands_;
    std::vector<std::string> executedCommands_;
};

}  // namespace cssv34::reconstructed
