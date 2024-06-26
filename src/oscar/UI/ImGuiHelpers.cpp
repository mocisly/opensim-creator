#include "ImGuiHelpers.h"

#include <oscar/Graphics/Camera.h>
#include <oscar/Graphics/Color.h>
#include <oscar/Graphics/RenderTexture.h>
#include <oscar/Graphics/Texture2D.h>
#include <oscar/Maths/CollisionTests.h>
#include <oscar/Maths/Eulers.h>
#include <oscar/Maths/Mat4.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Maths/PolarPerspectiveCamera.h>
#include <oscar/Maths/Quat.h>
#include <oscar/Maths/Rect.h>
#include <oscar/Maths/VecFunctions.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Maths/Vec3.h>
#include <oscar/Maths/Vec4.h>
#include <oscar/UI/oscimgui.h>
#include <oscar/UI/oscimgui_internal.h>
#include <oscar/UI/ui_graphics_backend.h>
#include <oscar/Utils/Algorithms.h>
#include <oscar/Utils/UID.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <string>

using namespace osc::literals;
using namespace osc;
namespace rgs = std::ranges;

namespace
{
    inline constexpr float c_default_drag_threshold = 5.0f;

    template<
        rgs::random_access_range TCollection,
        rgs::random_access_range UCollection
    >
    requires
        std::convertible_to<typename TCollection::value_type, float> and
        std::convertible_to<typename UCollection::value_type, float>
    float diff(const TCollection& older, const UCollection& newer, size_t n)
    {
        for (size_t i = 0; i < n; ++i) {
            if (static_cast<float>(older[i]) != static_cast<float>(newer[i])) {
                return newer[i];
            }
        }
        return static_cast<float>(older[0]);
    }

    Vec2 centroid_of(const ImRect& r)
    {
        return 0.5f * (Vec2{r.Min} + Vec2{r.Max});
    }

    Vec2 dimensions_of(const ImRect& r)
    {
        return Vec2{r.Max} - Vec2{r.Min};
    }

    float shortest_edge_length_of(const ImRect& r)
    {
        const Vec2 dimensions = dimensions_of(r);
        return rgs::min(dimensions);
    }

    ImU32 brighten(ImU32 color, float factor)
    {
        const Color srgb = ui::to_color(color);
        const Color brightened = factor * srgb;
        const Color clamped = clamp_to_ldr(brightened);
        return ui::to_ImU32(clamped);
    }
}

void osc::ui::apply_dark_theme()
{
    // see: https://github.com/ocornut/imgui/issues/707
    // this one: https://github.com/ocornut/imgui/issues/707#issuecomment-512669512

    auto& style = ui::get_style();

    style.FrameRounding = 0.0f;
    style.GrabRounding = 20.0f;
    style.GrabMinSize = 10.0f;

    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.2f, 0.22f, 0.24f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.32f, 0.35f, 0.70f);  // contrasts against other Header* elements (#677)
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.6f);
}

bool osc::ui::update_polar_camera_from_mouse_inputs(
    PolarPerspectiveCamera& camera,
    Vec2 viewport_dimensions)
{
    bool modified = false;

    // handle mousewheel scrolling
    if (ui::get_io().MouseWheel != 0.0f) {
        camera.radius *= 1.0f - 0.1f * ui::get_io().MouseWheel;
        modified = true;
    }

    // these camera controls try to be the union of other GUIs (e.g. Blender)
    //
    // left drag: drags/orbits camera
    // left drag + L/R SHIFT: pans camera (CUSTOM behavior: can be handy on laptops where right-click + drag sucks)
    // left drag + L/R CTRL: zoom camera (CUSTOM behavior: can be handy on laptops where right-click + drag sucks)
    // middle drag: drags/orbits camera (Blender behavior)
    // middle drag + L/R SHIFT: pans camera (Blender behavior)
    // middle drag + L/R CTRL: zooms camera (Blender behavior)
    // right drag: pans camera
    //
    // the reason it's like this is to please legacy users from a variety of
    // other GUIs and users who use modelling software like Blender (which is
    // more popular among newer users looking to make new models)

    const float aspect_ratio = aspect_ratio_of(viewport_dimensions);

    const bool left_dragging = ui::is_mouse_dragging(ImGuiMouseButton_Left);
    const bool middle_dragging = ui::is_mouse_dragging(ImGuiMouseButton_Middle);
    const Vec2 delta = ui::get_io().MouseDelta;

    if (delta != Vec2{0.0f, 0.0f} and (left_dragging or middle_dragging)) {
        if (is_ctrl_down()) {
            camera.pan(aspect_ratio, delta/viewport_dimensions);
            modified = true;
        }
        else if (is_ctrl_or_super_down()) {
            camera.radius *= 1.0f + 4.0f*delta.y/viewport_dimensions.y;
            modified = true;
        }
        else {
            camera.drag(delta/viewport_dimensions);
            modified = true;
        }
    }
    else if (ui::is_mouse_dragging(ImGuiMouseButton_Right)) {
        if (is_alt_down()) {
            camera.radius *= 1.0f + 4.0f*delta.y/viewport_dimensions.y;
            modified = true;
        }
        else {
            camera.pan(aspect_ratio, delta/viewport_dimensions);
            modified = true;
        }
    }

    if (modified) {
        camera.rescale_znear_and_zfar_based_on_radius();
    }

    return modified;
}

bool osc::ui::update_polar_camera_from_keyboard_inputs(
    PolarPerspectiveCamera& camera,
    const Rect& viewport_rect,
    std::optional<AABB> maybe_scene_aabb)
{
    const bool shift_down = is_shift_down();
    const bool ctrl_or_super_down = is_ctrl_or_super_down();

    if (ui::is_key_released(ImGuiKey_X)) {
        if (ctrl_or_super_down) {
            focus_along_minus_x(camera);
            return true;
        } else {
            focus_along_x(camera);
            return true;
        }
    }
    else if (ui::is_key_pressed(ImGuiKey_Y)) {
        // Ctrl+Y already does something?
        if (not ctrl_or_super_down) {
            focus_along_y(camera);
            return true;
        }
    }
    else if (ui::is_key_pressed(ImGuiKey_F)) {
        if (ctrl_or_super_down) {
            if (maybe_scene_aabb) {
                auto_focus(
                    camera,
                    *maybe_scene_aabb,
                    aspect_ratio_of(viewport_rect)
                );
                return true;
            }
        }
        else {
            reset(camera);
            return true;
        }
    }
    else if (ctrl_or_super_down and ui::is_key_pressed(ImGuiKey_8)) {
        if (maybe_scene_aabb) {
            auto_focus(
                camera,
                *maybe_scene_aabb,
                aspect_ratio_of(viewport_rect)
            );
            return true;
        }
    }
    else if (ui::is_key_down(ImGuiKey_UpArrow)) {
        if (ctrl_or_super_down) {
            // pan
            camera.pan(aspect_ratio_of(viewport_rect), {0.0f, -0.1f});
        }
        else if (shift_down) {
            camera.phi -= 90_deg;  // rotate in 90-deg increments
        }
        else {
            camera.phi -= 10_deg;  // rotate in 10-deg increments
        }
        return true;
    }
    else if (ui::is_key_down(ImGuiKey_DownArrow)) {
        if (ctrl_or_super_down) {
            // pan
            camera.pan(aspect_ratio_of(viewport_rect), {0.0f, +0.1f});
        }
        else if (shift_down) {
            // rotate in 90-deg increments
            camera.phi += 90_deg;
        }
        else {
            // rotate in 10-deg increments
            camera.phi += 10_deg;
        }
        return true;
    }
    else if (ui::is_key_down(ImGuiKey_LeftArrow)) {
        if (ctrl_or_super_down) {
            // pan
            camera.pan(aspect_ratio_of(viewport_rect), {-0.1f, 0.0f});
        }
        else if (shift_down) {
            // rotate in 90-deg increments
            camera.theta += 90_deg;
        }
        else {
            // rotate in 10-deg increments
            camera.theta += 10_deg;
        }
        return true;
    }
    else if (ui::is_key_down(ImGuiKey_RightArrow)) {
        if (ctrl_or_super_down) {
            // pan
            camera.pan(aspect_ratio_of(viewport_rect), {+0.1f, 0.0f});
        }
        else if (shift_down) {
            camera.theta -= 90_deg;  // rotate in 90-deg increments
        }
        else {
            camera.theta -= 10_deg;  // rotate in 10-deg increments
        }
        return true;
    }
    else if (ui::is_key_down(ImGuiKey_Minus)) {
        camera.radius *= 1.1f;
        return true;
    }
    else if (ui::is_key_down(ImGuiKey_Equal)) {
        camera.radius *= 0.9f;
        return true;
    }
    return false;
}

bool osc::ui::update_polar_camera_from_all_inputs(
    PolarPerspectiveCamera& camera,
    const Rect& viewport_rect,
    std::optional<AABB> maybe_scene_aabb)
{

    ImGuiIO& io = ui::get_io();

    // we don't check `io.WantCaptureMouse` because clicking/dragging on an ImGui::Image
    // is classed as a mouse interaction
    const bool mouse_handled =
        update_polar_camera_from_mouse_inputs(camera, dimensions_of(viewport_rect));
    const bool keyboard_handled = not io.WantCaptureKeyboard ?
        update_polar_camera_from_keyboard_inputs(camera, viewport_rect, maybe_scene_aabb) :
        false;

    return mouse_handled or keyboard_handled;
}

void osc::ui::update_camera_from_all_inputs(Camera& camera, Eulers& eulers)
{
    const Vec3 front = camera.direction();
    const Vec3 up = camera.upwards_direction();
    const Vec3 right = cross(front, up);
    const Vec2 mouseDelta = ui::get_io().MouseDelta;

    const float speed = 10.0f;
    const float displacement = speed * ui::get_io().DeltaTime;
    const Radians sensitivity{0.005f};

    // keyboard: changes camera position
    Vec3 pos = camera.position();
    if (ui::is_key_down(ImGuiKey_W)) {
        pos += displacement * front;
    }
    if (ui::is_key_down(ImGuiKey_S)) {
        pos -= displacement * front;
    }
    if (ui::is_key_down(ImGuiKey_A)) {
        pos -= displacement * right;
    }
    if (ui::is_key_down(ImGuiKey_D)) {
        pos += displacement * right;
    }
    if (ui::is_key_down(ImGuiKey_Space)) {
        pos += displacement * up;
    }
    if (ui::get_io().KeyCtrl) {
        pos -= displacement * up;
    }
    camera.set_position(pos);

    eulers.x += sensitivity * -mouseDelta.y;
    eulers.x = clamp(eulers.x, -90_deg + 0.1_rad, 90_deg - 0.1_rad);
    eulers.y += sensitivity * -mouseDelta.x;
    eulers.y = mod(eulers.y, 360_deg);

    camera.set_rotation(to_worldspace_rotation_quat(eulers));
}

Rect osc::ui::content_region_avail_as_screen_rect()
{
    const Vec2 top_left = ui::get_cursor_screen_pos();
    return Rect{top_left, top_left + ui::get_content_region_avail()};
}

void osc::ui::draw_image(const Texture2D& texture)
{
    draw_image(texture, texture.dimensions());
}

void osc::ui::draw_image(const Texture2D& texture, Vec2 dimensions)
{
    const Vec2 uv0 = {0.0f, 1.0f};
    const Vec2 uv1 = {1.0f, 0.0f};
    draw_image(texture, dimensions, uv0, uv1);
}

void osc::ui::draw_image(
    const Texture2D& texture,
    Vec2 dimensions,
    Vec2 top_left_texture_coordinate,
    Vec2 bottom_right_texture_coordinate)
{
    const auto handle = ui::graphics_backend::allocate_texture_for_current_frame(texture);
    ImGui::Image(handle, dimensions, top_left_texture_coordinate, bottom_right_texture_coordinate);
}

void osc::ui::draw_image(const RenderTexture& texture)
{
    return draw_image(texture, texture.dimensions());
}

void osc::ui::draw_image(const RenderTexture& texture, Vec2 dimensions)
{
    const Vec2 uv0 = {0.0f, 1.0f};
    const Vec2 uv1 = {1.0f, 0.0f};
    const auto handle = ui::graphics_backend::allocate_texture_for_current_frame(texture);
    ImGui::Image(handle, dimensions, uv0, uv1);
}

Vec2 osc::ui::calc_button_size(CStringView content)
{
    return ui::calc_text_size(content) + 2.0f*ui::get_style_frame_padding();
}

float osc::ui::calc_button_width(CStringView content)
{
    return calc_button_size(content).x;
}

bool osc::ui::draw_button_nobg(CStringView label, Vec2 dimensions)
{
    push_style_color(ImGuiCol_Button, Color::clear());
    push_style_color(ImGuiCol_ButtonHovered, Color::clear());
    const bool rv = ui::draw_button(label, dimensions);
    pop_style_color(2);

    return rv;
}

bool osc::ui::draw_image_button(
    CStringView label,
    const Texture2D& texture,
    Vec2 dimensions,
    const Rect& texture_coordinates)
{
    const auto handle = ui::graphics_backend::allocate_texture_for_current_frame(texture);
    return ImGui::ImageButton(label.c_str(), handle, dimensions, texture_coordinates.p1, texture_coordinates.p2);
}

bool osc::ui::draw_image_button(CStringView label, const Texture2D& texture, Vec2 dimensions)
{
    return draw_image_button(label, texture, dimensions, Rect{{0.0f, 1.0f}, {1.0f, 0.0f}});
}

Rect osc::ui::get_last_drawn_item_screen_rect()
{
    return {ui::get_item_topleft(), ui::get_item_bottomright()};
}

ui::HittestResult osc::ui::hittest_last_drawn_item()
{
    return hittest_last_drawn_item(c_default_drag_threshold);
}

ui::HittestResult osc::ui::hittest_last_drawn_item(float drag_threshold)
{
    HittestResult rv;
    rv.item_screen_rect.p1 = ui::get_item_topleft();
    rv.item_screen_rect.p2 = ui::get_item_bottomright();
    rv.is_hovered = ui::is_item_hovered();
    rv.is_left_click_released_without_dragging = rv.is_hovered and is_mouse_released_without_dragging(ImGuiMouseButton_Left, drag_threshold);
    rv.is_right_click_released_without_dragging = rv.is_hovered and is_mouse_released_without_dragging(ImGuiMouseButton_Right, drag_threshold);
    return rv;
}

bool osc::ui::any_of_keys_down(std::span<const ImGuiKey> keys)
{
    return rgs::any_of(keys, ui::is_key_down);
}

bool osc::ui::any_of_keys_down(std::initializer_list<const ImGuiKey> keys)
{
    return any_of_keys_down(std::span<ImGuiKey const>{keys.begin(), keys.end()});
}

bool osc::ui::any_of_keys_pressed(std::span<const ImGuiKey> keys)
{
    return rgs::any_of(keys, [](ImGuiKey k) { return ui::is_key_pressed(k); });
}
bool osc::ui::any_of_keys_pressed(std::initializer_list<const ImGuiKey> keys)
{
    return any_of_keys_pressed(std::span<const ImGuiKey>{keys.begin(), keys.end()});
}

bool osc::ui::is_ctrl_down()
{
    return ui::get_io().KeyCtrl;
}

bool osc::ui::is_ctrl_or_super_down()
{
    return ui::get_io().KeyCtrl or ui::get_io().KeySuper;
}

bool osc::ui::is_shift_down()
{
    return ui::get_io().KeyShift;
}

bool osc::ui::is_alt_down()
{
    return ui::get_io().KeyAlt;
}

bool osc::ui::is_mouse_released_without_dragging(ImGuiMouseButton mouse_button)
{
    return is_mouse_released_without_dragging(mouse_button, c_default_drag_threshold);
}

bool osc::ui::is_mouse_released_without_dragging(ImGuiMouseButton mouse_button, float threshold)
{
    if (not ui::is_mouse_released(mouse_button)) {
        return false;
    }

    const Vec2 drag_delta = ImGui::GetMouseDragDelta(mouse_button);

    return length(drag_delta) < threshold;
}

bool osc::ui::is_mouse_dragging_with_any_button_down()
{
    return
        ui::is_mouse_dragging(ImGuiMouseButton_Left) or
        ui::is_mouse_dragging(ImGuiMouseButton_Middle) or
        ui::is_mouse_dragging(ImGuiMouseButton_Right);
}

void osc::ui::begin_tooltip(std::optional<float> wrap_width)
{
    begin_tooltip_nowrap();
    ImGui::PushTextWrapPos(wrap_width.value_or(ImGui::GetFontSize() * 35.0f));
}

void osc::ui::end_tooltip(std::optional<float>)
{
    ImGui::PopTextWrapPos();
    end_tooltip_nowrap();
}

void osc::ui::draw_tooltip_header_text(CStringView content)
{
    draw_text_unformatted(content);
}

void osc::ui::draw_tooltip_description_spacer()
{
    ui::draw_dummy({0.0f, 1.0f});
}

void osc::ui::draw_tooltip_description_text(CStringView content)
{
    draw_text_faded(content);
}

void osc::ui::draw_tooltip_body_only(CStringView content)
{
    begin_tooltip();
    draw_tooltip_header_text(content);
    end_tooltip();
}

void osc::ui::draw_tooltip_body_only_if_item_hovered(
    CStringView content,
    ImGuiHoveredFlags flags)
{
    if (ui::is_item_hovered(flags)) {
        draw_tooltip_body_only(content);
    }
}

void osc::ui::draw_tooltip(CStringView header, CStringView description)
{
    begin_tooltip();
    draw_tooltip_header_text(header);
    if (not description.empty()) {
        draw_tooltip_description_spacer();
        draw_tooltip_description_text(description);
    }
    end_tooltip();
}

void osc::ui::draw_tooltip_if_item_hovered(
    CStringView header,
    CStringView description,
    ImGuiHoveredFlags flags)
{
    if (ui::is_item_hovered(flags)) {
        draw_tooltip(header, description);
    }
}

void osc::ui::draw_help_marker(CStringView header, CStringView desc)
{
    ui::draw_text_disabled("(?)");
    draw_tooltip_if_item_hovered(header, desc, ImGuiHoveredFlags_None);
}

void osc::ui::draw_help_marker(CStringView content)
{
    ui::draw_text_disabled("(?)");
    draw_tooltip_if_item_hovered(content, {}, ImGuiHoveredFlags_None);
}

bool osc::ui::draw_string_input(CStringView label, std::string& edited_string, ImGuiInputTextFlags flags)
{
    return ImGui::InputText(label.c_str(), &edited_string, flags);  // uses `imgui_stdlib`
}

bool osc::ui::draw_float_meters_input(CStringView label, float& v, float step, float step_fast, ImGuiInputTextFlags flags)
{
    return ui::draw_float_input(label, &v, step, step_fast, "%.6f", flags);
}

bool osc::ui::draw_float3_meters_input(CStringView label, Vec3& vec, ImGuiInputTextFlags flags)
{
    return ui::draw_float3_input(label, value_ptr(vec), "%.6f", flags);
}

bool osc::ui::draw_float_meters_slider(CStringView label, float& v, float v_min, float v_max, ImGuiSliderFlags flags)
{
    return ui::draw_float_slider(label, &v, v_min, v_max, "%.6f", flags);
}

bool osc::ui::draw_float_kilogram_input(CStringView label, float& v, float step, float step_fast, ImGuiInputTextFlags flags)
{
    return draw_float_meters_input(label, v, step, step_fast, flags);
}

bool osc::ui::draw_angle_input(CStringView label, Radians& v)
{
    float degrees_float = Degrees{v}.count();
    if (ui::draw_float_input(label, &degrees_float)) {
        v = Radians{Degrees{degrees_float}};
        return true;
    }
    return false;
}

bool osc::ui::draw_angle3_input(
    CStringView label,
    Vec<3, Radians>& angles,
    CStringView format)
{
    Vec3 dvs = {Degrees{angles.x}.count(), Degrees{angles.y}.count(), Degrees{angles.z}.count()};
    if (ui::draw_vec3_input(label, dvs, format.c_str())) {
        angles = Vec<3, Radians>{Vec<3, Degrees>{dvs}};
        return true;
    }
    return false;
}

bool osc::ui::draw_angle_slider(CStringView label, Radians& v, Radians min, Radians max)
{
    float degrees_float = Degrees{v}.count();
    const Degrees degrees_min{min};
    const Degrees degrees_max{max};
    if (ui::draw_float_slider(label, &degrees_float, degrees_min.count(), degrees_max.count())) {
        v = Radians{Degrees{degrees_float}};
        return true;
    }
    return false;
}

ImU32 osc::ui::to_ImU32(const Color& color)
{
    return ui::to_ImU32(Vec4{color});
}

Color osc::ui::to_color(ImU32 u32color)
{
    return Color{Vec4{ImGui::ColorConvertU32ToFloat4(u32color)}};
}

Color osc::ui::to_color(const ImVec4& v)
{
    return {v.x, v.y, v.z, v.w};
}

ImVec4 osc::ui::to_ImVec4(const Color& color)
{
    return ImVec4{Vec4{color}};
}

ImGuiWindowFlags osc::ui::get_minimal_panel_flags()
{
    return
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar;
}

Rect osc::ui::get_main_viewport_workspace_uiscreenspace_rect()
{
    const ImGuiViewport& viewport = *ui::get_main_viewport();

    return Rect{
        viewport.WorkPos,
        Vec2{viewport.WorkPos} + Vec2{viewport.WorkSize}
    };
}

Rect osc::ui::get_main_viewport_workspace_screenspace_rect()
{
    const ImGuiViewport& viewport = *ui::get_main_viewport();
    const Vec2 bottom_left_uiscreenspace = Vec2{viewport.WorkPos} + Vec2{0.0f, viewport.WorkSize.y};
    const Vec2 bottom_left_screenspace = Vec2{bottom_left_uiscreenspace.x, viewport.Size.y - bottom_left_uiscreenspace.y};
    const Vec2 top_right_screenspace = bottom_left_screenspace + Vec2{viewport.WorkSize};

    return {bottom_left_screenspace, top_right_screenspace};
}

Vec2 osc::ui::get_main_viewport_workspace_screen_dimensions()
{
    return dimensions_of(get_main_viewport_workspace_uiscreenspace_rect());
}

bool osc::ui::is_mouse_in_main_viewport_workspace()
{
    const Vec2 mousepos = ui::get_mouse_pos();
    const Rect hitRect = get_main_viewport_workspace_uiscreenspace_rect();

    return is_intersecting(hitRect, mousepos);
}

bool osc::ui::begin_main_viewport_top_bar(CStringView label, float height, ImGuiWindowFlags flags)
{
    // https://github.com/ocornut/imgui/issues/3518
    auto* const viewport = static_cast<ImGuiViewportP*>(static_cast<void*>(ui::get_main_viewport()));
    return ImGui::BeginViewportSideBar(label.c_str(), viewport, ImGuiDir_Up, height, flags);
}


bool osc::ui::begin_main_viewport_bottom_bar(CStringView label)
{
    // https://github.com/ocornut/imgui/issues/3518
    auto* const viewport = static_cast<ImGuiViewportP*>(static_cast<void*>(ui::get_main_viewport()));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    const float height = ui::get_frame_height() + ui::get_style_panel_padding().y;

    return ImGui::BeginViewportSideBar(label.c_str(), viewport, ImGuiDir_Down, height, flags);
}

bool osc::ui::draw_button_centered(CStringView label)
{
    const float button_width = ui::calc_text_size(label).x + 2.0f*ui::get_style_frame_padding().x;
    const float midpoint = ui::get_cursor_screen_pos().x + 0.5f*ui::get_content_region_avail().x;
    const float button_start_x = midpoint - 0.5f*button_width;

    ui::set_cursor_screen_pos({button_start_x, ui::get_cursor_screen_pos().y});

    return ui::draw_button(label);
}

void osc::ui::draw_text_centered(CStringView content)
{
    const float panel_width = ui::get_panel_size().x;
    const float text_width   = ui::calc_text_size(content).x;

    ui::set_cursor_pos_x(0.5f * (panel_width - text_width));
    draw_text_unformatted(content);
}

void osc::ui::draw_text_panel_centered(CStringView content)
{
    const auto panel_dimensions = ui::get_panel_size();
    const auto text_dimensions = ui::calc_text_size(content);

    ui::set_cursor_pos(0.5f * (panel_dimensions - text_dimensions));
    draw_text_unformatted(content);
}

void osc::ui::draw_text_disabled_and_centered(CStringView content)
{
    ui::begin_disabled();
    draw_text_centered(content);
    ui::end_disabled();
}

void osc::ui::draw_text_disabled_and_panel_centered(CStringView content)
{
    ui::begin_disabled();
    draw_text_panel_centered(content);
    ui::end_disabled();
}

void osc::ui::draw_text_column_centered(CStringView content)
{
    const float column_width = ui::get_column_width();
    const float column_offset = ui::get_cursor_pos().x;
    const float text_width = ui::calc_text_size(content).x;

    ui::set_cursor_pos_x(column_offset + 0.5f*(column_width-text_width));
    draw_text_unformatted(content);
}

void osc::ui::draw_text_faded(CStringView content)
{
    ui::push_style_color(ImGuiCol_Text, Color{0.7f, 0.7f, 0.7f});
    draw_text_unformatted(content);
    ui::pop_style_color();
}

void osc::ui::draw_text_warning(CStringView content)
{
    push_style_color(ImGuiCol_Text, Color::yellow());
    draw_text_unformatted(content);
    pop_style_color();
}

bool osc::ui::should_save_last_drawn_item_value()
{
    if (ui::is_item_deactivated_after_edit()) {
        return true;  // ImGui detected that the item was deactivated after an edit
    }

    if (ImGui::IsItemEdited() and any_of_keys_pressed({ImGuiKey_Enter, ImGuiKey_Tab})) {
        return true;  // user pressed enter/tab after editing
    }

    return false;
}

void osc::ui::pop_item_flags(int n)
{
    for (int i = 0; i < n; ++i) {
        ImGui::PopItemFlag();
    }
}

bool osc::ui::draw_combobox(
    CStringView label,
    size_t* current,
    size_t size,
    const std::function<CStringView(size_t)>& accessor)
{
    const CStringView preview = current != nullptr ?
        accessor(*current) :
        CStringView{};

    if (not ui::begin_combobox(label, preview, ImGuiComboFlags_None)) {
        return false;
    }

    bool changed = false;
    for (size_t i = 0; i < size; ++i) {
        ui::push_id(static_cast<int>(i));
        const bool is_selected = current != nullptr and *current == i;
        if (ui::draw_selectable(accessor(i), is_selected)) {
            changed = true;
            if (current != nullptr) {
                *current = i;
            }
        }
        if (is_selected) {
            ImGui::SetItemDefaultFocus();
        }
        ui::pop_id();
    }

    ui::end_combobox();

    if (changed) {
        ImGui::MarkItemEdited(ImGui::GetCurrentContext()->LastItemData.ID);
    }

    return changed;
}

bool osc::ui::draw_combobox(
    CStringView label,
    size_t* current,
    std::span<const CStringView> items)
{
    return draw_combobox(
        label,
        current,
        items.size(),
        [items](size_t i) { return items[i]; }
    );
}

void osc::ui::draw_vertical_separator()
{
    ui::draw_separator(ImGuiSeparatorFlags_Vertical);
}

void osc::ui::draw_same_line_with_vertical_separator()
{
    ui::same_line();
    draw_vertical_separator();
    ui::same_line();
}

bool osc::ui::draw_float_circular_slider(
    CStringView label,
    float* v,
    float min,
    float max,
    CStringView format,
    ImGuiSliderFlags flags)
{
    // this implementation was initially copied from `ImGui::SliderFloat` and written in a
    // similar style to code in `imgui_widgets.cpp` (see https://github.com/ocornut/imgui),
    // but has since mutated
    //
    // the display style etc. uses ideas from XEMU (https://github.com/xemu-project/xemu), which
    // contains custom widgets like sliders etc. - I liked XEMU's style but I needed some
    // features that `ImGui::SliderFloat` has, so I ended up with this mash-up

    // prefetch top-level state
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
    {
        // skip drawing: the window is not visible or it is clipped
        return false;
    }
    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label.c_str());

    // calculate top-level item info for early-cull checks etc.
    const Vec2 label_size = ui::calc_text_size(label, true);
    const Vec2 frame_dims = {ImGui::CalcItemWidth(), label_size.y + 2.0f*style.FramePadding.y};
    const Vec2 cursor_screen_pos = ui::get_cursor_screen_pos();
    const ImRect frame_bounds = {cursor_screen_pos, cursor_screen_pos + frame_dims};
    const float label_width_with_spacing = label_size.x > 0.0f ? label_size.x + style.ItemInnerSpacing.x : 0.0f;
    const ImRect total_bounds = {frame_bounds.Min, Vec2{frame_bounds.Max} + Vec2{label_width_with_spacing, 0.0f}};

    const bool temporary_text_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    ImGui::ItemSize(total_bounds, style.FramePadding.y);
    if (not ImGui::ItemAdd(total_bounds, id, &frame_bounds, temporary_text_input_allowed ? ImGuiItemFlags_Inputable : 0)) {
        // skip drawing: the slider item is off-screen or not interactable
        return false;
    }
    const bool is_hovered = ImGui::ItemHoverable(frame_bounds, id, g.LastItemData.InFlags);  // hovertest the item

    // figure out whether the user is (temporarily) editing the slider as an input text box
    bool temporary_text_input_active = temporary_text_input_allowed and ImGui::TempInputIsActive(id);
    if (not temporary_text_input_active) {
        // tabbing or double clicking the slider temporarily transforms it into an input box
        const bool clicked = is_hovered and ui::is_mouse_clicked(ImGuiMouseButton_Left, id);
        const bool double_clicked = (is_hovered and g.IO.MouseClickedCount[0] == 2 and ImGui::TestKeyOwner(ImGuiKey_MouseLeft, id));
        const bool make_active = (clicked or double_clicked or g.NavActivateId == id);

        if (make_active and (clicked or double_clicked)) {
            ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);  // tell ImGui that left-click is locked from further interaction etc. this frame
        }
        if (make_active and temporary_text_input_allowed) {
            if ((clicked and g.IO.KeyCtrl) or double_clicked or (g.NavActivateId == id and (g.NavActivateFlags & ImGuiActivateFlags_PreferInput))) {
                temporary_text_input_active = true;
            }
        }

        // if it's decided that the text input should be made active, then make it active
        // by focusing on it (e.g. give it keyboard focus)
        if (make_active and not temporary_text_input_active) {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }
    }

    // if the user is editing the slider as an input text box then draw that instead of the slider
    if (temporary_text_input_active) {
        const bool should_clamp_textual_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0;

        return ImGui::TempInputScalar(
            frame_bounds,
            id,
            label.c_str(),
            ImGuiDataType_Float,
            static_cast<void*>(v),
            format.c_str(),
            should_clamp_textual_input ? static_cast<const void*>(&min) : nullptr,
            should_clamp_textual_input ? static_cast<const void*>(&max) : nullptr
        );
    }

    // else: draw the slider (remainder of this func)

    // calculate slider behavior (interaction, etc.)
    //
    // note: I haven't studied `ImGui::SliderBehaviorT` in-depth. I'm just going to
    // go ahead and assume that it's doing the interaction/hittest/mutation logic
    // and leaves rendering to us.
    ImRect grab_bounding_box{};
    bool value_changed = ImGui::SliderBehaviorT<float, float, float>(
        frame_bounds,
        id,
        ImGuiDataType_Float,
        v,
        min,
        max,
        format.c_str(),
        flags,
        &grab_bounding_box
    );
    if (value_changed) {
        ImGui::MarkItemEdited(id);
    }

    // render
    const bool use_custom_rendering = true;
    if (use_custom_rendering) {
        const Vec2 slider_nob_center = ::centroid_of(grab_bounding_box);
        const float slider_nob_radius = 0.75f * shortest_edge_length_of(grab_bounding_box);
        const float slider_rail_thickness = 0.5f * slider_nob_radius;
        const float slider_rail_top_y = slider_nob_center.y - 0.5f*slider_rail_thickness;
        const float slider_rail_bottom_y = slider_nob_center.y + 0.5f*slider_rail_thickness;

        const bool is_active = g.ActiveId == id;
        const ImU32 rail_color = ui::get_color_ImU32(is_hovered ? ImGuiCol_FrameBgHovered : is_active ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
        const ImU32 grab_color = ui::get_color_ImU32(is_active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);

        // render left-hand rail (brighter)
        {
            const Vec2 lhs_rail_topleft = {frame_bounds.Min.x, slider_rail_top_y};
            const Vec2 lhs_rail_bottomright = {slider_nob_center.x, slider_rail_bottom_y};
            const ImU32 brightened_rail_color = brighten(rail_color, 2.0f);

            window->DrawList->AddRectFilled(
                lhs_rail_topleft,
                lhs_rail_bottomright,
                brightened_rail_color,
                g.Style.FrameRounding
            );
        }

        // render right-hand rail
        {
            const Vec2 rhs_rail_topleft = {slider_nob_center.x, slider_rail_top_y};
            const Vec2 rhs_rail_bottomright = {frame_bounds.Max.x, slider_rail_bottom_y};

            window->DrawList->AddRectFilled(
                rhs_rail_topleft,
                rhs_rail_bottomright,
                rail_color,
                g.Style.FrameRounding
            );
        }

        // render slider grab on top of rail
        window->DrawList->AddCircleFilled(
            slider_nob_center,
            slider_nob_radius,  // visible nob is slightly smaller than virtual nob
            grab_color
        );

        // render current slider value using user-provided display format
        {
            std::array<char, 64> buf{};
            const char* const buf_end = buf.data() + ImGui::DataTypeFormatString(buf.data(), static_cast<int>(buf.size()), ImGuiDataType_Float, v, format.c_str());
            if (g.LogEnabled) {
                ImGui::LogSetNextTextDecoration("{", "}");
            }
            ImGui::RenderTextClipped(frame_bounds.Min, frame_bounds.Max, buf.data(), buf_end, nullptr, ImVec2(0.5f, 0.5f));
        }

        // render input label in remaining space
        if (label_size.x > 0.0f) {
            ImGui::RenderText(ImVec2(frame_bounds.Max.x + style.ItemInnerSpacing.x, frame_bounds.Min.y + style.FramePadding.y), label.c_str());
        }
    }
    else {
        // render slider background frame
        {
            const ImU32 frame_color = ui::get_color_ImU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : is_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
            ImGui::RenderNavHighlight(frame_bounds, id);
            ImGui::RenderFrame(frame_bounds.Min, frame_bounds.Max, frame_color, true, g.Style.FrameRounding);
        }

        // render slider grab handle
        if (grab_bounding_box.Max.x > grab_bounding_box.Min.x) {
            window->DrawList->AddRectFilled(grab_bounding_box.Min, grab_bounding_box.Max, ui::get_color_ImU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);
        }

        // render current slider value using user-provided display format
        {
            std::array<char, 64> buf{};
            const char* const buf_end = buf.data() + ImGui::DataTypeFormatString(buf.data(), static_cast<int>(buf.size()), ImGuiDataType_Float, v, format.c_str());
            if (g.LogEnabled) {
                ImGui::LogSetNextTextDecoration("{", "}");
            }
            ImGui::RenderTextClipped(frame_bounds.Min, frame_bounds.Max, buf.data(), buf_end, nullptr, ImVec2(0.5f, 0.5f));
        }

        // render input label in remaining space
        if (label_size.x > 0.0f) {
            ImGui::RenderText(ImVec2(frame_bounds.Max.x + style.ItemInnerSpacing.x, frame_bounds.Min.y + style.FramePadding.y), label.c_str());
        }
    }

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (temp_input_allowed ? ImGuiItemStatusFlags_Inputable : 0));

    return value_changed;
}
