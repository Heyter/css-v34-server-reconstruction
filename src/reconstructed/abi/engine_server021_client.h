#pragma once

#include <cstddef>
#include <cstdint>

#include "abi/vtable_client.h"
#include "public/build4100/edict.h"
#include "public/build4100/sendtable.h"
#include "public/build4100/vector.h"

namespace cssv34::reconstructed::abi {

// build4100 CBitVec<255> storage used by VEngineServer021
// Message_DetermineMulticastRecipients. The template rounds 255 bits to eight
// 32-bit words.
struct Build4100PlayerBitVec {
    std::uint32_t words[8]{};

    bool Test(const std::size_t bit) const noexcept {
        return bit < 255u && (words[bit >> 5u] & (std::uint32_t{1} << (bit & 31u))) != 0;
    }
};
static_assert(sizeof(Build4100PlayerBitVec) == 32, "build4100 CBitVec<255> storage");

// Thin client-side view of the engine-owned VEngineServer021 object passed to
// server_i486.so through the engine CreateInterface factory.
//
// This class does not own the object and does not fabricate a vtable. It only
// dispatches through build4100 slots that are already proven by the ABI oracle.
class EngineServer021Client final {
public:
    EngineServer021Client() noexcept = default;
    explicit EngineServer021Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept;

    bool IsDedicatedServer() const noexcept;                 // slot 2
    bool IsInEditMode() const noexcept;                      // slot 3
    int GetPlayerUserId(const build4100::Edict* edict) const noexcept;       // slot 15
    const char* GetPlayerNetworkIdString(const build4100::Edict* edict) const noexcept; // slot 16
    int IndexOfEdict(const build4100::Edict* edict) const noexcept; // slot 18
    build4100::Edict* PEntityOfEntIndex(int index) const noexcept;  // slot 19
    build4100::Edict* CreateEdict(int forcedIndex) const noexcept;  // slot 21
    void RemoveEdict(build4100::Edict* edict) const noexcept;       // slot 22
    void* PvAllocEntPrivateData(std::int32_t bytes) const noexcept;  // slot 23
    void FreeEntPrivateData(void* pointer) const noexcept;           // slot 24
    void ServerCommand(const char* command) const noexcept;          // slot 36
    void ServerExecute() const noexcept;                             // slot 37
    void MessageDetermineMulticastRecipients(bool usePas, const build4100::Vector& origin,
                                              Build4100PlayerBitVec& playerBits) const noexcept; // slot 41
    void* UserMessageBegin(void* recipientFilter, int messageType) const noexcept; // slot 43
    void MessageEnd() const noexcept;                                // slot 44
    void* ResolveEngineSymbol(const char* symbol) const noexcept;
    const char* CmdArgs() const noexcept;                            // slot 48
    int CmdArgc() const noexcept;                                    // slot 49
    const char* CmdArgv(int index) const noexcept;                   // slot 50
    bool LockNetworkStringTables(bool lock) const noexcept;          // slot 56
    build4100::Edict* CreateFakeClient(const char* netname) const noexcept; // slot 57
    const char* GetClientConVarValue(int clientIndex, const char* name) const noexcept; // slot 58
    const char* ParseFile(const char* data, char* token, int maxLen) const noexcept; // slot 59
    void PlaybackTempEntity(void* recipientFilter, float delay, const void* sender,
                            const build4100::SendTable* sendTable, int classId) const noexcept; // slot 64
    const char* GetMapEntitiesString() const noexcept;               // slot 74
    void LogPrint(const char* text) const noexcept;                  // slot 76
    void NotifyEdictFlagsChange(int index) const noexcept;           // slot 93
    void* GetChangeAccessor(build4100::Edict* edict) const noexcept; // slot 98
    bool MarkEdictStateChangedFull(build4100::Edict* edict) const noexcept;

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
