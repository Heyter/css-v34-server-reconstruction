#include "abi/engine_entity_bridge.h"

#include "public/build4100/check_transmit.h"
#include "abi/server_class_registry.h"
#include "abi/engine_probe.h"
#include "server/entity_factory_registry.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdio>

namespace cssv34::reconstructed::abi {

void EngineEntityBridge::Bind(const EngineServer021Client* const engine, const std::size_t maxEntities) {
    engine_ = engine;
    world_ = EntityWorld(maxEntities);
    bindings_.clear();
    edictBase_ = nullptr;
    edictCount_ = 0;
}

void EngineEntityBridge::Unbind() noexcept {
    for (auto& binding : bindings_) {
        if (binding.edict) {
            binding.edict->stateFlags = binding.previousStateFlags;
            binding.edict->networkable08 = binding.previousNetworkable08;
            binding.edict->unknown0c = binding.previousUnknown0c;
        }
        if (binding.networkable) binding.networkable->Release();
    }
    world_.Reset();
    bindings_.clear();
    engine_ = nullptr;
    edictBase_ = nullptr;
    edictCount_ = 0;
}

void EngineEntityBridge::ServerActivate(build4100::Edict* const base, const int count) noexcept {
    edictBase_ = base;
    edictCount_ = count > 0 ? count : 0;
}

int EngineEntityBridge::EngineIndexForEdict(const build4100::Edict* const edict) const noexcept {
    if (!edict) return -1;

    // ServerActivate's `count` is a high-water mark at activation time, not the
    // capacity of the engine's edict array.  CreateEdict/CreateFakeClient can
    // populate later slots without moving the base.  Geometry against the fixed
    // build4100 MAX_EDICTS pool therefore remains valid for post-activation edicts.
    if (edictBase_) {
        const auto base = reinterpret_cast<std::uintptr_t>(edictBase_);
        const auto value = reinterpret_cast<std::uintptr_t>(edict);
        if (value >= base) {
            const auto delta = value - base;
            if ((delta % sizeof(build4100::Edict)) == 0) {
                const auto rawIndex = delta / sizeof(build4100::Edict);
                if (rawIndex < build4100::kMaxEdicts) return static_cast<int>(rawIndex);
            }
        }
        return -1;
    }

    // Pre-activation/unit-test fallback only. Avoid re-entering the engine from
    // the normal activated path while CreateFakeClient builds a CGameClient.
    if (engine_ && engine_->IsValid()) {
        const int engineIndex = engine_->IndexOfEdict(const_cast<build4100::Edict*>(edict));
        if (engineIndex >= 0 && engineIndex < static_cast<int>(build4100::kMaxEdicts)) {
            return engineIndex;
        }
    }
    return -1;
}

build4100::Edict* EngineEntityBridge::BoundEdictForEngineIndex(const unsigned int index) const noexcept {
    if (index >= build4100::kMaxEdicts) return nullptr;
    for (const auto& binding : bindings_) {
        if (binding.edict && EngineIndexForEdict(binding.edict) == static_cast<int>(index)) {
            return binding.edict;
        }
    }
    return nullptr;
}

std::uint32_t EngineEntityBridge::MakeHandle(build4100::Edict* const edict,
                                              const std::size_t fallbackIndex) const noexcept {
    int index = EngineIndexForEdict(edict);
    if (index < 0) index = static_cast<int>(fallbackIndex);
    const std::uint32_t entry = static_cast<std::uint32_t>(index) & 0x0fffu;
    const std::uint32_t serial = edict ? static_cast<std::uint32_t>(edict->networkSerial) & 0x000fffffu : 0u;
    return entry | (serial << 12u);
}

bool EngineEntityBridge::AttachClient(const std::size_t clientIndex, build4100::Edict* const edict) {
    if (!edict || clientIndex == 0) return false;
    if (MutableBinding(edict)) return true;
    const EntityHandle handle = world_.Spawn<PlayerEntity>(clientIndex);
    if (!handle.IsValid()) return false;
    auto networkable = std::make_unique<EngineNetworkableShim>(
        edict, ServerClasses().FindByNetworkName("CCSPlayer"), "player", MakeHandle(edict, clientIndex));
    void* const baseEntity = networkable->GetBaseEntity();
    const std::uint32_t previousStateFlags = edict->stateFlags;
    const std::uint32_t previousNetworkable08 = edict->networkable08;
    const std::uint32_t previousUnknown0c = edict->unknown0c;
    edict->stateFlags = build4100::kEdictFull;
#if UINTPTR_MAX == 0xffffffffu
    edict->networkable08 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(networkable.get()));
    edict->unknown0c = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&networkable->Unknown()));
#endif
    {
        char detail[224];
        std::snprintf(detail, sizeof(detail),
                      "index=%zu edict=%p state=0x%08x networkable=%p unknown=%p",
                      clientIndex, static_cast<void*>(edict), edict->stateFlags,
                      static_cast<void*>(networkable.get()), static_cast<void*>(&networkable->Unknown()));
        EngineProbe("EDICT_INTERFACE_BIND_OK", detail);
    }
    bindings_.push_back(EngineEntityBinding{edict, baseEntity, handle, clientIndex, std::move(networkable),
                                            previousStateFlags, previousNetworkable08, previousUnknown0c});
    return true;
}

EntityHandle EngineEntityBridge::AttachGeneric(build4100::Edict* const edict, const char* const classname,
                                                     const std::uint32_t transmitState) {
    if (!edict) return {};
    if (const auto* existing = BindingForEdict(edict)) return existing->entity;
    const char* const safeClass = classname ? classname : "entity";
    const auto factory = EntityFactories().Resolve(safeClass);
    EntityHandle handle{};
    if (factory.kind == EntityFactoryKind::Weapon && factory.weapon != build4100::CSWeaponID::None) {
        handle = world_.Spawn<WeaponEntity>(factory.weapon);
    } else {
        handle = world_.Spawn<GameEntity>(EntityKind::Generic, safeClass);
    }
    if (!handle.IsValid()) return {};
    build4100::ServerClass* serverClass = factory.networkClass
        ? ServerClasses().FindByNetworkName(factory.networkClass) : nullptr;
    auto networkable = std::make_unique<EngineNetworkableShim>(edict, serverClass, safeClass,
                                                                MakeHandle(edict, handle.index));
    void* const baseEntity = networkable->GetBaseEntity();
    const std::uint32_t previousStateFlags = edict->stateFlags;
    const std::uint32_t previousNetworkable08 = edict->networkable08;
    const std::uint32_t previousUnknown0c = edict->unknown0c;
    // Match CBaseEntity::SetTransmitState: only 0x38 is the transmit-mode field.
    // Generic reconstruction callers choose the mode explicitly; planted_c4 uses
    // FL_EDICT_ALWAYS because original CPlantedC4::ShouldTransmit returns ALWAYS.
    edict->stateFlags = build4100::WithEdictTransmitState(build4100::kEdictFull, transmitState);
#if UINTPTR_MAX == 0xffffffffu
    edict->networkable08 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(networkable.get()));
    edict->unknown0c = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&networkable->Unknown()));
#endif
    bindings_.push_back(EngineEntityBinding{edict, baseEntity, handle, 0, std::move(networkable),
                                            previousStateFlags, previousNetworkable08, previousUnknown0c,
                                            transmitState & build4100::kEdictTransmitMask});
    return handle;
}

bool EngineEntityBridge::Detach(build4100::Edict* const edict, const bool removeEngineEdict) noexcept {
    auto findBinding = [this, edict]() {
        return std::find_if(bindings_.begin(), bindings_.end(),
                            [edict](const EngineEntityBinding& value) { return value.edict == edict; });
    };

    auto it = findBinding();
    if (it == bindings_.end()) {
        if (removeEngineEdict && engine_ && engine_->IsValid() && edict) engine_->RemoveEdict(edict);
        return false;
    }

    if (removeEngineEdict && engine_ && engine_->IsValid()) {
        // build4100 VEngineServer021::RemoveEdict -> ED_Free calls back into
        // IServerGameEnts::FreeContainingEntity for the same edict.  That callback
        // routes to Detach(edict, false), which removes this binding.  Do not touch
        // the vector iterator before or after the engine call without re-finding it:
        // the old implementation erased the binding recursively and then erased the
        // stale outer iterator a second time, corrupting the binding vector and
        // stalling the next GameFrame when planted_c4 was recycled.
        //
        // Keep the networkable/unknown shim fully alive while RemoveEdict executes;
        // the engine is allowed to query it until FreeContainingEntity detaches it.
        engine_->RemoveEdict(edict);

        it = findBinding();
        if (it == bindings_.end()) {
            char detail[112];
            std::snprintf(detail, sizeof(detail), "edict=%p callback_detached=1", static_cast<void*>(edict));
            EngineProbe("ENTITY_BRIDGE_REMOVE_REENTRY_OK", detail);
            return true;
        }

        // Defensive fallback for a host that frees the edict without making the
        // FreeContainingEntity callback.  The engine already owns the edict state at
        // this point, so only release source-side ownership; never restore the old
        // edict flags/pointers over the engine's post-RemoveEdict state.
        if (it->networkable) it->networkable->Release();
        (void)world_.Remove(it->entity);
        bindings_.erase(it);
        char detail[112];
        std::snprintf(detail, sizeof(detail), "edict=%p callback_detached=0", static_cast<void*>(edict));
        EngineProbe("ENTITY_BRIDGE_REMOVE_FALLBACK", detail);
        return true;
    }

    if (it->networkable) it->networkable->Release();
    (void)world_.Remove(it->entity);
    if (it->edict) {
        it->edict->stateFlags = it->previousStateFlags;
        it->edict->networkable08 = it->previousNetworkable08;
        it->edict->unknown0c = it->previousUnknown0c;
    }
    bindings_.erase(it);
    return true;
}

void EngineEntityBridge::ObserveBaseEntity(build4100::Edict* const edict, void* const baseEntity) noexcept {
    if (!edict || !baseEntity) return;
    if (auto* binding = MutableBinding(edict)) binding->baseEntity = baseEntity;
}

void EngineEntityBridge::Tick(const float now) noexcept { world_.Tick(now); }

build4100::Edict* EngineEntityBridge::EdictForBaseEntity(void* const baseEntity) const noexcept {
    if (!baseEntity) return nullptr;
    for (const auto& binding : bindings_) if (binding.baseEntity == baseEntity) return binding.edict;
    return nullptr;
}

void* EngineEntityBridge::BaseEntityForEdict(build4100::Edict* const edict) const noexcept {
    if (const auto* binding = BindingForEdict(edict)) return binding->baseEntity;
    return nullptr;
}

build4100::IServerNetworkable* EngineEntityBridge::NetworkableForEdict(build4100::Edict* const edict) const noexcept {
    if (const auto* binding = BindingForEdict(edict)) return binding->networkable.get();
    return nullptr;
}

build4100::IServerUnknown* EngineEntityBridge::UnknownForEdict(build4100::Edict* const edict) const noexcept {
    if (const auto* binding = BindingForEdict(edict)) {
        return binding->networkable ? &binding->networkable->Unknown() : nullptr;
    }
    return nullptr;
}

const EngineEntityBinding* EngineEntityBridge::BindingForEdict(build4100::Edict* const edict) const noexcept {
    for (const auto& binding : bindings_) if (binding.edict == edict) return &binding;
    return nullptr;
}

std::size_t EngineEntityBridge::CollectForcedTransmitIndices(unsigned short* const out,
                                                               const std::size_t capacity) const noexcept {
    if (!out || capacity == 0 || !edictBase_) return 0;

    std::size_t written = 0;
    for (const auto& binding : bindings_) {
        // Player edicts already participate in the engine's normal PVS/candidate
        // construction.  Reconstructed dynamic non-player entities currently do
        // not have a real CCollisionProperty/ShouldTransmit implementation, so
        // build4100 can omit them from the candidate array even though their
        // IServerNetworkable and ServerClass are valid.  Force only those bridge-
        // owned generic edicts into m_pTransmitEdict until that entity-side PVS
        // layer is reconstructed.
        if (binding.clientIndex != 0 || !binding.edict || !binding.networkable ||
            binding.networkable->Released() || build4100::IsFree(*binding.edict) ||
            binding.desiredTransmitState == build4100::kEdictTransmitDontSend) {
            continue;
        }

        const int rawIndex = EngineIndexForEdict(binding.edict);
        if (rawIndex <= 0 || rawIndex >= static_cast<int>(build4100::kMaxEdicts)) continue;

        if (written >= capacity) break;
        out[written++] = static_cast<unsigned short>(rawIndex);
    }
    return written;
}

bool EngineEntityBridge::SetGenericTransmitState(build4100::Edict* const edict,
                                                  const std::uint32_t transmitState) noexcept {
    auto* const binding = MutableBinding(edict);
    if (!binding || binding->clientIndex != 0 || !binding->edict) return false;
    binding->desiredTransmitState = transmitState & build4100::kEdictTransmitMask;
    binding->edict->stateFlags = build4100::WithEdictTransmitState(
        binding->edict->stateFlags | build4100::kEdictFull, binding->desiredTransmitState);
    return true;
}

bool EngineEntityBridge::GenericTransmitStateForEngineIndex(
    const unsigned int index, std::uint32_t& transmitState) const noexcept {
    build4100::Edict* const edict = BoundEdictForEngineIndex(index);
    if (!edict) return false;
    const EngineEntityBinding* const binding = BindingForEdict(edict);
    if (!binding || binding->clientIndex != 0 || !binding->networkable ||
        binding->networkable->Released()) {
        return false;
    }
    transmitState = binding->desiredTransmitState;
    return true;
}

EngineEntityBinding* EngineEntityBridge::MutableBinding(build4100::Edict* const edict) noexcept {
    for (auto& binding : bindings_) if (binding.edict == edict) return &binding;
    return nullptr;
}

}  // namespace cssv34::reconstructed::abi
