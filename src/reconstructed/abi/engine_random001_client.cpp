#include "abi/engine_random001_client.h"

namespace cssv34::reconstructed::abi {

int EngineRandom001Client::RandomInt(const int minValue, const int maxValue) const noexcept {
    using Fn = int (*)(void*, int, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 2);
    return fn ? fn(object_, minValue, maxValue) : minValue;
}

}  // namespace cssv34::reconstructed::abi
