#include "server/lag_compensation.h"

#include <algorithm>
#include <limits>

namespace cssv34::reconstructed {

bool LagCompensationHistory::Configure(const std::size_t maxClients,
                                       const std::size_t recordsPerClient) {
    capacity_ = std::max<std::size_t>(1, recordsPerClient);
    clients_.assign(maxClients + 1, ClientHistory{});
    for (auto& client : clients_) client.records.resize(capacity_);
    return true;
}

void LagCompensationHistory::Reset() noexcept {
    for (auto& client : clients_) {
        client.next = 0;
        client.count = 0;
    }
}

bool LagCompensationHistory::ResetClient(const std::size_t clientIndex) noexcept {
    if (clientIndex == 0 || clientIndex >= clients_.size()) return false;
    clients_[clientIndex].next = 0;
    clients_[clientIndex].count = 0;
    return true;
}

bool LagCompensationHistory::Record(const std::size_t clientIndex,
                                    const LagCompensationRecord& record) noexcept {
    if (clientIndex == 0 || clientIndex >= clients_.size() || capacity_ == 0) return false;
    ClientHistory& history = clients_[clientIndex];

    // Multiple callbacks may sample the same server tick. Replace the newest
    // sample instead of wasting history depth or selecting an older origin.
    if (history.count > 0) {
        const std::size_t newest = (history.next + capacity_ - 1) % capacity_;
        if (history.records[newest].tick == record.tick) {
            history.records[newest] = record;
            return true;
        }
    }

    history.records[history.next] = record;
    history.next = (history.next + 1) % capacity_;
    history.count = std::min(capacity_, history.count + 1);
    return true;
}

bool LagCompensationHistory::Select(const std::size_t clientIndex,
                                    const std::int32_t requestedTick,
                                    const std::int32_t currentTick,
                                    const std::int32_t maxRewindTicks,
                                    LagCompensationRecord& out) const noexcept {
    if (clientIndex == 0 || clientIndex >= clients_.size() || capacity_ == 0) return false;
    const ClientHistory& history = clients_[clientIndex];
    if (history.count == 0) return false;

    const std::int32_t targetTick = std::min(requestedTick, currentTick);
    bool found = false;
    std::int32_t bestTick = std::numeric_limits<std::int32_t>::min();
    LagCompensationRecord best{};

    for (std::size_t n = 0; n < history.count; ++n) {
        const std::size_t index = (history.next + capacity_ - 1 - n) % capacity_;
        const LagCompensationRecord& candidate = history.records[index];
        if (candidate.tick > targetTick) continue;
        if (!found || candidate.tick > bestTick) {
            best = candidate;
            bestTick = candidate.tick;
            found = true;
        }
    }

    // A just-connected client can have only a record newer than the usercmd.
    // Prefer that newest sample rather than dropping a valid shot entirely.
    if (!found) {
        const std::size_t newest = (history.next + capacity_ - 1) % capacity_;
        best = history.records[newest];
        bestTick = best.tick;
        found = true;
    }

    const std::int32_t age = currentTick >= bestTick ? currentTick - bestTick : 0;
    if (maxRewindTicks >= 0 && age > maxRewindTicks) return false;
    out = best;
    return true;
}

std::size_t LagCompensationHistory::Count(const std::size_t clientIndex) const noexcept {
    return clientIndex < clients_.size() ? clients_[clientIndex].count : 0;
}

}  // namespace cssv34::reconstructed
