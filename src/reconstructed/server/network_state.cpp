#include "server/network_state.h"

#include <algorithm>

namespace cssv34::reconstructed {
NetworkGameState NetworkStatePublisher::Capture(const ClientRegistry& clients, const CsGameState& game) {
    NetworkGameState snapshot;
    snapshot.sequence = nextSequence_++;
    snapshot.round = game.RoundNumber();
    snapshot.tScore = static_cast<std::int16_t>(game.TerroristScore());
    snapshot.ctScore = static_cast<std::int16_t>(game.CounterTerroristScore());
    snapshot.roundPhase = static_cast<std::uint8_t>(game.RoundPhase());
    snapshot.objective = static_cast<std::uint8_t>(game.MapObjective());
    for (std::size_t i = 1; i <= clients.MaxClients(); ++i) {
        const ClientRecord* p = clients.Get(i);
        if (!p || p->state == ClientLifecycleState::Empty) continue;
        NetworkPlayerState net;
        net.index = static_cast<std::uint8_t>(i);
        net.team = static_cast<std::int8_t>(p->teamIndex);
        net.health = static_cast<std::int16_t>(std::clamp(p->health, -32768, 32767));
        net.armor = static_cast<std::int16_t>(std::clamp(p->armorValue, -32768, 32767));
        net.frags = static_cast<std::int16_t>(std::clamp(p->fragCount, -32768, 32767));
        net.deaths = static_cast<std::int16_t>(std::clamp(p->deathCount, -32768, 32767));
        net.money = p->account;
        if (p->isDead) net.flags |= 0x01;
        if (p->hasHelmet) net.flags |= 0x02;
        if (p->hasDefuser) net.flags |= 0x04;
        if (p->hasC4) net.flags |= 0x08;
        if (p->inBuyZone) net.flags |= 0x10;
        snapshot.players.push_back(net);
    }
    last_ = snapshot;
    return snapshot;
}
} // namespace cssv34::reconstructed
