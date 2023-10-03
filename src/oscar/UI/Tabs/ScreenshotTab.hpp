#pragma once

#include <oscar/UI/Tabs/Tab.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/UID.hpp>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { struct Screenshot; }
namespace osc { class TabHost; }

namespace osc
{
    class ScreenshotTab final : public Tab {
    public:
        ScreenshotTab(ParentPtr<TabHost> const&, Screenshot&&);
        ScreenshotTab(ScreenshotTab const&) = delete;
        ScreenshotTab(ScreenshotTab&&) noexcept;
        ScreenshotTab& operator=(ScreenshotTab const&) = delete;
        ScreenshotTab& operator=(ScreenshotTab&&) noexcept;
        ~ScreenshotTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnDrawMainMenu() final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}