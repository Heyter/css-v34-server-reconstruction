#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

#include "abi/server_class_registry.h"

using namespace cssv34;
using namespace cssv34::reconstructed::abi;

namespace {

bool IsArrayElementName(const char* name) {
    return name && std::strlen(name) == 3 &&
           std::isdigit(static_cast<unsigned char>(name[0])) &&
           std::isdigit(static_cast<unsigned char>(name[1])) &&
           std::isdigit(static_cast<unsigned char>(name[2]));
}

const build4100::SendProp* FindProp(const build4100::SendTable* table, const char* name) {
    if (!table || !name) return nullptr;
    for (int i = 0; i < table->propCount; ++i) {
        if (table->props[i].varName && std::strcmp(table->props[i].varName, name) == 0) {
            return &table->props[i];
        }
    }
    return nullptr;
}

void WalkTable(const build4100::SendTable* table,
               std::set<const build4100::SendTable*>& visited,
               std::size_t& props,
               std::size_t& links,
               std::size_t& arrays) {
    assert(table);
    if (!visited.insert(table).second) return;
    assert(table->netTableName && table->netTableName[0]);
    const bool generatedFixedArray = std::strncmp(table->netTableName, "DT_", 3) != 0 &&
                                     std::strncmp(table->netTableName, "_ST_", 4) != 0 &&
                                     std::strncmp(table->netTableName, "_LPT_", 5) != 0;
    const bool generatedUtlMain = std::strncmp(table->netTableName, "_ST_", 4) == 0;
    const bool generatedUtlLength = std::strncmp(table->netTableName, "_LPT_", 5) == 0;
    assert(table->propCount >= 0);
    if (table->propCount > 0) assert(table->props);
    props += static_cast<std::size_t>(table->propCount);

    for (int i = 0; i < table->propCount; ++i) {
        const auto& prop = table->props[i];
        assert(prop.varName && prop.varName[0]);
        if (generatedFixedArray) {
            assert(IsArrayElementName(prop.varName));
            assert(prop.excludeDataTableName &&
                   std::strcmp(prop.excludeDataTableName, table->netTableName) == 0);
        } else if (generatedUtlMain) {
            if (i == 0) {
                assert(std::strcmp(prop.varName, "lengthproxy") == 0);
            } else {
                assert(IsArrayElementName(prop.varName));
            }
        } else if (generatedUtlLength) {
            assert(i == 0);
            assert(std::strncmp(prop.varName, "lengthprop", 10) == 0);
        }
        if (prop.type == build4100::SendPropType::DataTable) {
            const bool utlVectorElement = generatedUtlMain && i > 0 && (prop.flags & 0x200);
            // build4100's generated UtlVector<DataTable> element props are special:
            // they keep a child SendTable but clear dataTableProxyFn and mark 0x200.
            if (utlVectorElement) {
                assert(prop.dataTableProxyFn == nullptr);
            } else {
                assert(prop.dataTableProxyFn);
            }
            assert(prop.dataTable && "reachable live datatable link must resolve");
            ++links;
            WalkTable(prop.dataTable, visited, props, links, arrays);
        } else if (prop.type == build4100::SendPropType::Array) {
            ++arrays;
            assert(i > 0);
            assert(prop.arrayProp == &table->props[i - 1]);
            assert((prop.arrayProp->flags & build4100::kSendPropInsideArray) != 0);
            assert(prop.elements > 0);
            assert(prop.elementStride >= 0);
            assert(prop.proxyFn && "DPT_Array top prop must use SendProxy_Empty-compatible proxy");
        } else if (prop.flags & 0x40) {
            assert(prop.excludeDataTableName);
        } else {
            const bool utlVectorElement = generatedUtlMain && i > 0;
            if (utlVectorElement) {
                // Generated UtlVector elements use extraData + elementStride; build4100
                // clears the direct proxy on these child props.
                assert(prop.proxyFn == nullptr);
                assert(prop.extraData != nullptr);
            } else {
                assert(prop.proxyFn && "primitive live prop needs safe proxy");
            }
        }
    }
}

}  // namespace

int main() {
    auto& registry = ServerClasses();
    assert(registry.Count() == 177);
    assert(registry.OriginalMetadataTableCount() == 198);
    assert(registry.LiveTableCount() == 198);
    assert(registry.LivePropTotal() == 1088);
    assert(registry.LiveDataTableLinkCount() == 212);
    assert(registry.LiveFixedArrayCount() == 27);
    assert(registry.LiveFixedArrayElementCount() == 1096);
    assert(registry.LiveUtlVectorCount() == 2);
    assert(registry.LiveUtlVectorElementCapacity() == 31);
    assert(registry.LiveUtlVectorGeneratedPropCount() == 35);
    assert(registry.LiveInternalArrayCount() == 10);
    assert(registry.LiveInternalArrayElementCapacity() == 194);

    // v1.8.34zj: the engine mutates ServerClass::instanceBaselineIndex after
    // SV_CreateBaseline. Exercise the census writer with two materialized
    // baselines so the live Docker probe has a host-side regression contract.
    auto* baselinePlayer = registry.FindByNetworkName("CCSPlayer");
    auto* baselineC4 = registry.FindByNetworkName("CPlantedC4");
    assert(baselinePlayer && baselineC4);
    assert(baselinePlayer->instanceBaselineIndex == 0xffff);
    assert(baselineC4->instanceBaselineIndex == 0xffff);
    baselinePlayer->instanceBaselineIndex = 7;
    baselineC4->instanceBaselineIndex = 3;
    const char* baselineCensus = "/tmp/cssv34-instancebaseline-smoke.tsv";
    assert(DumpRuntimeInstanceBaselines(baselineCensus) == 2);
    {
        std::ifstream input(baselineCensus);
        assert(input.good());
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        assert(text.find("#build4100-instancebaseline-v1") != std::string::npos);
        assert(text.find("1\tCCSPlayer\tDT_CSPlayer\t7\t1") != std::string::npos);
        assert(text.find("6\tCPlantedC4\tDT_PlantedC4\t3\t1") != std::string::npos);
    }
    // v1.8.34zk: late-spawn ServerClasses may legitimately retain the 0xffff
    // sentinel because SV_CreateBaseline is a one-shot pre-GameFrame pass. The
    // real build4100 lifecycle observed this for CCSPlayer while CPlantedC4 was
    // materialized before ServerActivate returned. Preserve both states.
    baselinePlayer->instanceBaselineIndex = 0xffff;
    baselineC4->instanceBaselineIndex = 0;
    assert(DumpRuntimeInstanceBaselines(baselineCensus) == 1);
    {
        std::ifstream input(baselineCensus);
        assert(input.good());
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        assert(text.find("1\tCCSPlayer\tDT_CSPlayer\t65535\t0") != std::string::npos);
        assert(text.find("6\tCPlantedC4\tDT_PlantedC4\t0\t1") != std::string::npos);
    }

    // v1.8.34zp: after changelevel to a map such as cs_office, the same valid
    // 177-row census can contain no materialized baseline at the pre-connect
    // checkpoint. The writer must still publish the all-sentinel snapshot.
    baselineC4->instanceBaselineIndex = 0xffff;
    assert(DumpRuntimeInstanceBaselines(baselineCensus) == 0);
    {
        std::ifstream input(baselineCensus);
        assert(input.good());
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        assert(text.find("1\tCCSPlayer\tDT_CSPlayer\t65535\t0") != std::string::npos);
        assert(text.find("6\tCPlantedC4\tDT_PlantedC4\t65535\t0") != std::string::npos);
    }
    std::remove(baselineCensus);
    baselinePlayer->instanceBaselineIndex = 0xffff;
    baselineC4->instanceBaselineIndex = 0xffff;

    std::set<const build4100::SendTable*> visited;
    std::size_t reachableProps = 0;
    std::size_t reachableLinks = 0;
    std::size_t reachableArrays = 0;
    std::size_t serverClasses = 0;
    for (auto* serverClass = registry.Head(); serverClass; serverClass = serverClass->next) {
        assert(serverClass->networkName && serverClass->table);
        assert(serverClass->table->propCount >= 0);
        if (serverClass->table->propCount > 0) assert(serverClass->table->props);
        WalkTable(serverClass->table, visited, reachableProps, reachableLinks, reachableArrays);
        ++serverClasses;
    }

    assert(serverClasses == 177);
    // The exact build4100 root set now covers every network class. Reachability
    // must strictly expand over the former 15-root subset while every traversed
    // datatable link is required to resolve by WalkTable().
    assert(visited.size() == 229);
    assert(reachableProps == 2219);
    assert(reachableLinks == 212);
    assert(reachableArrays == 10);

    // Original build4100 has exactly two network-class roots with zero direct
    // SendProps.  They still own valid SendTables/CSendTablePrecalc objects and
    // must not be rejected by the runtime flattened-schema dumper.
    auto* baseTempEntity = registry.FindByNetworkName("CBaseTempEntity");
    assert(baseTempEntity && baseTempEntity->table);
    assert(std::strcmp(baseTempEntity->table->netTableName, "DT_BaseTempEntity") == 0);
    assert(baseTempEntity->table->propCount == 0);
    assert(baseTempEntity->table->props == nullptr);

    // v1.8.35g: TE_FireBullets is serialized from a short-lived sender object
    // through VEngineServer021::PlaybackTempEntity, not from ClientRegistry.
    // Verify the exact build4100 sender offsets and direct value proxies.
    auto* teFireBullets = registry.FindByNetworkName("CTEFireBullets");
    assert(teFireBullets && teFireBullets->classId == 120 && teFireBullets->table);
    assert(std::strcmp(teFireBullets->table->netTableName, "DT_TEFireBullets") == 0);
    assert(teFireBullets->table->propCount == 8);
    const auto* teOrigin = FindProp(teFireBullets->table, "m_vecOrigin");
    const auto* tePitch = FindProp(teFireBullets->table, "m_vecAngles[0]");
    const auto* teYaw = FindProp(teFireBullets->table, "m_vecAngles[1]");
    const auto* teWeapon = FindProp(teFireBullets->table, "m_iWeaponID");
    const auto* teMode = FindProp(teFireBullets->table, "m_iMode");
    const auto* teSeed = FindProp(teFireBullets->table, "m_iSeed");
    const auto* tePlayer = FindProp(teFireBullets->table, "m_iPlayer");
    const auto* teSpread = FindProp(teFireBullets->table, "m_flSpread");
    assert(teOrigin && tePitch && teYaw && teWeapon && teMode && teSeed && tePlayer && teSpread);
    assert(teOrigin->offset == 16 && tePitch->offset == 28 && teYaw->offset == 32);
    assert(teWeapon->offset == 40 && teMode->offset == 44 && teSeed->offset == 48);
    assert(tePlayer->offset == 12 && teSpread->offset == 52);
    assert(tePitch->lowValue == 0.0f && tePitch->highValue == 360.0f && tePitch->bits == 13);
    assert(teYaw->lowValue == 0.0f && teYaw->highValue == 360.0f && teYaw->bits == 13);
    assert(teSpread->lowValue == 0.0f && teSpread->highValue == 1.0f && teSpread->bits == 10);
    assert(teSpread->highLowMul > 1022.0f && teSpread->highLowMul < 1024.0f);
    alignas(4) unsigned char teData[56]{};
    const std::int32_t playerValue = 3, weaponValue = 16, modeValue = 1, seedValue = 123;
    const float pitchValue = 15.5f, yawValue = 271.25f, spreadValue = 0.15f;
    const float originValue[3] = {10.0f, 20.0f, 30.0f};
    std::memcpy(teData + 12, &playerValue, sizeof(playerValue));
    std::memcpy(teData + 16, originValue, sizeof(originValue));
    std::memcpy(teData + 28, &pitchValue, sizeof(pitchValue));
    std::memcpy(teData + 32, &yawValue, sizeof(yawValue));
    std::memcpy(teData + 40, &weaponValue, sizeof(weaponValue));
    std::memcpy(teData + 44, &modeValue, sizeof(modeValue));
    std::memcpy(teData + 48, &seedValue, sizeof(seedValue));
    std::memcpy(teData + 52, &spreadValue, sizeof(spreadValue));
    std::int32_t intOut = 0; float floatOut = 0.0f; float vectorOut[3]{};
    tePlayer->proxyFn(tePlayer, teData, teData + tePlayer->offset, &intOut, 0, 0);
    assert(intOut == playerValue);
    teWeapon->proxyFn(teWeapon, teData, teData + teWeapon->offset, &intOut, 0, 0);
    assert(intOut == weaponValue);
    teMode->proxyFn(teMode, teData, teData + teMode->offset, &intOut, 0, 0);
    assert(intOut == modeValue);
    teSeed->proxyFn(teSeed, teData, teData + teSeed->offset, &intOut, 0, 0);
    assert(intOut == seedValue);
    tePitch->proxyFn(tePitch, teData, teData + tePitch->offset, &floatOut, 0, 0);
    assert(floatOut == pitchValue);
    teYaw->proxyFn(teYaw, teData, teData + teYaw->offset, &floatOut, 0, 0);
    assert(floatOut == yawValue);
    teSpread->proxyFn(teSpread, teData, teData + teSpread->offset, &floatOut, 0, 0);
    assert(floatOut == spreadValue);
    teOrigin->proxyFn(teOrigin, teData, teData + teOrigin->offset, vectorOut, 0, 0);
    assert(vectorOut[0] == 10.0f && vectorOut[1] == 20.0f && vectorOut[2] == 30.0f);

    auto* gameRulesProxy = registry.FindByNetworkName("CGameRulesProxy");
    assert(gameRulesProxy && gameRulesProxy->table);
    assert(std::strcmp(gameRulesProxy->table->netTableName, "DT_GameRulesProxy") == 0);
    assert(gameRulesProxy->table->propCount == 0);
    assert(gameRulesProxy->table->props == nullptr);

    auto* player = registry.FindByNetworkName("CCSPlayer");
    assert(player && player->table && player->table->propCount == 38);
    assert(std::strcmp(player->table->netTableName, "DT_CSPlayer") == 0);
    assert(player->table->props[0].type == build4100::SendPropType::DataTable);
    assert(player->table->props[0].dataTable);
    assert(std::strcmp(player->table->props[0].dataTable->netTableName, "DT_BasePlayer") == 0);

    auto* resource = registry.FindByNetworkName("CCSPlayerResource");
    assert(resource && resource->table && resource->table->propCount == 17);
    const auto* hostageAlive = FindProp(resource->table, "m_bHostageAlive");
    assert(hostageAlive && hostageAlive->type == build4100::SendPropType::DataTable);
    assert(hostageAlive->dataTable && std::strcmp(hostageAlive->dataTable->netTableName, "m_bHostageAlive") == 0);
    assert(hostageAlive->dataTable->propCount == 12);
    assert(std::strcmp(hostageAlive->dataTable->props[0].varName, "000") == 0);
    assert(hostageAlive->dataTable->props[0].offset == 0);
    assert(hostageAlive->dataTable->props[11].offset == 11);
    assert(hostageAlive->dataTable->props[11].bits == 1);

    const auto* playerResource = registry.FindLiveTableByName("DT_PlayerResource");
    assert(playerResource);
    const auto* ping = FindProp(playerResource, "m_iPing");
    assert(ping && ping->dataTable && ping->dataTable->propCount == 65);
    assert(std::strcmp(ping->dataTable->props[64].varName, "064") == 0);
    assert(ping->dataTable->props[64].offset == 64 * 4);
    assert(ping->dataTable->props[64].bits == 10);

    const auto* overlayVars = registry.FindLiveTableByName("DT_OverlayVars");
    assert(overlayVars && overlayVars->propCount == 1);
    const auto* animOverlay = FindProp(overlayVars, "m_AnimOverlay");
    assert(animOverlay && animOverlay->type == build4100::SendPropType::DataTable);
    assert(animOverlay->offset == 0 && animOverlay->dataTable);
    assert(std::strcmp(animOverlay->dataTable->netTableName, "_ST_m_AnimOverlay_15") == 0);
    assert(animOverlay->dataTable->propCount == 16);
    assert(std::strcmp(animOverlay->dataTable->props[0].varName, "lengthproxy") == 0);
    assert(animOverlay->dataTable->props[0].dataTable);
    assert(std::strcmp(animOverlay->dataTable->props[0].dataTable->netTableName,
                       "_LPT_m_AnimOverlay_15") == 0);
    assert(animOverlay->dataTable->props[0].dataTable->propCount == 1);
    assert(animOverlay->dataTable->props[1].type == build4100::SendPropType::DataTable);
    assert((animOverlay->dataTable->props[1].flags & 0x200) != 0);
    assert(animOverlay->dataTable->props[1].elementStride == 0);
    assert(animOverlay->dataTable->props[1].offset == 0);
    assert(animOverlay->dataTable->props[1].proxyFn == nullptr);
    assert(animOverlay->dataTable->props[1].dataTableProxyFn == nullptr);
    assert(animOverlay->dataTable->props[1].dataTable != nullptr);
    assert(std::strcmp(animOverlay->dataTable->props[1].dataTable->netTableName,
                       "DT_Animationlayer") == 0);
    assert(animOverlay->dataTable->props[15].elementStride == 14);

    const auto* scene = registry.FindLiveTableByName("DT_SceneEntity");
    assert(scene);
    const auto* actorList = FindProp(scene, "m_hActorList");
    assert(actorList && actorList->dataTable);
    assert(std::strcmp(actorList->dataTable->netTableName, "_ST_m_hActorList_16") == 0);
    assert(actorList->dataTable->propCount == 17);
    assert(actorList->dataTable->props[1].type == build4100::SendPropType::Int);
    assert(actorList->dataTable->props[1].bits == 21);
    assert((actorList->dataTable->props[1].flags & 1) != 0);
    assert(actorList->dataTable->props[1].elementStride == 0);
    assert(actorList->dataTable->props[1].offset == 0);
    assert(actorList->dataTable->props[1].proxyFn == nullptr);
    assert(actorList->dataTable->props[16].elementStride == 15);

    auto* planted = registry.FindByNetworkName("CPlantedC4");
    assert(planted && planted->table && planted->table->propCount == 6);
    assert(planted->table->props[0].dataTable);
    assert(std::strcmp(planted->table->props[0].dataTable->netTableName, "DT_BaseAnimating") == 0);
    const auto* bombTicking = FindProp(planted->table, "m_bBombTicking");
    assert(bombTicking);
    assert(bombTicking->type == build4100::SendPropType::Int);
    assert(bombTicking->bits == 1);
    assert((bombTicking->flags & build4100::kSendPropUnsigned) != 0);

    const auto* basePlayerTable = registry.FindLiveTableByName("DT_BasePlayer");
    assert(basePlayerTable);
    const build4100::SendProp* viewModelArray = nullptr;
    for (int i = 0; i < basePlayerTable->propCount; ++i) {
        const auto& prop = basePlayerTable->props[i];
        if (prop.type == build4100::SendPropType::Array &&
            std::strcmp(prop.varName, "m_hViewModel") == 0) {
            viewModelArray = &prop;
            break;
        }
    }
    assert(viewModelArray && viewModelArray->elements == 2 && viewModelArray->elementStride == 4);
    assert(viewModelArray->arrayProp && viewModelArray->arrayProp->type == build4100::SendPropType::Int);
    assert(viewModelArray->arrayProp->bits == 21);

    const auto* teamTable = registry.FindLiveTableByName("DT_Team");
    assert(teamTable);
    const build4100::SendProp* teamArray = nullptr;
    for (int i = 0; i < teamTable->propCount; ++i) {
        const auto& prop = teamTable->props[i];
        if (prop.type == build4100::SendPropType::Array) teamArray = &prop;
    }
    assert(teamArray && std::strcmp(teamArray->varName, "\"player_array\"") == 0);
    assert(teamArray->elements == 64 && teamArray->elementStride == 0);
    assert(teamArray->arrayLengthProxy && teamArray->arrayLengthProxy(nullptr, 0) == 0);

    std::cout << "SENDTABLE_LIVE_GRAPH=PASS roots=177 reachable_tables=" << visited.size()
              << " reachable_props=" << reachableProps << " nested_links=" << reachableLinks
              << " fixed_arrays=" << registry.LiveFixedArrayCount()
              << " fixed_array_elements=" << registry.LiveFixedArrayElementCount()
              << " utl_vectors=" << registry.LiveUtlVectorCount()
              << " utl_capacity=" << registry.LiveUtlVectorElementCapacity()
              << " live_links=" << registry.LiveDataTableLinkCount()
              << " total_live_props=" << registry.LivePropTotal()
              << " planted_c4_bool_unsigned=1" << '\n';
}
