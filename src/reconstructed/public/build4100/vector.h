#pragma once

#include <cstddef>

namespace cssv34::build4100 {

struct Vector {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static_assert(sizeof(Vector) == 0x0c);
static_assert(offsetof(Vector, x) == 0x00);
static_assert(offsetof(Vector, y) == 0x04);
static_assert(offsetof(Vector, z) == 0x08);

}  // namespace cssv34::build4100
