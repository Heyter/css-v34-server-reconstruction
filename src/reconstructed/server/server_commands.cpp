#include "server/server_commands.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace cssv34::reconstructed {

CVarRegistry::CVarRegistry() {
    values_ = {
        {"mp_startmoney", "800"}, {"mp_maxmoney", "16000"}, {"mp_freezetime", "6"},
        {"mp_roundtime", "3"}, {"mp_buytime", "1.5"}, {"mp_c4timer", "45"},
        {"mp_friendlyfire", "0"}, {"sv_cheats", "0"}, {"bot_quota", "0"},
    };
}
bool CVarRegistry::Set(const std::string& name, const std::string& value) {
    if (name.empty()) return false;
    values_[name] = value;
    return true;
}
const std::string* CVarRegistry::Get(const std::string& name) const noexcept {
    const auto it = values_.find(name); return it == values_.end() ? nullptr : &it->second;
}
float CVarRegistry::GetFloat(const std::string& name, const float fallback) const noexcept {
    const auto* v = Get(name); return v ? std::strtof(v->c_str(), nullptr) : fallback;
}
int CVarRegistry::GetInt(const std::string& name, const int fallback) const noexcept {
    const auto* v = Get(name); return v ? std::atoi(v->c_str()) : fallback;
}
bool CVarRegistry::GetBool(const std::string& name, const bool fallback) const noexcept {
    return GetInt(name, fallback ? 1 : 0) != 0;
}

std::vector<std::string> ServerCommandProcessor::Tokenize(const std::string& line) {
    std::istringstream stream(line); std::vector<std::string> out; std::string token;
    while (stream >> token) out.push_back(token);
    return out;
}
void ServerCommandProcessor::ApplyCVars() {
    game_.SetFriendlyFire(cvars_.GetBool("mp_friendlyfire"));
    game_.ConfigureRoundTiming(cvars_.GetFloat("mp_freezetime", 6.0f),
                               cvars_.GetFloat("mp_roundtime", 3.0f) * 60.0f, 5.0f);
}
bool ServerCommandProcessor::BuyAlias(const std::size_t clientIndex, const std::string& alias, const float now) {
    build4100::CSWeaponID weapon = build4100::AliasToWeaponID(alias.c_str());
    if (weapon != build4100::CSWeaponID::None) return game_.BuyWeapon(clientIndex, weapon, now) == BuyResult::Purchased;
    if (alias == "vest") return game_.BuyEquipment(clientIndex, build4100::CSEquipment::Kevlar, now) == BuyResult::Purchased;
    if (alias == "vesthelm") return game_.BuyEquipment(clientIndex, build4100::CSEquipment::AssaultSuit, now) == BuyResult::Purchased;
    if (alias == "defuser") return game_.BuyEquipment(clientIndex, build4100::CSEquipment::Defuser, now) == BuyResult::Purchased;
    if (alias == "nvgs") return game_.BuyEquipment(clientIndex, build4100::CSEquipment::NightVision, now) == BuyResult::Purchased;
    return false;
}
CommandResult ServerCommandProcessor::ExecuteClient(const std::size_t clientIndex,
                                                     const std::string& line, const float now) {
    const auto args = Tokenize(line); if (args.empty()) return CommandResult::InvalidArguments;
    ClientRecord* player = clients_.Get(clientIndex);
    if (!player || player->state == ClientLifecycleState::Empty) return CommandResult::Rejected;
    if (args[0] == "jointeam") {
        if (args.size() != 2) return CommandResult::InvalidArguments;
        return game_.JoinTeam(clientIndex, std::atoi(args[1].c_str())) ? CommandResult::Executed : CommandResult::Rejected;
    }
    if (args[0] == "buy") {
        if (args.size() != 2) return CommandResult::InvalidArguments;
        return BuyAlias(clientIndex, args[1], now) ? CommandResult::Executed : CommandResult::Rejected;
    }
    if (args[0] == "autobuy" || args[0] == "rebuy") {
        bool changed = false;
        if (!game_.HasPrimaryWeapon(clientIndex)) changed |= BuyAlias(clientIndex, player->teamIndex == 2 ? "ak47" : "m4a1", now);
        changed |= game_.BuyAmmo(clientIndex, player->weapons.ActiveWeapon(), now) == BuyResult::Purchased;
        return changed ? CommandResult::Executed : CommandResult::Rejected;
    }
    if (args[0] == "drop") {
        return game_.DropPrimaryWeapon(clientIndex) || game_.DropSecondaryWeapon(clientIndex)
                   ? CommandResult::Executed : CommandResult::Rejected;
    }
    if (args[0] == "kill") {
        return game_.Kill(clientIndex, clientIndex) ? CommandResult::Executed : CommandResult::Rejected;
    }
    if (args[0] == "use" && args.size() == 2) {
        const auto id = build4100::ClassnameToWeaponID(args[1].c_str());
        return game_.SelectWeapon(clientIndex, id) ? CommandResult::Executed : CommandResult::Rejected;
    }
    if (args[0] == "say" || args[0] == "say_team" || args[0] == "radio1" || args[0] == "radio2" || args[0] == "radio3") {
        auto& event = events_.Emit(args[0] == "say_team" ? "player_say_team" : "player_say", now);
        event.ints["userid"] = player->userId;
        event.strings["text"] = line.substr(args[0].size());
        return CommandResult::Executed;
    }
    return CommandResult::Unknown;
}
CommandResult ServerCommandProcessor::ExecuteServer(const std::string& line, const float now) {
    (void)now;
    const auto args = Tokenize(line); if (args.empty()) return CommandResult::InvalidArguments;
    if (args[0] == "mp_restartgame") return CommandResult::Executed;
    if (args.size() == 2 && cvars_.Set(args[0], args[1])) { ApplyCVars(); return CommandResult::Executed; }
    return CommandResult::Unknown;
}

} // namespace cssv34::reconstructed
