#pragma once

#include <GWCA/Constants/Skills.h>

#include <GWCA/Managers/SkillbarMgr.h>

#include <Utils/GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

#include <Defines.h>

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

    void Initialize() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawHelp() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

private:
    /*
    // Send a teambuild
    static void Send(const TeamBuild& tbuild);
    // Send a specific build from a teambuild
    static void Send(const TeamBuild& tbuild, unsigned int idx);
    static void SendPcons(const TeamBuild& tbuild, unsigned int idx, bool include_build_name = true);
    // Load a specific build from a teambuild (and any applicable pcons)
    static void Load(const TeamBuild& tbuild, unsigned int idx) const;
    // Toggle pcons for a specific build
    static void LoadPcons(const TeamBuild& tbuild, unsigned int idx) const;
    // View a specific build from a teambuild
    static void View(const TeamBuild& tbuild, unsigned int idx);
    // Load build by name or code, without specific teambuild assigned.
    static void Load(const char* build_name);
    // Load build by teambuild name and build name
    static void Load(const char* tbuild_name, const char* build_name);

    static bool BuildSkillTemplateString(const TeamBuild& tbuild, unsigned int idx, char* out, unsigned int out_len);

    static void DrawBuildSection(TeamBuild& tbuild, unsigned int idx);
    
    // Attempt to add a preferred build by code and name
    static const char* AddPreferredBuild(const char* code);

    static bool GetCurrentSkillBar(char* out, size_t out_len);*/
};
