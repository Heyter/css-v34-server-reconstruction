#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

#include "abi/server_class_registry.h"
#include "server/cs_game_state.h"

using namespace cssv34;
using namespace cssv34::reconstructed;
using namespace cssv34::reconstructed::abi;

namespace {

const build4100::SendProp* FindDirectProp(const build4100::SendTable* table, const char* name) {
    assert(table && name);
    for (int i = 0; i < table->propCount; ++i) {
        if (table->props[i].varName && std::strcmp(table->props[i].varName, name) == 0) {
            return &table->props[i];
        }
    }
    return nullptr;
}

void ValidateTable(const build4100::SendTable* table,
                   std::set<const build4100::SendTable*>& visited,
                   std::size_t& propCount,
                   std::size_t& dataTableLinks,
                   std::size_t& arrayCount) {
    assert(table);
    if (!visited.insert(table).second) return;
    assert(table->netTableName && table->netTableName[0]);
    assert(table->propCount >= 0);
    if (table->propCount) assert(table->props);

    for (int i = 0; i < table->propCount; ++i) {
        const auto& prop = table->props[i];
        assert(prop.varName && prop.varName[0]);
        const auto type = static_cast<int>(prop.type);
        assert(type >= static_cast<int>(build4100::SendPropType::Int));
        assert(type < static_cast<int>(build4100::SendPropType::Count));
        assert(prop.elements >= 1);
        ++propCount;

        if (prop.flags & 0x40) {
            assert(prop.excludeDataTableName && prop.excludeDataTableName[0]);
        }

        if (prop.type == build4100::SendPropType::DataTable) {
            assert(prop.dataTable && "client schema cannot encode a null datatable child");
            assert(prop.dataTable->netTableName && prop.dataTable->netTableName[0]);
            ++dataTableLinks;
            ValidateTable(prop.dataTable, visited, propCount, dataTableLinks, arrayCount);
            continue;
        }

        // build4100 has no VectorXY/Int64 wire types. DPT_Array is a genuine
        // protocol-7 type and pairs with the immediately preceding inside-array
        // element SendProp.
        if (prop.type == build4100::SendPropType::Array) {
            ++arrayCount;
            assert(i > 0);
            assert(prop.arrayProp == &table->props[i - 1]);
            assert((prop.arrayProp->flags & build4100::kSendPropInsideArray) != 0);
            assert(prop.elements > 0);
            assert(prop.elementStride >= 0);
            continue;
        }
        if (prop.type == build4100::SendPropType::Int) {
            assert(prop.bits >= 0 && prop.bits <= 32);
        }
    }
}

}  // namespace

int main() {
    auto& registry = ServerClasses();
    assert(registry.Count() == 177);

    std::set<std::string> networkNames;
    std::set<std::string> rootTableNames;
    std::set<const build4100::SendTable*> visited;
    std::size_t props = 0;
    std::size_t links = 0;
    std::size_t arrays = 0;
    int expectedClassId = 0;

    for (auto* serverClass = registry.Head(); serverClass; serverClass = serverClass->next) {
        assert(serverClass->networkName && serverClass->networkName[0]);
        assert(serverClass->table);
        assert(serverClass->classId == expectedClassId++);
        assert(serverClass->instanceBaselineIndex == 0xffff);
        assert(networkNames.insert(serverClass->networkName).second);
        assert(rootTableNames.insert(serverClass->table->netTableName).second);
        ValidateTable(serverClass->table, visited, props, links, arrays);
    }

    assert(expectedClassId == 177);
    assert(networkNames.size() == 177);
    assert(rootTableNames.size() == 177);
    assert(visited.size() == 229);
    assert(props == 2219);
    assert(links == 212);
    assert(arrays == 10);
    static_assert(build4100::kSendPropXYZE == 0x80);
    static_assert(build4100::kSendPropInsideArray == 0x100);

    const auto* localPlayer = registry.FindLiveTableByName("DT_LocalPlayerExclusive");
    assert(localPlayer);
    const build4100::SendProp* ground = nullptr;
    for (int i = 0; i < localPlayer->propCount; ++i) {
        if (std::strcmp(localPlayer->props[i].varName, "m_hGroundEntity") == 0) {
            ground = &localPlayer->props[i];
            break;
        }
    }
    assert(ground && ground->type == build4100::SendPropType::Int);
    assert(ground->bits == 21);
    assert((ground->flags & 0x400) != 0);

    // v1.8.12 gameplay-backed SendProp contract. The proxies consume the same
    // ClientRecord/CsGameState that drives reconstructed gameplay instead of a
    // probe-only pulse. Exercise both inherited and direct CCSPlayer leaves.
    ClientRegistry clients;
    assert(clients.Configure(4));
    build4100::Edict playerEdict{};
    char reject[64]{};
    assert(clients.Connect(1, &playerEdict, "schema_player", "loopback", reject, sizeof(reject)));
    assert(clients.PutInServer(1, &playerEdict, "schema_player"));
    CsGameState state(clients);
    state.Configure();
    assert(state.OnClientPutInServer(1));
    assert(state.JoinTeam(1, static_cast<int>(build4100::CSTeam::Terrorist)));
    assert(state.EnterGame(1));
    ClientRecord* record = clients.Get(1);
    assert(record);
    record->armorValue = 75;
    record->inBuyZone = true;
    record->hasHelmet = true;
    record->hasDefuser = true;
    BindGameplayClientRegistry(&clients);

    auto readInt = [](const build4100::SendProp* prop, const int objectId) {
        assert(prop && prop->proxyFn && prop->type == build4100::SendPropType::Int);
        std::int32_t out = -777;
        prop->proxyFn(prop, nullptr, nullptr, &out, 0, objectId);
        return out;
    };
    auto readFloat = [](const build4100::SendProp* prop, const int objectId) {
        assert(prop && prop->proxyFn && prop->type == build4100::SendPropType::Float);
        float out = -777.0f;
        prop->proxyFn(prop, nullptr, nullptr, &out, 0, objectId);
        return out;
    };
    auto readVector = [](const build4100::SendProp* prop, const int objectId) {
        assert(prop && prop->proxyFn && prop->type == build4100::SendPropType::Vector);
        build4100::Vector out{-777.0f, -777.0f, -777.0f};
        prop->proxyFn(prop, nullptr, nullptr, &out, 0, objectId);
        return out;
    };

    const auto* csPlayer = registry.FindLiveTableByName("DT_CSPlayer");
    const auto* basePlayer = registry.FindLiveTableByName("DT_BasePlayer");
    const auto* baseEntity = registry.FindLiveTableByName("DT_BaseEntity");
    const auto* csLocalPlayer = registry.FindLiveTableByName("DT_CSLocalPlayerExclusive");
    const auto* csNonLocalPlayer = registry.FindLiveTableByName("DT_CSNonLocalPlayerExclusive");
    assert(csPlayer && basePlayer && baseEntity && localPlayer && csLocalPlayer && csNonLocalPlayer);
    assert(readInt(FindDirectProp(baseEntity, "m_iTeamNum"), 1) == 2);
    assert(readInt(FindDirectProp(basePlayer, "m_iHealth"), 1) == 100);
    const auto* currentFov = FindDirectProp(basePlayer, "m_iFOV");
    const auto* defaultFov = FindDirectProp(basePlayer, "m_iDefaultFOV");
    assert(currentFov && defaultFov && currentFov->bits == 8 && defaultFov->bits == 8);
    record->currentFov = 40;
    record->defaultFov = 90;
    assert(readInt(currentFov, 1) == 40);
    assert(readInt(defaultFov, 1) == 90);
    record->currentFov = 10;
    assert(readInt(currentFov, 1) == 10);
    record->currentFov = 0;
    assert(readInt(currentFov, 1) == 0);
    assert(readInt(FindDirectProp(csPlayer, "m_iAccount"), 1) == 800);
    assert(readInt(FindDirectProp(csPlayer, "m_iPlayerState"), 1) ==
           static_cast<int>(build4100::CSPlayerState::Active));
    assert(readInt(FindDirectProp(csPlayer, "m_ArmorValue"), 1) == 75);
    assert(readInt(FindDirectProp(csPlayer, "m_bInBuyZone"), 1) == 1);
    assert(readInt(FindDirectProp(csPlayer, "m_bHasHelmet"), 1) == 1);
    assert(readInt(FindDirectProp(csPlayer, "m_bHasDefuser"), 1) == 1);

    // v1.8.14: eye angles are sourced from the same ClientRecord written by
    // ProcessUsercmds.  SendPropAngle is a scaled [0,360] float in build4100.
    record->absAngles = build4100::QAngle{12.0f, 123.0f, 0.0f};
    const auto* eyePitch = FindDirectProp(csPlayer, "m_angEyeAngles[0]");
    const auto* eyeYaw = FindDirectProp(csPlayer, "m_angEyeAngles[1]");
    assert(eyePitch && eyeYaw);
    assert(eyePitch->bits == 11 && eyeYaw->bits == 11);
    assert(eyePitch->lowValue == 0.0f && eyePitch->highValue == 360.0f);
    assert(eyeYaw->lowValue == 0.0f && eyeYaw->highValue == 360.0f);
    const float expectedAngleMul = 2047.0f / 360.0f;
    assert(eyePitch->highLowMul == expectedAngleMul);
    assert(eyeYaw->highLowMul == expectedAngleMul);
    assert(readFloat(eyePitch, 1) == 12.0f);
    assert(readFloat(eyeYaw, 1) == 123.0f);
    record->absAngles = build4100::QAngle{-10.0f, 370.0f, 0.0f};
    assert(readFloat(eyePitch, 1) == 350.0f);
    assert(readFloat(eyeYaw, 1) == 10.0f);

    // v1.8.15: usercmd movement state is source-owned and exported through the
    // original build4100 origin/velocity SendProp contracts.
    record->absOrigin = build4100::Vector{1.25f, -2.5f, 64.0f};
    record->absVelocity = build4100::Vector{200.0f, -125.0f, 12.0f};
    const auto* origin = FindDirectProp(baseEntity, "m_vecOrigin");
    const auto* localOrigin = FindDirectProp(csLocalPlayer, "m_vecOrigin");
    const auto* nonLocalOrigin = FindDirectProp(csNonLocalPlayer, "m_vecOrigin");
    const auto* velocityX = FindDirectProp(localPlayer, "m_vecVelocity[0]");
    const auto* velocityY = FindDirectProp(localPlayer, "m_vecVelocity[1]");
    const auto* velocityZ = FindDirectProp(localPlayer, "m_vecVelocity[2]");
    assert(origin && localOrigin && nonLocalOrigin && velocityX && velocityY && velocityZ);
    assert((origin->flags & build4100::kSendPropCoord) != 0);
    assert((localOrigin->flags & build4100::kSendPropNoScale) != 0);
    assert((localOrigin->flags & build4100::kSendPropChangesOften) != 0);
    assert((nonLocalOrigin->flags & build4100::kSendPropCoord) != 0);
    assert((nonLocalOrigin->flags & build4100::kSendPropChangesOften) != 0);
    const auto originValue = readVector(origin, 1);
    const auto localOriginValue = readVector(localOrigin, 1);
    const auto nonLocalOriginValue = readVector(nonLocalOrigin, 1);
    assert(originValue.x == 1.25f && originValue.y == -2.5f && originValue.z == 64.0f);
    assert(localOriginValue.x == 1.25f && localOriginValue.y == -2.5f && localOriginValue.z == 64.0f);
    assert(nonLocalOriginValue.x == 1.25f && nonLocalOriginValue.y == -2.5f && nonLocalOriginValue.z == 64.0f);

    // Exact build4100 recipient masks: BasePlayer::localdata and
    // CSPlayer::cslocaldata go only to objectId; csnonlocaldata goes to all
    // recipients except objectId. This also selects the NOSCALE local origin
    // instead of the BaseEntity origin excluded by DT_CSPlayer.
    const auto* baseLocalLink = FindDirectProp(basePlayer, "localdata");
    const auto* csLocalLink = FindDirectProp(csPlayer, "cslocaldata");
    const auto* csNonLocalLink = FindDirectProp(csPlayer, "csnonlocaldata");
    assert(baseLocalLink && csLocalLink && csNonLocalLink);
    assert(baseLocalLink->dataTableProxyFn && csLocalLink->dataTableProxyFn && csNonLocalLink->dataTableProxyFn);
    std::array<std::uint32_t, 8> recipients{};
    csLocalLink->dataTableProxyFn(csLocalLink, nullptr, nullptr, recipients.data(), 3);
    assert(recipients[0] == (1u << 2));
    for (std::size_t i = 1; i < recipients.size(); ++i) assert(recipients[i] == 0u);
    recipients.fill(0u);
    baseLocalLink->dataTableProxyFn(baseLocalLink, nullptr, nullptr, recipients.data(), 3);
    assert(recipients[0] == (1u << 2));
    for (std::size_t i = 1; i < recipients.size(); ++i) assert(recipients[i] == 0u);
    recipients.fill(0u);
    csNonLocalLink->dataTableProxyFn(csNonLocalLink, nullptr, nullptr, recipients.data(), 3);
    assert(recipients[0] == (0xffffffffu & ~(1u << 2)));
    for (std::size_t i = 1; i < recipients.size(); ++i) assert(recipients[i] == 0xffffffffu);
    assert(velocityX->bits == 20 && velocityY->bits == 20 && velocityZ->bits == 16);
    assert(velocityX->lowValue == -2048.0f && velocityX->highValue == 2048.0f);
    assert(velocityY->lowValue == -2048.0f && velocityY->highValue == 2048.0f);
    assert(velocityZ->lowValue == -2048.0f && velocityZ->highValue == 2048.0f);
    assert((velocityX->flags & build4100::kSendPropChangesOften) != 0);
    assert((velocityY->flags & build4100::kSendPropChangesOften) != 0);
    assert((velocityZ->flags & build4100::kSendPropChangesOften) != 0);
    assert(velocityX->highLowMul == 1048575.0f / 4096.0f);
    assert(velocityY->highLowMul == 1048575.0f / 4096.0f);
    assert(velocityZ->highLowMul == 65535.0f / 4096.0f);
    assert(readFloat(velocityX, 1) == 200.0f);
    assert(readFloat(velocityY, 1) == -125.0f);
    assert(readFloat(velocityZ, 1) == 12.0f);

    // Unbound object ids fail closed to zero; unrelated props remain safe-zero.
    assert(readInt(FindDirectProp(csPlayer, "m_iThrowGrenadeCounter"), 1) == 0);
    BindGameplayClientRegistry(nullptr);
    assert(readInt(FindDirectProp(csPlayer, "m_iAccount"), 1) == 0);
    std::cout << "SENDPROP_GAMEPLAY_PROXY=PASS account=800 health=100 team=2 state=0 armor=75 buyzone=1 helmet=1 defuser=1 eyeangles=clientrecord_11bit_0_360_highlowmul movement=csplayer_local_noscale+nonlocal_coord+recipient_masks+velocity_-2048_2048" << '\n';

    std::cout << "SENDTABLE_CLIENT_SCHEMA=PASS protocol=7 classes=177 reachable_tables="
              << visited.size() << " reachable_props=" << props
              << " datatable_links=" << links << " dpt_arrays=" << arrays
              << " xyze_build4100=scalar_vector class_ids=0..176\n";
}
