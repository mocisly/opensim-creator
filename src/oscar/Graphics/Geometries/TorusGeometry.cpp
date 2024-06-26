#include "TorusGeometry.h"

#include <oscar/Graphics/Mesh.h>
#include <oscar/Maths/Angle.h>
#include <oscar/Maths/TrigonometricFunctions.h>
#include <oscar/Maths/UnitVec3.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Maths/Vec3.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace osc;
using namespace osc::literals;

osc::TorusGeometry::TorusGeometry(
    float tube_center_radius,
    float tube_radius,
    size_t num_radial_segments,
    size_t num_tubular_segments,
    Radians arc)
{
    // the implementation of this was initially translated from `three.js`'s
    // `TorusGeometry`, which has excellent documentation and source code. The
    // code was then subsequently mutated to suit OSC, C++ etc.
    //
    // https://threejs.org/docs/#api/en/geometries/TorusGeometry

    const auto fnum_radial_segments = static_cast<float>(num_radial_segments);
    const auto fnum_tubular_segments = static_cast<float>(num_tubular_segments);

    std::vector<uint32_t> indices;
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;

    for (size_t j = 0; j <= num_radial_segments; ++j) {
        const auto fj = static_cast<float>(j);
        for (size_t i = 0; i <= num_tubular_segments; ++i) {
            const auto fi = static_cast<float>(i);
            const Radians u = fi/fnum_tubular_segments * arc;
            const Radians v = fj/fnum_radial_segments * 360_deg;

            const Vec3& vertex = vertices.emplace_back(
                (tube_center_radius + tube_radius * cos(v)) * cos(u),
                (tube_center_radius + tube_radius * cos(v)) * sin(u),
                tube_radius * sin(v)
            );
            normals.push_back(UnitVec3{
                vertex.x - tube_center_radius*cos(u),
                vertex.y - tube_center_radius*sin(u),
                vertex.z - 0.0f,
            });
            uvs.emplace_back(fi/fnum_tubular_segments, fj/fnum_radial_segments);
        }
    }

    for (size_t j = 1; j <= num_radial_segments; ++j) {
        for (size_t i = 1; i <= num_tubular_segments; ++i) {
            const auto a = static_cast<uint32_t>((num_tubular_segments + 1)*(j + 0) + i - 1);
            const auto b = static_cast<uint32_t>((num_tubular_segments + 1)*(j - 1) + i - 1);
            const auto c = static_cast<uint32_t>((num_tubular_segments + 1)*(j - 1) + i);
            const auto d = static_cast<uint32_t>((num_tubular_segments + 1)*(j + 0) + i);

            indices.insert(indices.end(), {a, b, d});
            indices.insert(indices.end(), {b, c, d});
        }
    }

    mesh_.set_vertices(vertices);
    mesh_.set_normals(normals);
    mesh_.set_tex_coords(uvs);
    mesh_.set_indices(indices);
}
