#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace cssv34::reconstructed {

struct MapEntityRecord {
    std::unordered_map<std::string, std::string> keyValues;
    const std::string* Find(const char* key) const noexcept;
    const std::string* Classname() const noexcept { return Find("classname"); }
};

class MapEntityLump final {
public:
    bool Parse(const char* text);
    const std::vector<MapEntityRecord>& Entities() const noexcept { return entities_; }
    std::size_t Count() const noexcept { return entities_.size(); }
    std::size_t CountClass(const char* classname) const noexcept;
    bool HasWorldspawn() const noexcept;

private:
    std::vector<MapEntityRecord> entities_;
};

} // namespace cssv34::reconstructed
