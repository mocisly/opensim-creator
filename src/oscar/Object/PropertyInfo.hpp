#pragma once

#include <oscar/Utils/StringName.hpp>
#include <oscar/Variant/Variant.hpp>
#include <oscar/Variant/VariantType.hpp>

#include <string_view>

namespace osc
{
    class PropertyInfo final {
    public:
        PropertyInfo() = default;

        PropertyInfo(
            std::string_view name_,
            Variant defaultValue_) :

            PropertyInfo{StringName{name_}, std::move(defaultValue_)}
        {
        }

        PropertyInfo(
            StringName const& name_,
            Variant defaultValue_
        );

        StringName const& getName() const
        {
            return m_Name;
        }

        VariantType getType() const
        {
            return m_DefaultValue.getType();
        }

        Variant const& getDefaultValue() const
        {
            return m_DefaultValue;
        }

        friend bool operator==(PropertyInfo const&, PropertyInfo const&) = default;

    private:
        StringName m_Name;
        Variant m_DefaultValue;
    };
}