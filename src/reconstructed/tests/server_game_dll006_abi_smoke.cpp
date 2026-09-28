#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <vector>

#include "public/build4100/edict.h"
#include "public/build4100/globalvars.h"
#include "public/build4100/player_info.h"
#include "public/build4100/sendtable.h"
#include "public/build4100/sendproxies.h"
#include "public/build4100/player_state.h"
#include "public/build4100/usercmd.h"
#include "public/build4100/vector.h"
#include "tests/usercmd_wire_test_utils.h"

namespace {

using CreateInterfaceFn = void* (*)(const char*, int*);
using FactoryFn = void* (*)(const char*, int*);

constexpr char kEntityLump[] = "{\n\"classname\" \"worldspawn\"\n}\n";

struct FakeEngineState {
    std::vector<std::string> commands;
    int executeCalls = 0;
};
struct FakeEngineObject { void** vtable = nullptr; FakeEngineState* state = nullptr; };
struct PlainObject { void** vtable = nullptr; };
struct StringTableObject { void** vtable = nullptr; const char* name = nullptr; };
struct StringTableCreateCall { std::string name; int maxEntries; int fixedSize; int networkBits; };

FakeEngineObject gEngineObject;
void* gEngineVtable[99] = {};
FakeEngineState gEngineState;
// Back the engine/client-range fixture with enough storage to exercise a
// deliberately foreign edict pointer without invoking C++ out-of-bounds UB.
// The mock engine still exposes only indices 0..32 below, matching maxClients=32.
cssv34::build4100::Edict gEngineEdicts[64] = {};

PlainObject gEngineTraceObject;
void* gEngineTraceVtable[5] = {};
PlainObject gStringContainerObject;
void* gStringContainerVtable[3] = {};
StringTableObject gStringTables[7];
void* gStringTableVtables[7][10] = {};
std::vector<StringTableCreateCall> gStringTableCreates;
int gMaterialsAddCalls = 0;

PlainObject gToolsObject;
void* gToolsVtable[9] = {};
int gToolCalls[9] = {};

PlainObject gCvarObject;
void* gCvarVtable[13] = {};
PlainObject gEventManagerObject;
void* gEventManagerVtable[12] = {};
PlainObject gVoiceServerObject;
void* gVoiceServerVtable[4] = {};
PlainObject gEventObject;
void* gEventVtable[3] = {};
std::vector<std::string> gLoadedEventFiles;
std::vector<std::string> gGameEventListenerNames;
void* gGameEventListener = nullptr;
int gGameEventAddListenerCalls = 0;
int gGameEventRemoveListenerCalls = 0;
int gGameEventCreateCalls = 0;
int gGameEventFreeCalls = 0;
struct VoiceListenCall { int receiver; int sender; bool listen; };
std::vector<VoiceListenCall> gVoiceListenCalls;
std::vector<std::string> gRegisteredCvars;
std::vector<void*> gRegisteredCvarBases;
int gCvarUnlinkFlags = 0;
int gEngineRemoveEdictCalls = 0;
int gEngineCreateEdictCalls = 0;
bool gNetworkStringTablesLocked = true;
int gNetworkStringTableLockCalls = 0;
int gFakeClientCreateCalls = 0;
int gEngineIndexOfEdictCalls = 0;
int gEngineNetworkIdCalls = 0;

int EngineGetPlayerUserId(void* self, const cssv34::build4100::Edict* edict) {
    assert(self == &gEngineObject);
    if (!edict || edict < &gEngineEdicts[0] || edict >= &gEngineEdicts[33]) return -1;
    return 1000 + static_cast<int>(edict - &gEngineEdicts[0]);
}
const char* EngineGetPlayerNetworkIdString(void* self, const cssv34::build4100::Edict* edict) {
    assert(self == &gEngineObject);
    ++gEngineNetworkIdCalls;
    return edict == &gEngineEdicts[1] ? "STEAM_0:1:2" : nullptr;
}
int EngineIndexOfEdict(void* self, const cssv34::build4100::Edict* edict) {
    assert(self == &gEngineObject);
    ++gEngineIndexOfEdictCalls;
    if (!edict || edict < &gEngineEdicts[0] || edict >= &gEngineEdicts[33]) return -1;
    return static_cast<int>(edict - &gEngineEdicts[0]);
}
cssv34::build4100::Edict* EngineCreateEdict(void* self, int forcedIndex) {
    assert(self == &gEngineObject);
    assert(forcedIndex == -1);
    ++gEngineCreateEdictCalls;
    // This ABI fixture has maxClients=32 and only edicts 0..32, so it has no
    // legal non-player slot to hand out. Returning null is deliberate: the
    // regression here is ordering (the attempt must happen in ServerActivate),
    // while the real-engine oracle verifies successful pre-baseline allocation.
    return nullptr;
}
void EngineRemoveEdict(void* self, cssv34::build4100::Edict* edict) {
    assert(self == &gEngineObject);
    assert(edict);
    ++gEngineRemoveEdictCalls;
    edict->stateFlags |= cssv34::build4100::kEdictFree;
}
void EngineServerCommand(void* self, const char* command) {
    auto* object = static_cast<FakeEngineObject*>(self);
    assert(object && object->state && command);
    object->state->commands.emplace_back(command);
}
void EngineServerExecute(void* self) {
    auto* object = static_cast<FakeEngineObject*>(self);
    assert(object && object->state);
    ++object->state->executeCalls;
}
bool EngineLockNetworkStringTables(void* self, const bool lock) {
    assert(self == &gEngineObject);
    const bool previous = gNetworkStringTablesLocked;
    gNetworkStringTablesLocked = lock;
    ++gNetworkStringTableLockCalls;
    return previous;
}
cssv34::build4100::Edict* EngineCreateFakeClient(void* self, const char* name) {
    assert(self == &gEngineObject && name);
    char expected[48]{};
    std::snprintf(expected, sizeof(expected), "cssv34_bot_%02d", (gFakeClientCreateCalls % 2) + 1);
    assert(std::strcmp(name, expected) == 0);
    const int edictIndex = 20 + gFakeClientCreateCalls;
    assert(edictIndex < 33);
    ++gFakeClientCreateCalls;
    return &gEngineEdicts[edictIndex];
}
int MaterialsAddString(void* self, const char* value, int length, const void* userData) {
    auto* table = static_cast<StringTableObject*>(self);
    assert(table && table->name && std::strcmp(table->name, "Materials") == 0);
    assert(value && std::strcmp(value, "debug/debugempty") == 0);
    assert(length == -1 && userData == nullptr);
    ++gMaterialsAddCalls;
    return 0;
}
void* CreateStringTable(void* self, const char* tableName, int maxEntries,
                        int fixedSize, int networkBits) {
    assert(self == &gStringContainerObject && tableName && gStringTableCreates.size() < 7);
    const std::size_t index = gStringTableCreates.size();
    gStringTableCreates.push_back({tableName, maxEntries, fixedSize, networkBits});
    gStringTables[index].name = tableName;
    return &gStringTables[index];
}
void Tool2(void*) { ++gToolCalls[2]; }
void Tool3(void*) { ++gToolCalls[3]; }
void Tool4(void*) { ++gToolCalls[4]; }
void Tool5(void*) { ++gToolCalls[5]; }
void Tool6(void*) { ++gToolCalls[6]; }
void Tool7(void*) { ++gToolCalls[7]; }
void Tool8(void*) { ++gToolCalls[8]; }

void CvarRegister(void* self, void* commandBase) {
    assert(self == &gCvarObject && commandBase);
    auto** vt = *reinterpret_cast<void***>(commandBase);
    assert(vt && vt[5]);
    using GetNameFn = const char* (*)(void*);
    GetNameFn getName = nullptr;
    std::memcpy(&getName, &vt[5], sizeof(getName));
    const char* name = getName(commandBase);
    assert(name && *name);
    gRegisteredCvars.emplace_back(name);
    gRegisteredCvarBases.push_back(commandBase);
}
void* CvarFindVar(void* self, const char* name) {
    assert(self == &gCvarObject);
    assert(name);
    return nullptr;
}
void* CvarGetCommands(void* self) { assert(self == &gCvarObject); return nullptr; }
void CvarUnlink(void* self, int flags) {
    assert(self == &gCvarObject);
    gCvarUnlinkFlags = flags;
}

int GameEventLoadEventsFromFile(void* self, const char* filename) {
    assert(self == &gEventManagerObject);
    assert(filename);
    gLoadedEventFiles.emplace_back(filename);
    if (std::strcmp(filename, "resource/gameevents.res") == 0) return 1;
    if (std::strcmp(filename, "resource/ModEvents.res") == 0) return 1;
    return 0;
}

bool GameEventAddListener(void* self, void* listener, const char* name, bool serverSide) {
    assert(self == &gEventManagerObject);
    assert(listener && name && *name && serverSide);
    auto** listenerVtable = *reinterpret_cast<void***>(listener);
    assert(listenerVtable && listenerVtable[0] && listenerVtable[1] && listenerVtable[2]);
    gGameEventListener = listener;
    gGameEventListenerNames.emplace_back(name);
    ++gGameEventAddListenerCalls;
    return true;
}

void GameEventRemoveListener(void* self, void* listener) {
    assert(self == &gEventManagerObject);
    assert(listener == gGameEventListener);
    ++gGameEventRemoveListenerCalls;
}

void* GameEventCreate(void* self, const char* name, bool force) {
    assert(self == &gEventManagerObject);
    assert(name && *name);
    // The build4100 preflight deliberately uses force=false; registration of
    // a server-side listener is what makes CreateEvent return non-null.
    if (!force) {
        bool registered = false;
        for (const auto& eventName : gGameEventListenerNames) {
            if (eventName == name) { registered = true; break; }
        }
        if (!registered) return nullptr;
    }
    ++gGameEventCreateCalls;
    return &gEventObject;
}

void GameEventFree(void* self, void* event) {
    assert(self == &gEventManagerObject);
    assert(event == &gEventObject);
    ++gGameEventFreeCalls;
}

bool VoiceGetClientListening(void* self, int receiver, int sender) {
    assert(self == &gVoiceServerObject);
    for (auto it = gVoiceListenCalls.rbegin(); it != gVoiceListenCalls.rend(); ++it) {
        if (it->receiver == receiver && it->sender == sender) return it->listen;
    }
    return false;
}

bool VoiceSetClientListening(void* self, int receiver, int sender, bool listen) {
    assert(self == &gVoiceServerObject);
    assert(receiver >= 1 && receiver <= 32);
    assert(sender >= 1 && sender <= 32);
    gVoiceListenCalls.push_back({receiver, sender, listen});
    return true;
}


void EngineTraceRay(void*, const void*, std::uint32_t, void*, void*) {}

void* EngineFactory(const char* name, int* returnCode) {
    void* result = nullptr;
    if (name && std::strcmp(name, "VEngineServer021") == 0) result = &gEngineObject;
    else if (name && std::strcmp(name, "EngineTraceServer003") == 0) result = &gEngineTraceObject;
    else if (name && std::strcmp(name, "VEngineServerStringTable001") == 0) result = &gStringContainerObject;
    else if (name && std::strcmp(name, "VSERVERENGINETOOLS001") == 0) result = &gToolsObject;
    else if (name && std::strcmp(name, "VEngineCvar003") == 0) result = &gCvarObject;
    else if (name && std::strcmp(name, "GAMEEVENTSMANAGER002") == 0) result = &gEventManagerObject;
    else if (name && std::strcmp(name, "VoiceServer001") == 0) result = &gVoiceServerObject;
    if (returnCode) *returnCode = result ? 0 : 1;
    return result;
}

template <typename Fn>
Fn Slot(void** vtable, std::size_t index) {
    Fn fn = nullptr;
    static_assert(sizeof(fn) == sizeof(vtable[index]));
    std::memcpy(&fn, &vtable[index], sizeof(fn));
    return fn;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: servergamedll006-abi-smoke <server_i486.so>\n";
        return 2;
    }

    gEngineVtable[15] = reinterpret_cast<void*>(&EngineGetPlayerUserId);
    gEngineVtable[16] = reinterpret_cast<void*>(&EngineGetPlayerNetworkIdString);
    gEngineVtable[18] = reinterpret_cast<void*>(&EngineIndexOfEdict);
    gEngineVtable[21] = reinterpret_cast<void*>(&EngineCreateEdict);
    gEngineVtable[22] = reinterpret_cast<void*>(&EngineRemoveEdict);
    gEngineVtable[36] = reinterpret_cast<void*>(&EngineServerCommand);
    gEngineVtable[37] = reinterpret_cast<void*>(&EngineServerExecute);
    gEngineVtable[56] = reinterpret_cast<void*>(&EngineLockNetworkStringTables);
    gEngineVtable[57] = reinterpret_cast<void*>(&EngineCreateFakeClient);
    gEngineObject.vtable = gEngineVtable;
    gEngineTraceVtable[4] = reinterpret_cast<void*>(&EngineTraceRay);
    gEngineTraceObject.vtable = gEngineTraceVtable;
    gEngineObject.state = &gEngineState;

    gStringContainerVtable[2] = reinterpret_cast<void*>(&CreateStringTable);
    gStringContainerObject.vtable = gStringContainerVtable;
    for (std::size_t i = 0; i < 7; ++i) gStringTables[i].vtable = gStringTableVtables[i];
    gStringTableVtables[2][9] = reinterpret_cast<void*>(&MaterialsAddString);

    gToolsVtable[2] = reinterpret_cast<void*>(&Tool2);
    gToolsVtable[3] = reinterpret_cast<void*>(&Tool3);
    gToolsVtable[4] = reinterpret_cast<void*>(&Tool4);
    gToolsVtable[5] = reinterpret_cast<void*>(&Tool5);
    gToolsVtable[6] = reinterpret_cast<void*>(&Tool6);
    gToolsVtable[7] = reinterpret_cast<void*>(&Tool7);
    gToolsVtable[8] = reinterpret_cast<void*>(&Tool8);
    gToolsObject.vtable = gToolsVtable;

    gCvarVtable[5] = reinterpret_cast<void*>(&CvarRegister);
    gCvarVtable[7] = reinterpret_cast<void*>(&CvarFindVar);
    gCvarVtable[9] = reinterpret_cast<void*>(&CvarGetCommands);
    gCvarVtable[10] = reinterpret_cast<void*>(&CvarUnlink);
    gCvarObject.vtable = gCvarVtable;

    gEventManagerVtable[2] = reinterpret_cast<void*>(&GameEventLoadEventsFromFile);
    gEventManagerVtable[4] = reinterpret_cast<void*>(&GameEventAddListener);
    gEventManagerVtable[6] = reinterpret_cast<void*>(&GameEventRemoveListener);
    gEventManagerVtable[7] = reinterpret_cast<void*>(&GameEventCreate);
    gEventManagerVtable[11] = reinterpret_cast<void*>(&GameEventFree);
    gEventManagerObject.vtable = gEventManagerVtable;
    gEventObject.vtable = gEventVtable;
    gVoiceServerVtable[2] = reinterpret_cast<void*>(&VoiceGetClientListening);
    gVoiceServerVtable[3] = reinterpret_cast<void*>(&VoiceSetClientListening);
    gVoiceServerObject.vtable = gVoiceServerVtable;

    void* handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << '\n';
        return 3;
    }
    void* createInterfaceSymbol = dlsym(handle, "CreateInterface");
    assert(createInterfaceSymbol);
    CreateInterfaceFn createInterface = nullptr;
    std::memcpy(&createInterface, &createInterfaceSymbol, sizeof(createInterface));

    int rc = -1;
    assert(createInterface("NoSuchInterface001", &rc) == nullptr && rc != 0);

    // Exact build4100 VServerDllSharedAppSystems001 runtime contract.
    rc = -1;
    void* sharedObject = createInterface("VServerDllSharedAppSystems001", &rc);
    assert(sharedObject && rc == 0);
    auto** sharedVtable = *reinterpret_cast<void***>(sharedObject);
    assert(sharedVtable && sharedVtable[0] && sharedVtable[1] && sharedVtable[2]);
    using SharedCountFn = int (*)(void*);
    using SharedNameFn = const char* (*)(void*, int);
    assert(Slot<SharedCountFn>(sharedVtable, 0)(sharedObject) == 2);
    assert(std::strcmp(Slot<SharedNameFn>(sharedVtable, 1)(sharedObject, 0), "soundemittersystem") == 0);
    assert(std::strcmp(Slot<SharedNameFn>(sharedVtable, 2)(sharedObject, 0), "VSoundEmitter002") == 0);
    assert(std::strcmp(Slot<SharedNameFn>(sharedVtable, 1)(sharedObject, 1), "scenefilecache") == 0);
    assert(std::strcmp(Slot<SharedNameFn>(sharedVtable, 2)(sharedObject, 1), "SceneFileCache001") == 0);
    assert(Slot<SharedNameFn>(sharedVtable, 1)(sharedObject, -1) == nullptr);
    assert(Slot<SharedNameFn>(sharedVtable, 2)(sharedObject, 2) == nullptr);

    rc = -1;
    void* clientsObject = createInterface("ServerGameClients004", &rc);
    assert(clientsObject && rc == 0);
    auto** clientsVtable = *reinterpret_cast<void***>(clientsObject);
    assert(clientsVtable);
    for (std::size_t slot = 0; slot <= 15; ++slot) assert(clientsVtable[slot]);
    rc = -1;
    assert(createInterface("ServerGameClients003", &rc) == clientsObject && rc == 0);

    using GetPlayerLimitsFn = void (*)(void*, int&, int&, int&);
    const auto getPlayerLimits = Slot<GetPlayerLimitsFn>(clientsVtable, 0);
    int minPlayers = -1, maxPlayers = -1, defaultMaxPlayers = -1;
    getPlayerLimits(clientsObject, minPlayers, maxPlayers, defaultMaxPlayers);
    assert(minPlayers == 1 && maxPlayers == 64 && defaultMaxPlayers == 32);

    rc = -1;
    void* object = createInterface("ServerGameDLL006", &rc);
    assert(object && rc == 0);
    auto** vtable = *reinterpret_cast<void***>(object);
    assert(vtable);
    for (std::size_t slot = 0; slot <= 32; ++slot) assert(vtable[slot]);

    using DLLInitFn = bool (*)(void*, FactoryFn, FactoryFn, FactoryFn, void*);
    using GameInitFn = bool (*)(void*);
    using LevelInitFn = bool (*)(void*, const char*, const char*, const char*, const char*, bool, bool);
    using ServerActivateFn = void (*)(void*, void*, int, int);
    using FrameFn = void (*)(void*, bool);
    using VoidFn = void (*)(void*);
    using GetTickIntervalFn = float (*)(void*);
    using GetPointerFn = void* (*)(void*);
    using GetStringFn = const char* (*)(void*);
    using SaveInitFn = void* (*)(void*, int);
    using SaveFieldsFn = void (*)(void*, void*, const char*, void*, void*, void*, int);
    using SavePtrFn = void (*)(void*, void*);
    using RestoreFn = void (*)(void*, void*, bool);
    using IsRestoringFn = bool (*)(void*);
    using TransitionFn = int (*)(void*, void*, int);
    using UserMessageInfoFn = bool (*)(void*, int, char*, int, int&);
    const auto dllInit = Slot<DLLInitFn>(vtable, 0);
    const auto gameInit = Slot<GameInitFn>(vtable, 1);
    const auto levelInit = Slot<LevelInitFn>(vtable, 2);
    const auto serverActivate = Slot<ServerActivateFn>(vtable, 3);
    const auto gameFrame = Slot<FrameFn>(vtable, 4);
    const auto preClientUpdate = Slot<FrameFn>(vtable, 5);
    const auto levelShutdown = Slot<VoidFn>(vtable, 6);
    const auto gameShutdown = Slot<VoidFn>(vtable, 7);
    const auto dllShutdown = Slot<VoidFn>(vtable, 8);
    const auto getTickInterval = Slot<GetTickIntervalFn>(vtable, 9);
    const auto getAllServerClasses = Slot<GetPointerFn>(vtable, 10);
    const auto getGameDescription = Slot<GetStringFn>(vtable, 11);
    const auto createNetworkStringTables = Slot<VoidFn>(vtable, 12);
    const auto saveInit = Slot<SaveInitFn>(vtable, 13);
    const auto saveWriteFields = Slot<SaveFieldsFn>(vtable, 14);
    const auto saveReadFields = Slot<SaveFieldsFn>(vtable, 15);
    const auto saveGlobalState = Slot<SavePtrFn>(vtable, 16);
    const auto restoreGlobalState = Slot<SavePtrFn>(vtable, 17);
    const auto preSave = Slot<SavePtrFn>(vtable, 18);
    const auto save = Slot<SavePtrFn>(vtable, 19);
    using GetSaveCommentFn = void (*)(void*, char*, int);
    const auto getSaveComment = Slot<GetSaveCommentFn>(vtable, 20);
    const auto writeSaveHeaders = Slot<SavePtrFn>(vtable, 21);
    const auto readRestoreHeaders = Slot<SavePtrFn>(vtable, 22);
    const auto restore = Slot<RestoreFn>(vtable, 23);
    const auto isRestoring = Slot<IsRestoringFn>(vtable, 24);
    const auto createEntityTransitionList = Slot<TransitionFn>(vtable, 25);
    const auto buildAdjacentMapList = Slot<VoidFn>(vtable, 26);
    const auto getUserMessageInfo = Slot<UserMessageInfoFn>(vtable, 27);
    const auto getStandardSendProxies = Slot<GetPointerFn>(vtable, 28);
    const auto postInit = Slot<VoidFn>(vtable, 29);

    cssv34::build4100::GlobalVarsBase globals{};
    globals.maxClients = 32;
    globals.interval_per_tick = 0.015f;
    assert(getTickInterval(object) == 0.015f);
    assert(std::strcmp(getGameDescription(object), "Counter-Strike: Source") == 0);
    auto* serverClass = static_cast<cssv34::build4100::ServerClass*>(getAllServerClasses(object));
    constexpr const char* kExpectedServerClasses[] = {
        "CWorld", "CCSPlayer", "CCSTeam", "CCSPlayerResource", "CCSRagdoll",
        "CHostage", "CPlantedC4", "CAK47", "CWeaponM4A1", "CWeaponAWP",
        "CC4", "CFlashbang", "CHEGrenade", "CSmokeGrenade",
        "CBaseCSGrenadeProjectile"
    };
    constexpr const char* kExpectedSendTables[] = {
        "DT_WORLD", "DT_CSPlayer", "DT_CSTeam", "DT_CSPlayerResource", "DT_CSRagdoll",
        "DT_CHostage", "DT_PlantedC4", "DT_WeaponAK47", "DT_WeaponM4A1", "DT_WeaponAWP",
        "DT_WeaponC4", "DT_Flashbang", "DT_HEGrenade", "DT_SmokeGrenade",
        "DT_BaseCSGrenadeProjectile"
    };
    constexpr int kExpectedLivePropCounts[] = {
        11, 38, 1, 17, 11, 14, 6, 1, 3, 1, 4, 1, 1, 1, 2
    };
    for (std::size_t i = 0; i < std::size(kExpectedServerClasses); ++i) {
        assert(serverClass);
        assert(std::strcmp(serverClass->networkName, kExpectedServerClasses[i]) == 0);
        assert(serverClass->table && std::strcmp(serverClass->table->netTableName, kExpectedSendTables[i]) == 0);
        assert(serverClass->table->propCount == kExpectedLivePropCounts[i]);
        assert(serverClass->table->props != nullptr);
        assert(serverClass->classId == static_cast<int>(i));
        assert(serverClass->instanceBaselineIndex == 0xffff);
        serverClass = serverClass->next;
    }
    std::size_t fullServerClassCount = std::size(kExpectedServerClasses);
    for (; serverClass; serverClass = serverClass->next) {
        assert(serverClass->networkName && serverClass->networkName[0]);
        assert(serverClass->table && serverClass->table->netTableName);
        assert(serverClass->table->propCount >= 0);
        if (serverClass->table->propCount > 0) assert(serverClass->table->props);
        assert(serverClass->classId == static_cast<int>(fullServerClassCount));
        assert(serverClass->instanceBaselineIndex == 0xffff);
        ++fullServerClassCount;
    }
    assert(fullServerClassCount == 177);
    assert(saveInit(object, 1024) == nullptr);
    saveWriteFields(object, nullptr, "probe", nullptr, nullptr, nullptr, 0);
    saveReadFields(object, nullptr, "probe", nullptr, nullptr, nullptr, 0);
    saveGlobalState(object, nullptr); restoreGlobalState(object, nullptr); preSave(object, nullptr); save(object, nullptr);
    char saveComment[16] = "dirty"; getSaveComment(object, saveComment, sizeof(saveComment)); assert(saveComment[0] == '\0');
    writeSaveHeaders(object, nullptr); readRestoreHeaders(object, nullptr); restore(object, nullptr, false);
    assert(!isRestoring(object));
    assert(createEntityTransitionList(object, nullptr, 128) == 0);
    buildAdjacentMapList(object);
    char userMessageName[16] = "dirty"; int userMessageSize = 123;
    assert(getUserMessageInfo(object, 1, userMessageName, sizeof(userMessageName), userMessageSize));
    assert(std::strcmp(userMessageName, "Train") == 0 && userMessageSize == 1);
    assert(getUserMessageInfo(object, 3, userMessageName, sizeof(userMessageName), userMessageSize));
    assert(std::strcmp(userMessageName, "SayText") == 0 && userMessageSize == -1);
    assert(!getUserMessageInfo(object, 99, userMessageName, sizeof(userMessageName), userMessageSize));
    assert(userMessageName[0] == '\0' && userMessageSize == -1);
    // Original build4100 CServerGameDLL::GetStandardSendProxies returns null;
    // the engine then uses its own standard proxy set.
    assert(getStandardSendProxies(object) == nullptr);
    assert(!dllInit(object, nullptr, nullptr, nullptr, &globals));
    assert(dllInit(object, &EngineFactory, nullptr, nullptr, &globals));
    assert(gLoadedEventFiles.size() == 2);
    assert(gGameEventAddListenerCalls == 3);
    assert(gGameEventListenerNames.size() == 3);
    assert(gGameEventListenerNames[0] == "player_team");
    assert(gGameEventListenerNames[1] == "player_disconnect");
    assert(gGameEventListenerNames[2] == "player_connect");
    assert(gGameEventCreateCalls == 3 && gGameEventFreeCalls == 3);
    assert(gLoadedEventFiles.size() == 2);
    assert(gLoadedEventFiles[0] == "resource/gameevents.res");
    assert(gLoadedEventFiles[1] == "resource/ModEvents.res");
    assert(getTickInterval(object) == globals.interval_per_tick);
    assert(gRegisteredCvars.size() == 11);
    constexpr const char* kExpectedCvars[] = {
        "mp_startmoney", "mp_maxmoney", "mp_freezetime", "mp_roundtime",
        "mp_buytime", "mp_c4timer", "mp_friendlyfire", "bot_quota",
        "mp_autoteambalance", "mp_limitteams", "v34_rebuild_status"
    };
    for (std::size_t i = 0; i < std::size(kExpectedCvars); ++i)
        assert(gRegisteredCvars[i] == kExpectedCvars[i]);

    void* botQuotaBase = nullptr;
    for (void* base : gRegisteredCvarBases) {
        auto** baseVtable = *reinterpret_cast<void***>(base);
        using GetNameFn = const char* (*)(void*);
        const char* registeredName = Slot<GetNameFn>(baseVtable, 5)(base);
        if (registeredName && std::strcmp(registeredName, "bot_quota") == 0) {
            botQuotaBase = base;
            break;
        }
    }
    assert(botQuotaBase);
    auto** botQuotaVtable = *reinterpret_cast<void***>(botQuotaBase);
    using SetConVarIntFn = void (*)(void*, int);
    Slot<SetConVarIntFn>(botQuotaVtable, 12)(botQuotaBase, 2);

    rc = -1;
    void* pimObject = createInterface("PlayerInfoManager002", &rc);
    assert(pimObject && rc == 0);
    auto** pimVtable = *reinterpret_cast<void***>(pimObject);
    using GetPlayerInfoFn = void* (*)(void*, cssv34::build4100::Edict*);
    using GetGlobalVarsFn = cssv34::build4100::GlobalVarsBase* (*)(void*);
    const auto getPlayerInfo = Slot<GetPlayerInfoFn>(pimVtable, 0);
    const auto getGlobalVars = Slot<GetGlobalVarsFn>(pimVtable, 1);
    assert(getPlayerInfo(pimObject, &gEngineEdicts[1]) == nullptr);
    assert(getGlobalVars(pimObject) == &globals);
    rc = -1;
    assert(createInterface("PlayerInfoManager001", &rc) == pimObject && rc == 0);

    rc = -1;
    void* entsObject = createInterface("ServerGameEnts001", &rc);
    assert(entsObject && rc == 0);
    auto** entsVtable = *reinterpret_cast<void***>(entsObject);
    assert(entsVtable);
    for (std::size_t slot = 0; slot <= 7; ++slot) assert(entsVtable[slot]);
    using FreeContainingEntityFn = void (*)(void*, cssv34::build4100::Edict*);
    const auto freeContainingEntity = Slot<FreeContainingEntityFn>(entsVtable, 4);
    gEngineEdicts[5].stateFlags = 0;
    const int removeCallsBeforeFreeContaining = gEngineRemoveEdictCalls;
    freeContainingEntity(entsObject, &gEngineEdicts[5]);
    // build4100 ED_ClearEdict invokes this callback while allocating/reusing an
    // edict.  It must never re-enter IVEngineServer::RemoveEdict.
    assert(gEngineRemoveEdictCalls == removeCallsBeforeFreeContaining);
    assert(!cssv34::build4100::IsFree(gEngineEdicts[5]));

    rc = -1;
    void* movementObject = createInterface("GameMovement001", &rc);
    assert(movementObject && rc == 0);
    auto** movementVtable = *reinterpret_cast<void***>(movementObject);
    assert(movementVtable);
    for (std::size_t slot = 0; slot <= 5; ++slot) assert(movementVtable[slot]);
    using MovementHullFn = const cssv34::build4100::Vector& (*)(void*, bool);
    const auto getMoveMins = Slot<MovementHullFn>(movementVtable, 3);
    const auto getMoveMaxs = Slot<MovementHullFn>(movementVtable, 4);
    const auto getMoveView = Slot<MovementHullFn>(movementVtable, 5);
    const auto& standMins = getMoveMins(movementObject, false);
    const auto& standMaxs = getMoveMaxs(movementObject, false);
    const auto& duckMaxs = getMoveMaxs(movementObject, true);
    const auto& standView = getMoveView(movementObject, false);
    const auto& duckView = getMoveView(movementObject, true);
    assert(standMins.x == -16.0f && standMins.y == -16.0f && standMins.z == 0.0f);
    assert(standMaxs.x == 16.0f && standMaxs.y == 16.0f && standMaxs.z == 72.0f);
    assert(duckMaxs.z == 36.0f && standView.z == 64.0f && duckView.z == 28.0f);

    using ClientConnectFn = bool (*)(void*, cssv34::build4100::Edict*, const char*, const char*, char*, int);
    using ClientActiveFn = void (*)(void*, cssv34::build4100::Edict*, bool);
    using ClientEdictFn = void (*)(void*, cssv34::build4100::Edict*);
    using ClientPutFn = void (*)(void*, cssv34::build4100::Edict*, const char*);
    using SetCommandClientFn = void (*)(void*, int);
    using ClientVisibilityFn = void (*)(void*, cssv34::build4100::Edict*, cssv34::build4100::Edict*, unsigned char*, int);
    using ProcessUsercmdsFn = float (*)(void*, cssv34::build4100::Edict*, void*, int, int, int, bool, bool);
    using GetPlayerStateFn = void* (*)(void*, cssv34::build4100::Edict*);
    using ClientEarPositionFn = void (*)(void*, cssv34::build4100::Edict*, cssv34::build4100::Vector*);
    using GetReplayDelayFn = int (*)(void*, cssv34::build4100::Edict*, int&);
    using GetBugReportInfoFn = void (*)(void*, char*, int);
    using NetworkIDValidatedFn = void (*)(void*, const char*, const char*);

    const auto clientConnect = Slot<ClientConnectFn>(clientsVtable, 1);
    const auto clientActive = Slot<ClientActiveFn>(clientsVtable, 2);
    const auto clientDisconnect = Slot<ClientEdictFn>(clientsVtable, 3);
    const auto clientPutInServer = Slot<ClientPutFn>(clientsVtable, 4);
    const auto clientCommand = Slot<ClientEdictFn>(clientsVtable, 5);
    const auto setCommandClient = Slot<SetCommandClientFn>(clientsVtable, 6);
    const auto clientSettingsChanged = Slot<ClientEdictFn>(clientsVtable, 7);
    const auto clientSetupVisibility = Slot<ClientVisibilityFn>(clientsVtable, 8);
    const auto processUsercmds = Slot<ProcessUsercmdsFn>(clientsVtable, 9);
    const auto postClientMessagesSent = Slot<VoidFn>(clientsVtable, 10);
    const auto getPlayerState = Slot<GetPlayerStateFn>(clientsVtable, 11);
    const auto clientEarPosition = Slot<ClientEarPositionFn>(clientsVtable, 12);
    const auto getReplayDelay = Slot<GetReplayDelayFn>(clientsVtable, 13);
    const auto getBugReportInfo = Slot<GetBugReportInfoFn>(clientsVtable, 14);
    const auto networkIDValidated = Slot<NetworkIDValidatedFn>(clientsVtable, 15);

    char reject[64] = {};
    assert(!clientConnect(clientsObject, &gEngineEdicts[0], "world", "local", reject, sizeof(reject)));
    assert(clientConnect(clientsObject, &gEngineEdicts[1], "alpha", "10.0.0.1", reject, sizeof(reject)));
    assert(!clientConnect(clientsObject, &gEngineEdicts[1], "duplicate", "10.0.0.2", reject, sizeof(reject)));
    clientPutInServer(clientsObject, &gEngineEdicts[1], "alpha");
    clientActive(clientsObject, &gEngineEdicts[1], false);
    // VoiceServer001 is engine-owned: active clients must seed the hearing matrix.
    // With alpha as the first active client there are no cross-client pairs yet.
    assert(gVoiceListenCalls.empty());
    void* playerInfo = getPlayerInfo(pimObject, &gEngineEdicts[1]);
    assert(playerInfo);
    auto** playerInfoVtable = *reinterpret_cast<void***>(playerInfo);
    assert(playerInfoVtable);
    for (std::size_t slot = 0; slot <= 23; ++slot) assert(playerInfoVtable[slot]);

    using PIStringFn = const char* (*)(void*);
    using PIIntFn = int (*)(void*);
    using PIBoolFn = bool (*)(void*);
    using PIChangeTeamFn = void (*)(void*, int);
    using PIVectorFn = cssv34::build4100::Vector (*)(void*);
    using PIBotCmdFn = cssv34::build4100::BotCmd (*)(void*);

    const auto piGetName = Slot<PIStringFn>(playerInfoVtable, 0);
    const auto piGetUserId = Slot<PIIntFn>(playerInfoVtable, 1);
    const auto piGetNetworkId = Slot<PIStringFn>(playerInfoVtable, 2);
    const auto piGetTeam = Slot<PIIntFn>(playerInfoVtable, 3);
    const auto piChangeTeam = Slot<PIChangeTeamFn>(playerInfoVtable, 4);
    const auto piGetFrags = Slot<PIIntFn>(playerInfoVtable, 5);
    const auto piGetDeaths = Slot<PIIntFn>(playerInfoVtable, 6);
    const auto piIsConnected = Slot<PIBoolFn>(playerInfoVtable, 7);
    const auto piGetArmor = Slot<PIIntFn>(playerInfoVtable, 8);
    const auto piIsHLTV = Slot<PIBoolFn>(playerInfoVtable, 9);
    const auto piIsPlayer = Slot<PIBoolFn>(playerInfoVtable, 10);
    const auto piIsFake = Slot<PIBoolFn>(playerInfoVtable, 11);
    const auto piIsDead = Slot<PIBoolFn>(playerInfoVtable, 12);
    const auto piInVehicle = Slot<PIBoolFn>(playerInfoVtable, 13);
    const auto piIsObserver = Slot<PIBoolFn>(playerInfoVtable, 14);
    const auto piGetAbsOrigin = Slot<PIVectorFn>(playerInfoVtable, 15);
    const auto piGetAbsAngles = Slot<PIVectorFn>(playerInfoVtable, 16);
    const auto piGetMins = Slot<PIVectorFn>(playerInfoVtable, 17);
    const auto piGetMaxs = Slot<PIVectorFn>(playerInfoVtable, 18);
    const auto piGetWeapon = Slot<PIStringFn>(playerInfoVtable, 19);
    const auto piGetModel = Slot<PIStringFn>(playerInfoVtable, 20);
    const auto piGetHealth = Slot<PIIntFn>(playerInfoVtable, 21);
    const auto piGetMaxHealth = Slot<PIIntFn>(playerInfoVtable, 22);
    const auto piGetLastUserCmd = Slot<PIBotCmdFn>(playerInfoVtable, 23);

    assert(std::strcmp(piGetName(playerInfo), "alpha") == 0);
    assert(piGetUserId(playerInfo) == 1001);
    assert(std::strcmp(piGetNetworkId(playerInfo), "STEAM_0:1:2") == 0);
    assert(piGetTeam(playerInfo) == 0);
    piChangeTeam(playerInfo, 2);
    assert(piGetTeam(playerInfo) == 2);
    assert(piGetFrags(playerInfo) == 0 && piGetDeaths(playerInfo) == 0);
    assert(piIsConnected(playerInfo));
    assert(piGetArmor(playerInfo) == 0 && !piIsHLTV(playerInfo));
    assert(piIsPlayer(playerInfo) && !piIsFake(playerInfo));
    assert(!piIsDead(playerInfo) && !piInVehicle(playerInfo) && !piIsObserver(playerInfo));
    const auto absOrigin = piGetAbsOrigin(playerInfo);
    const auto absAngles = piGetAbsAngles(playerInfo);
    const auto mins = piGetMins(playerInfo);
    const auto maxs = piGetMaxs(playerInfo);
    assert(absOrigin.x == 0.0f && absOrigin.y == 0.0f && absOrigin.z == 0.0f);
    assert(absAngles.x == 0.0f && absAngles.y == 0.0f && absAngles.z == 0.0f);
    assert(mins.x == 0.0f && mins.y == 0.0f && mins.z == 0.0f);
    assert(maxs.x == 0.0f && maxs.y == 0.0f && maxs.z == 0.0f);
    assert(piGetWeapon(playerInfo) == nullptr);
    assert(std::strcmp(piGetModel(playerInfo), "") == 0);
    assert(piGetHealth(playerInfo) == 100 && piGetMaxHealth(playerInfo) == 100);
    const auto lastCmd = piGetLastUserCmd(playerInfo);
    assert(lastCmd.commandNumber == 0 && lastCmd.tickCount == 0 && lastCmd.buttons == 0);
    setCommandClient(clientsObject, 0);
    clientCommand(clientsObject, &gEngineEdicts[1]);
    clientSettingsChanged(clientsObject, &gEngineEdicts[1]);
    unsigned char pvs[8] = {1,2,3,4,5,6,7,8};
    const unsigned char expectedPvs[8] = {1,2,3,4,5,6,7,8};
    clientSetupVisibility(clientsObject, &gEngineEdicts[1], &gEngineEdicts[1], pvs, sizeof(pvs));
    assert(std::memcmp(pvs, expectedPvs, sizeof(pvs)) == 0);
    cssv34::build4100::UserCmd wireZero;
    cssv34::build4100::UserCmd wireFirst;
    wireFirst.commandNumber = 100;
    wireFirst.tickCount = 200;
    wireFirst.viewAngles = {90.0f, 180.0f, 45.0f};
    wireFirst.forwardMove = 250.0f;
    wireFirst.sideMove = -125.0f;
    wireFirst.buttons = 5;
    wireFirst.weaponSelect = 7;
    wireFirst.weaponSubtype = 2;
    wireFirst.mouseDx = 9;
    wireFirst.mouseDy = -10;
    cssv34::build4100::UserCmd wireSecond = wireFirst;
    wireSecond.commandNumber = 101;
    wireSecond.tickCount = 201;
    wireSecond.viewAngles.y = 270.0f;
    wireSecond.buttons = 9;

    cssv34::reconstructed::test::BitWriter wireWriter;
    cssv34::reconstructed::test::WriteDeltaUserCmd(wireWriter, wireFirst, wireZero);
    cssv34::reconstructed::test::WriteDeltaUserCmd(wireWriter, wireSecond, wireFirst);
    auto wireReader = cssv34::reconstructed::test::MakeBitReader(wireWriter);
    assert(processUsercmds(clientsObject, &gEngineEdicts[1], &wireReader, 1, 2, 0, false, false) == globals.interval_per_tick);
    assert(!wireReader.overflow && wireReader.curBit == wireWriter.BitCount());
    const auto decodedLastCmd = piGetLastUserCmd(playerInfo);
    assert(decodedLastCmd.commandNumber == 101 && decodedLastCmd.tickCount == 201);
    assert(decodedLastCmd.buttons == 9 && decodedLastCmd.weaponSelect == 7 && decodedLastCmd.weaponSubtype == 2);
    assert(decodedLastCmd.viewAngles.x == 90.0f && decodedLastCmd.viewAngles.y == 270.0f && decodedLastCmd.viewAngles.z == 45.0f);
    const auto movedOrigin = piGetAbsOrigin(playerInfo);
    // numCmds=1 means only the newest decoded command is simulated.  Source
    // ground acceleration (5 * 0.015 * cappedWishSpeed) produces 18.75 u/s
    // along yaw=270 forward=250, side=-125 rather than the old direct teleport.
    assert(std::fabs(movedOrigin.x - 0.125779f) < 0.001f);
    assert(std::fabs(movedOrigin.y + 0.251558f) < 0.001f);
    assert(std::fabs(movedOrigin.z) < 0.001f);

    auto ignoredReader = cssv34::reconstructed::test::MakeBitReader(wireWriter);
    assert(processUsercmds(clientsObject, &gEngineEdicts[1], &ignoredReader, 1, 2, 0, true, false) == 0.0f);
    assert(!ignoredReader.overflow && ignoredReader.curBit == wireWriter.BitCount());

    auto tooManyReader = cssv34::reconstructed::test::MakeBitReader(wireWriter);
    assert(processUsercmds(clientsObject, &gEngineEdicts[1], &tooManyReader, 1, 29, 0, false, false) == 0.0f);
    assert(tooManyReader.overflow);

    postClientMessagesSent(clientsObject);
    auto* playerState = static_cast<cssv34::build4100::PlayerState*>(
        getPlayerState(clientsObject, &gEngineEdicts[1]));
    assert(playerState);
    assert(std::strcmp(playerState->_GetClassName(), "CPlayerState") == 0);
    assert(!playerState->deadflag && playerState->frags == 0 && playerState->deaths == 0);
    assert(playerState->vAngle.x == 90.0f && playerState->vAngle.y == 270.0f && playerState->vAngle.z == 45.0f);
    auto** playerStateVtable = *reinterpret_cast<void***>(playerState);
    assert(playerStateVtable);
    for (std::size_t slot = 0; slot <= 4; ++slot) assert(playerStateVtable[slot]);
    cssv34::build4100::Vector ear{1.0f,2.0f,3.0f};
    clientEarPosition(clientsObject, &gEngineEdicts[1], &ear);
    assert(std::fabs(ear.x - movedOrigin.x) < 0.001f);
    assert(std::fabs(ear.y - movedOrigin.y) < 0.001f);
    assert(std::fabs(ear.z - (movedOrigin.z + 64.0f)) < 0.001f);

    // v1.8.34za regression: ClientEarPosition must follow reconstructed
    // gameplay origin instead of the pre-z zero-vector stub. Verify both the
    // standing and ducked view offsets through the real ProcessUsercmds path.
    cssv34::build4100::UserCmd duckCmd{};
    duckCmd.commandNumber = 102;
    duckCmd.tickCount = 202;
    duckCmd.viewAngles = wireSecond.viewAngles;
    duckCmd.buttons = 1 << 2;
    cssv34::reconstructed::test::BitWriter duckWriter;
    cssv34::reconstructed::test::WriteDeltaUserCmd(duckWriter, duckCmd, wireZero);
    auto duckReader = cssv34::reconstructed::test::MakeBitReader(duckWriter);
    assert(processUsercmds(clientsObject, &gEngineEdicts[1], &duckReader, 1, 1, 0, false, false) == globals.interval_per_tick);
    assert(!duckReader.overflow && duckReader.curBit == duckWriter.BitCount());
    const auto duckOrigin = piGetAbsOrigin(playerInfo);
    cssv34::build4100::Vector duckEar{};
    clientEarPosition(clientsObject, &gEngineEdicts[1], &duckEar);
    assert(std::fabs(duckEar.x - duckOrigin.x) < 0.001f);
    assert(std::fabs(duckEar.y - duckOrigin.y) < 0.001f);
    assert(std::fabs(duckEar.z - (duckOrigin.z + 46.0f)) < 0.001f);

    // Foreign edicts must still resolve safely to the zero vector, and a null
    // destination is explicitly tolerated by the ABI callback.
    cssv34::build4100::Vector foreignEar{9.0f, 9.0f, 9.0f};
    clientEarPosition(clientsObject, &gEngineEdicts[63], &foreignEar);
    assert(foreignEar.x == 0.0f && foreignEar.y == 0.0f && foreignEar.z == 0.0f);
    clientEarPosition(clientsObject, &gEngineEdicts[1], nullptr);
    int replayEntity = 123;
    assert(getReplayDelay(clientsObject, &gEngineEdicts[1], replayEntity) == 0 && replayEntity == -1);
    char bugInfo[16] = "dirty";
    getBugReportInfo(clientsObject, bugInfo, sizeof(bugInfo));
    assert(bugInfo[0] == '\0');
    networkIDValidated(clientsObject, "alpha", "STEAM_0:1:2");
    assert(std::strcmp(piGetNetworkId(playerInfo), "STEAM_0:1:2") == 0);
    clientDisconnect(clientsObject, &gEngineEdicts[1]);
    assert(getPlayerInfo(pimObject, &gEngineEdicts[1]) == nullptr);
    assert(clientConnect(clientsObject, &gEngineEdicts[1], "reconnect", "10.0.0.3", reject, sizeof(reject)));
    clientDisconnect(clientsObject, &gEngineEdicts[1]);

    postInit(object);
    assert(gameInit(object));
    createNetworkStringTables(object);
    constexpr const char* kExpectedNames[] = {"EffectDispatch","VguiScreen","Materials","InfoPanel","Scenes","GameRulesCreation","BlackMarketTable"};
    constexpr int kExpectedMax[] = {1024,256,1024,128,4096,1,1};
    assert(gStringTableCreates.size() == 7);
    for (std::size_t i = 0; i < 7; ++i) {
        assert(gStringTableCreates[i].name == kExpectedNames[i]);
        assert(gStringTableCreates[i].maxEntries == kExpectedMax[i]);
    }
    assert(gMaterialsAddCalls == 1);

    for (int level = 0; level < 2; ++level) {
        assert(levelInit(object, "probe_map", kEntityLump, nullptr, nullptr, false, false));
        const int createEdictBeforeActivate = gEngineCreateEdictCalls;
        serverActivate(object, gEngineEdicts, 33, globals.maxClients);
        // v1.8.34q: planted_c4 reservation must be attempted before
        // ServerActivate returns, because build4100 runs SV_CreateBaseline next.
        // This fixture intentionally has no non-player edict, so only the exact
        // lifecycle ordering is asserted here; Docker verifies baseline creation.
        assert(gEngineCreateEdictCalls == createEdictBeforeActivate + 1);
        // CreateFakeClient is not allowed from inside ServerActivate. The first
        // post-activation GameFrame is the safe engine boundary, even when the
        // server reports simulating=false. One fake client is created per frame.
        assert(gFakeClientCreateCalls == level * 2);

        // Regression for real CreateFakeClient re-entry: once ServerActivate
        // supplies the engine edict array, client callbacks must not recurse
        // through IVEngineServer::IndexOfEdict/GetPlayerNetworkIDString.
        const int botIndex = 20 + level * 2;
        const int indexCallsBeforeBot = gEngineIndexOfEdictCalls;
        const int networkCallsBeforeBot = gEngineNetworkIdCalls;
        char botName[48]{};
        std::snprintf(botName, sizeof(botName), "cssv34_bot_%02d", 1);
        // Real build4100 fake clients skip ClientConnect and re-enter directly
        // through ClientPutInServer -> ClientActive. This must adopt the slot.
        clientPutInServer(clientsObject, &gEngineEdicts[botIndex], botName);
        const std::size_t voiceCallsBeforeBot = gVoiceListenCalls.size();
        clientActive(clientsObject, &gEngineEdicts[botIndex], false);
        // This fixture disconnected the earlier real client before entering the
        // fake-client lifecycle loop. Therefore this manually adopted bot has
        // no active peer yet and ClientActive must not invent a voice pair.
        assert(gVoiceListenCalls.size() == voiceCallsBeforeBot);
        assert(gEngineIndexOfEdictCalls == indexCallsBeforeBot);
        assert(gEngineNetworkIdCalls == networkCallsBeforeBot);
        const std::size_t voiceCallsBeforeDisconnect = gVoiceListenCalls.size();
        clientDisconnect(clientsObject, &gEngineEdicts[botIndex]);
        assert(gVoiceListenCalls.size() >= voiceCallsBeforeDisconnect + 2);
        assert(!gVoiceListenCalls[voiceCallsBeforeDisconnect + 0].listen);
        assert(!gVoiceListenCalls[voiceCallsBeforeDisconnect + 1].listen);

        for (int frame = 0; frame < 16; ++frame) {
            globals.tickcount = level * 16 + frame;
            globals.curtime = static_cast<float>(globals.tickcount) * globals.interval_per_tick;
            globals.frametime = globals.interval_per_tick;
            globals.absoluteframetime = globals.interval_per_tick;
            gameFrame(object, frame != 0);
            preClientUpdate(object, frame != 0);
            if (frame == 0) {
                assert(gFakeClientCreateCalls == level * 2 + 1);
            } else if (frame == 1) {
                assert(gFakeClientCreateCalls == (level + 1) * 2);
            }
        }
        levelShutdown(object);
    }

    assert(gFakeClientCreateCalls == 4);
    assert(gNetworkStringTableLockCalls == 8);
    assert(gNetworkStringTablesLocked);
    // DEBUG queues game.cfg plus one skill1.cfg per level. RELEASE additionally
    // queues server.cfg in the same GameInit ServerExecute batch so production
    // configuration matches the stock dedicated server without changing the
    // deterministic DEBUG regression profile.
    assert((gEngineState.commands.size() == 3 || gEngineState.commands.size() == 4) &&
           gEngineState.executeCalls == 3);
    assert(gEngineState.commands[0] == "exec game.cfg\n");
    const bool releaseConfigQueued = gEngineState.commands.size() == 4;
    if (releaseConfigQueued) assert(gEngineState.commands[1] == "exec server.cfg\n");
    const std::size_t skillOffset = releaseConfigQueued ? 2u : 1u;
    assert(gEngineState.commands[skillOffset + 0] == "exec skill1.cfg\n");
    assert(gEngineState.commands[skillOffset + 1] == "exec skill1.cfg\n");
    assert(gToolCalls[2] == 2 && gToolCalls[3] == 2 && gToolCalls[4] == 2 && gToolCalls[5] == 2);
    assert(gToolCalls[6] == 32 && gToolCalls[7] == 32 && gToolCalls[8] == 32);

    gameShutdown(object);
    dllShutdown(object);
    assert(gGameEventRemoveListenerCalls == 1);
    assert(!gameInit(object));
    assert(getGlobalVars(pimObject) == nullptr);
    assert(!clientConnect(clientsObject, &gEngineEdicts[1], "after", "10.0.0.4", reject, sizeof(reject)));
    assert((gCvarUnlinkFlags & (1 << 2)) != 0);
    dlclose(handle);

    std::cout << "SERVERGAMECLIENTS004_LIMITS=" << minPlayers << ',' << maxPlayers << ',' << defaultMaxPlayers << '\n';
    std::cout << "SERVERGAMECLIENTS004_VTABLE=0..15\n";
    std::cout << "GAMEMOVEMENT001_VTABLE=destructor_pair,ProcessMovement,mins,maxs,viewoffset\n";
    std::cout << "SERVERGAMECLIENTS004_LIFECYCLE_V0_8_0=PASS\n";
    std::cout << "SERVERGAMECLIENTS004_PROCESSUSERCMDS_BFREAD=PASS\n";
    std::cout << "SERVERGAMECLIENTS004_EARPOSITION=PASS gameplay_origin=1 standing=64 ducked=46 invalid=zero bounds_safe=1\n";
    std::cout << "PLAYERINFO_VTABLE_0_23_V0_8_0=PASS\n";
    std::cout << "CPLAYERSTATE_VTABLE_0_4_V0_8_0=PASS\n";
    std::cout << "PLAYERINFOMANAGER001_002_V0_5_0=PASS\n";
    std::cout << "SERVERGAMEDLL006_EXACT_SLOTS_0_32=PASS save_restore_signatures=PASS\n";
    std::cout << "SERVERGAMEDLL006_GAMEINIT_DIRECT=PASS levelinit_cycles=2\n";
    std::cout << "SERVERGAMEDLL006_GAMEEVENTS=PASS slots=2,4,6,7,11 manifests=gameevents.res,ModEvents.res listeners=player_team,player_disconnect,player_connect preflight=PASS\n";
    std::cout << "SERVERGAMEDLL006_VOICESERVER001=PASS slots=2,3 active_matrix=PASS disconnect_clear=PASS\n";
    std::cout << "SERVERGAMEDLL006_ENGINE_EDICT_BASE=PASS nonnull_activate=PASS remove_edict_bridge=PASS\n";
    std::cout << "SERVERGAMEENTS001_FREECONTAINING=PASS no_removeedict_reentry=PASS ed_clear_safe=PASS\n";
    std::cout << "SERVERGAMEDLL006_FAKECLIENT_QUOTA=PASS engine_slot57=PASS postactivate_gameframe=PASS one_per_frame=PASS bots_per_level=2 networkstring_unlock=PASS\n";
    std::cout << "SERVERGAMEDLL006_C4_PREBASELINE=PASS serveractivate_createedict_attempt=PASS\n";
    std::cout << "SERVERGAMECLIENTS004_FAKECLIENT_REENTRY=PASS edict_base_index=PASS no_engine_reentry=PASS\n";
    std::cout << "SERVERCLASS_SENDTABLE_GRAPH=PASS classes=177 live_root_props=PASS standard_proxies=ENGINE_DEFAULT_NULL\n";
    std::cout << "ENGINE_CVAR_BRIDGE=PASS vars=10 commands=1 unlink_gamedll=PASS\n";
    std::cout << "SERVERGAMEDLL006_ABI_V1_3_0=PASS levels=2 frames=32 maxClients=32\n";
    return 0;
}
