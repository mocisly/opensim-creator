#pragma once

#include <oscar/UI/Panels/IPanel.h>
#include <oscar/Utils/CStringView.h>

#include <memory>
#include <string>
#include <string_view>

namespace OpenSim { class ComponentPath; }
namespace osc { class UndoableModelStatePair; }
namespace osc { class Widget; }

namespace osc
{
    class ModelMusclePlotPanel final : public IPanel {
    public:
        ModelMusclePlotPanel(
            Widget&,
            std::shared_ptr<UndoableModelStatePair>,
            std::string_view panelName
        );
        ModelMusclePlotPanel(
            Widget&,
            std::shared_ptr<UndoableModelStatePair>,
            std::string_view panelName,
            const OpenSim::ComponentPath& coordPath,
            const OpenSim::ComponentPath& musclePath
        );
        ModelMusclePlotPanel(const ModelMusclePlotPanel&) = delete;
        ModelMusclePlotPanel(ModelMusclePlotPanel&&) noexcept;
        ModelMusclePlotPanel& operator=(const ModelMusclePlotPanel&) = delete;
        ModelMusclePlotPanel& operator=(ModelMusclePlotPanel&&) noexcept;
        ~ModelMusclePlotPanel() noexcept;

    private:
        CStringView impl_get_name() const;
        bool impl_is_open() const;
        void impl_open();
        void impl_close();
        void impl_on_draw();

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
