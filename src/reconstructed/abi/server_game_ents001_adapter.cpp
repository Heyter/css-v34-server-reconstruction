#include "abi/server_game_ents001_adapter.h"

#include "abi/engine_entity_bridge.h"
#include "abi/engine_server021_client.h"
#include "abi/engine_probe.h"
#include "public/build4100/server_unknown.h"

#include <cstdint>
#include <cstring>
#include <cstdio>

namespace cssv34::reconstructed::abi {
namespace {

constexpr std::size_t kBaseEntityEdictOffset = 0x168;
}  // namespace

void ServerGameEnts001Adapter::Bind(EngineEntityBridge* const bridge,
                                      const EngineServer021Client* const engine) noexcept {
    bridge_ = bridge;
    engine_ = engine;
}

void ServerGameEnts001Adapter::Unbind() noexcept {
    bridge_ = nullptr;
    engine_ = nullptr;
    debugBase_ = nullptr;
    touchCalls_ = 0;
    checkTransmitCalls_ = 0;
    checkTransmitMarked_ = 0;
    checkTransmitForcedGeneric_ = 0;
    checkTransmitFreshGeneric_ = 0;
    checkTransmitProbeDone_ = false;
    checkTransmitForcedProbeDone_ = false;
}

void ServerGameEnts001Adapter::SetDebugEdictBase(build4100::Edict* base) noexcept {
    debugBase_ = base;
}

void ServerGameEnts001Adapter::MarkEntitiesAsTouching(build4100::Edict* first,
                                                       build4100::Edict* second) noexcept {
    // The original resolves both edicts through IServerNetworkable::GetBaseEntity
    // before dispatching the touch trace. Until CBaseEntity touch dispatch is
    // reconstructed, retain the observable call accounting without inventing
    // entity-side layout.
    if (first && second) ++touchCalls_;
}

void ServerGameEnts001Adapter::FreeContainingEntity(build4100::Edict* edict) noexcept {
    if (!edict) return;

    // Exact build4100 lifecycle: CVEngineServer::CreateEdict -> ED_Alloc ->
    // ED_ClearEdict calls IServerGameEnts::FreeContainingEntity before it
    // reuses an edict.  The original CServerGameEnts implementation tail-jumps
    // to game-side FreeContainingEntity(edict_t*), which destroys/detaches the
    // containing CBaseEntity but never calls IVEngineServer::RemoveEdict.
    // Re-entering RemoveEdict from this callback recurses back through ED_Free /
    // ED_ClearEdict and can hang the server during dynamic edict allocation.
    if (bridge_) {
        (void)bridge_->Detach(edict, false);
    }
}

build4100::Edict* ServerGameEnts001Adapter::BaseEntityToEdict(void* entity) noexcept {
    if (!entity) return nullptr;
    if (bridge_) {
        if (auto* bound = bridge_->EdictForBaseEntity(entity)) return bound;
    }
    // Exact build4100 CServerGameEnts::BaseEntityToEdict behavior recovered
    // from 0x0068bf60: load *(entity + 0x168).
    build4100::Edict* result = nullptr;
    std::memcpy(&result,
                static_cast<const unsigned char*>(entity) + kBaseEntityEdictOffset,
                sizeof(result));
    return result;
}

void* ServerGameEnts001Adapter::EdictToBaseEntity(build4100::Edict* edict) noexcept {
    if (!edict) return nullptr;
    if (bridge_) {
        if (void* bound = bridge_->BaseEntityForEdict(edict)) return bound;
    }
    // Exact build4100 path recovered from 0x0068bf80.  The pointer stored at
    // edict+0x0c is IServerUnknown*, whose slot 6 is GetBaseEntity().  This is
    // now expressed through the recovered interface rather than an untyped
    // vtable call.
    if (edict->unknown0c == 0) return nullptr;
    const std::uintptr_t address = static_cast<std::uintptr_t>(edict->unknown0c);
    auto* const unknown = reinterpret_cast<build4100::IServerUnknown*>(address);
    void* const result = unknown->GetBaseEntity();
    if (bridge_ && result) bridge_->ObserveBaseEntity(edict, result);
    return result;
}

void ServerGameEnts001Adapter::CheckTransmit(void* info,
                                              const unsigned short* indices,
                                              const int count) noexcept {
    ++checkTransmitCalls_;
    if (!info || count < 0 || (count > 0 && !indices)) return;

    auto* const layout = static_cast<build4100::CheckTransmitInfoLayout*>(info);
    std::size_t marked = 0;

#if UINTPTR_MAX == 0xffffffffu
    auto* const transmit = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uintptr_t>(layout->transmitEdict2008));
    auto* const always = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uintptr_t>(layout->transmitAlways200c));

    // Transmit every engine-selected candidate.  Player edicts and stock engine
    // entities reach this list through build4100's normal PVS path.
    for (int i = 0; i < count; ++i) {
        const unsigned int index = indices[i];
        if (index >= build4100::kMaxEdicts) continue;

        // build4100 may rebuild/clear the raw FL_EDICT_* transmit bits before
        // this callback. Bridge-owned generic entities therefore keep their
        // requested transmit policy separately. In particular, a pre-reserved
        // planted_c4 shell must stay absent while parked in DONT_SEND even if
        // the engine included its edict in the candidate array.
        std::uint32_t genericTransmitState = build4100::kEdictTransmitFullCheck;
        if (bridge_ && bridge_->GenericTransmitStateForEngineIndex(index, genericTransmitState) &&
            genericTransmitState == build4100::kEdictTransmitDontSend) {
            build4100::ClearTransmitBit(transmit, index);
            build4100::ClearTransmitBit(always, index);
            continue;
        }

        if (!build4100::TestTransmitBit(transmit, index)) ++marked;
        build4100::SetTransmitBit(transmit, index);
    }

    // Bridge-owned dynamic non-player entities (currently planted_c4, and later
    // reconstructed weapons/projectiles) expose a valid IServerNetworkable but do
    // not yet own the original CBaseEntity collision/PVS machinery.  The engine can
    // therefore call GetServerClass() yet omit the edict from `indices`, producing
    // an empty svc_PacketEntities snapshot.  Force those generic edicts into the
    // final transmit bitset; this is the same bitset consumed by the pack-entities
    // stage after IServerGameEnts::CheckTransmit returns.
    unsigned short forced[64]{};
    const std::size_t forcedCount = bridge_
        ? bridge_->CollectForcedTransmitIndices(forced, sizeof(forced) / sizeof(forced[0]))
        : 0;
    std::size_t forcedAdded = 0;
    std::size_t forcedPolicyAlways = 0;
    std::size_t forcedAlwaysBits = 0;
    std::size_t forcedFresh = 0;
    unsigned int firstForced = 0;
    std::uint32_t firstForcedFlags = 0;
    for (std::size_t i = 0; i < forcedCount; ++i) {
        const unsigned int index = forced[i];
        if (index == 0 || index >= build4100::kMaxEdicts) continue;
        if (firstForced == 0) firstForced = index;
        if (!build4100::TestTransmitBit(transmit, index)) {
            ++marked;
            ++forcedAdded;
        }
        build4100::SetTransmitBit(transmit, index);

        // Preserve the bridge-owned transmit policy independently from the
        // optional m_pTransmitAlways bitset.  build4100 passes nullptr for that
        // bitset on the live client path observed by this reconstruction; the
        // authoritative ALWAYS state then remains FL_EDICT_ALWAYS on the edict
        // plus the normal m_pTransmitEdict bit.  When the engine does provide
        // m_pTransmitAlways, mirror the policy into it as well.
        std::uint32_t genericTransmitState = build4100::kEdictTransmitFullCheck;
        const bool hasGenericTransmitState = bridge_ &&
            bridge_->GenericTransmitStateForEngineIndex(index, genericTransmitState);
        if (hasGenericTransmitState &&
            genericTransmitState == build4100::kEdictTransmitAlways) {
            ++forcedPolicyAlways;
            if (always) {
                build4100::SetTransmitBit(always, index);
                ++forcedAlwaysBits;
            }
        }

        // Bridge-owned generic entities do not have the original CBaseEntity
        // NetworkVar chain, so no game-side setter calls CBaseEdict::StateChanged()
        // when their SendProp backing state changes. TakeTickSnapshot has already
        // selected the edict before this callback; PackEntities_Normal runs right
        // after it and reads FL_EDICT_CHANGED from the live edict. Refresh the full
        // change contract here, at the pack boundary, so a reserved/reused generic
        // (notably planted_c4) cannot reuse the parked PackedEntity payload.
        if (engine_ && bridge_) {
            if (build4100::Edict* const edict = bridge_->BoundEdictForEngineIndex(index)) {
                const bool accessorInvalidated = engine_->MarkEdictStateChangedFull(edict);
                engine_->NotifyEdictFlagsChange(static_cast<int>(index));
                const bool fullChanged =
                    (edict->stateFlags & (build4100::kEdictChanged | build4100::kEdictFullChanged)) ==
                    (build4100::kEdictChanged | build4100::kEdictFullChanged);
                if (fullChanged && accessorInvalidated) {
                    ++forcedFresh;
                    ++checkTransmitFreshGeneric_;
                }
                if (index == firstForced) firstForcedFlags = edict->stateFlags;
            }
        }
    }
    checkTransmitForcedGeneric_ += forcedAdded;
    if (forcedCount > 0 && !checkTransmitForcedProbeDone_) {
        checkTransmitForcedProbeDone_ = true;
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "forced=%zu added=%zu policy_always=%zu always_bits=%zu always_mask=%s fresh=%zu first=%u firstFlags=0x%08x candidates=%d pvsSize=%d",
                      forcedCount, forcedAdded, forcedPolicyAlways, forcedAlwaysBits,
                      always ? "present" : "absent", forcedFresh, firstForced,
                      firstForcedFlags, count, layout->pvsSize2004);
        EngineProbe("CheckTransmitGeneric_OK", detail);
    }
#else
    // The real build is i386.  Keeping the host path pointer-free lets the exact
    // 32-bit CCheckTransmitInfo layout remain statically testable on 64-bit hosts.
    (void)layout;
#endif

    checkTransmitMarked_ += marked;
    if (!checkTransmitProbeDone_) {
        checkTransmitProbeDone_ = true;
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "count=%d marked=%zu pvsSize=%d client=0x%08x transmit=0x%08x always=0x%08x",
                      count, marked, layout->pvsSize2004, layout->clientEdict00,
                      layout->transmitEdict2008, layout->transmitAlways200c);
        EngineProbe("CheckTransmit_OK", detail);
    }
}

ServerGameEnts001Adapter& ServerGameEnts001Instance() noexcept {
    static ServerGameEnts001Adapter value;
    return value;
}

}  // namespace cssv34::reconstructed::abi
