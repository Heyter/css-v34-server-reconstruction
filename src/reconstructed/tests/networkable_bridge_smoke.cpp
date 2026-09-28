#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "abi/engine_entity_bridge.h"
#include "abi/engine_server021_client.h"
#include "abi/server_class_registry.h"
#include "abi/server_game_ents001_adapter.h"
#include "public/build4100/server_unknown.h"

using namespace cssv34;
using namespace cssv34::reconstructed;
using namespace cssv34::reconstructed::abi;

namespace {
struct ReentrantEngineObject {
    void** vtable = nullptr;
};

ReentrantEngineObject gReentrantEngine;
void* gReentrantEngineVtable[23] = {};
int gReentrantRemoveCalls = 0;


struct FreshEngineObject {
    void** vtable = nullptr;
};

FreshEngineObject gFreshEngine;
void* gFreshEngineVtable[99] = {};
build4100::ChangeInfoAccessor gFreshAccessor{};
int gFreshNotifyCalls = 0;
int gFreshNotifyIndex = -1;
int gFreshAccessorCalls = 0;

void FreshEngineNotifyEdictFlagsChange(void* self, int index) {
    assert(self == &gFreshEngine);
    ++gFreshNotifyCalls;
    gFreshNotifyIndex = index;
}

void* FreshEngineGetChangeAccessor(void* self, build4100::Edict* edict) {
    assert(self == &gFreshEngine);
    assert(edict != nullptr);
    ++gFreshAccessorCalls;
    return &gFreshAccessor;
}

void ReentrantEngineRemoveEdict(void* self, build4100::Edict* edict) {
    assert(self == &gReentrantEngine);
    assert(edict);
    ++gReentrantRemoveCalls;

    // Mirror build4100 ED_Free: the engine calls back into
    // IServerGameEnts::FreeContainingEntity before it finalizes the edict.
    ServerGameEnts001Instance().FreeContainingEntity(edict);
    edict->stateFlags |= build4100::kEdictFree;
}
}  // namespace

template <class Fn> static Fn Slot(void** table, std::size_t index) {
    Fn fn = nullptr; std::memcpy(&fn, &table[index], sizeof(fn)); return fn;
}

int main() {
    build4100::Edict edict{}; edict.stateFlags = 0x40u; edict.networkSerial = 7;
    edict.networkable08 = 0x55667788u;
    edict.unknown0c = 0x11223344u;
    EngineEntityBridge bridge; bridge.Bind(nullptr, 64);
    assert(bridge.AttachClient(1, &edict));
    auto* networkable = bridge.NetworkableForEdict(&edict);
    auto* unknown = bridge.UnknownForEdict(&edict);
    assert(networkable && unknown);

    assert(edict.stateFlags == build4100::kEdictFull);
#if UINTPTR_MAX == 0xffffffffu
    assert(edict.networkable08 == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(networkable)));
    assert(edict.unknown0c == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(unknown)));
#else
    assert(edict.networkable08 == 0x55667788u);
    assert(edict.unknown0c == 0x11223344u);
#endif

    void** vt = *reinterpret_cast<void***>(networkable);
    assert(vt);
    using HandleFn = build4100::IHandleEntity* (*)(void*);
    using ServerClassFn = build4100::ServerClass* (*)(void*);
    using IntFn = int (*)(void*);
    using AddFn = void (*)(void*, int);
    using EdictFn = build4100::Edict* (*)(void*);
    using StringFn = const char* (*)(void*);
    using VoidFn = void (*)(void*);
    using PtrFn = void* (*)(void*);
    using PvsFn = build4100::PVSInfo* (*)(void*);
    auto* entityHandle = Slot<HandleFn>(vt,0)(networkable);
    assert(entityHandle == unknown);
    auto* sc = Slot<ServerClassFn>(vt,1)(networkable);
    assert(sc && std::strcmp(sc->networkName, "CCSPlayer") == 0);
    assert(Slot<IntFn>(vt,2)(networkable) == 0);
    Slot<AddFn>(vt,3)(networkable, 0x20);
    assert(Slot<IntFn>(vt,2)(networkable) == 0x20);
    assert(Slot<EdictFn>(vt,4)(networkable) == &edict);
    assert(std::strcmp(Slot<StringFn>(vt,5)(networkable), "player") == 0);
    assert(Slot<IntFn>(vt,7)(networkable) == 0);
    assert(Slot<PtrFn>(vt,8)(networkable) == nullptr);
    void* base = Slot<PtrFn>(vt,9)(networkable); assert(base == unknown);
    auto* pvsInfo = Slot<PvsFn>(vt,10)(networkable); assert(pvsInfo != nullptr);
    assert(sizeof(build4100::PVSInfo) == 0x7c);
    assert(reinterpret_cast<std::uintptr_t>(&pvsInfo->clusters) - reinterpret_cast<std::uintptr_t>(pvsInfo) == 0x04);
    assert(reinterpret_cast<std::uintptr_t>(&pvsInfo->headNode) - reinterpret_cast<std::uintptr_t>(pvsInfo) == 0x64);
    assert(reinterpret_cast<std::uintptr_t>(&pvsInfo->center) - reinterpret_cast<std::uintptr_t>(pvsInfo) == 0x70);

    // Exact build4100 IServerUnknown chain:
    // 0/1 destructors, 2 SetRefEHandle, 3 GetRefEHandle,
    // 4 GetCollideable, 5 GetNetworkable, 6 GetBaseEntity.
    void** uvt = *reinterpret_cast<void***>(unknown);
    assert(uvt);
    using SetRefFn = void (*)(void*, const build4100::BaseHandle&);
    using GetRefFn = const build4100::BaseHandle& (*)(void*);
    // GetEntityHandle must return the IHandleEntity object, not the packed
    // CBaseHandle value.  The engine dereferences this virtual interface during
    // post-ClientActive entity/snapshot processing.
    assert(entityHandle == static_cast<build4100::IHandleEntity*>(unknown));
    build4100::BaseHandle replacement{0x00abc123u};
    Slot<SetRefFn>(uvt, 2)(unknown, replacement);
    assert(Slot<GetRefFn>(uvt, 3)(unknown).value == replacement.value);
    assert(Slot<PtrFn>(uvt, 4)(unknown) == nullptr);
    assert(Slot<PtrFn>(uvt, 5)(unknown) == networkable);
    assert(Slot<PtrFn>(uvt, 6)(unknown) == base);

    assert(bridge.BaseEntityForEdict(&edict) == base);
    assert(bridge.EdictForBaseEntity(base) == &edict);

    auto& classes = ServerClasses();
    assert(classes.OriginalMetadataTableCount() == 198);
    const auto* planted = classes.FindOriginalMetadataByTableName("DT_PlantedC4");
    assert(planted && planted->propCount == 6);
    assert(std::strcmp(planted->props[0].name, "baseclass") == 0);
    assert(std::strcmp(planted->props[1].name, "m_bBombTicking") == 0);
    assert(planted->props[1].absoluteOffset == 0x47c);
    assert(std::strcmp(planted->props[5].name, "m_flDefuseCountDown") == 0);
    assert(planted->props[5].absoluteOffset == 0x49c);
    const auto* playerMeta = classes.FindOriginalMetadataByTableName("DT_CSPlayer");
    assert(playerMeta && playerMeta->propCount == 38);
    const auto* baseEntityMeta = classes.FindOriginalMetadataByTableName("DT_BaseEntity");
    assert(baseEntityMeta && baseEntityMeta->propCount == 25);

    auto& ents = ServerGameEnts001Instance();
    ents.Bind(&bridge, nullptr);
    assert(ents.EdictToBaseEntity(&edict) == base);
    assert(ents.BaseEntityToEdict(base) == &edict);

    static_assert(sizeof(build4100::CheckTransmitInfoLayout) == 0x2010);
    static_assert(offsetof(build4100::CheckTransmitInfoLayout, pvsSize2004) == 0x2004);
    static_assert(offsetof(build4100::CheckTransmitInfoLayout, transmitEdict2008) == 0x2008);
    static_assert(offsetof(build4100::CheckTransmitInfoLayout, transmitAlways200c) == 0x200c);
#if UINTPTR_MAX == 0xffffffffu
    build4100::CheckTransmitInfoLayout transmitInfo{};
    std::uint32_t transmitWords[build4100::kTransmitWords]{};
    std::uint32_t alwaysWords[build4100::kTransmitWords]{};
    transmitInfo.clientEdict00 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&edict));
    transmitInfo.pvsSize2004 = 250;
    transmitInfo.transmitEdict2008 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(transmitWords));
    transmitInfo.transmitAlways200c = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(alwaysWords));
    const unsigned short candidates[] = {1, 2, 31, 32, 2047, 4095};
    ents.CheckTransmit(&transmitInfo, candidates, static_cast<int>(sizeof(candidates) / sizeof(candidates[0])));
    assert(ents.CheckTransmitCalls() == 1);
    assert(ents.CheckTransmitMarked() == 5);
    assert(build4100::TestTransmitBit(transmitWords, 1));
    assert(build4100::TestTransmitBit(transmitWords, 2));
    assert(build4100::TestTransmitBit(transmitWords, 31));
    assert(build4100::TestTransmitBit(transmitWords, 32));
    assert(build4100::TestTransmitBit(transmitWords, 2047));
    assert(!build4100::TestTransmitBit(alwaysWords, 1));
#endif
    ents.Unbind();

    // Dynamic generic entities are not guaranteed to enter build4100's PVS
    // candidate array because the reconstruction intentionally does not fake a
    // CCollisionProperty/ShouldTransmit implementation.  They must still be
    // discoverable by exact edict index so CheckTransmit can force them into
    // the final transmit bitset.  Client bindings must not be duplicated here.
    std::array<build4100::Edict, 16> forcedEdicts{};
    for (std::size_t i = 0; i < forcedEdicts.size(); ++i) {
        forcedEdicts[i].stateFlags = 0x40u;
        forcedEdicts[i].networkSerial = static_cast<int>(20 + i);
    }
    EngineEntityBridge forcedBridge;
    forcedBridge.Bind(nullptr, 64);
    // Only slots [0,2] existed when ServerActivate ran.  Slots 3 and 5 model
    // CreateFakeClient/CreateEdict allocations made later in the same fixed pool.
    forcedBridge.ServerActivate(forcedEdicts.data(), 3);
    assert(forcedBridge.EngineEdictCount() == 3);
    assert(forcedBridge.EngineIndexForEdict(&forcedEdicts[3]) == 3);
    assert(forcedBridge.EngineIndexForEdict(&forcedEdicts[5]) == 5);
    assert(forcedBridge.AttachClient(3, &forcedEdicts[3]));
    const EntityHandle forcedC4 = forcedBridge.AttachGeneric(
        &forcedEdicts[5], "planted_c4", build4100::kEdictTransmitDontSend);
    assert(forcedC4.IsValid());
    assert((forcedEdicts[5].stateFlags & build4100::kEdictFull) != 0);
    assert(build4100::HasEdictTransmitState(forcedEdicts[5], build4100::kEdictTransmitDontSend));
    assert(build4100::WithEdictTransmitState(0xffffffffu, build4100::kEdictTransmitFullCheck) ==
           (0xffffffffu & ~build4100::kEdictTransmitMask));
    unsigned short forcedIndices[8]{};
    std::size_t forcedCount = forcedBridge.CollectForcedTransmitIndices(
        forcedIndices, sizeof(forcedIndices) / sizeof(forcedIndices[0]));
    assert(forcedCount == 0);
    assert(forcedBridge.SetGenericTransmitState(
        &forcedEdicts[5], build4100::kEdictTransmitAlways));
    // Simulate build4100 clearing/rebuilding the raw FL_EDICT_* transmit field
    // before CheckTransmit. The bridge policy must remain ALWAYS independently.
    forcedEdicts[5].stateFlags = build4100::WithEdictTransmitState(
        forcedEdicts[5].stateFlags, build4100::kEdictTransmitFullCheck);
    std::uint32_t desiredTransmit = 0xffffffffu;
    assert(forcedBridge.GenericTransmitStateForEngineIndex(5, desiredTransmit));
    assert(desiredTransmit == build4100::kEdictTransmitAlways);
    forcedCount = forcedBridge.CollectForcedTransmitIndices(
        forcedIndices, sizeof(forcedIndices) / sizeof(forcedIndices[0]));
    assert(forcedCount == 1);
    assert(forcedIndices[0] == 5);
    assert(forcedBridge.BoundEdictForEngineIndex(5) == &forcedEdicts[5]);

#if UINTPTR_MAX == 0xffffffffu
    gFreshEngineVtable[93] = reinterpret_cast<void*>(&FreshEngineNotifyEdictFlagsChange);
    gFreshEngineVtable[98] = reinterpret_cast<void*>(&FreshEngineGetChangeAccessor);
    gFreshEngine.vtable = gFreshEngineVtable;
    EngineServer021Client freshEngine(&gFreshEngine);
    gFreshNotifyCalls = 0;
    gFreshNotifyIndex = -1;
    gFreshAccessorCalls = 0;
    gFreshAccessor.changeInfo = 9;
    gFreshAccessor.changeInfoSerialNumber = 77;
    forcedEdicts[5].stateFlags &= ~(build4100::kEdictChanged | build4100::kEdictFullChanged);

    ents.Bind(&forcedBridge, &freshEngine);
    build4100::CheckTransmitInfoLayout forcedTransmitInfo{};
    std::uint32_t forcedTransmitWords[build4100::kTransmitWords]{};
    std::uint32_t forcedAlwaysWords[build4100::kTransmitWords]{};
    forcedTransmitInfo.clientEdict00 = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&forcedEdicts[3]));
    forcedTransmitInfo.pvsSize2004 = 250;
    forcedTransmitInfo.transmitEdict2008 = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(forcedTransmitWords));
    forcedTransmitInfo.transmitAlways200c = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(forcedAlwaysWords));
    const unsigned short forcedCandidates[] = {3};
    ents.CheckTransmit(&forcedTransmitInfo, forcedCandidates, 1);
    assert(build4100::TestTransmitBit(forcedTransmitWords, 3));
    assert(build4100::TestTransmitBit(forcedTransmitWords, 5));
    assert(ents.CheckTransmitForcedGeneric() == 1);
    assert(build4100::TestTransmitBit(forcedAlwaysWords, 5));
    assert(ents.CheckTransmitFreshGeneric() == 1);
    assert((forcedEdicts[5].stateFlags & build4100::kEdictChanged) != 0);
    assert((forcedEdicts[5].stateFlags & build4100::kEdictFullChanged) != 0);
    assert(gFreshAccessor.changeInfoSerialNumber == 0);
    assert(gFreshAccessorCalls == 1);
    assert(gFreshNotifyCalls == 1);
    assert(gFreshNotifyIndex == 5);

    // Live build4100 can pass a null m_pTransmitAlways while still expecting
    // FL_EDICT_ALWAYS entities to remain in m_pTransmitEdict. The bridge policy
    // must therefore not depend on the optional secondary bitset being present.
    build4100::CheckTransmitInfoLayout nullAlwaysTransmitInfo = forcedTransmitInfo;
    std::uint32_t nullAlwaysTransmitWords[build4100::kTransmitWords]{};
    nullAlwaysTransmitInfo.transmitEdict2008 = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(nullAlwaysTransmitWords));
    nullAlwaysTransmitInfo.transmitAlways200c = 0;
    gFreshAccessor.changeInfoSerialNumber = 91;
    ents.CheckTransmit(&nullAlwaysTransmitInfo, forcedCandidates, 1);
    assert(build4100::TestTransmitBit(nullAlwaysTransmitWords, 3));
    assert(build4100::TestTransmitBit(nullAlwaysTransmitWords, 5));
    assert(ents.CheckTransmitForcedGeneric() == 2);
    assert(ents.CheckTransmitFreshGeneric() == 2);
    assert(gFreshAccessor.changeInfoSerialNumber == 0);
    assert(gFreshAccessorCalls == 2);
    assert(gFreshNotifyCalls == 2);
    assert(gFreshNotifyIndex == 5);

    assert(forcedBridge.SetGenericTransmitState(
        &forcedEdicts[5], build4100::kEdictTransmitDontSend));
    // Simulate the engine presenting the parked generic as a normal candidate.
    // CheckTransmit must actively clear both masks from our persistent policy.
    forcedEdicts[5].stateFlags = build4100::WithEdictTransmitState(
        forcedEdicts[5].stateFlags, build4100::kEdictTransmitFullCheck);
    const unsigned short parkedCandidates[] = {3, 5};
    build4100::SetTransmitBit(forcedTransmitWords, 5);
    build4100::SetTransmitBit(forcedAlwaysWords, 5);
    const int refreshCallsBeforeParked = gFreshAccessorCalls;
    ents.CheckTransmit(&forcedTransmitInfo, parkedCandidates, 2);
    assert(!build4100::TestTransmitBit(forcedTransmitWords, 5));
    assert(!build4100::TestTransmitBit(forcedAlwaysWords, 5));
    assert(gFreshAccessorCalls == refreshCallsBeforeParked);
    ents.Unbind();
#endif

    assert(forcedBridge.Detach(&forcedEdicts[5], false));
    assert(forcedBridge.Detach(&forcedEdicts[3], false));

    Slot<VoidFn>(vt,6)(networkable);
    auto* shim = dynamic_cast<EngineNetworkableShim*>(networkable);
    assert(shim && shim->Released());

    assert(bridge.Detach(&edict, false));
    assert(edict.stateFlags == 0x40u);
    assert(edict.networkable08 == 0x55667788u);
    assert(edict.unknown0c == 0x11223344u);

    // Regression for the live planted_c4 removal path.  VEngineServer021::
    // RemoveEdict re-enters the DLL through FreeContainingEntity, so the outer
    // EngineEntityBridge::Detach must tolerate the nested removal and must not
    // erase a stale std::vector iterator after the callback returns.
    gReentrantEngineVtable[22] = reinterpret_cast<void*>(&ReentrantEngineRemoveEdict);
    gReentrantEngine.vtable = gReentrantEngineVtable;
    EngineServer021Client reentrantEngine(&gReentrantEngine);
    EngineEntityBridge reentrantBridge;
    reentrantBridge.Bind(&reentrantEngine, 64);

    build4100::Edict dynamicEdict{};
    dynamicEdict.stateFlags = 0x40u;
    dynamicEdict.networkSerial = 11;
    dynamicEdict.networkable08 = 0x13572468u;
    dynamicEdict.unknown0c = 0x24681357u;
    const EntityHandle dynamicHandle = reentrantBridge.AttachGeneric(
        &dynamicEdict, "planted_c4", build4100::kEdictTransmitAlways);
    assert(dynamicHandle.IsValid());
    assert(reentrantBridge.BindingForEdict(&dynamicEdict) != nullptr);

    ents.Bind(&reentrantBridge, &reentrantEngine);
    gReentrantRemoveCalls = 0;
    assert(reentrantBridge.Detach(&dynamicEdict, true));
    assert(gReentrantRemoveCalls == 1);
    assert(reentrantBridge.BindingForEdict(&dynamicEdict) == nullptr);
    assert(build4100::IsFree(dynamicEdict));
    ents.Unbind();

    std::cout << "NETWORKABLE_BRIDGE=PASS networkable_slots_0_12 entity_handle_ptr unknown_slots_0_6 base_networkable_null pvsinfo_0x7c base_edict_0x168 edict_networkable_0x08 edict_unknown_0x0c edict_full_0x04 restore_all checktransmit_layout_0x2010 transmit_candidates sendprop_metadata_198 removeedict_reentry_safe=1 forced_generic_transmit=1 planted_c4_always=1 dynamic_postactivate_edict=1 planted_c4_reserved_dontsend=1 persistent_generic_transmit_policy=1 full_edict_changed_contract=1 generic_pack_boundary_refresh=1 nullable_transmit_always=1\n";
}
