cmake_minimum_required(VERSION 3.20)

project(css_v34_rebuild_tools LANGUAGES C CXX)

# Finalization profiles. DEBUG preserves all reconstruction/probe telemetry so
# the long soak/regression gates can be resumed later. RELEASE compiles the
# optional EngineProbe channel out of server_i486.so and is the profile used
# for the final parity/RC stage.
set(V34_BUILD_PROFILE "DEBUG" CACHE STRING "Reconstruction build profile: DEBUG or RELEASE")
set_property(CACHE V34_BUILD_PROFILE PROPERTY STRINGS DEBUG RELEASE)
string(TOUPPER "${V34_BUILD_PROFILE}" V34_BUILD_PROFILE_NORMALIZED)
if(NOT V34_BUILD_PROFILE_NORMALIZED STREQUAL "DEBUG" AND
   NOT V34_BUILD_PROFILE_NORMALIZED STREQUAL "RELEASE")
  message(FATAL_ERROR "V34_BUILD_PROFILE must be DEBUG or RELEASE (got '${V34_BUILD_PROFILE}')")
endif()

if(V34_BUILD_PROFILE_NORMALIZED STREQUAL "RELEASE")
  set(V34_ENGINE_PROBE_ENABLED 0)
  set(V34_RELEASE_PROFILE 1)
else()
  set(V34_ENGINE_PROBE_ENABLED 1)
  set(V34_RELEASE_PROFILE 0)
endif()
message(STATUS "V34 build profile: ${V34_BUILD_PROFILE_NORMALIZED}; engine probe: ${V34_ENGINE_PROBE_ENABLED}")

option(
  V34_FORCE_32BIT
  "Build helper tools as 32-bit i386"
  ON
)

if(V34_FORCE_32BIT)
  add_compile_options(-m32)
  add_link_options(-m32)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(
  interface-probe
  tools/interface_probe/interface_probe.c
)

target_link_libraries(
  interface-probe
  PRIVATE
  dl
)

target_compile_options(
  interface-probe
  PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(
  vtable-probe
  tools/vtable-probe/vtable_probe.cpp
)

target_link_libraries(
  vtable-probe
  PRIVATE
  dl
)

target_compile_options(
  vtable-probe
  PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(
  hybrid-loader
  tools/hybrid-loader/hybrid_loader.cpp
)

target_link_libraries(
  hybrid-loader
  PRIVATE
  dl
)

target_compile_options(
  hybrid-loader
  PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(
  dllinit-trace
  tools/dllinit-trace/dllinit_trace.cpp
)

target_link_libraries(
  dllinit-trace
  PRIVATE
  dl
)

target_compile_options(
  dllinit-trace
  PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(
  dllinit-progress
  tools/dllinit-progress/dllinit_progress.cpp
)

target_link_libraries(
  dllinit-progress
  PRIVATE
  dl
)

target_compile_options(
  dllinit-progress
  PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

target_compile_features(
  dllinit-progress
  PRIVATE
  cxx_std_17
)

target_link_options(
  dllinit-progress
  PRIVATE
  -Wl,--export-dynamic
)

option(
  V34_BUILD_RECONSTRUCTION_CORE
  "Build the source-level build4100 reconstruction core"
  ON
)

if(V34_BUILD_RECONSTRUCTION_CORE)
  add_library(
    reconstruction-core
    STATIC
    src/reconstructed/engine/edict_pool.cpp
    src/reconstructed/engine/private_data_allocator.cpp
    src/reconstructed/engine/engine_server.cpp
    src/reconstructed/server/client_registry.cpp
    src/reconstructed/server/entity_world.cpp
    src/reconstructed/server/entity_factory_registry.cpp
    src/reconstructed/server/game_events.cpp
    src/reconstructed/server/network_state.cpp
    src/reconstructed/server/server_commands.cpp
    src/reconstructed/server/weapon_runtime.cpp
    src/reconstructed/server/weapon_script_profile.cpp
    src/reconstructed/server/weapon_ballistics.cpp
    src/reconstructed/server/lag_compensation.cpp
    src/reconstructed/server/game_runtime.cpp
    src/reconstructed/server/map_entity_lump.cpp
    src/reconstructed/server/bomb_objective.cpp
    src/reconstructed/server/hostage_objective.cpp
    src/reconstructed/server/player_damage.cpp
    src/reconstructed/server/score_economy.cpp
    src/reconstructed/server/weapon_inventory.cpp
    src/reconstructed/server/buy_economy.cpp
    src/reconstructed/server/cs_game_state.cpp
    src/reconstructed/server/usercmd_codec.cpp
    src/reconstructed/server/server_game_dll.cpp
  )

  target_include_directories(
    reconstruction-core
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reconstructed"
  )

  target_compile_features(
    reconstruction-core
    PUBLIC
    cxx_std_17
  )

  target_compile_options(
    reconstruction-core
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -fvisibility=hidden
  )

  set_target_properties(
    reconstruction-core
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  add_library(
    reconstruction-abi-clients
    STATIC
    src/reconstructed/abi/engine_cvar003_client.cpp
    src/reconstructed/abi/engine_random001_client.cpp
    src/reconstructed/abi/engine_server021_client.cpp
    src/reconstructed/abi/engine_sound_server003_client.cpp
    src/reconstructed/abi/engine_trace_server003_client.cpp
    src/reconstructed/abi/game_event_manager002_client.cpp
    src/reconstructed/abi/network_string_table_client.cpp
    src/reconstructed/abi/server_engine_tools001_client.cpp
  )

  target_include_directories(
    reconstruction-abi-clients
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reconstructed"
  )

  target_compile_features(
    reconstruction-abi-clients
    PUBLIC
    cxx_std_17
  )

  target_compile_options(
    reconstruction-abi-clients
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -fvisibility=hidden
  )

  set_target_properties(
    reconstruction-abi-clients
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  add_library(
    server-reconstructed-i486
    SHARED
    src/reconstructed/abi/engine_probe.cpp
    src/reconstructed/abi/engine_cvar_bridge.cpp
    src/reconstructed/abi/engine_entity_bridge.cpp
    src/reconstructed/abi/engine_networkable_shim.cpp
    src/reconstructed/abi/engine_unknown_shim.cpp
    src/reconstructed/abi/player_info_adapter.cpp
    src/reconstructed/abi/player_info_manager002_adapter.cpp
    src/reconstructed/abi/server_game_ents001_adapter.cpp
    src/reconstructed/abi/game_movement001_adapter.cpp
    src/reconstructed/abi/server_game_clients004_adapter.cpp
    src/reconstructed/abi/server_game_dll006_adapter.cpp
    src/reconstructed/abi/server_class_registry.cpp
  )

  target_link_libraries(
    server-reconstructed-i486
    PRIVATE
    reconstruction-core
    reconstruction-abi-clients
    ${CMAKE_DL_LIBS}
  )

  target_include_directories(
    server-reconstructed-i486
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reconstructed"
  )

  target_compile_features(
    server-reconstructed-i486
    PRIVATE
    cxx_std_17
  )

  target_compile_definitions(
    server-reconstructed-i486
    PRIVATE
    V34_ENGINE_PROBE_ENABLED=${V34_ENGINE_PROBE_ENABLED}
    V34_RELEASE_PROFILE=${V34_RELEASE_PROFILE}
  )

  target_compile_options(
    server-reconstructed-i486
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -fvisibility=hidden
  )

  target_link_options(
    server-reconstructed-i486
    PRIVATE
    -Wl,--no-undefined
    -Wl,--exclude-libs,ALL
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/config/server-reconstructed.map"
  )

  set_target_properties(
    server-reconstructed-i486
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "server_i486"
  )

  add_executable(
    servergamedll006-abi-smoke
    src/reconstructed/tests/server_game_dll006_abi_smoke.cpp
  )

  target_include_directories(
    servergamedll006-abi-smoke
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reconstructed"
  )

  target_compile_features(
    servergamedll006-abi-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    servergamedll006-abi-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=array-bounds
    -UNDEBUG
  )

  target_link_libraries(
    servergamedll006-abi-smoke
    PRIVATE
    dl
  )

  add_executable(
    shared-abi-clients-smoke
    src/reconstructed/tests/shared_abi_clients_smoke.cpp
  )

  target_link_libraries(
    shared-abi-clients-smoke
    PRIVATE
    reconstruction-abi-clients
  )

  target_compile_features(
    shared-abi-clients-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    shared-abi-clients-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    client-registry-smoke
    src/reconstructed/tests/client_registry_smoke.cpp
  )

  target_link_libraries(
    client-registry-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    client-registry-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    client-registry-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    cs-game-state-smoke
    src/reconstructed/tests/cs_game_state_smoke.cpp
  )

  target_link_libraries(
    cs-game-state-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    cs-game-state-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    cs-game-state-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )


  add_executable(
    bomb-round-objective-smoke
    src/reconstructed/tests/bomb_round_objective_smoke.cpp
  )

  target_link_libraries(
    bomb-round-objective-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    bomb-round-objective-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    bomb-round-objective-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    hostage-round-objective-smoke
    src/reconstructed/tests/hostage_round_objective_smoke.cpp
  )

  target_link_libraries(
    hostage-round-objective-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    hostage-round-objective-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    hostage-round-objective-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    weapon-inventory-smoke
    src/reconstructed/tests/weapon_inventory_smoke.cpp
  )

  target_link_libraries(
    weapon-inventory-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    weapon-inventory-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    weapon-inventory-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    buy-economy-smoke
    src/reconstructed/tests/buy_economy_smoke.cpp
  )

  target_link_libraries(
    buy-economy-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    buy-economy-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    buy-economy-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    weapon-fire-reload-smoke
    src/reconstructed/tests/weapon_fire_reload_smoke.cpp
  )

  target_link_libraries(
    weapon-fire-reload-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    weapon-fire-reload-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    weapon-fire-reload-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    combat-ballistics-smoke
    src/reconstructed/tests/combat_ballistics_smoke.cpp
  )

  target_link_libraries(
    combat-ballistics-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    combat-ballistics-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    combat-ballistics-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    player-damage-smoke
    src/reconstructed/tests/player_damage_smoke.cpp
  )

  target_link_libraries(
    player-damage-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    player-damage-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    player-damage-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    score-economy-smoke
    src/reconstructed/tests/score_economy_smoke.cpp
  )

  target_link_libraries(
    score-economy-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    score-economy-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    score-economy-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    round-lifecycle-smoke
    src/reconstructed/tests/round_lifecycle_smoke.cpp
  )

  target_link_libraries(
    round-lifecycle-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    round-lifecycle-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    round-lifecycle-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    player-state-usercmd-smoke
    src/reconstructed/tests/player_state_usercmd_smoke.cpp
  )

  target_link_libraries(
    player-state-usercmd-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    player-state-usercmd-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    player-state-usercmd-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    usercmd-bitbuf-smoke
    src/reconstructed/tests/usercmd_bitbuf_smoke.cpp
  )

  target_link_libraries(
    usercmd-bitbuf-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    usercmd-bitbuf-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    usercmd-bitbuf-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  add_executable(
    entity-runtime-smoke
    src/reconstructed/tests/entity_runtime_smoke.cpp
  )
  target_link_libraries(entity-runtime-smoke PRIVATE reconstruction-core)
  target_compile_features(entity-runtime-smoke PRIVATE cxx_std_17)
  target_compile_options(entity-runtime-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    commands-network-smoke
    src/reconstructed/tests/commands_network_smoke.cpp
  )
  target_link_libraries(commands-network-smoke PRIVATE reconstruction-core)
  target_compile_features(commands-network-smoke PRIVATE cxx_std_17)
  target_compile_options(commands-network-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    weapon-grenade-runtime-smoke
    src/reconstructed/tests/weapon_grenade_runtime_smoke.cpp
  )
  target_link_libraries(weapon-grenade-runtime-smoke PRIVATE reconstruction-core)
  target_compile_features(weapon-grenade-runtime-smoke PRIVATE cxx_std_17)
  target_compile_options(weapon-grenade-runtime-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    end-to-end-match-smoke
    src/reconstructed/tests/end_to_end_match_smoke.cpp
  )
  target_link_libraries(end-to-end-match-smoke PRIVATE reconstruction-core)
  target_compile_features(end-to-end-match-smoke PRIVATE cxx_std_17)
  target_compile_options(end-to-end-match-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    map-entity-lump-smoke
    src/reconstructed/tests/map_entity_lump_smoke.cpp
  )
  target_link_libraries(map-entity-lump-smoke PRIVATE reconstruction-core)
  target_compile_features(map-entity-lump-smoke PRIVATE cxx_std_17)
  target_compile_options(map-entity-lump-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    networkable-bridge-smoke
    src/reconstructed/tests/networkable_bridge_smoke.cpp
    src/reconstructed/abi/engine_entity_bridge.cpp
    src/reconstructed/abi/engine_networkable_shim.cpp
    src/reconstructed/abi/engine_unknown_shim.cpp
    src/reconstructed/abi/engine_probe.cpp
    src/reconstructed/abi/server_game_ents001_adapter.cpp
    src/reconstructed/abi/server_class_registry.cpp
  )
  target_link_libraries(networkable-bridge-smoke PRIVATE reconstruction-core reconstruction-abi-clients)
  target_include_directories(networkable-bridge-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/reconstructed")
  target_compile_features(networkable-bridge-smoke PRIVATE cxx_std_17)
  target_compile_options(networkable-bridge-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)


  add_executable(
    sendtable-live-graph-smoke
    src/reconstructed/tests/sendtable_live_graph_smoke.cpp
    src/reconstructed/abi/server_class_registry.cpp
  )
  target_include_directories(sendtable-live-graph-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/reconstructed")
  target_link_libraries(sendtable-live-graph-smoke PRIVATE reconstruction-core)
  target_compile_features(sendtable-live-graph-smoke PRIVATE cxx_std_17)
  target_compile_options(sendtable-live-graph-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    sendtable-client-schema-smoke
    src/reconstructed/tests/sendtable_client_schema_smoke.cpp
    src/reconstructed/abi/server_class_registry.cpp
  )
  target_include_directories(sendtable-client-schema-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/reconstructed")
  target_link_libraries(sendtable-client-schema-smoke PRIVATE reconstruction-core)
  target_compile_features(sendtable-client-schema-smoke PRIVATE cxx_std_17)
  target_compile_options(sendtable-client-schema-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    entity-factory-registry-smoke
    src/reconstructed/tests/entity_factory_registry_smoke.cpp
  )
  target_link_libraries(entity-factory-registry-smoke PRIVATE reconstruction-core)
  target_compile_features(entity-factory-registry-smoke PRIVATE cxx_std_17)
  target_compile_options(entity-factory-registry-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

  add_executable(
    factory-differential-smoke
    src/reconstructed/tests/factory_differential_smoke.cpp
  )
  target_compile_features(factory-differential-smoke PRIVATE cxx_std_17)
  target_compile_options(factory-differential-smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  target_link_libraries(factory-differential-smoke PRIVATE reconstruction-core dl)

  add_executable(
    reconstruction-core-smoke
    src/reconstructed/tests/reconstruction_core_smoke.cpp
  )

  target_link_libraries(
    reconstruction-core-smoke
    PRIVATE
    reconstruction-core
  )

  target_compile_features(
    reconstruction-core-smoke
    PRIVATE
    cxx_std_17
  )

  target_compile_options(
    reconstruction-core-smoke
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
  )

  enable_testing()
  add_test(
    NAME reconstruction-core-smoke
    COMMAND reconstruction-core-smoke
  )

  add_test(
    NAME client-registry-smoke
    COMMAND client-registry-smoke
  )

  add_test(
    NAME shared-abi-clients-smoke
    COMMAND shared-abi-clients-smoke
  )

  add_test(
    NAME cs-game-state-smoke
    COMMAND cs-game-state-smoke
  )

  add_test(
    NAME bomb-round-objective-smoke
    COMMAND bomb-round-objective-smoke
  )

  add_test(
    NAME hostage-round-objective-smoke
    COMMAND hostage-round-objective-smoke
  )

  add_test(
    NAME weapon-inventory-smoke
    COMMAND weapon-inventory-smoke
  )

  add_test(
    NAME buy-economy-smoke
    COMMAND buy-economy-smoke
  )

  add_test(
    NAME weapon-fire-reload-smoke
    COMMAND weapon-fire-reload-smoke
  )

  add_test(
    NAME combat-ballistics-smoke
    COMMAND combat-ballistics-smoke
  )

  add_test(
    NAME player-damage-smoke
    COMMAND player-damage-smoke
  )

  add_test(
    NAME score-economy-smoke
    COMMAND score-economy-smoke
  )

  add_test(
    NAME round-lifecycle-smoke
    COMMAND round-lifecycle-smoke
  )

  add_test(
    NAME player-state-usercmd-smoke
    COMMAND player-state-usercmd-smoke
  )

  add_test(
    NAME usercmd-bitbuf-smoke
    COMMAND usercmd-bitbuf-smoke
  )

  add_test(NAME entity-runtime-smoke COMMAND entity-runtime-smoke)
  add_test(NAME commands-network-smoke COMMAND commands-network-smoke)
  add_test(NAME weapon-grenade-runtime-smoke COMMAND weapon-grenade-runtime-smoke)
  add_test(NAME end-to-end-match-smoke COMMAND end-to-end-match-smoke)
  add_test(NAME map-entity-lump-smoke COMMAND map-entity-lump-smoke)
  add_test(NAME networkable-bridge-smoke COMMAND networkable-bridge-smoke)
  add_test(NAME sendtable-live-graph-smoke COMMAND sendtable-live-graph-smoke)
  add_test(NAME sendtable-client-schema-smoke COMMAND sendtable-client-schema-smoke)
  add_test(NAME entity-factory-registry-smoke COMMAND entity-factory-registry-smoke)

  add_test(
    NAME servergamedll006-abi-smoke
    COMMAND servergamedll006-abi-smoke $<TARGET_FILE:server-reconstructed-i486>
  )
endif()
