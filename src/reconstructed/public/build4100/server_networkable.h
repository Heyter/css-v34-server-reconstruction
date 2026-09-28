#pragma once

#include <cstddef>
#include <cstdint>

#include "public/build4100/edict.h"
#include "public/build4100/sendtable.h"

namespace cssv34::build4100 {

class IHandleEntity;

// Exact build4100 PVSInfo_t recovered from CVEngineServer::BuildEntityClusterList.
// The engine writes this structure through IServerNetworkable::GetPVSInfo().
struct PVSInfo {
    std::int32_t clusterCount = 0;
    std::int32_t clusters[24]{};
    std::int32_t headNode = 0;
    std::int32_t areaNum = 0;
    std::int32_t areaNum2 = 0;
    float center[3]{};
};

static_assert(sizeof(PVSInfo) == 0x7c, "build4100 PVSInfo_t must be 124 bytes");
static_assert(offsetof(PVSInfo, clusters) == 0x04);
static_assert(offsetof(PVSInfo, headNode) == 0x64);
static_assert(offsetof(PVSInfo, areaNum) == 0x68);
static_assert(offsetof(PVSInfo, areaNum2) == 0x6c);
static_assert(offsetof(PVSInfo, center) == 0x70);

// Exact address-point order proven from _ZTV18IServerNetworkable in build4100
// (0x3c bytes total = 2 Itanium ABI header words + 13 slots).  The destructor
// is declared last because the original D1/D0 entries are slots 11/12.
class IServerNetworkable {
public:
    virtual IHandleEntity* GetEntityHandle() = 0; // 0
    virtual ServerClass* GetServerClass() = 0;   // 1
    virtual int GetEFlags() const = 0;           // 2
    virtual void AddEFlags(int flags) = 0;       // 3
    virtual Edict* GetEdict() const = 0;         // 4
    virtual const char* GetClassName() const = 0;// 5
    virtual void Release() = 0;                  // 6
    virtual int AreaNum() const = 0;             // 7
    virtual void* GetBaseNetworkable() = 0;      // 8
    virtual void* GetBaseEntity() = 0;           // 9
    virtual PVSInfo* GetPVSInfo() = 0;           // 10
    virtual ~IServerNetworkable() = default;     // 11/12
};

} // namespace cssv34::build4100
