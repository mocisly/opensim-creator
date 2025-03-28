#pragma once

#include <liboscar/Graphics/Color.h>
#include <liboscar/Graphics/Cubemap.h>
#include <liboscar/Graphics/RenderTexture.h>
#include <liboscar/Graphics/SharedColorRenderBuffer.h>
#include <liboscar/Graphics/SharedDepthStencilRenderBuffer.h>
#include <liboscar/Graphics/Texture2D.h>
#include <liboscar/Maths/Mat3.h>
#include <liboscar/Maths/Mat4.h>
#include <liboscar/Maths/Vec2.h>
#include <liboscar/Maths/Vec3.h>
#include <liboscar/Maths/Vec4.h>
#include <liboscar/Utils/Typelist.h>

namespace osc::detail
{
    // A typelist of all "base" types supported by the graphics backend.
    using MaterialValueBaseTypes = Typelist<
        bool,
        Color,
        Cubemap,
        float,
        int,
        Mat3,
        Mat4,
        RenderTexture,
        SharedColorRenderBuffer,
        SharedDepthStencilRenderBuffer,
        Texture2D,
        Vec2,
        Vec3,
        Vec4
    >;
}
