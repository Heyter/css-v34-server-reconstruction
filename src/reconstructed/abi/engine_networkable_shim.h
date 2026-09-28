#pragma once

#include <cstdint>

#include "abi/engine_unknown_shim.h"
#include "public/build4100/server_networkable.h"

namespace cssv34::reconstructed::abi {

class EngineNetworkableShim final : public build4100::IServerNetworkable {
public:
    EngineNetworkableShim() = default;
    EngineNetworkableShim(build4100::Edict* edict, build4100::ServerClass* serverClass,
                          const char* className, std::uint32_t entityHandle) noexcept;

    void Rebind(build4100::Edict* edict, build4100::ServerClass* serverClass,
                const char* className, std::uint32_t entityHandle) noexcept;

    build4100::IHandleEntity* GetEntityHandle() override;
    build4100::ServerClass* GetServerClass() override;
    int GetEFlags() const override;
    void AddEFlags(int flags) override;
    build4100::Edict* GetEdict() const override;
    const char* GetClassName() const override;
    void Release() override;
    int AreaNum() const override;
    void* GetBaseNetworkable() override;
    void* GetBaseEntity() override;
    build4100::PVSInfo* GetPVSInfo() override;
    ~EngineNetworkableShim() override = default;

    bool Released() const noexcept { return released_; }
    EngineServerUnknownShim& Unknown() noexcept { return unknown_; }
    const EngineServerUnknownShim& Unknown() const noexcept { return unknown_; }

private:
    build4100::Edict* edict_ = nullptr;
    build4100::ServerClass* serverClass_ = nullptr;
    const char* className_ = "entity";
    std::uint32_t entityHandle_ = 0xffffffffu;
    int eflags_ = 0;
    int areaNum_ = 0;
    bool released_ = false;
    bool entityHandleProbeDone_ = false;
    bool serverClassProbeDone_ = false;
    mutable bool edictProbeDone_ = false;
    bool pvsInfoProbeDone_ = false;
    EngineServerUnknownShim unknown_{};

    // build4100 CServerNetworkProperty keeps this exact 0x7c-byte structure
    // inline. CVEngineServer::BuildEntityClusterList writes it in-place.
    build4100::PVSInfo pvsInfo_{};
};

} // namespace cssv34::reconstructed::abi
