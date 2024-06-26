#include <oscar/Graphics/CameraProjection.h>

#include <gtest/gtest.h>
#include <oscar/Utils/EnumHelpers.h>

#include <cstddef>
#include <sstream>
#include <string>

using namespace osc;

TEST(CameraProjection, CanBeStreamed)
{
    for (size_t i = 0; i < num_options<CameraProjection>(); ++i) {
        std::stringstream ss;
        ss << static_cast<CameraProjection>(i);

        ASSERT_FALSE(ss.str().empty());
    }
}
