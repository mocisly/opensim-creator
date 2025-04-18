#pragma once

#include <libopensimcreator/Documents/Simulation/ISimulation.h>
#include <libopensimcreator/Documents/Simulation/SimulationClock.h>
#include <libopensimcreator/Documents/Simulation/SimulationReport.h>
#include <libopensimcreator/Documents/Simulation/SimulationStatus.h>

#include <liboscar/Utils/SynchronizedValueGuard.h>

#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace osc { class Environment; }
namespace osc { class OutputExtractor; }
namespace osc { class ParamBlock; }
namespace OpenSim { class Model; }
namespace SimTK { class State; }

namespace osc
{
    // a concrete value type wrapper for an `ISimulation`
    //
    // This is a value-type that can be compared, hashed, etc. for easier usage
    // by other parts of osc (e.g. aggregators, plotters)
    class Simulation final {
    public:
        template<std::derived_from<ISimulation> TConcreteSimulation>
        Simulation(TConcreteSimulation&& simulation) :
            m_Simulation{std::make_unique<TConcreteSimulation>(std::forward<TConcreteSimulation>(simulation))}
        {}

        SynchronizedValueGuard<const OpenSim::Model> getModel() const { return m_Simulation->getModel(); }

        size_t getNumReports() const { return m_Simulation->getNumReports(); }
        SimulationReport getSimulationReport(ptrdiff_t reportIndex) const { return m_Simulation->getSimulationReport(std::move(reportIndex)); }
        std::vector<SimulationReport> getAllSimulationReports() const { return m_Simulation->getAllSimulationReports(); }

        SimulationStatus getStatus() const { return m_Simulation->getStatus(); }
        SimulationClock::time_point getCurTime() { return m_Simulation->getCurTime(); }
        SimulationClock::time_point getStartTime() const { return m_Simulation->getStartTime(); }
        SimulationClock::time_point getEndTime() const { return m_Simulation->getEndTime(); }
        SimulationClock::duration   getDuration() const { return m_Simulation->getDuration(); }
        float getProgress() const { return m_Simulation->getProgress(); }
        const ParamBlock& getParams() const { return m_Simulation->getParams(); }
        std::span<const OutputExtractor> getOutputs() const { return m_Simulation->getOutputExtractors(); }

        bool canChangeEndTime() const { return m_Simulation->canChangeEndTime(); }
        void requestNewEndTime(SimulationClock::time_point t) { m_Simulation->requestNewEndTime(t); }

        void requestStop() { m_Simulation->requestStop(); }
        void stop() { m_Simulation->stop(); }

        float getFixupScaleFactor() const { return m_Simulation->getFixupScaleFactor(); }
        void setFixupScaleFactor(float v) { m_Simulation->setFixupScaleFactor(v); }

        std::shared_ptr<Environment> tryUpdEnvironment() { return m_Simulation->tryUpdEnvironment(); }

        operator ISimulation& () { return *m_Simulation; }
        operator const ISimulation& () const { return *m_Simulation; }

    private:
        std::unique_ptr<ISimulation> m_Simulation;
    };
}
