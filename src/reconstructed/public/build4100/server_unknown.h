#pragma once

#include <cstdint>

#include "public/build4100/server_networkable.h"

namespace cssv34::build4100 {

// Exact single-inheritance chain proven from build4100 RTTI:
// CBaseEntity -> IServerEntity -> IServerUnknown -> IHandleEntity.
// _ZTV13IHandleEntity is 0x18 bytes = 2 ABI header words + 4 slots.
// _ZTV14IServerUnknown is 0x24 bytes = 2 ABI header words + 7 slots.
struct BaseHandle {
    std::uint32_t value = 0xffffffffu;
};

class IHandleEntity {
public:
    virtual ~IHandleEntity() = default;                         // slots 0/1
    virtual void SetRefEHandle(const BaseHandle& handle) = 0;   // slot 2
    virtual const BaseHandle& GetRefEHandle() const = 0;        // slot 3
};

class IServerUnknown : public IHandleEntity {
public:
    ~IServerUnknown() override = default;                       // slots 0/1
    virtual void* GetCollideable() = 0;                         // slot 4
    virtual IServerNetworkable* GetNetworkable() = 0;           // slot 5
    virtual void* GetBaseEntity() = 0;                          // slot 6
};

} // namespace cssv34::build4100
