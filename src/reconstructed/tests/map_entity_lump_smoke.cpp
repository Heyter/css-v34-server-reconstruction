#include <cassert>
#include <iostream>

#include "server/map_entity_lump.h"

using namespace cssv34::reconstructed;

int main() {
    static constexpr char lump[] =
        "{\n\"classname\" \"worldspawn\"\n\"skyname\" \"sky_day01_01\"\n}\n"
        "{\n\"classname\" \"info_player_terrorist\"\n\"origin\" \"0 0 64\"\n}\n"
        "{\n\"classname\" \"info_player_counterterrorist\"\n\"origin\" \"128 0 64\"\n}\n"
        "{\n\"classname\" \"func_bomb_target\"\n}\n";
    MapEntityLump parsed;
    assert(parsed.Parse(lump));
    assert(parsed.Count() == 4);
    assert(parsed.HasWorldspawn());
    assert(parsed.CountClass("info_player_terrorist") == 1);
    assert(parsed.CountClass("info_player_counterterrorist") == 1);
    assert(parsed.CountClass("func_bomb_target") == 1);
    const auto* sky = parsed.Entities()[0].Find("skyname");
    assert(sky && *sky == "sky_day01_01");
    MapEntityLump bad;
    assert(!bad.Parse("{ \"classname\" \"worldspawn\""));
    std::cout << "MAP_ENTITY_LUMP=PASS entities=4\n";
}
