#pragma once

#include <oscar/UI/Widgets/SaveChangesPopup.h>

#include <filesystem>
#include <vector>
#include <optional>

namespace osc { class IMainUIStateAPI; }
namespace osc { template<typename T> class ParentPtr; }
namespace osc { class UndoableModelStatePair; }

namespace osc
{
    class MainMenuFileTab final {
    public:
        MainMenuFileTab();

        void onDraw(const ParentPtr<IMainUIStateAPI>&, UndoableModelStatePair* = nullptr);

        std::vector<std::filesystem::path> exampleOsimFiles;
        std::optional<SaveChangesPopup> maybeSaveChangesPopup;
    };

    class MainMenuAboutTab final {
    public:
        MainMenuAboutTab() {}

        void onDraw();
    };
}
