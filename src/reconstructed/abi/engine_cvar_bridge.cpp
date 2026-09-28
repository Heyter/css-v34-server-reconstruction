#include "abi/engine_cvar_bridge.h"

#include "abi/engine_probe.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cssv34::reconstructed::abi {
namespace {

const char* Safe(const char* value) noexcept { return value ? value : ""; }

void StatusCommand() {
    EngineProbe("ConCommand_DISPATCH", "v34_rebuild_status");
}

}  // namespace

EngineConVar::EngineConVar(const char* const name, const char* const defaultValue,
                           const int flags, const char* const helpText) noexcept
    : name_(Safe(name)), helpText_(Safe(helpText)), flags_(flags),
      parent_(this), defaultValue_(Safe(defaultValue)) {
    SetString(defaultValue_);
}

bool EngineConVar::IsCommand() const noexcept { return false; }
bool EngineConVar::IsBitSet(const int flag) const noexcept { return (flags_ & flag) != 0; }
void EngineConVar::AddFlags(const int flags) noexcept { flags_ |= flags; }
const char* EngineConVar::GetName() const noexcept { return name_; }
const char* EngineConVar::GetHelpText() const noexcept { return helpText_; }
bool EngineConVar::IsRegistered() const noexcept { return registered_; }
void EngineConVar::Create(const char* const name, const char* const helpText, const int flags) noexcept {
    name_ = Safe(name); helpText_ = Safe(helpText); flags_ = flags;
}
void EngineConVar::Init() noexcept { registered_ = true; }
void EngineConVar::SetValue(const char* const value) noexcept { SetString(value); }
void EngineConVar::SetValue(const float value) noexcept {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value));
    SetString(buffer);
}
void EngineConVar::SetValue(const int value) noexcept {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%d", value);
    SetString(buffer);
}
void EngineConVar::InternalSetValue(const char* const value) noexcept { SetString(value); }
void EngineConVar::InternalSetFloatValue(float value) noexcept {
    (void)ClampValue(value);
    SetValue(value);
}
void EngineConVar::InternalSetIntValue(const int value) noexcept { SetValue(value); }
bool EngineConVar::ClampValue(float& value) noexcept {
    const float original = value;
    if (hasMin_) value = std::max(value, minValue_);
    if (hasMax_) value = std::min(value, maxValue_);
    return value != original;
}
void EngineConVar::ChangeStringValue(const char* const value) noexcept { SetString(value); }
void EngineConVar::Create(const char* const name, const char* const defaultValue, const int flags,
                          const char* const helpText, const bool hasMin, const float minValue,
                          const bool hasMax, const float maxValue, void* const callback) noexcept {
    name_ = Safe(name);
    defaultValue_ = Safe(defaultValue);
    helpText_ = Safe(helpText);
    flags_ = flags;
    hasMin_ = hasMin;
    minValue_ = minValue;
    hasMax_ = hasMax;
    maxValue_ = maxValue;
    changeCallback_ = callback;
    SetString(defaultValue_);
}
void EngineConVar::SetString(const char* const value) noexcept {
    const char* const safe = Safe(value);
    std::strncpy(valueStorage_, safe, sizeof(valueStorage_) - 1);
    valueStorage_[sizeof(valueStorage_) - 1] = '\0';
    stringValue_ = valueStorage_;
    stringLength_ = static_cast<int>(std::strlen(valueStorage_) + 1);
    floatValue_ = std::strtof(valueStorage_, nullptr);
    intValue_ = std::atoi(valueStorage_);
}

EngineConCommand::EngineConCommand(const char* const name, const EngineCommandCallback callback,
                                   const int flags, const char* const helpText) noexcept
    : name_(Safe(name)), helpText_(Safe(helpText)), flags_(flags), callback_(callback) {}
bool EngineConCommand::IsCommand() const noexcept { return true; }
bool EngineConCommand::IsBitSet(const int flag) const noexcept { return (flags_ & flag) != 0; }
void EngineConCommand::AddFlags(const int flags) noexcept { flags_ |= flags; }
const char* EngineConCommand::GetName() const noexcept { return name_; }
const char* EngineConCommand::GetHelpText() const noexcept { return helpText_; }
bool EngineConCommand::IsRegistered() const noexcept { return registered_; }
void EngineConCommand::Create(const char* const name, const char* const helpText, const int flags) noexcept {
    name_ = Safe(name); helpText_ = Safe(helpText); flags_ = flags;
}
void EngineConCommand::Init() noexcept { registered_ = true; }
int EngineConCommand::AutoCompleteSuggest(const char* const partial, char commands[][64]) noexcept {
    (void)partial; (void)commands; return 0;
}
bool EngineConCommand::CanAutoComplete() noexcept { return hasCompletion_ && completion_; }
void EngineConCommand::Dispatch() noexcept { if (callback_) callback_(); }
void EngineConCommand::Create(const char* const name, const EngineCommandCallback callback,
                              const char* const helpText, const int flags,
                              const EngineCompletionCallback completion) noexcept {
    name_ = Safe(name); callback_ = callback; helpText_ = Safe(helpText); flags_ = flags;
    completion_ = completion; hasCompletion_ = completion != nullptr;
}

EngineCvarBridge::EngineCvarBridge() noexcept {
    const int flags = kFVarGameDll | kFVarNotify | kFVarReplicated;
    vars_[0] = std::make_unique<EngineConVar>("mp_startmoney", "800", flags);
    vars_[1] = std::make_unique<EngineConVar>("mp_maxmoney", "16000", flags);
    vars_[2] = std::make_unique<EngineConVar>("mp_freezetime", "6", flags);
    vars_[3] = std::make_unique<EngineConVar>("mp_roundtime", "3", flags);
    vars_[4] = std::make_unique<EngineConVar>("mp_buytime", "1.5", flags);
    vars_[5] = std::make_unique<EngineConVar>("mp_c4timer", "45", flags);
    vars_[6] = std::make_unique<EngineConVar>("mp_friendlyfire", "0", flags);
    vars_[7] = std::make_unique<EngineConVar>("bot_quota", "0", kFVarGameDll | kFVarNotify);
    // Stock CSS build4100 exposes both variables before server.cfg runs.  The
    // release parity oracle reads them through RCON and the stock server.cfg
    // overwrites mp_limitteams to 2 after the launch command supplied 0.
    vars_[8] = std::make_unique<EngineConVar>("mp_autoteambalance", "1", flags);
    vars_[9] = std::make_unique<EngineConVar>("mp_limitteams", "2", flags);
    statusCommand_ = std::make_unique<EngineConCommand>(
        "v34_rebuild_status", &StatusCommand, kFVarGameDll,
        "Print reconstruction bridge status");
}

bool EngineCvarBridge::Bind(void* const cvarObject) noexcept {
    Unbind();
    cvar_.Reset(cvarObject);
    if (!cvar_.IsValid()) return false;
    registeredCount_ = 0;
    for (auto& var : vars_) {
        if (!var) continue;
        if (cvar_.FindVar(var->GetName())) continue;
        cvar_.RegisterConCommandBase(var.get());
        var->MarkRegistered(true);
        ++registeredCount_;
    }
    if (statusCommand_ && !cvar_.FindVar(statusCommand_->GetName())) {
        cvar_.RegisterConCommandBase(statusCommand_.get());
        statusCommand_->MarkRegistered(true);
        ++registeredCount_;
    }
    return registeredCount_ > 0;
}

void EngineCvarBridge::Unbind() noexcept {
    if (cvar_.IsValid() && registeredCount_ > 0) cvar_.UnlinkVariables(kFVarGameDll);
    for (auto& var : vars_) if (var) var->MarkRegistered(false);
    if (statusCommand_) statusCommand_->MarkRegistered(false);
    registeredCount_ = 0;
    cvar_.Reset();
}

EngineConVar* EngineCvarBridge::FindLocal(const char* const name) noexcept {
    if (!name) return nullptr;
    for (auto& var : vars_) {
        if (var && std::strcmp(var->GetName(), name) == 0) return var.get();
    }
    return nullptr;
}

}  // namespace cssv34::reconstructed::abi
