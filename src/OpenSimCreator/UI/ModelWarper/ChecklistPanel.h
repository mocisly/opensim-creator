#pragma once

#include <OpenSimCreator/UI/ModelWarper/UIState.h>

#include <oscar/UI/Panels/StandardPanelImpl.h>

#include <memory>
#include <string_view>
#include <utility>

namespace OpenSim { class Frame; }
namespace OpenSim { class Mesh; }

namespace osc::mow
{
    class ChecklistPanel final : public StandardPanelImpl {
    public:
        ChecklistPanel(
            std::string_view panelName_,
            std::shared_ptr<UIState> state_) :

            StandardPanelImpl{panelName_},
            m_State{std::move(state_)}
        {}
    private:
        void impl_draw_content() final;

        std::shared_ptr<UIState> m_State;
    };
}
