#include "engine/edict_pool.h"

#include <cstdint>
#include <limits>

namespace cssv34::reconstructed {

EdictPool::EdictPool(const std::size_t maxEdicts) : edicts_(maxEdicts) {
    if (edicts_.empty()) {
        return;
    }

    // Index zero is the world edict and is never handed out by Create(-1).
    auto& world = edicts_[0];
    world.stateFlags &= ~build4100::kEdictFree;
    world.networkSerial = -1;
}

void EdictPool::Activate(build4100::Edict& edict) noexcept {
    const std::int32_t serial = edict.networkSerial;
    edict = build4100::Edict{};
    edict.stateFlags &= ~build4100::kEdictFree;
    edict.networkSerial = serial;
}

void EdictPool::Release(build4100::Edict& edict) noexcept {
    if (build4100::IsFree(edict)) {
        return;
    }

    edict.stateFlags |= build4100::kEdictFree;
    ++edict.networkSerial;
    edict.field08 = 0;
    edict.networkable0c = 0;
    edict.field10 = 0;
}

bool EdictPool::ConfigureClientSlots(const std::size_t maxClients) noexcept {
    if (edicts_.empty() || maxClients >= edicts_.size()) {
        return false;
    }

    // Changing ownership geometry while entities are live would let a dynamic
    // edict silently become a client edict (or vice versa). Require a clean
    // level boundary before reconfiguration.
    for (std::size_t index = 1; index < edicts_.size(); ++index) {
        if (!build4100::IsFree(edicts_[index])) {
            return false;
        }
    }

    reservedClientCount_ = maxClients;
    return true;
}

bool EdictPool::IsClientIndex(const std::size_t index) const noexcept {
    return index > 0 && index <= reservedClientCount_ && index < edicts_.size();
}

build4100::Edict* EdictPool::AcquireClient(const std::size_t clientIndex) noexcept {
    if (!IsClientIndex(clientIndex)) {
        return nullptr;
    }

    auto& candidate = edicts_[clientIndex];
    if (!build4100::IsFree(candidate)) {
        return nullptr;
    }

    Activate(candidate);
    return &candidate;
}

bool EdictPool::ReleaseClient(build4100::Edict* const edict) noexcept {
    const int index = IndexOf(edict);
    if (index <= 0 || !IsClientIndex(static_cast<std::size_t>(index))) {
        return false;
    }

    Release(edicts_[static_cast<std::size_t>(index)]);
    return true;
}

build4100::Edict* EdictPool::Create(const int forcedIndex) noexcept {
    if (edicts_.size() < 2) {
        return nullptr;
    }

    if (forcedIndex >= 0) {
        const auto index = static_cast<std::size_t>(forcedIndex);
        if (index == 0 || index >= edicts_.size() || IsClientIndex(index)) {
            return nullptr;
        }
        auto& candidate = edicts_[index];
        if (!build4100::IsFree(candidate)) {
            return nullptr;
        }
        Activate(candidate);
        return &candidate;
    }

    const std::size_t firstDynamic = reservedClientCount_ + 1;
    for (std::size_t index = firstDynamic; index < edicts_.size(); ++index) {
        auto& candidate = edicts_[index];
        if (build4100::IsFree(candidate)) {
            Activate(candidate);
            return &candidate;
        }
    }

    return nullptr;
}

bool EdictPool::Remove(build4100::Edict* const edict) noexcept {
    const int index = IndexOf(edict);
    if (index <= 0) {
        return false;
    }

    const auto position = static_cast<std::size_t>(index);
    if (IsClientIndex(position)) {
        // Client slots have separate ownership and must be released through
        // ReleaseClient(). This prevents dynamic-entity code from freeing an
        // engine-owned player edict.
        return false;
    }

    Release(edicts_[position]);
    return true;
}

int EdictPool::IndexOf(const build4100::Edict* const edict) const noexcept {
    if (!edict || edicts_.empty()) {
        return -1;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(edicts_.data());
    const auto address = reinterpret_cast<std::uintptr_t>(edict);
    const std::size_t bytes = edicts_.size() * sizeof(build4100::Edict);

    if (address < base || address - base >= bytes) {
        return -1;
    }

    const std::uintptr_t offset = address - base;
    if ((offset % sizeof(build4100::Edict)) != 0) {
        return -1;
    }

    const auto index = offset / sizeof(build4100::Edict);
    if (index > static_cast<std::uintptr_t>(std::numeric_limits<int>::max())) {
        return -1;
    }
    return static_cast<int>(index);
}

build4100::Edict* EdictPool::At(const std::size_t index) noexcept {
    return index < edicts_.size() ? &edicts_[index] : nullptr;
}

const build4100::Edict* EdictPool::At(const std::size_t index) const noexcept {
    return index < edicts_.size() ? &edicts_[index] : nullptr;
}

void EdictPool::ResetLevel() noexcept {
    for (std::size_t index = 1; index <= reservedClientCount_; ++index) {
        Release(edicts_[index]);
    }

    for (std::size_t index = reservedClientCount_ + 1; index < edicts_.size(); ++index) {
        Release(edicts_[index]);
    }

    if (!edicts_.empty()) {
        auto& world = edicts_[0];
        world.stateFlags &= ~build4100::kEdictFree;
        world.field08 = 0;
        world.networkable0c = 0;
        world.field10 = 0;
    }
}

std::size_t EdictPool::ActiveClientCount() const noexcept {
    std::size_t count = 0;
    for (std::size_t index = 1; index <= reservedClientCount_; ++index) {
        if (!build4100::IsFree(edicts_[index])) {
            ++count;
        }
    }
    return count;
}

std::size_t EdictPool::ActiveDynamicCount() const noexcept {
    std::size_t count = 0;
    for (std::size_t index = reservedClientCount_ + 1; index < edicts_.size(); ++index) {
        if (!build4100::IsFree(edicts_[index])) {
            ++count;
        }
    }
    return count;
}

}  // namespace cssv34::reconstructed
