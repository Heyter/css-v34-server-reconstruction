#pragma once

#include "abi/vtable_client.h"

namespace cssv34::reconstructed::abi {

using GlobalChangeCallbackFn = void (*)(void* conVar, const char* oldString);

class EngineCvar003Client final {
public:
    EngineCvar003Client() noexcept = default;
    explicit EngineCvar003Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }

    void RegisterConCommandBase(void* commandBase) const noexcept;                 // slot 5
    void* FindVar(const char* name) const noexcept;                               // slot 7
    void* GetCommands() const noexcept;                                            // slot 9
    void UnlinkVariables(int flags) const noexcept;                                // slot 10
    void InstallGlobalChangeCallback(GlobalChangeCallbackFn callback) const noexcept; // slot 11

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
