#include "abi/engine_server021_client.h"

#include <cstring>
#include <dlfcn.h>

namespace cssv34::reconstructed::abi {

bool EngineServer021Client::IsValid() const noexcept {
    if (!object_) {
        return false;
    }
    return detail::HasVTable(object_);
}

bool EngineServer021Client::IsDedicatedServer() const noexcept {
    using Fn = bool (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 2);
    return fn ? fn(object_) : false;
}

bool EngineServer021Client::IsInEditMode() const noexcept {
    using Fn = bool (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 3);
    return fn ? fn(object_) : false;
}

int EngineServer021Client::GetPlayerUserId(const build4100::Edict* const edict) const noexcept {
    using Fn = int (*)(void*, const build4100::Edict*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 15);
    return fn ? fn(object_, edict) : -1;
}

const char* EngineServer021Client::GetPlayerNetworkIdString(
    const build4100::Edict* const edict
) const noexcept {
    using Fn = const char* (*)(void*, const build4100::Edict*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 16);
    return fn ? fn(object_, edict) : nullptr;
}

int EngineServer021Client::IndexOfEdict(const build4100::Edict* const edict) const noexcept {
    using Fn = int (*)(void*, const build4100::Edict*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 18);
    return fn ? fn(object_, edict) : -1;
}

build4100::Edict* EngineServer021Client::PEntityOfEntIndex(const int index) const noexcept {
    using Fn = build4100::Edict* (*)(void*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 19);
    return fn ? fn(object_, index) : nullptr;
}

build4100::Edict* EngineServer021Client::CreateEdict(const int forcedIndex) const noexcept {
    using Fn = build4100::Edict* (*)(void*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 21);
    return fn ? fn(object_, forcedIndex) : nullptr;
}

void EngineServer021Client::RemoveEdict(build4100::Edict* const edict) const noexcept {
    using Fn = void (*)(void*, build4100::Edict*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 22);
    if (fn) {
        fn(object_, edict);
    }
}

void* EngineServer021Client::PvAllocEntPrivateData(const std::int32_t bytes) const noexcept {
    using Fn = void* (*)(void*, std::int32_t);
    const Fn fn = detail::VTableSlot<Fn>(object_, 23);
    return fn ? fn(object_, bytes) : nullptr;
}

void EngineServer021Client::FreeEntPrivateData(void* const pointer) const noexcept {
    using Fn = void (*)(void*, void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 24);
    if (fn) {
        fn(object_, pointer);
    }
}

void EngineServer021Client::ServerCommand(const char* const command) const noexcept {
    using Fn = void (*)(void*, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 36);
    if (fn) {
        fn(object_, command);
    }
}

void EngineServer021Client::ServerExecute() const noexcept {
    using Fn = void (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 37);
    if (fn) {
        fn(object_);
    }
}

void EngineServer021Client::MessageDetermineMulticastRecipients(
    const bool usePas, const build4100::Vector& origin,
    Build4100PlayerBitVec& playerBits) const noexcept {
    // CRecipientFilter::AddRecipientsByPAS @ server_i486.so 0x79aa20 zeroes a
    // CBitVec<255>, then dispatches VEngineServer021 +0xa4 (slot41) with
    // usepas=1, &origin and the bit-vector.
    using Fn = void (*)(void*, bool, const build4100::Vector*, Build4100PlayerBitVec*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 41);
    if (fn) {
        fn(object_, usePas, &origin, &playerBits);
    }
}

void* EngineServer021Client::UserMessageBegin(void* const recipientFilter,
                                                  const int messageType) const noexcept {
    using Fn = void* (*)(void*, void*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 43);
    return fn ? fn(object_, recipientFilter, messageType) : nullptr;
}

void EngineServer021Client::MessageEnd() const noexcept {
    using Fn = void (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 44);
    if (fn) {
        fn(object_);
    }
}

void* EngineServer021Client::ResolveEngineSymbol(const char* const symbol) const noexcept {
    if (!object_ || !symbol || !symbol[0]) return nullptr;

    // engine_i486.so is loaded RTLD_LOCAL by srcds, so RTLD_DEFAULT cannot
    // see its exported bf_write helpers. Recover the owning DSO from a proven
    // VEngineServer021 slot and resolve against that exact module instead.
    using SlotFn = void (*)(void*);
    const SlotFn slot = detail::VTableSlot<SlotFn>(object_, 43);
    if (!slot) return nullptr;

    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(slot), &info) == 0 || !info.dli_fname) return nullptr;

    void* handle = dlopen(info.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) {
        // Same pathname should already refer to the loaded engine DSO; this
        // fallback also works on older glibc where RTLD_NOLOAD matching is
        // stricter about the loader's original pathname spelling.
        handle = dlopen(info.dli_fname, RTLD_LAZY);
    }
    return handle ? dlsym(handle, symbol) : nullptr;
}

const char* EngineServer021Client::CmdArgs() const noexcept {
    using Fn = const char* (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 48);
    return fn ? fn(object_) : nullptr;
}

int EngineServer021Client::CmdArgc() const noexcept {
    using Fn = int (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 49);
    return fn ? fn(object_) : 0;
}

const char* EngineServer021Client::CmdArgv(const int index) const noexcept {
    using Fn = const char* (*)(void*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 50);
    return fn ? fn(object_, index) : nullptr;
}

bool EngineServer021Client::LockNetworkStringTables(const bool lock) const noexcept {
    using Fn = bool (*)(void*, bool);
    const Fn fn = detail::VTableSlot<Fn>(object_, 56);
    return fn ? fn(object_, lock) : lock;
}

build4100::Edict* EngineServer021Client::CreateFakeClient(const char* const netname) const noexcept {
    using Fn = build4100::Edict* (*)(void*, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 57);
    return fn ? fn(object_, netname) : nullptr;
}

const char* EngineServer021Client::GetClientConVarValue(const int clientIndex,
                                                         const char* const name) const noexcept {
    using Fn = const char* (*)(void*, int, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 58);
    return fn ? fn(object_, clientIndex, name) : nullptr;
}

const char* EngineServer021Client::ParseFile(
    const char* const data,
    char* const token,
    const int maxLen
) const noexcept {
    using Fn = const char* (*)(void*, const char*, char*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 59);
    return fn ? fn(object_, data, token, maxLen) : nullptr;
}

void EngineServer021Client::PlaybackTempEntity(
    void* const recipientFilter, const float delay, const void* const sender,
    const build4100::SendTable* const sendTable, const int classId) const noexcept {
    // CBaseTempEntity::Create @ server_i486.so 0x830cb0 dispatches
    // VEngineServer021 slot64 with this exact five-argument contract.
    using Fn = void (*)(void*, void*, float, const void*, const build4100::SendTable*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 64);
    if (fn && recipientFilter && sender && sendTable && classId >= 0) {
        fn(object_, recipientFilter, delay, sender, sendTable, classId);
    }
}

const char* EngineServer021Client::GetMapEntitiesString() const noexcept {
    using Fn = const char* (*)(void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 74);
    return fn ? fn(object_) : nullptr;
}

void EngineServer021Client::LogPrint(const char* const text) const noexcept {
    using Fn = void (*)(void*, const char*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 76);
    if (fn) {
        fn(object_, text);
    }
}

void EngineServer021Client::NotifyEdictFlagsChange(const int index) const noexcept {
    using Fn = void (*)(void*, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 93);
    if (fn) {
        fn(object_, index);
    }
}

void* EngineServer021Client::GetChangeAccessor(build4100::Edict* const edict) const noexcept {
    using Fn = void* (*)(void*, build4100::Edict*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 98);
    return fn ? fn(object_, edict) : nullptr;
}

bool EngineServer021Client::MarkEdictStateChangedFull(
    build4100::Edict* const edict) const noexcept {
    if (!edict) return false;

    // Exact CBaseEdict::StateChanged() contract from build4100-era SDK:
    // mark both CHANGED and FULL_EDICT_CHANGED, then invalidate the
    // per-edict change-info serial through VEngineServer021 slot98.
    // Merely setting FL_EDICT_CHANGED is insufficient once a PackedEntity
    // baseline already exists: an empty/old partial change list can make the
    // engine emit svc_PacketEntities with updated=0.
    edict->stateFlags |= build4100::kEdictChanged | build4100::kEdictFullChanged;
    auto* const accessor = static_cast<build4100::ChangeInfoAccessor*>(GetChangeAccessor(edict));
    if (!accessor) return false;
    accessor->changeInfoSerialNumber = 0;
    return true;
}

}  // namespace cssv34::reconstructed::abi
