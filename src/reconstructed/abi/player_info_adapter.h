#pragma once

#include <cstddef>

#include "abi/engine_server021_client.h"
#include "public/build4100/player_info.h"
#include "server/client_registry.h"

namespace cssv34::reconstructed::abi {

// Exact build4100 IPlayerInfo address-point geometry recovered from the
// original server_i486.so: 24 slots (0..23). There is deliberately no virtual
// destructor; adding one would shift every ABI slot.
class PlayerInfoAdapter final {
public:
    void Bind(const EngineServer021Client* engine, ClientRegistry* clients,
              std::size_t clientIndex) noexcept;
    void Unbind() noexcept;

    virtual const char* GetName();                         // 0
    virtual int GetUserID();                              // 1
    virtual const char* GetNetworkIDString();             // 2
    virtual int GetTeamIndex();                           // 3
    virtual void ChangeTeam(int team);                    // 4
    virtual int GetFragCount();                           // 5
    virtual int GetDeathCount();                          // 6
    virtual bool IsConnected();                           // 7
    virtual int GetArmorValue();                          // 8
    virtual bool IsHLTV();                                // 9
    virtual bool IsPlayer();                              // 10
    virtual bool IsFakeClient();                          // 11
    virtual bool IsDead();                                // 12
    virtual bool IsInAVehicle();                          // 13
    virtual bool IsObserver();                            // 14
    virtual build4100::Vector GetAbsOrigin();             // 15
    virtual build4100::QAngle GetAbsAngles();             // 16
    virtual build4100::Vector GetPlayerMins();            // 17
    virtual build4100::Vector GetPlayerMaxs();            // 18
    virtual const char* GetWeaponName();                   // 19
    virtual const char* GetModelName();                    // 20
    virtual int GetHealth();                              // 21
    virtual int GetMaxHealth();                           // 22
    virtual build4100::BotCmd GetLastUserCommand();       // 23

private:
    ClientRecord* Record() noexcept;
    const ClientRecord* Record() const noexcept;

    const EngineServer021Client* engine_ = nullptr;
    ClientRegistry* clients_ = nullptr;
    std::size_t clientIndex_ = 0;
    char networkIdCache_[64]{};
};

}  // namespace cssv34::reconstructed::abi
