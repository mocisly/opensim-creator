#include "UnitVec3.h"

#include <liboscar/Maths/GeometricFunctions.h>
#include <liboscar/Maths/Vec3.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace osc;

TEST(UnitVec3, default_constructor_fills_all_fields_with_NaNs)
{
    ASSERT_TRUE(std::isnan(UnitVec3{}[0]));
    ASSERT_TRUE(std::isnan(UnitVec3{}[1]));
    ASSERT_TRUE(std::isnan(UnitVec3{}[2]));
}

TEST(UnitVec3, is_constexpr_constructible)
{
    [[maybe_unused]] constexpr UnitVec3 v;
}

TEST(UnitVec3, unary_minus_is_constexpr)
{
    constexpr UnitVec3 v;
    [[maybe_unused]] constexpr UnitVec3 neg = -v;
}

TEST(UnitVec3, unary_plus_is_constexpr)
{
    constexpr UnitVec3 v;
    [[maybe_unused]] constexpr UnitVec3 neg = +v;
}

TEST(UnitVec3, normalizes_arguments_when_constructed_with_xyz_components)
{
    ASSERT_EQ(UnitVec3(2.0f, 0.0f, 0.0f), UnitVec3(1.0f, 0.0f, 0.0f));
    ASSERT_EQ(UnitVec3(0.0f, 3.0f, 0.0f), UnitVec3(0.0f, 1.0f, 0.0f));
}

TEST(UnitVec3, along_x_returns_a_UnitVec3_that_points_along_plus_x)
{
    static_assert(UnitVec3::along_x() == Vec3{1.0f, 0.0f, 0.0f});
}

TEST(UnitVec3, along_y_returns_a_UnitVec3_that_points_along_plus_y)
{
    static_assert(UnitVec3::along_y() == Vec3{0.0f, 1.0f, 0.0f});
}

TEST(UnitVec3, along_z_returns_a_UnitVec3_that_points_along_plus_z)
{
    static_assert(UnitVec3::along_z() == Vec3{0.0f, 0.0f, 1.0f});
}
