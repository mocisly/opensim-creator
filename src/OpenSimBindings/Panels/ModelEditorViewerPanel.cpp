#include "ModelEditorViewerPanel.hpp"

#include "src/OpenSimBindings/MiddlewareAPIs/EditorAPI.hpp"
#include "src/OpenSimBindings/Widgets/BasicWidgets.hpp"
#include "src/OpenSimBindings/Widgets/ComponentContextMenu.hpp"
#include "src/OpenSimBindings/Widgets/UiModelViewer.hpp"
#include "src/OpenSimBindings/OpenSimHelpers.hpp"
#include "src/OpenSimBindings/UndoableModelStatePair.hpp"
#include "src/Panels/StandardPanel.hpp"

#include <imgui.h>
#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentPath.h>

#include <memory>
#include <string_view>
#include <utility>

class osc::ModelEditorViewerPanel::Impl final : public osc::StandardPanel {
public:

    Impl(
        std::string_view panelName,
        MainUIStateAPI* mainUIStateAPI,
        EditorAPI* editorAPI,
        std::shared_ptr<UndoableModelStatePair> model) :

        StandardPanel{std::move(panelName)},
        m_MainUIStateAPI{mainUIStateAPI},
        m_EditorAPI{editorAPI},
        m_Model{std::move(model)}
    {
    }

private:
    void implBeforeImGuiBegin() final
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    }

    void implAfterImGuiBegin() final
    {
        ImGui::PopStyleVar();
    }

    void implDrawContent() final
    {
        std::optional<SceneCollision> const maybeCollision = m_Viewer.draw(*m_Model);

        OpenSim::Component const* maybeHover = maybeCollision ?
            osc::FindComponent(m_Model->getModel(), maybeCollision->decorationID) :
            nullptr;

        if (maybeHover)
        {
            // hovering: update hover and show tooltip
            m_Model->setHovered(maybeHover);
            DrawComponentHoverTooltip(*maybeHover);
        }
        else
        {
            m_Model->setHovered(nullptr);
        }

        if (m_Viewer.isLeftClicked())
        {
            // left-click: (de)select hover
            m_Model->setSelected(maybeHover);
        }

        if (m_Viewer.isRightClicked())
        {
            // right-click: draw a context menu
            std::string menuName = std::string{getName()} + "_contextmenu";
            OpenSim::ComponentPath path = osc::GetAbsolutePathOrEmpty(maybeHover);
            m_EditorAPI->pushPopup(std::make_unique<ComponentContextMenu>(menuName, m_MainUIStateAPI, m_EditorAPI, m_Model, path));
        }
    }

    MainUIStateAPI* m_MainUIStateAPI;
    EditorAPI* m_EditorAPI;
    std::shared_ptr<UndoableModelStatePair> m_Model;
    UiModelViewer m_Viewer;
};


// public API (PIMPL)

osc::ModelEditorViewerPanel::ModelEditorViewerPanel(
    std::string_view panelName,
    MainUIStateAPI* mainUIStateAPI,
    EditorAPI* editorAPI,
    std::shared_ptr<UndoableModelStatePair> model) :

    m_Impl{std::make_unique<Impl>(std::move(panelName), mainUIStateAPI, editorAPI, std::move(model))}
{
}
osc::ModelEditorViewerPanel::ModelEditorViewerPanel(ModelEditorViewerPanel&&) noexcept = default;
osc::ModelEditorViewerPanel& osc::ModelEditorViewerPanel::operator=(ModelEditorViewerPanel&&) noexcept = default;
osc::ModelEditorViewerPanel::~ModelEditorViewerPanel() noexcept = default;

osc::CStringView osc::ModelEditorViewerPanel::implGetName() const
{
    return m_Impl->getName();
}

bool osc::ModelEditorViewerPanel::implIsOpen() const
{
    return m_Impl->isOpen();
}

void osc::ModelEditorViewerPanel::implOpen()
{
    m_Impl->open();
}

void osc::ModelEditorViewerPanel::implClose()
{
    m_Impl->close();
}

void osc::ModelEditorViewerPanel::implDraw()
{
    m_Impl->draw();
}