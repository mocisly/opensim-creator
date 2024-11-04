#include "ModelViewerPanel.h"

#include <OpenSimCreator/Documents/Model/IModelStatePair.h>
#include <OpenSimCreator/Graphics/CachedModelRenderer.h>
#include <OpenSimCreator/UI/Shared/BasicWidgets.h>
#include <OpenSimCreator/UI/Shared/ModelSelectionGizmo.h>
#include <OpenSimCreator/UI/Shared/ModelViewerPanelLayer.h>
#include <OpenSimCreator/UI/Shared/ModelViewerPanelParameters.h>
#include <OpenSimCreator/UI/Shared/ModelViewerPanelRightClickEvent.h>
#include <OpenSimCreator/UI/Shared/ModelViewerPanelState.h>
#include <OpenSimCreator/Utils/OpenSimHelpers.h>

#include <OpenSim/Simulation/Model/Model.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Maths/Rect.h>
#include <oscar/Maths/Vec3.h>
#include <oscar/Platform/App.h>
#include <oscar/Platform/Log.h>
#include <oscar/UI/IconCache.h>
#include <oscar/UI/oscimgui.h>
#include <oscar/UI/Panels/StandardPanelImpl.h>
#include <oscar/UI/Widgets/GuiRuler.h>
#include <oscar/UI/Widgets/IconWithoutMenu.h>
#include <oscar/Utils/Algorithms.h>

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace osc;

namespace
{
    std::string GetSettingsKeyPrefixForPanel(std::string_view panelName)
    {
        std::stringstream ss;
        ss << "panels/" << panelName << '/';
        return std::move(ss).str();
    }

    class RulerLayer final : public ModelViewerPanelLayer {
    public:
        RulerLayer()
        {
            m_Ruler.start_measuring();
        }

    private:
        ModelViewerPanelLayerFlags implGetFlags() const final
        {
            return ModelViewerPanelLayerFlags::CapturesMouseInputs;
        }

        bool implHandleMouseInputs(
            ModelViewerPanelParameters&,
            ModelViewerPanelState&) final
        {
            return true;  // always handles the mouse
        }

        void implOnDraw(
            ModelViewerPanelParameters& params,
            ModelViewerPanelState& state) final
        {
            m_Ruler.on_draw(
                params.getRenderParams().camera,
                state.viewportRect,
                state.maybeBaseLayerHittest
            );
        }

        bool implShouldClose() const final
        {
            return !m_Ruler.is_measuring();
        }

        GuiRuler m_Ruler;
    };

    // model viewer layer that adds buttons for controling visualization
    // options and 3D manipulator gizmos
    class ButtonAndGizmoControlsLayer final : public ModelViewerPanelLayer {
    public:
        ButtonAndGizmoControlsLayer(
            std::string_view panelName_,
            std::shared_ptr<IModelStatePair> model_) :

            panel_name_{panelName_},
            m_Gizmo{std::move(model_)}
        {}
    private:
        ModelViewerPanelLayerFlags implGetFlags() const final
        {
            ModelViewerPanelLayerFlags flags = ModelViewerPanelLayerFlags::None;
            if (m_Gizmo.isUsing())
            {
                flags |= ModelViewerPanelLayerFlags::CapturesMouseInputs;
            }
            return flags;
        }

        float implGetBackgroundAlpha() const final
        {
            return 0.0f;
        }

        bool implHandleMouseInputs(
            ModelViewerPanelParameters&,
            ModelViewerPanelState&) final
        {
            // care: `isUsing` can return `true` even if it isn't being drawn this frame
            return m_Gizmo.isUsing();
        }

        bool implHandleKeyboardInputs(
            ModelViewerPanelParameters&,
            ModelViewerPanelState&) final
        {
            return m_Gizmo.handleKeyboardInputs();
        }

        void implOnDraw(
            ModelViewerPanelParameters& params,
            ModelViewerPanelState& state) final
        {
            // draw generic overlays (i.e. the buttons for toggling things)
            auto renderParamsBefore = params.getRenderParams();

            const bool edited = DrawViewerImGuiOverlays(
                params.updRenderParams(),
                state.getDrawlist(),
                state.maybeSceneAABB,
                state.viewportRect,
                *m_IconCache,
                [this, &state]() { return drawExtraTopButtons(state); }
            );

            if (edited)
            {
                log_debug("%s edited", panel_name_.c_str());

                const auto& renderParamsAfter = params.getRenderParams();

                SaveModelRendererParamsDifference(
                    renderParamsBefore,
                    renderParamsAfter,
                    GetSettingsKeyPrefixForPanel(panel_name_),
                    App::upd().upd_settings()
                );
            }

            // draw gizmo manipulators over the top
            m_Gizmo.onDraw(state.viewportRect, params.getRenderParams().camera);
        }

        bool implShouldClose() const final
        {
            return false;  // never closes
        }

        // draws extra top overlay buttons
        bool drawExtraTopButtons(
            ModelViewerPanelState& state)
        {
            bool edited = false;

            IconWithoutMenu rulerButton
            {
                m_IconCache->find_or_throw("ruler"),
                "Ruler",
                "Roughly measure something in the scene",
            };
            if (rulerButton.on_draw())
            {
                state.pushLayer(std::make_unique<RulerLayer>());
                edited = true;
            }

            ui::same_line();
            ui::draw_vertical_separator();
            ui::same_line();

            // draw translate/rotate/scale selector
            if (ui::draw_gizmo_op_selector(m_Gizmo, true, true, false)) {
                edited = true;
            }

            ui::push_style_var(ui::StyleVar::ItemSpacing, {0.0f, 0.0f});
            ui::same_line();
            ui::pop_style_var();

            // draw global/world selector
            if (ui::draw_gizmo_mode_selector(m_Gizmo)) {
                edited = true;
            }

            return edited;
        }

        std::shared_ptr<IconCache> m_IconCache = App::singleton<IconCache>(
            App::resource_loader().with_prefix("icons/"),
            ui::get_text_line_height()/128.0f
        );
        std::string panel_name_;
        ModelSelectionGizmo m_Gizmo;
    };

    // the "base" model viewer layer, which is the last layer to handle any input
    // etc. if no upper layer decides to handle it
    class BaseInteractionLayer final : public ModelViewerPanelLayer {
    private:
        void implOnNewFrame() final
        {
            m_IsHandlingMouseInputs = false;

        }

        bool implHandleKeyboardInputs(
            ModelViewerPanelParameters& params,
            ModelViewerPanelState& state) final
        {
            return ui::update_polar_camera_from_keyboard_inputs(
                params.updRenderParams().camera,
                state.viewportRect,
                state.maybeSceneAABB
            );
        }

        bool implHandleMouseInputs(
            ModelViewerPanelParameters& params,
            ModelViewerPanelState& state) final
        {
            m_IsHandlingMouseInputs = true;

            // try updating the camera (mouse panning, etc.)
            bool rv = ui::update_polar_camera_from_mouse_inputs(
                params.updRenderParams().camera,
                dimensions_of(state.viewportRect)
            );

            if (ui::is_mouse_dragging_with_any_button_down())
            {
                params.getModelSharedPtr()->setHovered(nullptr);
            }
            else if (state.maybeHoveredComponentAbsPath != GetAbsolutePathOrEmpty(params.getModelSharedPtr()->getHovered()))
            {
                // care: this code must check whether the hover != current hover
                // (even if null), because there might be multiple viewports open
                // (#582)
                params.getModelSharedPtr()->setHovered(
                    FindComponent(params.getModelSharedPtr()->getModel(), state.maybeHoveredComponentAbsPath)
                );
                rv = true;
            }

            // if left-clicked, update top-level model selection
            if (state.isLeftClickReleasedWithoutDragging)
            {
                params.getModelSharedPtr()->setSelected(
                    FindComponent(params.getModelSharedPtr()->getModel(), state.maybeHoveredComponentAbsPath)
                );
                rv = true;
            }

            return rv;
        }

        void implOnDraw(
            ModelViewerPanelParameters& params,
            ModelViewerPanelState& state) final
        {
            // hover, but not panning: show tooltip
            if (!state.maybeHoveredComponentAbsPath.toString().empty() &&
                m_IsHandlingMouseInputs &&
                !ui::is_mouse_dragging_with_any_button_down())
            {
                if (const OpenSim::Component* c = FindComponent(params.getModelSharedPtr()->getModel(), state.maybeHoveredComponentAbsPath))
                {
                    DrawComponentHoverTooltip(*c);
                }
            }

            // right-click: open context menu
            if (m_IsHandlingMouseInputs &&
                state.isRightClickReleasedWithoutDragging)
            {
                // right-click: pump a right-click event
                const ModelViewerPanelRightClickEvent e
                {
                    std::string{state.getPanelName()},
                    state.viewportRect,
                    state.maybeHoveredComponentAbsPath.toString(),
                    state.maybeBaseLayerHittest ? std::optional<Vec3>{state.maybeBaseLayerHittest->worldspace_location} : std::nullopt,
                };
                params.callOnRightClickHandler(e);
            }
        }

        bool implShouldClose() const final
        {
            return false;
        }

        bool m_IsHandlingMouseInputs = false;
    };
}

class osc::ModelViewerPanel::Impl final : public StandardPanelImpl {
public:

    Impl(
        std::string_view panelName_,
        ModelViewerPanelParameters parameters_,
        ModelViewerPanelFlags flags_) :

        StandardPanelImpl{panelName_},
        m_Parameters{std::move(parameters_)},
        m_State{name(), flags_}
    {
        // update this panel's rendering/state parameters from the runtime
        // configuration (e.g. user edits)
        //
        // each panel has its own configuration set (`panels/viewer0,1,2, etc.`)
        UpdModelRendererParamsFrom(
            App::settings(),
            GetSettingsKeyPrefixForPanel(panelName_),
            m_Parameters.updRenderParams()
        );
        pushLayer(std::make_unique<BaseInteractionLayer>());
        pushLayer(std::make_unique<ButtonAndGizmoControlsLayer>(panelName_, m_Parameters.getModelSharedPtr()));
    }

    bool isMousedOver() const
    {
        return m_RenderIsHovered;
    }

    bool isLeftClicked() const
    {
        return m_RenderIsHovered and m_State.isLeftClickReleasedWithoutDragging;
    }

    bool isRightClicked() const
    {
        return m_RenderIsHovered and m_State.isRightClickReleasedWithoutDragging;
    }

    ModelViewerPanelLayer& pushLayer(std::unique_ptr<ModelViewerPanelLayer> layer)
    {
        // care: do not push new layers directly into `m_Layers`, because `pushLayer` can be
        // called during iteration over `m_Layers` (e.g. during drawing)
        return m_State.pushLayer(std::move(layer));
    }

    void focusOn(const Vec3& pos)
    {
        m_Parameters.updRenderParams().camera.focus_point = -pos;
    }

    std::optional<Rect> getScreenRect() const
    {
        return m_State.viewportRect;
    }

    const PolarPerspectiveCamera& getCamera() const
    {
        return m_Parameters.getRenderParams().camera;
    }

    void setCamera(const PolarPerspectiveCamera& camera)
    {
        m_Parameters.updRenderParams().camera = camera;
    }

    void setModelState(const std::shared_ptr<IModelStatePair>& newModelState)
    {
        m_Parameters.setModelSharedPtr(newModelState);
    }

private:
    void impl_before_imgui_begin() final
    {
        ui::push_style_var(ui::StyleVar::WindowPadding, {0.0f, 0.0f});
    }

    void impl_after_imgui_begin() final
    {
        ui::pop_style_var();
    }

    void impl_draw_content() final
    {
        // HACK: garbage-collect one frame later, because the layers
        // may have submitted textures to ImGui that are then invalid
        // because GCing destroyed them before they were rendered
        layersGarbageCollect();

        m_State.viewportRect = ui::content_region_avail_as_screen_rect();
        m_State.isLeftClickReleasedWithoutDragging = ui::is_mouse_released_without_dragging(ui::MouseButton::Left);
        m_State.isRightClickReleasedWithoutDragging = ui::is_mouse_released_without_dragging(ui::MouseButton::Right);

        // if necessary, auto-focus the camera on the first frame
        if (m_IsFirstFrame)
        {
            m_State.updRenderer().autoFocusCamera(
                *m_Parameters.getModelSharedPtr(),
                m_Parameters.updRenderParams(),
                aspect_ratio_of(m_State.viewportRect)
            );
            m_IsFirstFrame = false;
        }

        layersOnNewFrame();

        // if the viewer is hovered, handle inputs
        if (m_RenderIsHovered)
        {
            layersHandleMouseInputs();

            if (!ui::wants_keyboard())
            {
                layersHandleKeyboardInputs();
            }
        }

        // render the 3D scene to a texture and present it via a ui::Image
        {
            RenderTexture& sceneTexture = m_State.updRenderer().onDraw(
                *m_Parameters.getModelSharedPtr(),
                m_Parameters.getRenderParams(),
                dimensions_of(m_State.viewportRect),
                App::get().anti_aliasing_level()
            );
            ui::draw_image(
                sceneTexture,
                dimensions_of(m_State.viewportRect)
            );

            // care: hittesting is done here, rather than using ui::is_panel_hovered, because
            // we care about whether the _render_ is hovered, not any part of the window (which
            // may include things like the title bar, etc.
            //
            // screwing this up can result in unusual camera behavior, e.g. the camera may move when
            // dragging a visualizer panel around (#739 #93)

            // check if the window is conditionally hovered: this returns true if no other window is
            // overlapping the editor panel, _but_ it also returns true if the user is only hovering
            // the title bar of the window, rather than specifically the render
            const bool windowHovered = ui::is_panel_hovered(ui::HoveredFlag::ChildWindows);

            // check if the 3D render is hovered - ignore blocking and overlapping because the layer
            // stack might be screwing with this
            const bool renderHoveredIgnoringOverlap = ui::is_item_hovered({
                ui::HoveredFlag::AllowWhenBlockedByActiveItem,
                ui::HoveredFlag::AllowWhenOverlapped,
            });

            m_RenderIsHovered = windowHovered && renderHoveredIgnoringOverlap;
        }

        // update state scene AABB
        m_State.maybeSceneAABB = m_State.getRenderer().bounds();

        // if hovering in 2D, 3D-hittest the scene
        if (m_RenderIsHovered and not (m_State.flags() & ModelViewerPanelFlag::NoHittest))
        {
            m_State.maybeBaseLayerHittest = m_State.getRenderer().getClosestCollision(
                m_Parameters.getRenderParams(),
                ui::get_mouse_pos(),
                m_State.viewportRect
            );
        }
        else
        {
            m_State.maybeBaseLayerHittest.reset();
        }

        // if there's a 3D-hit, transform it into an OpenSim-hit
        if (m_State.maybeBaseLayerHittest)
        {
            m_State.maybeHoveredComponentAbsPath = OpenSim::ComponentPath{std::string{m_State.maybeBaseLayerHittest->decoration_id}};
        }
        else
        {
            m_State.maybeHoveredComponentAbsPath = {};
        }

        layersDraw();
        layersPopQueuedNewLayers();
    }

    void layersOnNewFrame()
    {
        for (const auto& layerPtr : m_Layers)
        {
            layerPtr->onNewFrame();
        }
    }

    void layersHandleKeyboardInputs()
    {
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it)
        {
            if ((*it)->handleKeyboardInputs(m_Parameters, m_State))
            {
                return;
            }
        }
    }

    void layersHandleMouseInputs()
    {
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it)
        {
            if ((*it)->handleMouseInputs(m_Parameters, m_State) ||
                (*it)->getFlags() & ModelViewerPanelLayerFlags::CapturesMouseInputs)
            {
                return;
            }
        }
    }

    void layersDraw()
    {
        for (auto it = m_Layers.begin(); it != m_Layers.end(); ++it)
        {
            ModelViewerPanelLayer& layer = **it;

            ui::WindowFlags windowFlags = ui::get_minimal_panel_flags().without(ui::WindowFlag::NoInputs);

            // if any layer above this one captures mouse inputs then disable this layer's inputs
            if (find_if(it+1, m_Layers.end(), [](const auto& layerPtr) -> bool { return layerPtr->getFlags() & ModelViewerPanelLayerFlags::CapturesMouseInputs; }) != m_Layers.end())
            {
                windowFlags |= ui::WindowFlag::NoInputs;
            }

            // layers always have a background (although, it can be entirely invisible)
            windowFlags = windowFlags.without(ui::WindowFlag::NoBackground);
            ui::set_next_panel_bg_alpha(layer.getBackgroundAlpha());

            // draw the layer in a child window, so that ImGui understands that hittests
            // should happen window-by-window (otherwise, you'll have problems with overlapping
            // buttons, widgets, etc.)
            ui::set_next_panel_pos(m_State.viewportRect.p1);
            const std::string childID = std::to_string(std::distance(it, m_Layers.end()));
            if (ui::begin_child_panel(childID, dimensions_of(m_State.viewportRect), ui::ChildPanelFlags{}, windowFlags))
            {
                layer.onDraw(m_Parameters, m_State);
                ui::end_child_panel();
            }
        }
    }

    void layersGarbageCollect()
    {
        std::erase_if(m_Layers, [](const auto& layerPtr)
        {
            return layerPtr->shouldClose();
        });
    }

    void layersPopQueuedNewLayers()
    {
        m_State.flushLayerQueueTo(m_Layers);
    }

    ModelViewerPanelParameters m_Parameters;
    ModelViewerPanelState m_State;
    std::vector<std::unique_ptr<ModelViewerPanelLayer>> m_Layers;
    bool m_IsFirstFrame = true;
    bool m_RenderIsHovered = false;
};


osc::ModelViewerPanel::ModelViewerPanel(
    std::string_view panelName_,
    const ModelViewerPanelParameters& parameters_,
    ModelViewerPanelFlags flags_) :

    m_Impl{std::make_unique<Impl>(panelName_, parameters_, flags_)}
{}
osc::ModelViewerPanel::ModelViewerPanel(ModelViewerPanel&&) noexcept = default;
osc::ModelViewerPanel& osc::ModelViewerPanel::operator=(ModelViewerPanel&&) noexcept = default;
osc::ModelViewerPanel::~ModelViewerPanel() noexcept = default;

bool osc::ModelViewerPanel::isMousedOver() const
{
    return m_Impl->isMousedOver();
}

bool osc::ModelViewerPanel::isLeftClicked() const
{
    return m_Impl->isLeftClicked();
}

bool osc::ModelViewerPanel::isRightClicked() const
{
    return m_Impl->isRightClicked();
}

ModelViewerPanelLayer& osc::ModelViewerPanel::pushLayer(std::unique_ptr<ModelViewerPanelLayer> layer)
{
    return m_Impl->pushLayer(std::move(layer));
}

void osc::ModelViewerPanel::focusOn(const Vec3& pos)
{
    m_Impl->focusOn(pos);
}

std::optional<Rect> osc::ModelViewerPanel::getScreenRect() const
{
    return m_Impl->getScreenRect();
}

const PolarPerspectiveCamera& osc::ModelViewerPanel::getCamera() const
{
    return m_Impl->getCamera();
}

void osc::ModelViewerPanel::setCamera(const PolarPerspectiveCamera& camera)
{
    m_Impl->setCamera(camera);
}

void osc::ModelViewerPanel::setModelState(const std::shared_ptr<IModelStatePair>& newModelState)
{
    m_Impl->setModelState(newModelState);
}

CStringView osc::ModelViewerPanel::impl_get_name() const
{
    return m_Impl->name();
}

bool osc::ModelViewerPanel::impl_is_open() const
{
    return m_Impl->is_open();
}

void osc::ModelViewerPanel::impl_open()
{
    m_Impl->open();
}

void osc::ModelViewerPanel::impl_close()
{
    m_Impl->close();
}

void osc::ModelViewerPanel::impl_on_draw()
{
    m_Impl->on_draw();
}