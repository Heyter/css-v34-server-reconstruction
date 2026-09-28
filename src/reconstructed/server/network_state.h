#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "server/client_registry.h"
#include "server/cs_game_state.h"

namespace cssv34::reconstructed {

struct NetworkPlayerState {
    std::uint8_t index = 0;
    std::int8_t team = 0;
    std::int16_t health = 0;
    std::int16_t armor = 0;
    std::int16_t frags = 0;
    std::int16_t deaths = 0;
    std::int32_t money = 0;
    std::uint8_t flags = 0;
};

struct NetworkGameState {
    std::uint32_t sequence = 0;
    std::int32_t round = 0;
    std::int16_t tScore = 0;
    std::int16_t ctScore = 0;
    std::uint8_t roundPhase = 0;
    std::uint8_t objective = 0;
    std::vector<NetworkPlayerState> players;
};

class NetworkStatePublisher final {
public:
    NetworkGameState Capture(const ClientRegistry& clients, const CsGameState& game);
    const NetworkGameState& Last() const noexcept { return last_; }
private:
    std::uint32_t nextSequence_ = 1;
    NetworkGameState last_;
};

} // namespace cssv34::reconstructed
