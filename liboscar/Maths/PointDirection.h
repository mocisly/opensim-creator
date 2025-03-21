#pragma once

#include <liboscar/Maths/Vec3.h>

namespace osc
{
    struct PointDirection final {
        Vec3 point{};
        Vec3 direction = {0.0f, 1.0f, 0.0f};
    };
}
