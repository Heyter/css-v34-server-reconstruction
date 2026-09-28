#pragma once

#include <cstddef>
#include <vector>

#include "public/build4100/edict.h"

namespace cssv34::reconstructed {

class EdictPool {
public:
    explicit EdictPool(std::size_t maxEdicts);

    // Client edicts are engine-owned slots [1..maxClients]. Dynamic entity
    // allocation must never consume that range.
    bool ConfigureClientSlots(std::size_t maxClients) noexcept;
    build4100::Edict* AcquireClient(std::size_t clientIndex) noexcept;
    bool ReleaseClient(build4100::Edict* edict) noexcept;

    build4100::Edict* Create(int forcedIndex = -1) noexcept;
    bool Remove(build4100::Edict* edict) noexcept;

    int IndexOf(const build4100::Edict* edict) const noexcept;
    build4100::Edict* At(std::size_t index) noexcept;
    const build4100::Edict* At(std::size_t index) const noexcept;

    void ResetLevel() noexcept;

    std::size_t Capacity() const noexcept { return edicts_.size(); }
    std::size_t ReservedClientCount() const noexcept { return reservedClientCount_; }
    bool IsClientIndex(std::size_t index) const noexcept;
    std::size_t ActiveClientCount() const noexcept;
    std::size_t ActiveDynamicCount() const noexcept;

private:
    void Activate(build4100::Edict& edict) noexcept;
    void Release(build4100::Edict& edict) noexcept;

    std::vector<build4100::Edict> edicts_;
    std::size_t reservedClientCount_ = 0;
};

}  // namespace cssv34::reconstructed
