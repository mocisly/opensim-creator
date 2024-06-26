#pragma once

#include <iosfwd>

namespace osc
{
    // how texels should be sampled when a texture coordinate falls outside
    // the texture's bounds
    enum class TextureWrapMode {
        Repeat,
        Clamp,
        Mirror,
        NUM_OPTIONS,
    };

    std::ostream& operator<<(std::ostream&, TextureWrapMode);
}
