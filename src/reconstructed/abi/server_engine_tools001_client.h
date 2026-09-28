#pragma once

#include "abi/vtable_client.h"

namespace cssv34::reconstructed::abi {

class ServerEngineTools001Client final {
public:
    ServerEngineTools001Client() noexcept = default;
    explicit ServerEngineTools001Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }

    void LevelInitPreEntityAllTools() const noexcept;       // slot 2
    void LevelInitPostEntityAllTools() const noexcept;      // slot 3
    void LevelShutdownPreEntityAllTools() const noexcept;   // slot 4
    void LevelShutdownPostEntityAllTools() const noexcept;  // slot 5
    void FrameUpdatePreEntityThink() const noexcept;        // slot 6
    void FrameUpdatePostEntityThink() const noexcept;       // slot 7
    void PreClientUpdate() const noexcept;                  // slot 8

private:
    void CallVoid(std::size_t slot) const noexcept;
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
