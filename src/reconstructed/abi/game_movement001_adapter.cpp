#include "abi/game_movement001_adapter.h"

namespace cssv34::reconstructed::abi {
namespace {
constexpr build4100::Vector kStandMins{-16.0f, -16.0f, 0.0f};
constexpr build4100::Vector kStandMaxs{16.0f, 16.0f, 72.0f};
constexpr build4100::Vector kDuckMins{-16.0f, -16.0f, 0.0f};
constexpr build4100::Vector kDuckMaxs{16.0f, 16.0f, 36.0f};
constexpr build4100::Vector kStandView{0.0f, 0.0f, 64.0f};
constexpr build4100::Vector kDuckView{0.0f, 0.0f, 28.0f};
}

void GameMovement001Adapter::ProcessMovement(void* const player, void* const moveData) noexcept {
    if (player && moveData) {
        movingPlayer_ = player;
        ++processCalls_;
    }
}

const build4100::Vector& GameMovement001Adapter::GetPlayerMins(const bool ducked) const noexcept {
    return ducked ? kDuckMins : kStandMins;
}
const build4100::Vector& GameMovement001Adapter::GetPlayerMaxs(const bool ducked) const noexcept {
    return ducked ? kDuckMaxs : kStandMaxs;
}
const build4100::Vector& GameMovement001Adapter::GetPlayerViewOffset(const bool ducked) const noexcept {
    return ducked ? kDuckView : kStandView;
}

void GameMovement001Adapter::Reset() noexcept { movingPlayer_ = nullptr; }
void GameMovement001Adapter::StartTrackPredictionErrors(void* const player) noexcept { movingPlayer_ = player; }
void GameMovement001Adapter::FinishTrackPredictionErrors(void* const player) noexcept {
    if (movingPlayer_ == player) movingPlayer_ = nullptr;
}

GameMovement001Adapter& GameMovement001Instance() noexcept {
    static GameMovement001Adapter value;
    return value;
}

} // namespace cssv34::reconstructed::abi
