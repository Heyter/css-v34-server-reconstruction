#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <array>

#include "public/build4100/edict.h"
#include "server/client_registry.h"
#include "server/cs_game_state.h"
#include "server/entity_world.h"
#include "server/game_events.h"
#include "server/network_state.h"
#include "server/server_commands.h"
#include "server/weapon_runtime.h"

namespace cssv34::reconstructed {

struct RuntimePlayer {
    build4100::Edict edict{};
    EntityHandle entity{};
};

class GameRuntime final {
public:
    bool Initialize(std::size_t maxClients);
    bool ConnectPlayer(std::size_t index, const std::string& name, const std::string& address = "loopback");
    bool DisconnectPlayer(std::size_t index);
    bool JoinTeam(std::size_t index, int team);
    bool EnterGame(std::size_t index);

    void ConfigureObjective(CsMapObjective objective, std::size_t hostageCount = 0);
    void StartRound(float now);
    CsRoundTickResult Tick(float now, float frameTime);
    DamageApplyResult FireAt(std::size_t attacker, std::size_t victim,
                             build4100::HitGroup hitGroup, float now);
    WeaponActionResult Reload(std::size_t clientIndex, float now);
    bool SelectWeapon(std::size_t clientIndex, build4100::CSWeaponID weapon, float now);
    EntityHandle DropWeapon(std::size_t clientIndex, build4100::CSWeaponID weapon, float now);
    bool PickupWeapon(std::size_t clientIndex, EntityHandle weaponEntity, float now);
    bool ThrowGrenade(std::size_t clientIndex, build4100::CSWeaponID grenade, float now);
    bool PlantBomb(std::size_t clientIndex, float now);
    bool BeginDefuse(std::size_t clientIndex, float now);

    CommandResult ClientCommand(std::size_t index, const std::string& command, float now);
    CommandResult ServerCommand(const std::string& command, float now);

    ClientRegistry& Clients() noexcept { return clients_; }
    CsGameState& Game() noexcept { return game_; }
    EntityWorld& Entities() noexcept { return entities_; }
    GameEventBus& Events() noexcept { return events_; }
    const NetworkGameState& NetworkState() const noexcept { return network_.Last(); }
    WeaponRuntimeController& WeaponRuntime() noexcept { return weapons_; }
    const WeaponRuntimeController& WeaponRuntime() const noexcept { return weapons_; }
    std::size_t SmokeCloudCount() const noexcept { return smokeClouds_.size(); }
    ServerCommandProcessor& Commands() noexcept { return commands_; }
    const RuntimePlayer* Player(std::size_t index) const noexcept;

private:
    struct ActiveGrenade {
        EntityHandle handle{};
        GrenadeKind kind = GrenadeKind::HE;
        std::size_t ownerIndex = 0;
        build4100::Vector origin{};
        float detonateTime = 0.0f;
    };
    struct SmokeCloud {
        build4100::Vector origin{};
        float expireTime = 0.0f;
    };
    void PublishNetwork();
    void ProcessGrenades(float now);
    EntityHandle PlayerEntityHandle(std::size_t index) const noexcept;

    ClientRegistry clients_;
    CsGameState game_{clients_};
    EntityWorld entities_{2048};
    GameEventBus events_;
    NetworkStatePublisher network_;
    ServerCommandProcessor commands_{clients_, game_, events_};
    WeaponRuntimeController weapons_;
    std::vector<RuntimePlayer> players_;
    std::vector<ActiveGrenade> activeGrenades_;
    std::vector<SmokeCloud> smokeClouds_;
    float now_ = 0.0f;
};

} // namespace cssv34::reconstructed
