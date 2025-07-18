#include "MandelbrotTab.h"

#include <liboscar/oscar.h>

#include <limits>
#include <memory>

using namespace osc;

namespace
{
    Camera create_identity_camera()
    {
        Camera camera;
        camera.set_view_matrix_override(identity<Mat4>());
        camera.set_projection_matrix_override(identity<Mat4>());
        return camera;
    }
}

class osc::MandelbrotTab::Impl final : public TabPrivate {
public:
    static CStringView static_label() { return "oscar_demos/Mandelbrot"; }

    explicit Impl(MandelbrotTab& owner, Widget* parent) :
        TabPrivate{owner, parent, static_label()}
    {}

    bool on_event(Event& ev)
    {
        if (ev.type() == EventType::KeyUp) {
            return on_keyup(dynamic_cast<const KeyEvent&>(ev));
        }
        else if (ev.type() == EventType::MouseWheel) {
            const float factor = dynamic_cast<const MouseWheelEvent&>(ev).delta().y > 0 ? 0.9f : 1.11111111f;
            apply_zoom_to_camera(ui::get_mouse_ui_pos(), factor);
            return true;
        }
        else if (ev.type() == EventType::MouseMove) {
            apply_pan_to_camera(dynamic_cast<const MouseEvent&>(ev).delta());
            return true;
        }
        return false;
    }

    void on_draw()
    {
        main_window_workspace_screen_space_rect_ = ui::get_main_window_workspace_screen_space_rect();

        material_.set("uRescale", Vec2{1.0f, 1.0f});
        material_.set("uOffset", Vec2{});
        material_.set("uNumIterations", num_iterations_);
        graphics::draw(quad_mesh_, identity<Transform>(), material_, camera_);
        camera_.set_pixel_rect(main_window_workspace_screen_space_rect_);
        camera_.render_to_main_window();
    }

private:
    bool on_keyup(const KeyEvent& e)
    {
        if (e.combination() == Key::PageUp and num_iterations_ < std::numeric_limits<decltype(num_iterations_)>::max()) {
            num_iterations_ *= 2;
            return true;
        }
        if (e.combination() == Key::PageDown and num_iterations_ > 1) {
            num_iterations_ /= 2;
            return true;
        }
        return false;
    }

    void apply_zoom_to_camera(Vec2, float)
    {
        // TODO: zoom the mandelbrot viewport into the given ui space location by the given factor
    }

    void apply_pan_to_camera([[maybe_unused]] Vec2 screen_space_delta)
    {
        // TODO: pan the mandelbrot viewport by the given ui space offset vector
    }

    ResourceLoader loader_ = App::resource_loader();
    int num_iterations_ = 16;
    Rect normalized_mandelbrot_viewport_rect_ = Rect::from_corners({}, {1.0f, 1.0f});
    Rect main_window_workspace_screen_space_rect_;
    Mesh quad_mesh_ = PlaneGeometry{{.dimensions = Vec2{2.0f}}};
    Material material_{Shader{
        loader_.slurp("oscar_demos/shaders/Mandelbrot.vert"),
        loader_.slurp("oscar_demos/shaders/Mandelbrot.frag"),
    }};
    Camera camera_ = create_identity_camera();
};


CStringView osc::MandelbrotTab::id() { return Impl::static_label(); }

osc::MandelbrotTab::MandelbrotTab(Widget* parent) :
    Tab{std::make_unique<Impl>(*this, parent)}
{}
bool osc::MandelbrotTab::impl_on_event(Event& e) { return private_data().on_event(e); }
void osc::MandelbrotTab::impl_on_draw() { private_data().on_draw(); }
