#pragma once

#include <oscar/UI/oscimgui.h>

#include <memory>

namespace osc { struct PolarPerspectiveCamera; }
namespace osc { struct Rect; }
namespace osc { class UndoableModelStatePair; }

namespace osc
{
    class ModelSelectionGizmo final {
    public:
        explicit ModelSelectionGizmo(std::shared_ptr<UndoableModelStatePair>);
        ModelSelectionGizmo(const ModelSelectionGizmo&);
        ModelSelectionGizmo(ModelSelectionGizmo&&) noexcept;
        ModelSelectionGizmo& operator=(const ModelSelectionGizmo&);
        ModelSelectionGizmo& operator=(ModelSelectionGizmo&&) noexcept;
        ~ModelSelectionGizmo() noexcept;

        bool isUsing() const;
        bool isOver() const;

        bool handleKeyboardInputs();
        void onDraw(const Rect& screenRect, const PolarPerspectiveCamera&);

        ImGuizmo::OPERATION getOperation() const
        {
            return m_GizmoOperation;
        }
        void setOperation(ImGuizmo::OPERATION newOperation)
        {
            m_GizmoOperation = newOperation;
        }

        ImGuizmo::MODE getMode() const
        {
            return m_GizmoMode;
        }
        void setMode(ImGuizmo::MODE newMode)
        {
            m_GizmoMode = newMode;
        }

    private:
        std::shared_ptr<UndoableModelStatePair> m_Model;
        ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE m_GizmoMode = ImGuizmo::WORLD;
        bool m_WasUsingGizmoLastFrame = false;
    };
}
