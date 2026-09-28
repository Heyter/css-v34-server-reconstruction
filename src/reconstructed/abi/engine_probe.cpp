#include "abi/engine_probe.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace cssv34::reconstructed::abi {

#if V34_ENGINE_PROBE_ENABLED
void EngineProbe(const char* const event, const char* const detail) noexcept {
    if (!event || !*event) return;
    const char* const path = std::getenv("V34_ENGINE_PROBE_FILE");
    if (!path || !*path) return;

    // v1.8.35g gatefix1: current combat fidelity telemetry is >512 bytes.
    // Keep a complete probe record (including the trailing newline) so the next
    // event cannot be concatenated onto a silently truncated line.
    char line[1024];
    const int length = detail && *detail
        ? std::snprintf(line, sizeof(line), "%s %s\n", event, detail)
        : std::snprintf(line, sizeof(line), "%s\n", event);
    if (length <= 0) return;
    const std::size_t bytes = static_cast<std::size_t>(length) < sizeof(line)
        ? static_cast<std::size_t>(length)
        : sizeof(line) - 1;

    const int fd = ::open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    const char* cursor = line;
    std::size_t remaining = bytes;
    while (remaining != 0) {
        const ssize_t written = ::write(fd, cursor, remaining);
        if (written > 0) {
            cursor += written;
            remaining -= static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        break;
    }
    (void)::close(fd);
}

#endif  // V34_ENGINE_PROBE_ENABLED

}  // namespace cssv34::reconstructed::abi
