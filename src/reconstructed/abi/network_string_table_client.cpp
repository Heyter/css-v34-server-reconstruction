#include "abi/network_string_table_client.h"

namespace cssv34::reconstructed::abi {

void* NetworkStringTableContainer001Client::CreateStringTable(
    const char* const tableName,
    const int maxEntries,
    const int userDataFixedSize,
    const int userDataNetworkBits
) const noexcept {
    using Fn = void* (*)(void*, const char*, int, int, int);
    const Fn fn = detail::VTableSlot<Fn>(object_, 2);
    return fn ? fn(object_, tableName, maxEntries, userDataFixedSize, userDataNetworkBits) : nullptr;
}

int NetworkStringTableClient::AddString(
    const char* const value,
    const int length,
    const void* const userData
) const noexcept {
    using Fn = int (*)(void*, const char*, int, const void*);
    const Fn fn = detail::VTableSlot<Fn>(object_, 9);
    return fn ? fn(object_, value, length, userData) : -1;
}

}  // namespace cssv34::reconstructed::abi
