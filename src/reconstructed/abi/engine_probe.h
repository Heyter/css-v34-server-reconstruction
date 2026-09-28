#pragma once

#ifndef V34_ENGINE_PROBE_ENABLED
#define V34_ENGINE_PROBE_ENABLED 1
#endif

namespace cssv34::reconstructed::abi {

// Optional real-engine breadcrumb channel.
//
// DEBUG profile: compiled in, but still runtime-disabled unless
// V34_ENGINE_PROBE_FILE points at a writable file.
// RELEASE profile: compiled out at the call site so release binaries do not
// depend on the diagnostic file channel. Keep DEBUG available for future
// soak/regression work.
#if V34_ENGINE_PROBE_ENABLED
void EngineProbe(const char* event, const char* detail = nullptr) noexcept;
#else
inline void EngineProbe(const char*, const char* = nullptr) noexcept {}
#endif

}  // namespace cssv34::reconstructed::abi
