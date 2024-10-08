#pragma once

#include <OpenSimCreator/UI/Shared/ChooseComponentsEditorLayerParameters.h>
#include <OpenSimCreator/UI/Shared/ModelEditorViewerPanelLayer.h>

#include <memory>

namespace osc { class IModelStatePair; }
namespace osc { class ModelEditorViewerPanelParameters; }
namespace osc { class ModelEditorViewerPanelState; }

namespace osc
{
    // modal popup that prompts the user to select components in the model (e.g.
    // to define an edge, or a frame)
    class ChooseComponentsEditorLayer final : public ModelEditorViewerPanelLayer {
    public:
        ChooseComponentsEditorLayer(
            std::shared_ptr<IModelStatePair>,
            ChooseComponentsEditorLayerParameters
        );
        ChooseComponentsEditorLayer(const ChooseComponentsEditorLayer&) = delete;
        ChooseComponentsEditorLayer(ChooseComponentsEditorLayer&&) noexcept;
        ChooseComponentsEditorLayer& operator=(const ChooseComponentsEditorLayer&) = delete;
        ChooseComponentsEditorLayer& operator=(ChooseComponentsEditorLayer&&) noexcept;
        ~ChooseComponentsEditorLayer() noexcept;

    private:
        bool implHandleKeyboardInputs(
            ModelEditorViewerPanelParameters&,
            ModelEditorViewerPanelState&
        ) final;

        bool implHandleMouseInputs(
            ModelEditorViewerPanelParameters&,
            ModelEditorViewerPanelState&
        ) final;

        void implOnDraw(
            ModelEditorViewerPanelParameters&,
            ModelEditorViewerPanelState&
        ) final;

        float implGetBackgroundAlpha() const final;

        bool implShouldClose() const final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
