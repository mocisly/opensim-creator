#include "SharedDepthStencilRenderBuffer.h"

#include <liboscar/Graphics/ColorRenderBufferParams.h>
#include <liboscar/Graphics/DepthStencilRenderBufferParams.h>

#include <gtest/gtest.h>

using namespace osc;

TEST(SharedDepthStencilRenderBuffer, can_default_construct)
{
    [[maybe_unused]] const SharedDepthStencilRenderBuffer default_constructed;
}

TEST(SharedDepthStencilRenderBuffer, default_constructed_as_1x1_pixel_dimensions)
{
    ASSERT_EQ(SharedDepthStencilRenderBuffer{}.pixel_dimensions(), Vec2i(1, 1));
}

TEST(SharedDepthStencilRenderBuffer, default_constructed_with_1x_anti_aliasing)
{
    ASSERT_EQ(SharedDepthStencilRenderBuffer{}.anti_aliasing_level(), AntiAliasingLevel{1});
}

TEST(SharedDepthStencilRenderBuffer, default_constructed_has_Default_format)
{
    ASSERT_EQ(SharedDepthStencilRenderBuffer{}.format(), DepthStencilRenderBufferFormat::Default);
}

TEST(SharedDepthStencilRenderBuffer, can_construct_depth_buffer)
{
    [[maybe_unused]] const SharedDepthStencilRenderBuffer depth_buffer{DepthStencilRenderBufferParams{}};
}

TEST(SharedDepthStencilRenderBuffer, dimensionality_defaults_to_Tex2D)
{
    ASSERT_EQ(SharedDepthStencilRenderBuffer{}.dimensionality(), TextureDimensionality::Tex2D);
}

TEST(SharedDepthStencilRenderBuffer, dimensionality_is_based_on_parameters)
{
    const SharedDepthStencilRenderBuffer buffer{DepthStencilRenderBufferParams{.dimensionality = TextureDimensionality::Cube}};
    ASSERT_EQ(buffer.dimensionality(), TextureDimensionality::Cube);
}

TEST(SharedDepthStencilRenderBuffer, pixel_dimensions_is_based_on_parameters)
{
    const SharedDepthStencilRenderBuffer buffer{DepthStencilRenderBufferParams{.pixel_dimensions = Vec2i(3, 5)}};
    ASSERT_EQ(buffer.pixel_dimensions(), Vec2i(3,5));
}

TEST(SharedDepthStencilRenderBuffer, anti_aliasing_level_is_based_on_parameters)
{
    const SharedDepthStencilRenderBuffer buffer{DepthStencilRenderBufferParams{
        .anti_aliasing_level = AntiAliasingLevel{4}
    }};
    ASSERT_EQ(buffer.anti_aliasing_level(), AntiAliasingLevel{4});
}

TEST(SharedDepthStencilRenderBuffer, format_is_based_on_parameters)
{
    const SharedDepthStencilRenderBuffer buffer{DepthStencilRenderBufferParams{
        .format = DepthStencilRenderBufferFormat::D32_SFloat
    }};
    ASSERT_EQ(buffer.format(), DepthStencilRenderBufferFormat::D32_SFloat);
}
