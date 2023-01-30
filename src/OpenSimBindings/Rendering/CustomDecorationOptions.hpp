#pragma once

#include "src/OpenSimBindings/Rendering/MuscleColoringStyle.hpp"
#include "src/OpenSimBindings/Rendering/MuscleDecorationStyle.hpp"
#include "src/OpenSimBindings/Rendering/MuscleSizingStyle.hpp"

namespace osc
{
    class CustomDecorationOptions final {
    public:
        MuscleDecorationStyle getMuscleDecorationStyle() const
        {
            return m_MuscleDecorationStyle;
        }
        void setMuscleDecorationStyle(MuscleDecorationStyle s)
        {
            m_MuscleDecorationStyle = s;
        }

        MuscleColoringStyle getMuscleColoringStyle() const
        {
            return m_MuscleColoringStyle;
        }
        void setMuscleColoringStyle(MuscleColoringStyle s)
        {
            m_MuscleColoringStyle = s;
        }

        MuscleSizingStyle getMuscleSizingStyle() const
        {
            return m_MuscleSizingStyle;
        }
        void setMuscleSizingStyle(MuscleSizingStyle s)
        {
            m_MuscleSizingStyle = s;
        }

        bool getShouldShowScapulo() const
        {
            return m_ShouldShowScapulo;
        }
        void setShouldShowScapulo(bool v)
        {
            m_ShouldShowScapulo = v;
        }

        bool getShouldShowEffectiveMuscleLinesOfAction() const
        {
            return m_ShouldShowEffectiveMuscleLinesOfAction;
        }
        void setShouldShowEffectiveMuscleLinesOfAction(bool v)
        {
            m_ShouldShowEffectiveMuscleLinesOfAction = v;
        }

        bool getShouldShowAnatomicalMuscleLinesOfAction() const
        {
            return m_ShouldShowAnatomicalMuscleLinesOfAction;
        }
        void setShouldShowAnatomicalMuscleLinesOfAction(bool v)
        {
            m_ShouldShowAnatomicalMuscleLinesOfAction = v;
        }

        bool getShouldShowCentersOfMass() const
        {
            return m_ShouldShowCentersOfMass;
        }
        void setShouldShowCentersOfMass(bool v)
        {
            m_ShouldShowCentersOfMass = v;
        }

        bool getShouldShowPointToPointSprings() const
        {
            return m_ShouldShowPointToPointSprings;
        }
        void setShouldShowPointToPointSprings(bool v)
        {
            m_ShouldShowPointToPointSprings = v;
        }

    private:
        friend bool operator==(CustomDecorationOptions const&, CustomDecorationOptions const&);
        friend bool operator!=(CustomDecorationOptions const&, CustomDecorationOptions const&);

        MuscleDecorationStyle m_MuscleDecorationStyle = MuscleDecorationStyle::Default;
        MuscleColoringStyle m_MuscleColoringStyle = MuscleColoringStyle::Default;
        MuscleSizingStyle m_MuscleSizingStyle = MuscleSizingStyle::Default;
        bool m_ShouldShowScapulo = false;
        bool m_ShouldShowEffectiveMuscleLinesOfAction = false;
        bool m_ShouldShowAnatomicalMuscleLinesOfAction = false;
        bool m_ShouldShowCentersOfMass = false;
        bool m_ShouldShowPointToPointSprings = true;
    };

    inline bool operator==(CustomDecorationOptions const& a, CustomDecorationOptions const& b)
    {
        return
            a.m_MuscleDecorationStyle == b.m_MuscleDecorationStyle &&
            a.m_MuscleColoringStyle == b.m_MuscleColoringStyle &&
            a.m_MuscleSizingStyle == b.m_MuscleSizingStyle &&
            a.m_ShouldShowScapulo == b.m_ShouldShowScapulo &&
            a.m_ShouldShowEffectiveMuscleLinesOfAction == b.m_ShouldShowEffectiveMuscleLinesOfAction &&
            a.m_ShouldShowAnatomicalMuscleLinesOfAction == b.m_ShouldShowAnatomicalMuscleLinesOfAction &&
            a.m_ShouldShowCentersOfMass == b.m_ShouldShowCentersOfMass &&
            a.m_ShouldShowPointToPointSprings == b.m_ShouldShowPointToPointSprings;
    }

    inline bool operator!=(CustomDecorationOptions const& a, CustomDecorationOptions const& b)
    {
        return !(a == b);
    }
}