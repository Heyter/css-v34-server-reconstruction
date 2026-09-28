#!/usr/bin/env python3
import argparse
import json
import re
import subprocess
from pathlib import Path


def run(*args):
    return subprocess.check_output(args, text=True, errors="replace")


def interfaces(path):
    text = run("strings", "-a", str(path))
    pat = re.compile(r"^(?:(?:ServerGameDLL|ServerGameClients|ServerGameEnts|PlayerInfoManager|GameMovement)\d{3}|HLTVDirector001|VServerDllSharedAppSystems001)$")
    return sorted({line.strip() for line in text.splitlines() if pat.match(line.strip())})


def dyn_exports(path):
    out = run("nm", "-D", "--defined-only", str(path))
    vals = []
    for line in out.splitlines():
        parts = line.split()
        if parts:
            vals.append(parts[-1])
    return sorted(set(vals))


def symbols(path, needles):
    out = run("nm", "-C", str(path))
    return {needle: any(needle in line for line in out.splitlines()) for needle in needles}


def symbol_addresses(path, names):
    out = run("nm", "-C", str(path))
    result = {}
    for line in out.splitlines():
        parts = line.split(maxsplit=2)
        if len(parts) != 3:
            continue
        addr, _, name = parts
        for wanted in names:
            if name == wanted:
                result[wanted] = int(addr, 16)
    return result


def vtable_sizes(path):
    out = run("readelf", "-Ws", "--demangle", str(path))
    result = {}
    for line in out.splitlines():
        match = re.search(r"\b(\d+)\s+OBJECT\s+\S+\s+\S+\s+\S+\s+(vtable for .+)$", line)
        if match:
            result.setdefault(match.group(2), int(match.group(1)))
    return result


def relocations(path):
    out = run("readelf", "-rW", "--demangle", str(path))
    result = {}
    for line in out.splitlines():
        match = re.match(
            r"^([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+R_386_32\s+[0-9a-fA-F]+\s+(.+)$",
            line.strip(),
        )
        if match:
            result[int(match.group(1), 16)] = match.group(2).strip()
    return result


def elf(path):
    return run("file", str(path)).strip()


def slot_map_ok(relocs, base, expected):
    actual = []
    mismatches = []
    for slot, role in enumerate(expected):
        offset = base + slot * 4
        symbol = relocs.get(offset, "")
        ok = role in symbol
        actual.append({"slot": slot, "offset": f"0x{offset:08x}", "expected": role, "actual": symbol, "ok": ok})
        if not ok:
            mismatches.append(slot)
    return actual, mismatches


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--original", required=True)
    ap.add_argument("--rebuilt", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    original = Path(args.original)
    rebuilt = Path(args.rebuilt)

    needles = [
        "vtable for IPlayerInfo",
        "CPlayerInfo::GetName()",
        "CServerGameClients::ProcessUsercmds",
        "CCSGameRules::RestartRound",
        "ServerClass::ServerClass(char*, SendTable*)",
        "SendTable::SendTable(SendProp*, int, char*)",
        "g_pServerClassHead",
    ]
    address_names = [
        "AliasToWeaponID(char const*)",
        "WeaponIDToAlias(int)",
        "CServerGameDLL::DLLInit(void* (*)(char const*, int*), void* (*)(char const*, int*), void* (*)(char const*, int*), CGlobalVars*)",
        "ServerClass::ServerClass(char*, SendTable*)",
        "SendTable::SendTable(SendProp*, int, char*)",
        "g_pServerClassHead",
    ]

    ov = vtable_sizes(original)
    rv = vtable_sizes(rebuilt)
    original_vtables = {
        "IServerGameDLL": ov.get("vtable for IServerGameDLL"),
        "IServerGameClients": ov.get("vtable for IServerGameClients"),
        "IServerGameEnts": ov.get("vtable for IServerGameEnts"),
        "IPlayerInfo": ov.get("vtable for IPlayerInfo"),
        "IPlayerInfoManager": ov.get("vtable for IPlayerInfoManager"),
        "IGameMovement": ov.get("vtable for IGameMovement"),
    }
    rebuilt_vtables = {
        "IServerGameDLL": rv.get("vtable for cssv34::reconstructed::abi::ServerGameDll006Adapter"),
        "IServerGameClients": rv.get("vtable for cssv34::reconstructed::abi::ServerGameClients004Adapter"),
        "IServerGameEnts": rv.get("vtable for cssv34::reconstructed::abi::ServerGameEnts001Adapter"),
        "IPlayerInfo": rv.get("vtable for cssv34::reconstructed::abi::PlayerInfoAdapter"),
        "IPlayerInfoManager": rv.get("vtable for cssv34::reconstructed::abi::PlayerInfoManager002Adapter"),
        "IGameMovement": rv.get("vtable for cssv34::reconstructed::abi::GameMovement001Adapter"),
    }
    expected_sizes = {
        "IServerGameDLL": 140,
        "IServerGameClients": 72,
        "IServerGameEnts": 40,
        "IPlayerInfo": 104,
        "IPlayerInfoManager": 16,
        "IGameMovement": 32,
    }

    # These are address-point entries from the original build4100 derived vtables.
    # Base = first callable vtable entry, i.e. vtable symbol + 8 on i386 Itanium ABI.
    servergamedll_roles = [
        "CServerGameDLL::DLLInit(",
        "CServerGameDLL::GameInit()",
        "CServerGameDLL::LevelInit(",
        "CServerGameDLL::ServerActivate(",
        "CServerGameDLL::GameFrame(bool)",
        "CServerGameDLL::PreClientUpdate(bool)",
        "CServerGameDLL::LevelShutdown()",
        "CServerGameDLL::GameShutdown()",
        "CServerGameDLL::DLLShutdown()",
        "CServerGameDLL::GetTickInterval() const",
        "CServerGameDLL::GetAllServerClasses()",
        "CServerGameDLL::GetGameDescription()",
        "CServerGameDLL::CreateNetworkStringTables()",
        "CServerGameDLL::SaveInit(int)",
        "CServerGameDLL::SaveWriteFields(",
        "CServerGameDLL::SaveReadFields(",
        "CServerGameDLL::SaveGlobalState(",
        "CServerGameDLL::RestoreGlobalState(",
        "CServerGameDLL::PreSave(",
        "CServerGameDLL::Save(",
        "CServerGameDLL::GetSaveComment(char*, int)",
        "CServerGameDLL::WriteSaveHeaders(",
        "CServerGameDLL::ReadRestoreHeaders(",
        "CServerGameDLL::Restore(",
        "CServerGameDLL::IsRestoring()",
        "CServerGameDLL::CreateEntityTransitionList(",
        "CServerGameDLL::BuildAdjacentMapList()",
        "CServerGameDLL::GetUserMessageInfo(",
        "CServerGameDLL::GetStandardSendProxies()",
        "CServerGameDLL::PostInit()",
        "CServerGameDLL::Think(bool)",
        "CServerGameDLL::GetSaveCommentEx(",
        "CServerGameDLL::OnQueryCvarValueFinished(",
    ]
    servergameclients_roles = [
        "CServerGameClients::GetPlayerLimits(",
        "CServerGameClients::ClientConnect(",
        "CServerGameClients::ClientActive(",
        "CServerGameClients::ClientDisconnect(",
        "CServerGameClients::ClientPutInServer(",
        "CServerGameClients::ClientCommand(",
        "CServerGameClients::SetCommandClient(int)",
        "CServerGameClients::ClientSettingsChanged(",
        "CServerGameClients::ClientSetupVisibility(",
        "CServerGameClients::ProcessUsercmds(",
        "CServerGameClients::PostClientMessagesSent()",
        "CServerGameClients::GetPlayerState(",
        "CServerGameClients::ClientEarPosition(",
        "CServerGameClients::GetReplayDelay(",
        "CServerGameClients::GetBugReportInfo(",
        "CServerGameClients::NetworkIDValidated(",
    ]
    servergameents_roles = [
        "CServerGameEnts::~CServerGameEnts()",
        "CServerGameEnts::~CServerGameEnts()",
        "CServerGameEnts::SetDebugEdictBase(",
        "CServerGameEnts::MarkEntitiesAsTouching(",
        "CServerGameEnts::FreeContainingEntity(",
        "CServerGameEnts::BaseEntityToEdict(",
        "CServerGameEnts::EdictToBaseEntity(",
        "CServerGameEnts::CheckTransmit(",
    ]
    gamemovement_roles = [
        "CGameMovement::~CGameMovement()",
        "CGameMovement::~CGameMovement()",
        "CGameMovement::ProcessMovement(",
        "CGameMovement::GetPlayerMins(bool) const",
        "CGameMovement::GetPlayerMaxs(bool) const",
        "CGameMovement::GetPlayerViewOffset(bool) const",
    ]

    relocs = relocations(original)
    servergamedll_slots, servergamedll_bad = slot_map_ok(relocs, 0x00AF2B88, servergamedll_roles)
    servergameclients_slots, servergameclients_bad = slot_map_ok(relocs, 0x00AF2CC8, servergameclients_roles)
    servergameents_slots, servergameents_bad = slot_map_ok(relocs, 0x00AF2C68, servergameents_roles)
    gamemovement_slots, gamemovement_bad = slot_map_ok(relocs, 0x00B3DFE8, gamemovement_roles)

    rebuilt_contract_needles = [
        "ServerGameDll006Adapter::GetTickInterval() const",
        "ServerGameDll006Adapter::GetAllServerClasses()",
        "ServerGameDll006Adapter::SaveInit(int)",
        "ServerGameDll006Adapter::GetUserMessageInfo(",
        "ServerGameDll006Adapter::GetStandardSendProxies()",
        "ServerGameDll006Adapter::OnQueryCvarValueFinished(",
        "GameMovement001Adapter::ProcessMovement(",
        "GameMovement001Adapter::GetPlayerMins(bool) const",
        "GameMovement001Adapter::GetPlayerMaxs(bool) const",
        "GameMovement001Adapter::GetPlayerViewOffset(bool) const",
        "ServerClassRegistry::ServerClassRegistry()",
        "EngineCvarBridge::Bind(void*)",
        "EngineEntityBridge::ServerActivate(",
    ]
    rebuilt_contract_symbols = symbols(rebuilt, rebuilt_contract_needles)

    data = {
        "original": {
            "path": str(original),
            "file": elf(original),
            "interfaces": interfaces(original),
            "exports": dyn_exports(original),
            "symbols": symbols(original, needles),
            "symbol_addresses": symbol_addresses(original, address_names),
            "vtable_sizes": original_vtables,
            "slot_contracts": {
                "CServerGameDLL_0_32": servergamedll_slots,
                "CServerGameClients_0_15": servergameclients_slots,
                "CServerGameEnts_0_7": servergameents_slots,
                "CGameMovement_0_5": gamemovement_slots,
            },
        },
        "rebuilt": {
            "path": str(rebuilt),
            "file": elf(rebuilt),
            "interfaces": interfaces(rebuilt),
            "exports": dyn_exports(rebuilt),
            "vtable_sizes": rebuilt_vtables,
            "contract_symbols": rebuilt_contract_symbols,
        },
        "expected_vtable_sizes_i386": expected_sizes,
    }

    expected_interfaces = [
        "GameMovement001",
        "HLTVDirector001",
        "PlayerInfoManager001",
        "PlayerInfoManager002",
        "ServerGameClients003",
        "ServerGameClients004",
        "ServerGameDLL006",
        "ServerGameEnts001",
        "VServerDllSharedAppSystems001",
    ]
    data["checks"] = {
        "original_factory_surface": data["original"]["interfaces"] == expected_interfaces,
        "rebuilt_factory_strings": all(x in data["rebuilt"]["interfaces"] for x in expected_interfaces),
        "rebuilt_export_surface": data["rebuilt"]["exports"] == ["CreateInterface"],
        "elf32_both": "ELF 32-bit" in data["original"]["file"] and "ELF 32-bit" in data["rebuilt"]["file"],
        "original_debug_symbols": all(data["original"]["symbols"].values()),
        "original_vtable_geometry": original_vtables == expected_sizes,
        "rebuilt_vtable_geometry": rebuilt_vtables == expected_sizes,
        "original_servergamedll_slot_contract": not servergamedll_bad,
        "original_servergameclients_slot_contract": not servergameclients_bad,
        "original_servergameents_slot_contract": not servergameents_bad,
        "original_gamemovement_slot_contract": not gamemovement_bad,
        "rebuilt_named_slot_contract": all(rebuilt_contract_symbols.values()),
        "weapon_alias_offsets": (
            data["original"]["symbol_addresses"].get("AliasToWeaponID(char const*)") == 0x0095BDA0
            and data["original"]["symbol_addresses"].get("WeaponIDToAlias(int)") == 0x0095BE60
        ),
        "serverclass_sendtable_offsets": (
            data["original"]["symbol_addresses"].get("ServerClass::ServerClass(char*, SendTable*)") == 0x00791BF0
            and data["original"]["symbol_addresses"].get("SendTable::SendTable(SendProp*, int, char*)") in (0x009A68E0, 0x009A6F70)
            and data["original"]["symbol_addresses"].get("g_pServerClassHead") == 0x00CBAFD8
        ),
    }

    Path(args.out).write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    failed = [key for key, value in data["checks"].items() if not value]
    if failed:
        print("ABI_INVENTORY=FAIL " + ",".join(failed))
        if servergamedll_bad:
            print("SERVERGAMEDLL_SLOT_MISMATCH=" + ",".join(map(str, servergamedll_bad)))
        if servergameclients_bad:
            print("SERVERGAMECLIENTS_SLOT_MISMATCH=" + ",".join(map(str, servergameclients_bad)))
        if servergameents_bad:
            print("SERVERGAMEENTS_SLOT_MISMATCH=" + ",".join(map(str, servergameents_bad)))
        if gamemovement_bad:
            print("GAMEMOVEMENT_SLOT_MISMATCH=" + ",".join(map(str, gamemovement_bad)))
        raise SystemExit(1)

    print("ABI_INVENTORY=PASS interfaces=9 vtables=6 slot_contracts=ServerGameDLL0..32,ServerGameClients0..15,ServerGameEnts0..7,GameMovement0..5 serverclass_sendtable=0x14/0x14")


if __name__ == "__main__":
    main()
