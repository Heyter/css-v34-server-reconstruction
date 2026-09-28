#include "abi/engine_cvar003_client.h"

namespace cssv34::reconstructed::abi {

void EngineCvar003Client::RegisterConCommandBase(void* const commandBase) const noexcept {
    using Fn = void (*)(void*, void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 5);
    if (fn && commandBase) fn(object_, commandBase);
}

void* EngineCvar003Client::FindVar(const char* const name) const noexcept {
    using Fn = void* (*)(void*, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 7);
    return fn && name ? fn(object_, name) : nullptr;
}

void* EngineCvar003Client::GetCommands() const noexcept {
    using Fn = void* (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 9);
    return fn ? fn(object_) : nullptr;
}

void EngineCvar003Client::UnlinkVariables(const int flags) const noexcept {
    using Fn = void (*)(void*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 10);
    if (fn) fn(object_, flags);
}

void EngineCvar003Client::InstallGlobalChangeCallback(
    const GlobalChangeCallbackFn callback
) const noexcept {
    using Fn = void (*)(void*, GlobalChangeCallbackFn);
    const Fn fn = detail::VTableSlot<Fn>(object_, 11);
    if (fn) fn(object_, callback);
}

}  // namespace cssv34::reconstructed::abi
