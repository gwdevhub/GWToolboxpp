#pragma once

#include <ToolboxWindow.h>
#include <Utils/TeamBuild.h>

namespace GW::UI {
    enum class UIMessage : uint32_t;
}

class BuildsWindow : public ToolboxWindow {
    BuildsWindow() : ToolboxWindow() { show_menubutton = can_show_in_main_window; }
    ~BuildsWindow() override = default;
public:
    static BuildsWindow& Instance()
    {
        static BuildsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Builds"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_LIST; }

    struct Settings {
        bool order_by_name = false;
        bool auto_load_pcons = true;
        bool auto_send_pcons = true;
        bool hide_when_entering_explorable = false;
        bool one_teambuild_at_a_time = false;
    };

    // On-disk schema of builds.json
    struct BuildEntry {
        std::string name{};
        std::string code{};
        std::vector<std::string> pcons{};
    };
    struct TeamBuildEntry {
        std::string name{};
        std::string ui_id{};
        bool show_numbers = true;
        std::vector<BuildEntry> builds{};
    };
    struct BuildsFile {
        std::vector<std::string> preferred_skill_orders{};
        std::vector<TeamBuildEntry> teambuilds{};
    };

    void Initialize() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawHelp() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    // Add a player-layout teambuild to this window's list (e.g. from a received chat link).
    void AddTeambuild(TeamBuild tbuild);

private:
};
