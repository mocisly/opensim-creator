#pragma once

#include <oscar/UI/Panels/IPanel.h>
#include <oscar/Utils/CStringView.h>

#include <memory>
#include <string_view>

namespace osc { class Simulation; }
namespace osc { class ISimulatorUIAPI; }

namespace osc
{
    class SimulationDetailsPanel final : public IPanel {
    public:
        SimulationDetailsPanel(
            std::string_view panelName,
            ISimulatorUIAPI*,
            std::shared_ptr<const Simulation>
        );
        SimulationDetailsPanel(const SimulationDetailsPanel&) = delete;
        SimulationDetailsPanel(SimulationDetailsPanel&&) noexcept;
        SimulationDetailsPanel& operator=(const SimulationDetailsPanel&) = delete;
        SimulationDetailsPanel& operator=(SimulationDetailsPanel&&) noexcept;
        ~SimulationDetailsPanel() noexcept;

    private:
        CStringView impl_get_name() const final;
        bool impl_is_open() const final;
        void impl_open() final;
        void impl_close() final;
        void impl_on_draw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
