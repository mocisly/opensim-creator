#include <oscar/Graphics/Unorm8.h>

#include <gtest/gtest.h>
#include <oscar/Maths/Vec.h>
#include <oscar/Maths/Vec3.h>

#include <limits>

using namespace osc;

TEST(Unorm8, ComparisonBetweenBytesWorksAsExpected)
{
    static_assert(Unorm8{static_cast<std::byte>(0xfa)} == Unorm8{static_cast<std::byte>(0xfa)});
}

TEST(Unorm8, ComparisonBetweenConvertedFloatsWorksAsExpected)
{
    static_assert(Unorm8{0.5f} == Unorm8{0.5f});
}

TEST(Unorm8, NaNsAreConvertedToZero)
{
    static_assert(Unorm8(std::numeric_limits<float>::quiet_NaN()) == Unorm8{0.0f});
}

TEST(Unorm8, CanCreateVec3OfUnormsFromUsualVec3OfFloats)
{
    const Vec3 v{0.25f, 1.0f, 1.5f};
    const Vec<3, Unorm8> converted{v};
    const Vec<3, Unorm8> expected{Unorm8{0.25f}, Unorm8{1.0f}, Unorm8{1.5f}};
    ASSERT_EQ(converted, expected);
}

TEST(Unorm8, CanCreateUsualVec3FromVec3OfUNorms)
{
    const Vec<3, Unorm8> v{Unorm8{0.1f}, Unorm8{0.2f}, Unorm8{0.3f}};
    const Vec3 converted{v};
    const Vec3 expected{Unorm8{0.1f}.normalized_value(), Unorm8{0.2f}.normalized_value(), Unorm8{0.3f}.normalized_value()};
    ASSERT_EQ(converted, expected);
}

TEST(Unorm8, ConvertsAsExpected)
{
    ASSERT_EQ(Unorm8{0.5f}, Unorm8{static_cast<std::byte>(127)});
}
