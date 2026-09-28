#pragma once

#include "abi/vtable_client.h"

namespace cssv34::reconstructed::abi {

class GameEventClient final {
public:
    GameEventClient() noexcept = default;
    explicit GameEventClient(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }

    void SetBool(const char* key, bool value) const noexcept;           // slot 10
    void SetInt(const char* key, int value) const noexcept;             // slot 11
    void SetFloat(const char* key, float value) const noexcept;         // slot 12
    void SetString(const char* key, const char* value) const noexcept;  // slot 13

    void* Raw() const noexcept { return object_; }

private:
    void* object_ = nullptr;
};

class GameEventManager002Client final {
public:
    GameEventManager002Client() noexcept = default;
    explicit GameEventManager002Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }

    int LoadEventsFromFile(const char* filename) const noexcept;          // slot 2
    bool AddListener(void* listener, const char* name, bool serverSide) const noexcept; // slot 4
    void RemoveListener(void* listener) const noexcept;                  // slot 6
    GameEventClient CreateEvent(const char* name, bool force) const noexcept; // slot 7
    bool FireEvent(GameEventClient event, bool dontBroadcast) const noexcept; // slot 8
    void FreeEvent(GameEventClient event) const noexcept;                // slot 11

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
