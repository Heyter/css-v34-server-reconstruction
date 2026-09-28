#pragma once

#include <cstddef>
#include <unordered_map>

namespace cssv34::reconstructed {

class PrivateDataAllocator {
public:
    PrivateDataAllocator() = default;
    ~PrivateDataAllocator();

    PrivateDataAllocator(const PrivateDataAllocator&) = delete;
    PrivateDataAllocator& operator=(const PrivateDataAllocator&) = delete;

    void* Allocate(std::size_t bytes) noexcept;
    bool Free(void* pointer) noexcept;

    std::size_t ActiveCount() const noexcept { return active_.size(); }
    std::size_t AllocationSize(const void* pointer) const noexcept;

private:
    std::unordered_map<void*, std::size_t> active_;
};

}  // namespace cssv34::reconstructed
