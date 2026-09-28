#include "abi/engine_sound_server003_client.h"

namespace cssv34::reconstructed::abi {

float EngineSoundServer003Client::GetSoundDuration(const char* const sample) const noexcept {
    using Fn = float (*)(void*, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 3);
    return fn ? fn(object_, sample) : 0.0f;
}

}  // namespace cssv34::reconstructed::abi
