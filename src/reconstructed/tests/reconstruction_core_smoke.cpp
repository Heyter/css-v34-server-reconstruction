#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "engine/engine_server.h"
#include "server/server_game_dll.h"

namespace {

constexpr char kEntityLump[] =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "}\n";

void CheckZeroed(const void* pointer, const std::size_t bytes) {
    const auto* data = static_cast<const std::uint8_t*>(pointer);
    for (std::size_t i = 0; i < bytes; ++i) {
        assert(data[i] == 0);
    }
}

}  // namespace

int main() {
    using cssv34::build4100::IsFree;
    using cssv34::reconstructed::EngineServer;
    using cssv34::reconstructed::ServerGameDll;
    using cssv34::reconstructed::ServerLifecycleState;

    // Use room beyond the observed default 32-player limit so the first
    // ordinary dynamic entity can live strictly outside the client range.
    EngineServer engine(128);
    assert(engine.ConfigureClientSlots(32));
    assert(engine.MaxClients() == 32);

    ServerGameDll game(engine);

    assert(game.DLLInit());
    assert(game.GameInit());

    // GameInit-level command semantics: queue + execute, without exact call IDs.
    assert(engine.ServerCommand("exec game.cfg\n"));
    assert(engine.ServerExecute() == 1);

    std::int32_t dynamicSerialAfterFirstLevel = -999;
    std::int32_t clientSerialAfterFirstLevel = -999;

    for (int level = 0; level < 2; ++level) {
        assert(game.LevelInit("probe_map", kEntityLump));
        assert(game.ServerActivate());
        assert(engine.LevelActive());
        assert(std::strcmp(engine.GetMapEntitiesString(), kEntityLump) == 0);

        // v51.36.0 proved that this command legitimately repeats on second
        // LevelInit. Source reconstruction therefore models command semantics,
        // not the old harness' #2/#4 exact sequence.
        assert(engine.ServerCommand("exec skill1.cfg\n"));
        assert(engine.ServerExecute() == 1);

        // Slots 1..32 are structurally reserved for engine-owned clients.
        // Dynamic allocation must begin at maxClients + 1.
        assert(engine.CreateEdict(1) == nullptr);
        assert(engine.CreateEdict(32) == nullptr);

        auto* client1 = engine.AcquireClientEdict(1);
        auto* client32 = engine.AcquireClientEdict(32);
        assert(client1 != nullptr);
        assert(client32 != nullptr);
        assert(engine.AcquireClientEdict(0) == nullptr);
        assert(engine.AcquireClientEdict(33) == nullptr);
        assert(engine.IndexOfEdict(client1) == 1);
        assert(engine.IndexOfEdict(client32) == 32);
        assert(engine.Edicts().ActiveClientCount() == 2);

        // Dynamic removal cannot release an engine-owned client slot.
        assert(!engine.RemoveEdict(client1));

        auto* edict = engine.CreateEdict(-1);
        assert(edict != nullptr);
        assert(engine.IndexOfEdict(edict) == 33);
        assert(!IsFree(*edict));
        assert(engine.Edicts().ActiveDynamicCount() == 1);

        if (level == 1) {
            assert(edict->networkSerial == dynamicSerialAfterFirstLevel);
            assert(client1->networkSerial == clientSerialAfterFirstLevel);
        }

        // Explicit client release is independent from ordinary entity removal.
        assert(engine.ReleaseClientEdict(client1));
        assert(IsFree(*client1));
        assert(engine.Edicts().ActiveClientCount() == 1);
        client1 = engine.AcquireClientEdict(1);
        assert(client1 != nullptr);

        void* privateData = engine.PvAllocEntPrivateData(924);
        assert(privateData != nullptr);
        assert(engine.PrivateData().AllocationSize(privateData) == 924);
        CheckZeroed(privateData, 924);
        assert(engine.FreeEntPrivateData(privateData));
        assert(engine.PrivateData().ActiveCount() == 0);

        for (int frame = 0; frame < 16; ++frame) {
            assert(game.GameFrame(true));
            assert(game.PreClientUpdate(true));
        }

        assert(game.LevelShutdown());
        assert(!engine.LevelActive());
        assert(engine.Edicts().ActiveClientCount() == 0);
        assert(engine.Edicts().ActiveDynamicCount() == 0);

        const auto* recycledDynamic = engine.Edicts().At(33);
        const auto* recycledClient = engine.Edicts().At(1);
        assert(recycledDynamic != nullptr);
        assert(recycledClient != nullptr);
        assert(IsFree(*recycledDynamic));
        assert(IsFree(*recycledClient));
        dynamicSerialAfterFirstLevel = recycledDynamic->networkSerial;
        clientSerialAfterFirstLevel = recycledClient->networkSerial;
    }

    assert(engine.LevelGeneration() == 2);
    assert(game.GameFrameCalls() == 32);
    assert(game.PreClientUpdateCalls() == 32);

    const auto& commands = engine.ExecutedCommands();
    assert(commands.size() == 3);
    assert(commands[0] == "exec game.cfg\n");
    assert(commands[1] == "exec skill1.cfg\n");
    assert(commands[2] == "exec skill1.cfg\n");

    assert(game.GameShutdown());
    assert(game.DLLShutdown());
    assert(game.State() == ServerLifecycleState::DllStopped);

    std::cout << "RECONSTRUCTION_SOURCE_V0_2_3=PASS\n";
    std::cout << "maxClients=32 first_dynamic=33 client_ownership=PASS "
                 "levels=2 frames=32\n";
    return 0;
}
