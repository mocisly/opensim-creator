#include "TabTestingScreen.h"

#include <oscar/Platform/App.h>
#include <oscar/UI/Tabs/TabRegistryEntry.h>
#include <oscar/UI/Tabs/ITabHost.h>
#include <oscar/Utils/ParentPtr.h>

#include <SDL_events.h>
#include <oscar/UI/ui_context.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class osc::TabTestingScreen::Impl final :
    public std::enable_shared_from_this<Impl>,
    public IScreen,
    public ITabHost {
public:
    explicit Impl(TabRegistryEntry registry_entry) :
        registry_entry_{std::move(registry_entry)}
    {}

private:
    void impl_on_mount() override
    {
        current_tab_ = registry_entry_.construct_tab(ParentPtr<Impl>{shared_from_this()});
        ui::context::init();
        current_tab_->on_mount();
        App::upd().make_main_loop_polling();
    }

    void impl_on_unmount() override
    {
        App::upd().make_main_loop_waiting();
        current_tab_->on_unmount();
        ui::context::shutdown();
    }

    bool impl_on_event(const SDL_Event& e) override
    {
        bool handled = ui::context::on_event(e);
        handled = current_tab_->on_event(e) || handled;
        return handled;
    }

    void impl_on_tick() override
    {
        current_tab_->on_tick();
    }

    void impl_on_draw() override
    {
        App::upd().clear_screen();
        ui::context::on_start_new_frame();
        current_tab_->on_draw();
        ui::context::render();

        ++frames_shown_;
        if (frames_shown_ >= min_frames_shown_ and
            App::get().frame_start_time() >= close_time_) {

            App::upd().request_quit();
        }
    }

    UID impl_add_tab(std::unique_ptr<ITab>) override { return UID{}; }
    void impl_select_tab(UID) override {}
    void impl_close_tab(UID) override {}
    void impl_reset_imgui() override {}

    TabRegistryEntry registry_entry_;
    std::unique_ptr<ITab> current_tab_;
    size_t min_frames_shown_ = 2;
    size_t frames_shown_ = 0;
    AppClock::duration min_open_duration_ = AppSeconds{0};
    AppClock::time_point close_time_ = App::get().frame_start_time() + min_open_duration_;
};

osc::TabTestingScreen::TabTestingScreen(const TabRegistryEntry& registry_entry) :
    impl_{std::make_shared<Impl>(registry_entry)}
{}
void osc::TabTestingScreen::impl_on_mount() { impl_->on_mount(); }
void osc::TabTestingScreen::impl_on_unmount() { impl_->on_unmount(); }
bool osc::TabTestingScreen::impl_on_event(const SDL_Event& e) { return impl_->on_event(e); }
void osc::TabTestingScreen::impl_on_tick() { impl_->on_tick(); }
void osc::TabTestingScreen::impl_on_draw() { impl_->on_draw(); }
