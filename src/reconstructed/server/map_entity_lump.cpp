#include "server/map_entity_lump.h"

#include <cctype>
#include <cstring>
#include <utility>

namespace cssv34::reconstructed {
namespace {

void SkipWs(const char*& p) noexcept { while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p; }

bool ReadQuoted(const char*& p, std::string& out) {
    SkipWs(p);
    if (*p != '"') return false;
    ++p; out.clear();
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            ++p;
            if (*p == 'n') out.push_back('\n');
            else out.push_back(*p);
            ++p;
            continue;
        }
        out.push_back(*p++);
    }
    if (*p != '"') return false;
    ++p; return true;
}

} // namespace

const std::string* MapEntityRecord::Find(const char* const key) const noexcept {
    if (!key) return nullptr;
    const auto it = keyValues.find(key);
    return it == keyValues.end() ? nullptr : &it->second;
}

bool MapEntityLump::Parse(const char* text) {
    entities_.clear();
    if (!text) return true;
    const char* p = text;
    while (true) {
        SkipWs(p);
        if (!*p) return true;
        if (*p != '{') { entities_.clear(); return false; }
        ++p;
        MapEntityRecord record;
        while (true) {
            SkipWs(p);
            if (!*p) { entities_.clear(); return false; }
            if (*p == '}') { ++p; break; }
            std::string key, value;
            if (!ReadQuoted(p, key) || !ReadQuoted(p, value)) { entities_.clear(); return false; }
            record.keyValues[std::move(key)] = std::move(value);
        }
        entities_.push_back(std::move(record));
    }
}

std::size_t MapEntityLump::CountClass(const char* const classname) const noexcept {
    if (!classname) return 0;
    std::size_t count = 0;
    for (const auto& entity : entities_) {
        const auto* value = entity.Classname();
        if (value && *value == classname) ++count;
    }
    return count;
}

bool MapEntityLump::HasWorldspawn() const noexcept { return CountClass("worldspawn") == 1; }

} // namespace cssv34::reconstructed
