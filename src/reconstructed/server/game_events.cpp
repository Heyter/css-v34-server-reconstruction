#include "server/game_events.h"

namespace cssv34::reconstructed {
GameEvent& GameEventBus::Emit(std::string name, const float time) {
    events_.push_back(GameEvent{std::move(name), time, {}, {}, {}});
    return events_.back();
}
const GameEvent* GameEventBus::Last(const std::string& name) const noexcept {
    for (auto it = events_.rbegin(); it != events_.rend(); ++it) if (it->name == name) return &*it;
    return nullptr;
}
std::size_t GameEventBus::Count(const std::string& name) const noexcept {
    std::size_t count = 0; for (const auto& e : events_) if (e.name == name) ++count; return count;
}
} // namespace cssv34::reconstructed
