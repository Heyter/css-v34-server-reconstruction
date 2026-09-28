#include "abi/game_event_manager002_client.h"

namespace cssv34::reconstructed::abi {

int GameEventManager002Client::LoadEventsFromFile(const char* const filename) const noexcept {
    using Fn = int (*)(void*, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 2);
    return fn ? fn(object_, filename) : 0;
}

bool GameEventManager002Client::AddListener(void* const listener, const char* const name,
                                            const bool serverSide) const noexcept {
    using Fn = bool (*)(void*, void*, const char*, bool);
    const Fn fn = detail::VTableSlot<Fn>(object_, 4);
    return fn ? fn(object_, listener, name, serverSide) : false;
}

void GameEventClient::SetBool(const char* const key, const bool value) const noexcept {
    using Fn = void (*)(void*, const char*, bool);
    const Fn fn = detail::VTableSlot<Fn>(object_, 10);
    if (fn) fn(object_, key, value);
}

void GameEventClient::SetInt(const char* const key, const int value) const noexcept {
    using Fn = void (*)(void*, const char*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 11);
    if (fn) {
        fn(object_, key, value);
    }
}

void GameEventClient::SetFloat(const char* const key, const float value) const noexcept {
    using Fn = void (*)(void*, const char*, float);
    const Fn fn = detail::VTableSlot<Fn>(object_, 12);
    if (fn) fn(object_, key, value);
}

void GameEventClient::SetString(const char* const key, const char* const value) const noexcept {
    using Fn = void (*)(void*, const char*, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 13);
    if (fn) {
        fn(object_, key, value);
    }
}

void GameEventManager002Client::RemoveListener(void* const listener) const noexcept {
    using Fn = void (*)(void*, void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 6);
    if (fn) {
        fn(object_, listener);
    }
}

GameEventClient GameEventManager002Client::CreateEvent(
    const char* const name,
    const bool force
) const noexcept {
    using Fn = void* (*)(void*, const char*, bool);
    const Fn fn = detail::VTableSlot<Fn>(object_, 7);
    return GameEventClient(fn ? fn(object_, name, force) : nullptr);
}

bool GameEventManager002Client::FireEvent(
    const GameEventClient event,
    const bool dontBroadcast
) const noexcept {
    using Fn = bool (*)(void*, void*, bool);
    const Fn fn = detail::VTableSlot<Fn>(object_, 8);
    return fn ? fn(object_, event.Raw(), dontBroadcast) : false;
}

void GameEventManager002Client::FreeEvent(const GameEventClient event) const noexcept {
    using Fn = void (*)(void*, void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 11);
    if (fn && event.Raw()) {
        fn(object_, event.Raw());
    }
}

}  // namespace cssv34::reconstructed::abi
