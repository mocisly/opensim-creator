#include "LOGLShadowMappingTab.h"

#include <liboscar/oscar.h>

#include <memory>
#include <optional>

using namespace osc::literals;
using namespace osc;

namespace
{
    // this matches the plane vertices used in the LearnOpenGL tutorial
    Mesh generate_learnopengl_plane_mesh()
    {
        Mesh mesh;
        mesh.set_vertices({
            { 25.0f, -0.5f,  25.0f},
            {-25.0f, -0.5f,  25.0f},
            {-25.0f, -0.5f, -25.0f},

            { 25.0f, -0.5f,  25.0f},
            {-25.0f, -0.5f, -25.0f},
            { 25.0f, -0.5f, -25.0f},
        });
        mesh.set_normals({
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},

            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
        });
        mesh.set_tex_coords({
            {25.0f,  0.0f},
            {0.0f,  0.0f},
            {0.0f, 25.0f},

            {25.0f,  0.0f},
            {0.0f, 25.0f},
            {25.0f, 25.0f},
        });
        mesh.set_indices({0, 1, 2, 3, 4, 5});
        return mesh;
    }

    MouseCapturingCamera create_camera()
    {
        MouseCapturingCamera camera;
        camera.set_position({-2.0f, 1.0f, 0.0f});
        camera.set_clipping_planes({0.1f, 100.0f});
        return camera;
    }

    RenderTexture create_depth_texture()
    {
        return RenderTexture{{
            .pixel_dimensions = {1024, 1024},
            .color_format = ColorRenderBufferFormat::R8G8B8A8_UNORM,  // linear depth values
        }};
    }
}

class osc::LOGLShadowMappingTab::Impl final : public TabPrivate {
public:
    static CStringView static_label() { return "oscar_demos/learnopengl/AdvancedLighting/ShadowMapping"; }

    explicit Impl(LOGLShadowMappingTab& owner, Widget* parent) :
        TabPrivate{owner, parent, static_label()}
    {}

    void on_mount()
    {
        App::upd().make_main_loop_polling();
        camera_.on_mount();
    }

    void on_unmount()
    {
        camera_.on_unmount();
        App::upd().make_main_loop_waiting();
    }

    bool on_event(Event& e)
    {
        return camera_.on_event(e);
    }

    void on_draw()
    {
        camera_.on_draw();
        draw_3d_scene();
    }

private:
    void draw_3d_scene()
    {
        const Rect workspace_screen_space_rect = ui::get_main_window_workspace_screen_space_rect();
        const Vec2 workspace_screen_space_top_left = workspace_screen_space_rect.ypu_top_left();
        constexpr float depth_overlay_size = 200.0f;

        render_shadows_to_depth_texture();

        camera_.set_background_color({0.1f, 0.1f, 0.1f, 1.0f});

        scene_material_.set("uLightWorldPos", light_pos_);
        scene_material_.set("uViewWorldPos", camera_.position());
        scene_material_.set("uLightSpaceMat", latest_light_space_matrix_);
        scene_material_.set("uDiffuseTexture", wood_texture_);
        scene_material_.set("uShadowMapTexture", depth_texture_);

        draw_meshes_with_material(scene_material_);
        camera_.set_pixel_rect(workspace_screen_space_rect);
        camera_.render_to_main_window();
        camera_.set_pixel_rect(std::nullopt);
        graphics::blit_to_main_window(
            depth_texture_,
            Rect::from_corners(
                workspace_screen_space_top_left - Vec2{0.0f, depth_overlay_size},
                workspace_screen_space_top_left + Vec2{depth_overlay_size, 0.0f}
            )
        );

        scene_material_.unset("uShadowMapTexture");
    }

    void draw_meshes_with_material(const Material& material)
    {
        // floor
        graphics::draw(plane_mesh_, identity<Transform>(), material, camera_);

        // cubes
        graphics::draw(
            cube_mesh_,
            {.scale = Vec3{0.5f}, .position = {0.0f, 1.0f, 0.0f}},
            material,
            camera_
        );
        graphics::draw(
            cube_mesh_,
            {.scale = Vec3{0.5f}, .position = {2.0f, 0.0f, 1.0f}},
            material,
            camera_
        );
        graphics::draw(
            cube_mesh_,
            Transform{
                .scale = Vec3{0.25f},
                .rotation = angle_axis(60_deg, UnitVec3{1.0f, 0.0f, 1.0f}),
                .position = {-1.0f, 0.0f, 2.0f},
            },
            material,
            camera_
        );
    }

    void render_shadows_to_depth_texture()
    {
        const float znear = 1.0f;
        const float zfar = 7.5f;
        const Mat4 light_view_matrix = look_at(light_pos_, Vec3{0.0f}, {0.0f, 1.0f, 0.0f});
        const Mat4 light_projection_matrix = ortho(-10.0f, 10.0f, -10.0f, 10.0f, znear, zfar);
        latest_light_space_matrix_ = light_projection_matrix * light_view_matrix;

        draw_meshes_with_material(depth_material_);

        camera_.set_view_matrix_override(light_view_matrix);
        camera_.set_projection_matrix_override(light_projection_matrix);
        camera_.render_to(depth_texture_);
        camera_.set_view_matrix_override(std::nullopt);
        camera_.set_projection_matrix_override(std::nullopt);
    }

    ResourceLoader loader_ = App::resource_loader();
    MouseCapturingCamera camera_ = create_camera();
    Texture2D wood_texture_ = load_texture2D_from_image(
        loader_.open("oscar_demos/learnopengl/textures/wood.jpg"),
        ColorSpace::sRGB
    );
    Mesh cube_mesh_ = BoxGeometry{{.dimensions = Vec3{2.0f}}};
    Mesh plane_mesh_ = generate_learnopengl_plane_mesh();
    Material scene_material_{Shader{
        loader_.slurp("oscar_demos/learnopengl/shaders/AdvancedLighting/shadow_mapping/Scene.vert"),
        loader_.slurp("oscar_demos/learnopengl/shaders/AdvancedLighting/shadow_mapping/Scene.frag"),
    }};
    Material depth_material_{Shader{
        loader_.slurp("oscar_demos/learnopengl/shaders/AdvancedLighting/shadow_mapping/MakeShadowMap.vert"),
        loader_.slurp("oscar_demos/learnopengl/shaders/AdvancedLighting/shadow_mapping/MakeShadowMap.frag"),
    }};
    RenderTexture depth_texture_ = create_depth_texture();
    Mat4 latest_light_space_matrix_ = identity<Mat4>();
    Vec3 light_pos_ = {-2.0f, 4.0f, -1.0f};
};

CStringView osc::LOGLShadowMappingTab::id() { return Impl::static_label(); }

osc::LOGLShadowMappingTab::LOGLShadowMappingTab(Widget* parent) :
    Tab{std::make_unique<Impl>(*this, parent)}
{}
void osc::LOGLShadowMappingTab::impl_on_mount() { private_data().on_mount(); }
void osc::LOGLShadowMappingTab::impl_on_unmount() { private_data().on_unmount(); }
bool osc::LOGLShadowMappingTab::impl_on_event(Event& e) { return private_data().on_event(e); }
void osc::LOGLShadowMappingTab::impl_on_draw() { private_data().on_draw(); }
