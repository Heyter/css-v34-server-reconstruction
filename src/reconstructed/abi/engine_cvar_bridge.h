#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "abi/engine_cvar003_client.h"

namespace cssv34::reconstructed::abi {

constexpr int kFVarGameDll = 1 << 2;
constexpr int kFVarNotify = 1 << 8;
constexpr int kFVarReplicated = 1 << 13;

class EngineConVar final {
public:
    EngineConVar(const char* name, const char* defaultValue, int flags,
                 const char* helpText = "") noexcept;
    virtual ~EngineConVar() = default; // slots 0,1
    virtual bool IsCommand() const noexcept; // 2
    virtual bool IsBitSet(int flag) const noexcept; // 3
    virtual void AddFlags(int flags) noexcept; // 4
    virtual const char* GetName() const noexcept; // 5
    virtual const char* GetHelpText() const noexcept; // 6
    virtual bool IsRegistered() const noexcept; // 7
    virtual void Create(const char* name, const char* helpText, int flags) noexcept; // 8
    virtual void Init() noexcept; // 9
    virtual void SetValue(const char* value) noexcept; // 10
    virtual void SetValue(float value) noexcept; // 11
    virtual void SetValue(int value) noexcept; // 12
    virtual void InternalSetValue(const char* value) noexcept; // 13
    virtual void InternalSetFloatValue(float value) noexcept; // 14
    virtual void InternalSetIntValue(int value) noexcept; // 15
    virtual bool ClampValue(float& value) noexcept; // 16
    virtual void ChangeStringValue(const char* value) noexcept; // 17
    virtual void Create(const char* name, const char* defaultValue, int flags,
                        const char* helpText, bool hasMin, float minValue,
                        bool hasMax, float maxValue, void* callback) noexcept; // 18

    const char* GetString() const noexcept { return stringValue_; }
    float GetFloat() const noexcept { return floatValue_; }
    int GetInt() const noexcept { return intValue_; }
    void MarkRegistered(bool value) noexcept { registered_ = value; }

private:
    void SetString(const char* value) noexcept;

    void* next_ = nullptr;                  // +0x04 i386
    bool registered_ = false;               // +0x08
    std::uint8_t padding09_[3]{};
    const char* name_ = nullptr;             // +0x0c
    const char* helpText_ = nullptr;         // +0x10
    int flags_ = 0;                          // +0x14
    EngineConVar* parent_ = this;            // +0x18
    const char* defaultValue_ = nullptr;     // +0x1c
    char* stringValue_ = nullptr;            // +0x20
    int stringLength_ = 0;                   // +0x24
    float floatValue_ = 0.0f;                // +0x28
    int intValue_ = 0;                       // +0x2c
    bool hasMin_ = false;                    // +0x30
    std::uint8_t padding31_[3]{};
    float minValue_ = 0.0f;                  // +0x34
    bool hasMax_ = false;                    // +0x38
    std::uint8_t padding39_[3]{};
    float maxValue_ = 0.0f;                  // +0x3c
    void* changeCallback_ = nullptr;         // +0x40
    char valueStorage_[64]{};
};

using EngineCommandCallback = void (*)();
using EngineCompletionCallback = int (*)(const char*, char (*)[64]);

class EngineConCommand final {
public:
    EngineConCommand(const char* name, EngineCommandCallback callback, int flags,
                     const char* helpText = "") noexcept;
    virtual ~EngineConCommand() = default; // slots 0,1
    virtual bool IsCommand() const noexcept; // 2
    virtual bool IsBitSet(int flag) const noexcept; // 3
    virtual void AddFlags(int flags) noexcept; // 4
    virtual const char* GetName() const noexcept; // 5
    virtual const char* GetHelpText() const noexcept; // 6
    virtual bool IsRegistered() const noexcept; // 7
    virtual void Create(const char* name, const char* helpText, int flags) noexcept; // 8
    virtual void Init() noexcept; // 9
    virtual int AutoCompleteSuggest(const char* partial, char commands[][64]) noexcept; // 10
    virtual bool CanAutoComplete() noexcept; // 11
    virtual void Dispatch() noexcept; // 12
    virtual void Create(const char* name, EngineCommandCallback callback,
                        const char* helpText, int flags,
                        EngineCompletionCallback completion) noexcept; // 13

    void MarkRegistered(bool value) noexcept { registered_ = value; }

private:
    void* next_ = nullptr;                  // +0x04 i386
    bool registered_ = false;               // +0x08
    std::uint8_t padding09_[3]{};
    const char* name_ = nullptr;             // +0x0c
    const char* helpText_ = nullptr;         // +0x10
    int flags_ = 0;                          // +0x14
    EngineCommandCallback callback_ = nullptr; // +0x18
    EngineCompletionCallback completion_ = nullptr; // +0x1c
    bool hasCompletion_ = false;             // +0x20
    std::uint8_t padding21_[3]{};
};

class EngineCvarBridge final {
public:
    EngineCvarBridge() noexcept;
    bool Bind(void* cvarObject) noexcept;
    void Unbind() noexcept;
    bool IsBound() const noexcept { return cvar_.IsValid(); }
    std::size_t RegisteredCount() const noexcept { return registeredCount_; }
    EngineConVar* FindLocal(const char* name) noexcept;

private:
    EngineCvar003Client cvar_;
    std::array<std::unique_ptr<EngineConVar>, 10> vars_{};
    std::unique_ptr<EngineConCommand> statusCommand_;
    std::size_t registeredCount_ = 0;
};

}  // namespace cssv34::reconstructed::abi
