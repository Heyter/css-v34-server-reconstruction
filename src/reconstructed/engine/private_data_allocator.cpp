#include "engine/private_data_allocator.h"

#include <cstdlib>
#include <cstring>

namespace cssv34::reconstructed {

PrivateDataAllocator::~PrivateDataAllocator() {
    for (const auto& [pointer, bytes] : active_) {
        (void)bytes;
        std::free(pointer);
    }
}

void* PrivateDataAllocator::Allocate(const std::size_t bytes) noexcept {
    if (bytes == 0) {
        return nullptr;
    }

    void* pointer = std::malloc(bytes);
    if (!pointer) {
        return nullptr;
    }

    std::memset(pointer, 0, bytes);
    active_.emplace(pointer, bytes);
    return pointer;
}

bool PrivateDataAllocator::Free(void* const pointer) noexcept {
    if (!pointer) {
        return true;
    }

    const auto it = active_.find(pointer);
    if (it == active_.end()) {
        return false;
    }

    std::free(pointer);
    active_.erase(it);
    return true;
}

std::size_t PrivateDataAllocator::AllocationSize(const void* const pointer) const noexcept {
    if (!pointer) {
        return 0;
    }

    const auto it = active_.find(const_cast<void*>(pointer));
    return it == active_.end() ? 0 : it->second;
}

}  // namespace cssv34::reconstructed
