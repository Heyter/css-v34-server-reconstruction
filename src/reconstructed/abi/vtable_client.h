#pragma once

#include <cstddef>
#include <cstring>

namespace cssv34::reconstructed::abi::detail {

template <typename Fn>
Fn VTableSlot(void* object, const std::size_t index) noexcept {
    if (!object) {
        return nullptr;
    }

    auto*** objectVtable = reinterpret_cast<void***>(object);
    if (!objectVtable || !*objectVtable || !(*objectVtable)[index]) {
        return nullptr;
    }

    Fn fn = nullptr;
    static_assert(sizeof(fn) == sizeof((*objectVtable)[index]));
    std::memcpy(&fn, &(*objectVtable)[index], sizeof(fn));
    return fn;
}

inline bool HasVTable(void* object) noexcept {
    if (!object) {
        return false;
    }
    auto*** objectVtable = reinterpret_cast<void***>(object);
    return objectVtable && *objectVtable;
}

}  // namespace cssv34::reconstructed::abi::detail
