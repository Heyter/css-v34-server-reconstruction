#pragma once
#include <cstddef>
#include "public/build4100/edict.h"
#include "public/build4100/check_transmit.h"

namespace cssv34::reconstructed::abi {
class EngineEntityBridge;
class EngineServer021Client;

class ServerGameEnts001Adapter final {
public:
    virtual ~ServerGameEnts001Adapter() = default;
    virtual void SetDebugEdictBase(build4100::Edict* base) noexcept;
    virtual void MarkEntitiesAsTouching(build4100::Edict* first, build4100::Edict* second) noexcept;
    virtual void FreeContainingEntity(build4100::Edict* edict) noexcept;
    virtual build4100::Edict* BaseEntityToEdict(void* entity) noexcept;
    virtual void* EdictToBaseEntity(build4100::Edict* edict) noexcept;
    virtual void CheckTransmit(void* info, const unsigned short* indices, int count) noexcept;

    void Bind(EngineEntityBridge* bridge, const EngineServer021Client* engine) noexcept;
    void Unbind() noexcept;
    std::size_t TouchCalls() const noexcept { return touchCalls_; }
    std::size_t CheckTransmitCalls() const noexcept { return checkTransmitCalls_; }
    std::size_t CheckTransmitMarked() const noexcept { return checkTransmitMarked_; }
    std::size_t CheckTransmitForcedGeneric() const noexcept { return checkTransmitForcedGeneric_; }
    std::size_t CheckTransmitFreshGeneric() const noexcept { return checkTransmitFreshGeneric_; }
private:
    build4100::Edict* debugBase_ = nullptr;
    std::size_t touchCalls_ = 0;
    std::size_t checkTransmitCalls_ = 0;
    std::size_t checkTransmitMarked_ = 0;
    std::size_t checkTransmitForcedGeneric_ = 0;
    std::size_t checkTransmitFreshGeneric_ = 0;
    bool checkTransmitProbeDone_ = false;
    bool checkTransmitForcedProbeDone_ = false;
    EngineEntityBridge* bridge_ = nullptr;
    const EngineServer021Client* engine_ = nullptr;
};
ServerGameEnts001Adapter& ServerGameEnts001Instance() noexcept;
} // namespace cssv34::reconstructed::abi
