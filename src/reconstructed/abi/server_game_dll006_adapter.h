#pragma once

#include <array>
#include <cstddef>

#include "abi/engine_server021_client.h"
#include "abi/engine_trace_server003_client.h"
#include "abi/engine_cvar_bridge.h"
#include "abi/engine_entity_bridge.h"
#include "abi/game_event_manager002_client.h"
#include "abi/network_string_table_client.h"
#include "abi/server_engine_tools001_client.h"
#include "abi/voice_server001_client.h"
#include "engine/engine_server.h"
#include "public/build4100/edict.h"
#include "public/build4100/eiface_types.h"
#include "public/build4100/globalvars.h"
#include "server/server_game_dll.h"

namespace cssv34::reconstructed::abi {

using CreateInterfaceFn = void* (*)(const char* name, int* returnCode);

// Exact build4100 IServerGameDLL address-point order recovered from
// CServerGameDLL's original i386 vtable. There is deliberately no virtual
// destructor: slot 0 is DLLInit and the interface has exactly 33 methods.
class ServerGameDll006Adapter final {
public:
    ServerGameDll006Adapter();

    virtual bool DLLInit(CreateInterfaceFn engineFactory, CreateInterfaceFn physicsFactory,
                         CreateInterfaceFn fileSystemFactory, void* globals); // 0
    virtual bool GameInit();                                                  // 1
    virtual bool LevelInit(const char* mapName, const char* mapEntities,
                           const char* oldLevel, const char* landmarkName,
                           bool loadGame, bool background);                   // 2
    virtual void ServerActivate(build4100::Edict* edictList, int edictCount, int clientMax); // 3
    virtual void GameFrame(bool simulating);                                  // 4
    virtual void PreClientUpdate(bool simulating);                            // 5
    virtual void LevelShutdown();                                             // 6
    virtual void GameShutdown();                                              // 7
    virtual void DLLShutdown();                                               // 8
    virtual float GetTickInterval() const;                                    // 9
    virtual ServerClass* GetAllServerClasses();                               // 10
    virtual const char* GetGameDescription();                                 // 11
    virtual void CreateNetworkStringTables();                                 // 12
    virtual CSaveRestoreData* SaveInit(int size);                             // 13
    virtual void SaveWriteFields(CSaveRestoreData* save, const char* name, void* baseData,
                                 datamap_t* map, typedescription_t* fields, int fieldCount); // 14
    virtual void SaveReadFields(CSaveRestoreData* save, const char* name, void* baseData,
                                datamap_t* map, typedescription_t* fields, int fieldCount);  // 15
    virtual void SaveGlobalState(CSaveRestoreData* save);                     // 16
    virtual void RestoreGlobalState(CSaveRestoreData* save);                  // 17
    virtual void PreSave(CSaveRestoreData* save);                             // 18
    virtual void Save(CSaveRestoreData* save);                                // 19
    virtual void GetSaveComment(char* buffer, int maxLen);                    // 20
    virtual void WriteSaveHeaders(CSaveRestoreData* save);                    // 21
    virtual void ReadRestoreHeaders(CSaveRestoreData* save);                  // 22
    virtual void Restore(CSaveRestoreData* save, bool createPlayers);         // 23
    virtual bool IsRestoring();                                               // 24
    virtual int CreateEntityTransitionList(CSaveRestoreData* save, int maxList); // 25
    virtual void BuildAdjacentMapList();                                      // 26
    virtual bool GetUserMessageInfo(int msgType, char* name, int maxNameLength,
                                    int& size);                               // 27
    virtual CStandardSendProxiesV1* GetStandardSendProxies();                 // 28
    virtual void PostInit();                                                  // 29
    virtual void Think(bool finalTick);                                       // 30
    virtual void GetSaveCommentEx(char* buffer, int maxLen, float minutes, float seconds); // 31
    virtual void OnQueryCvarValueFinished(int cookie, build4100::Edict* player, int status,
                                          const char* cvarName, const char* cvarValue); // 32

    build4100::Edict* EngineEdictBase() const noexcept { return engineEdictBase_; }
    int EngineEdictCount() const noexcept { return engineEdictCount_; }

private:
    bool SynchronizeClientSlots(const char* phase);
    void QueueFakeClientsFromQuota();
    void SeedQueuedFakeClients();
    void TryDumpSendPropSchema();
    void TryDumpInstanceBaselines();

    EngineServer engine_;
    ServerGameDll game_;
    EngineServer021Client engineAbi_;
    EngineTraceServer003Client engineTraceAbi_;
    EngineCvarBridge cvarBridge_;
    EngineEntityBridge entityBridge_;
    GameEventManager002Client gameEventsAbi_;
    VoiceServer001Client voiceServerAbi_;
    NetworkStringTableContainer001Client stringTablesAbi_;
    ServerEngineTools001Client serverToolsAbi_;
    build4100::GlobalVarsBase* globals_ = nullptr;
    build4100::Edict* engineEdictBase_ = nullptr;
    int engineEdictCount_ = 0;
    std::array<void*, 7> networkStringTables_{};
    bool networkStringTablesReady_ = false;
    int fakeClientTarget_ = 0;
    int fakeClientsCreated_ = 0;
    int fakeClientSeedAttempts_ = 0;
    bool firstGameFrameSeen_ = false;
    bool coreEventListenerRegistered_ = false;
    bool sendPropSchemaDumped_ = false;
    int sendPropSchemaLastResult_ = 0;
    bool instanceBaselineDumped_ = false;
    int instanceBaselineLastResult_ = -9999;
};

}  // namespace cssv34::reconstructed::abi

extern "C" __attribute__((visibility("default")))
void* CreateInterface(const char* name, int* returnCode);
