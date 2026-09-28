#pragma once

#include "abi/vtable_client.h"

namespace cssv34::reconstructed::abi {

class NetworkStringTableContainer001Client final {
public:
    NetworkStringTableContainer001Client() noexcept = default;
    explicit NetworkStringTableContainer001Client(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }

    void* CreateStringTable(
        const char* tableName,
        int maxEntries,
        int userDataFixedSize = 0,
        int userDataNetworkBits = 0
    ) const noexcept;

private:
    void* object_ = nullptr;
};

class NetworkStringTableClient final {
public:
    NetworkStringTableClient() noexcept = default;
    explicit NetworkStringTableClient(void* object) noexcept : object_(object) {}

    void Reset(void* object = nullptr) noexcept { object_ = object; }
    bool IsValid() const noexcept { return detail::HasVTable(object_); }

    int AddString(
        const char* value,
        int length = -1,
        const void* userData = nullptr
    ) const noexcept;

private:
    void* object_ = nullptr;
};

}  // namespace cssv34::reconstructed::abi
