#pragma once

#include "abi/vtable_client.h"

namespace cssv34::reconstructed::abi {

// engine_i486.so build4100 VoiceServer001 / CVoiceServer vtable:
//   slot 0 dtor, slot 1 deleting dtor,
//   slot 2 GetClientListening(receiver, sender),
//   slot 3 SetClientListening(receiver, sender, listen).
// Client indices are one-based; CVoiceServer decrements both arguments before
// touching the per-sender hearing mask.
class VoiceServer001Client final {
public:
    VoiceServer001Client() noexcept = default;
    explicit VoiceServer001Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }

    bool GetClientListening(int receiver, int sender) const noexcept {
        using Fn = bool (*)(void*, int, int);
        const Fn fn = detail::VTableSlot<Fn>(object_, 2);
        return fn ? fn(object_, receiver, sender) : false;
    }

    bool SetClientListening(int receiver, int sender, bool listen) const noexcept {
        using Fn = bool (*)(void*, int, int, bool);
        const Fn fn = detail::VTableSlot<Fn>(object_, 3);
        return fn ? fn(object_, receiver, sender, listen) : false;
    }

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
