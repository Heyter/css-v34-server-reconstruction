#pragma once

#include "abi/vtable_client.h"

namespace cssv34::reconstructed::abi {

class EngineRandom001Client final {
public:
    EngineRandom001Client() noexcept = default;
    explicit EngineRandom001Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }
    int RandomInt(int minValue, int maxValue) const noexcept;  // slot 2

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
