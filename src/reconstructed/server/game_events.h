#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace cssv34::reconstructed {

struct GameEvent {
    std::string name;
    float time = 0.0f;
    std::unordered_map<std::string, int> ints;
    std::unordered_map<std::string, bool> bools;
    std::unordered_map<std::string, std::string> strings;
};

class GameEventBus final {
public:
    GameEvent& Emit(std::string name, float time);
    const std::vector<GameEvent>& Events() const noexcept { return events_; }
    const GameEvent* Last(const std::string& name) const noexcept;
    std::size_t Count(const std::string& name) const noexcept;
    void Clear() noexcept { events_.clear(); }
private:
    std::vector<GameEvent> events_;
};

} // namespace cssv34::reconstructed
