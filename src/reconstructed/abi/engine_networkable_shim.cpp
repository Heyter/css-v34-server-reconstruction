#include "abi/engine_networkable_shim.h"

#include <cstdio>

#include "abi/engine_probe.h"

namespace cssv34::reconstructed::abi {

EngineNetworkableShim::EngineNetworkableShim(build4100::Edict* const edict,
                                               build4100::ServerClass* const serverClass,
                                               const char* const className,
                                               const std::uint32_t entityHandle) noexcept {
    Rebind(edict, serverClass, className, entityHandle);
}

void EngineNetworkableShim::Rebind(build4100::Edict* const edict,
                                   build4100::ServerClass* const serverClass,
                                   const char* const className,
                                   const std::uint32_t entityHandle) noexcept {
    edict_ = edict;
    serverClass_ = serverClass;
    className_ = className ? className : "entity";
    entityHandle_ = entityHandle;
    eflags_ = 0;
    areaNum_ = 0;
    released_ = false;
    entityHandleProbeDone_ = false;
    serverClassProbeDone_ = false;
    edictProbeDone_ = false;
    pvsInfoProbeDone_ = false;
    pvsInfo_ = {};
    unknown_.Rebind(edict, this, entityHandle);
}

build4100::IHandleEntity* EngineNetworkableShim::GetEntityHandle() {
    if (!entityHandleProbeDone_) {
        entityHandleProbeDone_ = true;
        char detail[160];
        std::snprintf(detail, sizeof(detail), "edict=%p class=%s handle=0x%08x ptr=%p",
                      static_cast<void*>(edict_), className_ ? className_ : "", entityHandle_,
                      static_cast<void*>(&unknown_));
        EngineProbe("NetworkableGetEntityHandle_OK", detail);
    }
    return &unknown_;
}
build4100::ServerClass* EngineNetworkableShim::GetServerClass() {
    if (!serverClassProbeDone_) {
        serverClassProbeDone_ = true;
        char detail[144];
        std::snprintf(detail, sizeof(detail), "edict=%p class=%s serverclass=%p",
                      static_cast<void*>(edict_), className_ ? className_ : "",
                      static_cast<void*>(serverClass_));
        EngineProbe("NetworkableGetServerClass_OK", detail);
    }
    return serverClass_;
}
int EngineNetworkableShim::GetEFlags() const { return eflags_; }
void EngineNetworkableShim::AddEFlags(const int flags) { eflags_ |= flags; }
build4100::Edict* EngineNetworkableShim::GetEdict() const {
    if (!edictProbeDone_) {
        edictProbeDone_ = true;
        char detail[112];
        std::snprintf(detail, sizeof(detail), "edict=%p class=%s",
                      static_cast<void*>(edict_), className_ ? className_ : "");
        EngineProbe("NetworkableGetEdict_OK", detail);
    }
    return edict_;
}
const char* EngineNetworkableShim::GetClassName() const { return className_; }
void EngineNetworkableShim::Release() { released_ = true; }
int EngineNetworkableShim::AreaNum() const { return areaNum_; }
void* EngineNetworkableShim::GetBaseNetworkable() { return nullptr; }
void* EngineNetworkableShim::GetBaseEntity() { return unknown_.GetBaseEntity(); }
build4100::PVSInfo* EngineNetworkableShim::GetPVSInfo() {
    if (!pvsInfoProbeDone_) {
        pvsInfoProbeDone_ = true;
        char detail[128];
        std::snprintf(detail, sizeof(detail), "edict=%p class=%s ptr=%p bytes=124",
                      static_cast<void*>(edict_), className_ ? className_ : "",
                      static_cast<void*>(&pvsInfo_));
        EngineProbe("NetworkableGetPVSInfo_OK", detail);
    }
    return &pvsInfo_;
}

} // namespace cssv34::reconstructed::abi
