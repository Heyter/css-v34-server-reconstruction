#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "abi/engine_cvar003_client.h"
#include "abi/engine_random001_client.h"
#include "abi/engine_server021_client.h"
#include "abi/engine_sound_server003_client.h"
#include "abi/engine_trace_server003_client.h"
#include "abi/game_event_manager002_client.h"
#include "abi/network_string_table_client.h"
#include "abi/server_engine_tools001_client.h"
#include "public/build4100/globalvars.h"

namespace {

struct Object {
    void** vtable = nullptr;
};

struct EventObject {
    void** vtable = nullptr;
    std::string name;
    int intValue = 0;
    std::string stringValue;
    bool fired = false;
};

Object gEngine;
Object gStringContainer;
Object gStringTable;
Object gTools;
Object gRandom;
Object gSound;
Object gEventManager;
Object gTrace;
Object gCvar;
EventObject gEvent;

void* gEngineVtable[99] = {};
void* gStringContainerVtable[3] = {};
void* gStringTableVtable[10] = {};
void* gToolsVtable[9] = {};
void* gRandomVtable[3] = {};
void* gSoundVtable[4] = {};
void* gEventManagerVtable[12] = {};
void* gTraceVtable[20] = {};
int gWorldCollideable = 0;
void* gEventVtable[14] = {};
void* gCvarVtable[13] = {};

int gToolCalls[9] = {};
int gAddListenerCalls = 0;
int gRemoveListenerCalls = 0;
int gFreeEventCalls = 0;
std::vector<std::string> gLoadedEventFiles;
std::vector<std::string> gListenerEventNames;
cssv34::reconstructed::abi::GlobalChangeCallbackFn gInstalledCallback = nullptr;
std::vector<void*> gRegisteredCommandBases;
void* gFoundVar = reinterpret_cast<void*>(0x1234);
void* gCommandHead = reinterpret_cast<void*>(0x5678);
int gUnlinkFlags = 0;
std::vector<std::string> gCommands;
int gExecuteCalls = 0;
std::vector<std::string> gCmdArgs{"jointeam", "3"};
bool gNetworkStringTablesLocked = true;
int gFakeClientCalls = 0;
std::string gLastLog;
int gLastFlagsIndex = -1;
int gPrivateBuffer = 0;
cssv34::build4100::ChangeInfoAccessor gChangeAccessor{};
cssv34::build4100::Edict gEdicts[4] = {};

bool EngineDedicated(void*) { return true; }
bool EngineEditMode(void*) { return false; }
int EngineUserId(void*, const cssv34::build4100::Edict* edict) {
    if (!edict) return -1;
    for (int i = 0; i < 4; ++i) {
        if (edict == &gEdicts[i]) return 100 + i;
    }
    return -1;
}
const char* EngineNetworkId(void*, const cssv34::build4100::Edict* edict) {
    return edict == &gEdicts[1] ? "STEAM_0:1:2" : nullptr;
}
int EngineIndex(void*, const cssv34::build4100::Edict* edict) {
    if (!edict) return -1;
    for (int i = 0; i < 4; ++i) {
        if (edict == &gEdicts[i]) return i;
    }
    return -1;
}
cssv34::build4100::Edict* EngineEntity(void*, int index) {
    return index >= 0 && index < 4 ? &gEdicts[index] : nullptr;
}
cssv34::build4100::Edict* EngineCreate(void*, int index) {
    return index == 2 ? &gEdicts[2] : nullptr;
}
void EngineRemove(void*, cssv34::build4100::Edict* edict) {
    if (edict) edict->stateFlags |= cssv34::build4100::kEdictFree;
}
void* EngineAlloc(void*, std::int32_t bytes) {
    return bytes == static_cast<std::int32_t>(sizeof(gPrivateBuffer)) ? &gPrivateBuffer : nullptr;
}
void EngineFree(void*, void* pointer) {
    if (pointer == &gPrivateBuffer) gPrivateBuffer = 0;
}
void EngineCommand(void*, const char* command) {
    gCommands.emplace_back(command ? command : "");
}
void EngineExecute(void*) { ++gExecuteCalls; }
const char* EngineCmdArgs(void*) { return "jointeam 3"; }
int EngineCmdArgc(void*) { return static_cast<int>(gCmdArgs.size()); }
const char* EngineCmdArgv(void*, int index) {
    return index >= 0 && index < static_cast<int>(gCmdArgs.size()) ? gCmdArgs[static_cast<std::size_t>(index)].c_str() : "";
}
bool EngineLockNetworkStringTables(void*, bool lock) {
    const bool previous = gNetworkStringTablesLocked;
    gNetworkStringTablesLocked = lock;
    return previous;
}
cssv34::build4100::Edict* EngineCreateFakeClient(void*, const char* name) {
    assert(name && std::strcmp(name, "probe_bot") == 0);
    ++gFakeClientCalls;
    return &gEdicts[3];
}
const char* EngineGetClientConVarValue(void*, int clientIndex, const char* name) {
    assert(clientIndex == 3 && name && std::strcmp(name, "rate") == 0);
    return "30000";
}
const char* EngineParse(void*, const char* data, char* token, int maxLen) {
    if (!data || !token || maxLen < 4) return nullptr;
    std::strncpy(token, "tok", static_cast<std::size_t>(maxLen));
    token[maxLen - 1] = '\0';
    return data + std::strlen(data);
}
const char* EngineEntities(void*) { return "{\n}\n"; }
void EngineLog(void*, const char* text) { gLastLog = text ? text : ""; }
void EngineFlags(void*, int index) { gLastFlagsIndex = index; }
void* EngineAccessor(void*, cssv34::build4100::Edict*) { return &gChangeAccessor; }

void* CreateTable(void*, const char* name, int maxEntries, int fixedSize, int bits) {
    assert(name && std::strcmp(name, "InfoPanel") == 0);
    assert(maxEntries == 128);
    assert(fixedSize == 0);
    assert(bits == 0);
    return &gStringTable;
}
int AddString(void*, const char* value, int length, const void* userData) {
    assert(value && std::strcmp(value, "motd") == 0);
    assert(length == -1);
    assert(userData == nullptr);
    return 7;
}

void Tool2(void*) { ++gToolCalls[2]; }
void Tool3(void*) { ++gToolCalls[3]; }
void Tool4(void*) { ++gToolCalls[4]; }
void Tool5(void*) { ++gToolCalls[5]; }
void Tool6(void*) { ++gToolCalls[6]; }
void Tool7(void*) { ++gToolCalls[7]; }
void Tool8(void*) { ++gToolCalls[8]; }

int RandomInt(void*, int minValue, int maxValue) {
    assert(minValue <= maxValue);
    return maxValue;
}
float SoundDuration(void*, const char* sample) {
    assert(sample && *sample);
    return 1.0f;
}

int LoadEventsFromFile(void* self, const char* filename) {
    assert(self == &gEventManager);
    assert(filename);
    gLoadedEventFiles.emplace_back(filename);
    return (std::strcmp(filename, "resource/gameevents.res") == 0 ||
            std::strcmp(filename, "resource/ModEvents.res") == 0) ? 1 : 0;
}

bool AddListener(void* self, void* listener, const char* name, bool serverSide) {
    assert(self == &gEventManager);
    assert(listener != nullptr);
    assert(name && *name);
    assert(serverSide);
    ++gAddListenerCalls;
    gListenerEventNames.emplace_back(name);
    return true;
}
void RemoveListener(void*, void* listener) {
    assert(listener != nullptr);
    ++gRemoveListenerCalls;
}
void* CreateEvent(void*, const char* name, bool force) {
    assert(name && *name);
    assert(force);
    gEvent.name = name;
    gEvent.intValue = 0;
    gEvent.stringValue.clear();
    gEvent.fired = false;
    return &gEvent;
}
bool FireEvent(void*, void* event, bool dontBroadcast) {
    assert(event == &gEvent);
    assert(!dontBroadcast);
    gEvent.fired = true;
    return true;
}
void FreeEvent(void*, void* event) {
    assert(event == &gEvent);
    ++gFreeEventCalls;
}
void EventSetInt(void* event, const char* key, int value) {
    assert(event == &gEvent);
    assert(key && std::strcmp(key, "priority") == 0);
    gEvent.intValue = value;
}
void EventSetString(void* event, const char* key, const char* value) {
    assert(event == &gEvent);
    assert(key && std::strcmp(key, "objective") == 0);
    gEvent.stringValue = value ? value : "";
}

void* GetWorldCollideable(void* self) {
    assert(self == &gTrace);
    return &gWorldCollideable;
}

void ClipRayToCollideable(void* self, const cssv34::build4100::EngineTraceRay* ray,
                          std::uint32_t mask, void* collideable,
                          cssv34::build4100::EngineGameTrace* trace) {
    assert(self == &gTrace);
    assert(ray && trace);
    assert(collideable == &gWorldCollideable);
    assert(mask == cssv34::build4100::kMaskPlayerSolidBrushOnly);
    assert(!ray->isRay && ray->isSwept);
    assert(std::fabs(ray->extents.x - 16.0f) < 0.001f);
    assert(std::fabs(ray->extents.y - 16.0f) < 0.001f);
    assert(std::fabs(ray->extents.z - 36.0f) < 0.001f);
    trace->fraction = 0.5f;
    trace->startPos = cssv34::build4100::Vector{1.0f, 2.0f, 3.0f};
    trace->endPos = cssv34::build4100::Vector{1.0f, 2.0f, -61.0f};
    trace->plane.normal = cssv34::build4100::Vector{0.0f, 0.0f, 1.0f};
}

void RegisterCommandBase(void*, void* commandBase) {
    assert(commandBase);
    gRegisteredCommandBases.push_back(commandBase);
}
void* FindVar(void*, const char* name) {
    return name && std::strcmp(name, "existing") == 0 ? gFoundVar : nullptr;
}
void* GetCommands(void*) { return gCommandHead; }
void UnlinkVariables(void*, int flags) { gUnlinkFlags = flags; }
void InstallCallback(void*, cssv34::reconstructed::abi::GlobalChangeCallbackFn callback) {
    gInstalledCallback = callback;
}
void DummyCallback(void*, const char*) {}

}  // namespace

int main() {
    using namespace cssv34::reconstructed::abi;

    cssv34::build4100::GlobalVarsBase globals{};
    globals.maxClients = 32;
    globals.tickcount = 7;
    globals.curtime = 0.105f;
    globals.interval_per_tick = 0.015f;
    assert(sizeof(globals) == 0x20);
    assert(globals.maxClients == 32);

    gEngineVtable[2] = reinterpret_cast<void*>(&EngineDedicated);
    gEngineVtable[3] = reinterpret_cast<void*>(&EngineEditMode);
    gEngineVtable[15] = reinterpret_cast<void*>(&EngineUserId);
    gEngineVtable[16] = reinterpret_cast<void*>(&EngineNetworkId);
    gEngineVtable[18] = reinterpret_cast<void*>(&EngineIndex);
    gEngineVtable[19] = reinterpret_cast<void*>(&EngineEntity);
    gEngineVtable[21] = reinterpret_cast<void*>(&EngineCreate);
    gEngineVtable[22] = reinterpret_cast<void*>(&EngineRemove);
    gEngineVtable[23] = reinterpret_cast<void*>(&EngineAlloc);
    gEngineVtable[24] = reinterpret_cast<void*>(&EngineFree);
    gEngineVtable[36] = reinterpret_cast<void*>(&EngineCommand);
    gEngineVtable[37] = reinterpret_cast<void*>(&EngineExecute);
    gEngineVtable[48] = reinterpret_cast<void*>(&EngineCmdArgs);
    gEngineVtable[49] = reinterpret_cast<void*>(&EngineCmdArgc);
    gEngineVtable[50] = reinterpret_cast<void*>(&EngineCmdArgv);
    gEngineVtable[56] = reinterpret_cast<void*>(&EngineLockNetworkStringTables);
    gEngineVtable[57] = reinterpret_cast<void*>(&EngineCreateFakeClient);
    gEngineVtable[58] = reinterpret_cast<void*>(&EngineGetClientConVarValue);
    gEngineVtable[59] = reinterpret_cast<void*>(&EngineParse);
    gEngineVtable[74] = reinterpret_cast<void*>(&EngineEntities);
    gEngineVtable[76] = reinterpret_cast<void*>(&EngineLog);
    gEngineVtable[93] = reinterpret_cast<void*>(&EngineFlags);
    gEngineVtable[98] = reinterpret_cast<void*>(&EngineAccessor);
    gEngine.vtable = gEngineVtable;

    EngineServer021Client engine(&gEngine);
    assert(engine.IsValid());
    assert(engine.IsDedicatedServer());
    assert(!engine.IsInEditMode());
    assert(engine.GetPlayerUserId(&gEdicts[1]) == 101);
    assert(std::strcmp(engine.GetPlayerNetworkIdString(&gEdicts[1]), "STEAM_0:1:2") == 0);
    assert(engine.IndexOfEdict(&gEdicts[3]) == 3);
    assert(engine.PEntityOfEntIndex(1) == &gEdicts[1]);
    assert(engine.CreateEdict(2) == &gEdicts[2]);
    gEdicts[2].stateFlags = 0;
    engine.RemoveEdict(&gEdicts[2]);
    assert(cssv34::build4100::IsFree(gEdicts[2]));
    gPrivateBuffer = 123;
    assert(engine.PvAllocEntPrivateData(sizeof(gPrivateBuffer)) == &gPrivateBuffer);
    engine.FreeEntPrivateData(&gPrivateBuffer);
    assert(gPrivateBuffer == 0);
    engine.ServerCommand("exec skill1.cfg\n");
    engine.ServerExecute();
    assert(gCommands.size() == 1);
    assert(gExecuteCalls == 1);
    assert(std::strcmp(engine.CmdArgs(), "jointeam 3") == 0);
    assert(engine.CmdArgc() == 2);
    assert(std::strcmp(engine.CmdArgv(0), "jointeam") == 0);
    assert(std::strcmp(engine.CmdArgv(1), "3") == 0);
    assert(engine.LockNetworkStringTables(false));
    assert(!gNetworkStringTablesLocked);
    assert(engine.CreateFakeClient("probe_bot") == &gEdicts[3]);
    assert(gFakeClientCalls == 1);
    assert(std::strcmp(engine.GetClientConVarValue(3, "rate"), "30000") == 0);
    assert(!engine.LockNetworkStringTables(true));
    assert(gNetworkStringTablesLocked);
    char token[16] = {};
    const char parseInput[] = "abc";
    assert(engine.ParseFile(parseInput, token, sizeof(token)) == parseInput + 3);
    assert(std::strcmp(token, "tok") == 0);
    assert(std::strcmp(engine.GetMapEntitiesString(), "{\n}\n") == 0);
    engine.LogPrint("hello");
    assert(gLastLog == "hello");
    engine.NotifyEdictFlagsChange(3);
    assert(gLastFlagsIndex == 3);
    assert(engine.GetChangeAccessor(&gEdicts[1]) == &gChangeAccessor);
    gEdicts[1].stateFlags = cssv34::build4100::kEdictFull;
    gChangeAccessor.changeInfo = 17;
    gChangeAccessor.changeInfoSerialNumber = 55;
    assert(engine.MarkEdictStateChangedFull(&gEdicts[1]));
    assert((gEdicts[1].stateFlags & cssv34::build4100::kEdictChanged) != 0);
    assert((gEdicts[1].stateFlags & cssv34::build4100::kEdictFullChanged) != 0);
    assert(gChangeAccessor.changeInfo == 17);
    assert(gChangeAccessor.changeInfoSerialNumber == 0);

    gStringContainerVtable[2] = reinterpret_cast<void*>(&CreateTable);
    gStringTableVtable[9] = reinterpret_cast<void*>(&AddString);
    gStringContainer.vtable = gStringContainerVtable;
    gStringTable.vtable = gStringTableVtable;
    NetworkStringTableContainer001Client tables(&gStringContainer);
    void* tableRaw = tables.CreateStringTable("InfoPanel", 128, 0, 0);
    NetworkStringTableClient table(tableRaw);
    assert(table.IsValid());
    assert(table.AddString("motd", -1, nullptr) == 7);

    gToolsVtable[2] = reinterpret_cast<void*>(&Tool2);
    gToolsVtable[3] = reinterpret_cast<void*>(&Tool3);
    gToolsVtable[4] = reinterpret_cast<void*>(&Tool4);
    gToolsVtable[5] = reinterpret_cast<void*>(&Tool5);
    gToolsVtable[6] = reinterpret_cast<void*>(&Tool6);
    gToolsVtable[7] = reinterpret_cast<void*>(&Tool7);
    gToolsVtable[8] = reinterpret_cast<void*>(&Tool8);
    gTools.vtable = gToolsVtable;
    ServerEngineTools001Client tools(&gTools);
    tools.LevelInitPreEntityAllTools();
    tools.LevelInitPostEntityAllTools();
    tools.LevelShutdownPreEntityAllTools();
    tools.LevelShutdownPostEntityAllTools();
    tools.FrameUpdatePreEntityThink();
    tools.FrameUpdatePostEntityThink();
    tools.PreClientUpdate();
    for (int slot = 2; slot <= 8; ++slot) assert(gToolCalls[slot] == 1);

    gRandomVtable[2] = reinterpret_cast<void*>(&RandomInt);
    gRandom.vtable = gRandomVtable;
    EngineRandom001Client random(&gRandom);
    assert(random.RandomInt(0, 3) == 3);

    gSoundVtable[3] = reinterpret_cast<void*>(&SoundDuration);
    gSound.vtable = gSoundVtable;
    EngineSoundServer003Client sound(&gSound);
    assert(sound.GetSoundDuration("weapons/test.wav") == 1.0f);

    gTraceVtable[3] = reinterpret_cast<void*>(&ClipRayToCollideable);
    gTraceVtable[19] = reinterpret_cast<void*>(&GetWorldCollideable);
    gTrace.vtable = gTraceVtable;
    EngineTraceServer003Client engineTrace(&gTrace);
    cssv34::build4100::EngineGameTrace trace{};
    assert(engineTrace.TraceWorldHull(cssv34::build4100::Vector{1.0f, 2.0f, 3.0f},
                                      cssv34::build4100::Vector{1.0f, 2.0f, -125.0f},
                                      cssv34::build4100::Vector{-16.0f, -16.0f, 0.0f},
                                      cssv34::build4100::Vector{16.0f, 16.0f, 72.0f},
                                      cssv34::build4100::kMaskPlayerSolidBrushOnly, trace));
    assert(std::fabs(trace.fraction - 0.5f) < 0.001f);
    assert(trace.plane.normal.z == 1.0f);

    gEventManagerVtable[2] = reinterpret_cast<void*>(&LoadEventsFromFile);
    gEventManagerVtable[4] = reinterpret_cast<void*>(&AddListener);
    gEventManagerVtable[6] = reinterpret_cast<void*>(&RemoveListener);
    gEventManagerVtable[7] = reinterpret_cast<void*>(&CreateEvent);
    gEventManagerVtable[8] = reinterpret_cast<void*>(&FireEvent);
    gEventManagerVtable[11] = reinterpret_cast<void*>(&FreeEvent);
    gEventVtable[11] = reinterpret_cast<void*>(&EventSetInt);
    gEventVtable[13] = reinterpret_cast<void*>(&EventSetString);
    gEventManager.vtable = gEventManagerVtable;
    gEvent.vtable = gEventVtable;
    GameEventManager002Client events(&gEventManager);
    assert(events.LoadEventsFromFile("resource/gameevents.res") == 1);
    assert(events.LoadEventsFromFile("resource/ModEvents.res") == 1);
    assert(gLoadedEventFiles.size() == 2);
    assert(gLoadedEventFiles[0] == "resource/gameevents.res");
    assert(gLoadedEventFiles[1] == "resource/ModEvents.res");
    int listener = 1;
    assert(events.AddListener(&listener, "player_connect", true));
    assert(gAddListenerCalls == 1);
    assert(gListenerEventNames.size() == 1 && gListenerEventNames[0] == "player_connect");
    GameEventClient preflight = events.CreateEvent("player_connect", true);
    assert(preflight.IsValid());
    events.FreeEvent(preflight);
    assert(gFreeEventCalls == 1);
    events.RemoveListener(&listener);
    assert(gRemoveListenerCalls == 1);
    GameEventClient event = events.CreateEvent("round_start", true);
    assert(event.IsValid());
    event.SetInt("priority", 6);
    event.SetString("objective", "BOMB TARGET");
    assert(events.FireEvent(event, false));
    assert(gEvent.name == "round_start");
    assert(gEvent.intValue == 6);
    assert(gEvent.stringValue == "BOMB TARGET");
    assert(gEvent.fired);

    gCvarVtable[5] = reinterpret_cast<void*>(&RegisterCommandBase);
    gCvarVtable[7] = reinterpret_cast<void*>(&FindVar);
    gCvarVtable[9] = reinterpret_cast<void*>(&GetCommands);
    gCvarVtable[10] = reinterpret_cast<void*>(&UnlinkVariables);
    gCvarVtable[11] = reinterpret_cast<void*>(&InstallCallback);
    gCvar.vtable = gCvarVtable;
    EngineCvar003Client cvar(&gCvar);
    int fakeCommandBase = 7;
    cvar.RegisterConCommandBase(&fakeCommandBase);
    assert(gRegisteredCommandBases.size() == 1 && gRegisteredCommandBases[0] == &fakeCommandBase);
    assert(cvar.FindVar("existing") == gFoundVar);
    assert(cvar.FindVar("missing") == nullptr);
    assert(cvar.GetCommands() == gCommandHead);
    cvar.UnlinkVariables(0x2004);
    assert(gUnlinkFlags == 0x2004);
    cvar.InstallGlobalChangeCallback(&DummyCallback);
    assert(gInstalledCallback == &DummyCallback);

    std::cout << "SHARED_ABI_CLIENTS_V0_3_0=PASS\n";
    std::cout << "engine_slots=21 cmd_slots=48,49,50 stringtables=2 tools=7 events=load_slot2+5 random=1 sound=1 enginetrace_slot4=PASS cvar_slots=5,7,9,10,11 globals=0x20\n";
    return 0;
}
