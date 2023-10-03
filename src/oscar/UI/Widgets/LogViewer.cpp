#include "LogViewer.hpp"

#include <oscar/Bindings/ImGuiHelpers.hpp>
#include <oscar/Platform/App.hpp>
#include <oscar/Platform/Log.hpp>
#include <oscar/Platform/os.hpp>
#include <oscar/Utils/CircularBuffer.hpp>
#include <oscar/Utils/SynchronizedValue.hpp>

#include <imgui.h>

#include <string>
#include <sstream>
#include <type_traits>
#include <utility>

namespace
{
    ImVec4 ToColor(osc::LogLevel lvl)
    {
        switch (lvl)
        {
        case osc::LogLevel::trace:
            return ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
        case osc::LogLevel::debug:
            return ImVec4{0.8f, 0.8f, 0.8f, 1.0f};
        case osc::LogLevel::info:
            return ImVec4{0.5f, 0.5f, 1.0f, 1.0f};
        case osc::LogLevel::warn:
            return ImVec4{1.0f, 1.0f, 0.0f, 1.0f};
        case osc::LogLevel::err:
            return ImVec4{1.0f, 0.0f, 0.0f, 1.0f};
        case osc::LogLevel::critical:
            return ImVec4{1.0f, 0.0f, 0.0f, 1.0f};
        default:
            return ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
        }
    }

    void copyTracebackLogToClipboard()
    {
        std::stringstream ss;

        auto& guarded_content = osc::log::getTracebackLog();
        {
            auto const& content = guarded_content.lock();
            for (osc::LogMessage const& msg : *content)
            {
                ss << '[' << ToCStringView(msg.level) << "] " << msg.payload << '\n';
            }
        }

        std::string full_log_content = std::move(ss).str();

        osc::SetClipboardText(full_log_content.c_str());
    }
}

class osc::LogViewer::Impl final {
public:
    void onDraw()
    {
        auto logger = log::defaultLogger();
        if (!logger)
        {
            return;
        }

        // draw top menu bar
        if (ImGui::BeginMenuBar())
        {
            // draw level selector
            {
                LogLevel currentLevel = logger->get_level();
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::BeginCombo("level", ToCStringView(currentLevel).c_str()))
                {
                    for (LogLevel l = FirstLogLevel(); l <= LastLogLevel(); l = Next(l))
                    {
                        bool active = l == currentLevel;
                        if (ImGui::Selectable(ToCStringView(l).c_str(), &active))
                        {
                            logger->set_level(l);
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::SameLine();
            ImGui::Checkbox("autoscroll", &autoscroll);

            ImGui::SameLine();
            if (ImGui::Button("clear"))
            {
                log::getTracebackLog().lock()->clear();
            }
            App::upd().addFrameAnnotation("LogClearButton", osc::GetItemRect());

            ImGui::SameLine();
            if (ImGui::Button("turn off"))
            {
                logger->set_level(LogLevel::off);
            }

            ImGui::SameLine();
            if (ImGui::Button("copy to clipboard"))
            {
                copyTracebackLogToClipboard();
            }

            ImGui::Dummy({0.0f, 10.0f});

            ImGui::EndMenuBar();
        }

        // draw log content lines
        auto& guardedContent = log::getTracebackLog();
        auto const& contentGuard = guardedContent.lock();
        for (LogMessage const& msg : *contentGuard)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ToColor(msg.level));
            ImGui::Text("[%s]", ToCStringView(msg.level).c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", msg.payload.c_str());

            if (autoscroll)
            {
                ImGui::SetScrollHereY();
            }
        }
    }
private:
    bool autoscroll = true;
};


// public API

osc::LogViewer::LogViewer() :
    m_Impl{std::make_unique<Impl>()}
{
}
osc::LogViewer::LogViewer(LogViewer&&) noexcept = default;
osc::LogViewer& osc::LogViewer::operator=(LogViewer&&) noexcept = default;
osc::LogViewer::~LogViewer() noexcept = default;

void osc::LogViewer::onDraw()
{
    m_Impl->onDraw();
}