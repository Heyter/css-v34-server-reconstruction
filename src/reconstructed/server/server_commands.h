#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "server/cs_game_state.h"
#include "server/game_events.h"

namespace cssv34::reconstructed {

class CVarRegistry final {
public:
    CVarRegistry();
    bool Set(const std::string& name, const std::string& value);
    const std::string* Get(const std::string& name) const noexcept;
    float GetFloat(const std::string& name, float fallback = 0.0f) const noexcept;
    int GetInt(const std::string& name, int fallback = 0) const noexcept;
    bool GetBool(const std::string& name, bool fallback = false) const noexcept;
    std::size_t Count() const noexcept { return values_.size(); }
private:
    std::unordered_map<std::string, std::string> values_;
};

enum class CommandResult {
    Executed,
    Unknown,
    InvalidArguments,
    Rejected,
};

class ServerCommandProcessor final {
public:
    ServerCommandProcessor(ClientRegistry& clients, CsGameState& game, GameEventBus& events)
        : clients_(clients), game_(game), events_(events) {}
    CommandResult ExecuteClient(std::size_t clientIndex, const std::string& line, float now);
    CommandResult ExecuteServer(const std::string& line, float now);
    CVarRegistry& CVars() noexcept { return cvars_; }
    const CVarRegistry& CVars() const noexcept { return cvars_; }
private:
    static std::vector<std::string> Tokenize(const std::string& line);
    bool BuyAlias(std::size_t clientIndex, const std::string& alias, float now);
    void ApplyCVars();
    ClientRegistry& clients_;
    CsGameState& game_;
    GameEventBus& events_;
    CVarRegistry cvars_;
};

} // namespace cssv34::reconstructed
