#pragma once

#include "abi/vtable_client.h"

namespace cssv34::reconstructed::abi {

class EngineSoundServer003Client final {
public:
    EngineSoundServer003Client() noexcept = default;
    explicit EngineSoundServer003Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }
    float GetSoundDuration(const char* sample) const noexcept;  // slot 3

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
