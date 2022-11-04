#include "UndoRedoPanel.hpp"

#include "src/Utils/UndoRedo.hpp"
#include "src/Widgets/NamedPanel.hpp"

#include <imgui.h>

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

class osc::UndoRedoPanel::Impl final : public osc::NamedPanel {
public:
    Impl(
        std::string_view panelName_,
        std::shared_ptr<osc::UndoRedo> storage_) :

        NamedPanel{std::move(panelName_)},
        m_Storage{std::move(storage_)}
    {
    }

private:
    void implDraw() override
    {
        if (ImGui::Button("undo"))
        {
            m_Storage->undo();
        }

        ImGui::SameLine();

        if (ImGui::Button("redo"))
        {
            m_Storage->redo();
        }

        int imguiID = 0;

        // draw undo entries oldest (highest index) to newest (lowest index)
        for (ptrdiff_t i = m_Storage->getNumUndoEntriesi()-1; 0 <= i && i < m_Storage->getNumUndoEntriesi(); --i)
        {
            ImGui::PushID(imguiID++);
            if (ImGui::Selectable(m_Storage->getUndoEntry(i).getMessage().c_str()))
            {
                m_Storage->undoTo(i);
            }
            ImGui::PopID();
        }

        ImGui::PushID(imguiID++);
        ImGui::Text("  %s", m_Storage->getHead().getMessage().c_str());
        ImGui::PopID();

        // draw redo entries oldest (lowest index) to newest (highest index)
        for (size_t i = 0; i < m_Storage->getNumRedoEntries(); ++i)
        {
            ImGui::PushID(imguiID++);
            if (ImGui::Selectable(m_Storage->getRedoEntry(i).getMessage().c_str()))
            {
                m_Storage->redoTo(i);
            }
            ImGui::PopID();
        }
    }

    std::shared_ptr<osc::UndoRedo> m_Storage;
};

// public API (PIMPL)

osc::UndoRedoPanel::UndoRedoPanel(std::string_view panelName_, std::shared_ptr<osc::UndoRedo> storage_) :
    m_Impl{new Impl{std::move(panelName_), std::move(storage_)}}
{
}

osc::UndoRedoPanel::UndoRedoPanel(UndoRedoPanel&& tmp) noexcept :
    m_Impl{std::exchange(tmp.m_Impl, nullptr)}
{
}

osc::UndoRedoPanel& osc::UndoRedoPanel::operator=(UndoRedoPanel&& tmp) noexcept
{
    std::swap(m_Impl, tmp.m_Impl);
    return *this;
}

osc::UndoRedoPanel::~UndoRedoPanel() noexcept
{
    delete m_Impl;
}

void osc::UndoRedoPanel::draw()
{
    m_Impl->draw();
}