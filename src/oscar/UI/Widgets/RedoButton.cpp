#include "RedoButton.h"

#include <oscar/UI/oscimgui.h>
#include <oscar/Utils/UndoRedo.h>

#include <IconsFontAwesome5.h>

#include <memory>

osc::RedoButton::RedoButton(std::shared_ptr<UndoRedoBase> undo_redo) :
    undo_redo_{std::move(undo_redo)}
{}

osc::RedoButton::~RedoButton() noexcept = default;

void osc::RedoButton::onDraw()
{
    int ui_id = 0;

    ui::push_style_var(ImGuiStyleVar_ItemSpacing, {0.0f, 0.0f});

    bool was_disabled = false;
    if (not undo_redo_->can_redo()) {
        ui::begin_disabled();
        was_disabled = true;
    }
    if (ui::draw_button(ICON_FA_REDO)) {
        undo_redo_->redo();
    }

    ui::same_line();

    ui::push_style_var(ImGuiStyleVar_FramePadding, {0.0f, ui::get_style_frame_padding().y});
    ui::draw_button(ICON_FA_CARET_DOWN);
    ui::pop_style_var();

    if (was_disabled) {
        ui::end_disabled();
    }

    if (ui::begin_popup_context_menu("##OpenRedoMenu", ImGuiPopupFlags_MouseButtonLeft)) {
        for (ptrdiff_t i = 0; i < undo_redo_->num_redo_entriesi(); ++i) {
            ui::push_id(ui_id++);
            if (ui::draw_selectable(undo_redo_->redo_entry_at(i).message())) {
                undo_redo_->redo_to(i);
            }
            ui::pop_id();
        }
        ui::end_popup();
    }

    ui::pop_style_var();
}
