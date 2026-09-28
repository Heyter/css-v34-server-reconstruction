#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "public/build4100/vector.h"

namespace cssv34::reconstructed {

struct LagCompensationRecord {
    std::int32_t tick = 0;
    float simulationTime = 0.0f;
    build4100::Vector origin{};
    build4100::Vector mins{};
    build4100::Vector maxs{};
    bool alive = false;
};

class LagCompensationHistory final {
public:
    bool Configure(std::size_t maxClients, std::size_t recordsPerClient = 64);
    void Reset() noexcept;
    bool ResetClient(std::size_t clientIndex) noexcept;
    bool Record(std::size_t clientIndex, const LagCompensationRecord& record) noexcept;

    // Select the newest record at or before requestedTick. Future requests use
    // the newest available record. Records older than maxRewindTicks are
    // rejected so stale positions cannot become hittable after long stalls.
    bool Select(std::size_t clientIndex, std::int32_t requestedTick,
                std::int32_t currentTick, std::int32_t maxRewindTicks,
                LagCompensationRecord& out) const noexcept;

    std::size_t Count(std::size_t clientIndex) const noexcept;

private:
    struct ClientHistory {
        std::vector<LagCompensationRecord> records;
        std::size_t next = 0;
        std::size_t count = 0;
    };

    std::vector<ClientHistory> clients_;
    std::size_t capacity_ = 0;
};

}  // namespace cssv34::reconstructed
