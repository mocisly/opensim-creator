#include "MandelbrotTab.h"

#include <oscar/oscar.h>
#include <SDL_events.h>

#include <limits>
#include <memory>

using namespace osc;

namespace
{
    constexpr CStringView c_tab_string_id = "Demos/Mandelbrot";

    Camera create_identity_camera()
    {
        Camera camera;
        camera.set_view_matrix_override(identity<Mat4>());
        camera.set_projection_matrix_override(identity<Mat4>());
        return camera;
    }
}

class osc::MandelbrotTab::Impl final : public StandardTabImpl {
public:
    Impl() : StandardTabImpl{c_tab_string_id}
    {}

private:
    bool impl_on_event(const SDL_Event& e) final
    {
        if (e.type == SDL_KEYUP and e.key.keysym.sym == SDLK_PAGEUP and num_iterations_ < std::numeric_limits<decltype(num_iterations_)>::max()) {
            num_iterations_ *= 2;
            return true;
        }
        if (e.type == SDL_KEYUP and e.key.keysym.sym == SDLK_PAGEDOWN and num_iterations_ > 1) {
            num_iterations_ /= 2;
            return true;
        }
        if (e.type == SDL_MOUSEWHEEL) {
            const float factor = e.wheel.y > 0 ? 0.9f : 1.11111111f;
            apply_zoom_to_camera(ui::get_io().MousePos, factor);
            return true;
        }
        if (e.type == SDL_MOUSEMOTION and (e.motion.state & SDL_BUTTON_LMASK) != 0) {
            const Vec2 screenSpacePanAmount = {static_cast<float>(e.motion.xrel), static_cast<float>(e.motion.yrel)};
            apply_pan_to_camera(screenSpacePanAmount);
            return true;
        }
        return false;
    }

    void impl_on_draw() final
    {
        main_viewport_workspace_screenspace_rect_ = ui::get_main_viewport_workspace_screenspace_rect();

        material_.set_vec2("uRescale", {1.0f, 1.0f});
        material_.set_vec2("uOffset", {});
        material_.set_int("uNumIterations", num_iterations_);
        graphics::draw(quad_mesh_, identity<Transform>(), material_, camera_);
        camera_.set_pixel_rect(main_viewport_workspace_screenspace_rect_);
        camera_.render_to_screen();
    }

    void apply_zoom_to_camera(Vec2, float)
    {
        // TODO: zoom the mandelbrot viewport into the given screen-space location by the given factor
    }

    void apply_pan_to_camera(Vec2)
    {
        // TODO: pan the mandelbrot viewport by the given screen-space offset vector
    }

    ResourceLoader loader_ = App::resource_loader();
    int num_iterations_ = 16;
    Rect normalized_mandelbrot_viewport_ = {{}, {1.0f, 1.0f}};
    Rect main_viewport_workspace_screenspace_rect_ = {};
    Mesh quad_mesh_ = PlaneGeometry{2.0f, 2.0f, 1, 1};
    Material material_{Shader{
        loader_.slurp("oscar_demos/shaders/Mandelbrot.vert"),
        loader_.slurp("oscar_demos/shaders/Mandelbrot.frag"),
    }};
    Camera camera_ = create_identity_camera();
};


CStringView osc::MandelbrotTab::id()
{
    return c_tab_string_id;
}

osc::MandelbrotTab::MandelbrotTab(const ParentPtr<ITabHost>&) :
    impl_{std::make_unique<Impl>()}
{}

osc::MandelbrotTab::MandelbrotTab(MandelbrotTab&&) noexcept = default;
osc::MandelbrotTab& osc::MandelbrotTab::operator=(MandelbrotTab&&) noexcept = default;
osc::MandelbrotTab::~MandelbrotTab() noexcept = default;

UID osc::MandelbrotTab::impl_get_id() const
{
    return impl_->id();
}

CStringView osc::MandelbrotTab::impl_get_name() const
{
    return impl_->name();
}

bool osc::MandelbrotTab::impl_on_event(const SDL_Event& e)
{
    return impl_->on_event(e);
}

void osc::MandelbrotTab::impl_on_draw()
{
    impl_->on_draw();
}
