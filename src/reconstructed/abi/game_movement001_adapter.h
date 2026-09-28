#pragma once

#include <cstddef>

#include "public/build4100/vector.h"

namespace cssv34::reconstructed::abi {

// Exact build4100 IGameMovement/CGameMovement address-point geometry recovered
// from the original derived vtable: destructor pair, ProcessMovement, then the
// three hull/view accessors. Reset/prediction tracking exist in later SDKs but
// are NOT virtual slots in CSS build4100.
class GameMovement001Adapter final {
public:
    virtual ~GameMovement001Adapter() = default;                                  // slots 0,1
    virtual void ProcessMovement(void* player, void* moveData) noexcept;           // slot 2
    virtual const build4100::Vector& GetPlayerMins(bool ducked) const noexcept;    // slot 3
    virtual const build4100::Vector& GetPlayerMaxs(bool ducked) const noexcept;    // slot 4
    virtual const build4100::Vector& GetPlayerViewOffset(bool ducked) const noexcept; // slot 5

    // Non-virtual helpers for standalone reconstruction tests. Deliberately do
    // not enter the build4100 interface vtable.
    void Reset() noexcept;
    void StartTrackPredictionErrors(void* player) noexcept;
    void FinishTrackPredictionErrors(void* player) noexcept;
    std::size_t ProcessCalls() const noexcept { return processCalls_; }

private:
    void* movingPlayer_ = nullptr;
    std::size_t processCalls_ = 0;
};

GameMovement001Adapter& GameMovement001Instance() noexcept;

} // namespace cssv34::reconstructed::abi
