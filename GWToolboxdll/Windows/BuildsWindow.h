#pragma once

#include <GWCA/Constants/Skills.h>

#include <GWCA/Managers/SkillbarMgr.h>

#include <Utils/GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>

namespace GW {
    namespace UI {
        enum class UIMessage : uint32_t;
    }
}

class BuildsWindow : public ToolboxWindow {
    BuildsWindow() = default;
    ~BuildsWindow() = default;

    struct Build {
        Build(const char* n, const char* c);
        char name[128];
        char code[128];
        const GW::Constants::SkillID* skills();
        const GW::SkillbarMgr::SkillTemplate* decode();
        bool decoded() { return !(skill_template.primary == GW::Constants::Profession::None && skill_template.secondary == GW::Constants::Profession::None); }
        GW::SkillbarMgr::SkillTemplate skill_template;
        // Vector of pcons to use for this build, listed by ini name e.g. "cupcake"
        std::set<std::string> pcons;
    };
    struct TeamBuild {
        static unsigned int cur_ui_id;
        TeamBuild(const char* n)
            : ui_id(++cur_ui_id) {
            GuiUtils::StrCopy(name, n, sizeof(name));
        }
        bool edit_open = false;
        int edit_pcons = -1;
        bool show_numbers = false;
        char name[128];
        std::vector<Build> builds;
        unsigned int ui_id; // should be const but then assignment operator doesn't get created automatically, and I'm too lazy to redefine it, so just don't change this value, okay?
    };

public:
    static BuildsWindow& Instance() {
        static BuildsWindow instance;
        return instance;
    }
    static void CmdLoad(const wchar_t* message, int argc, LPWSTR* argv);

    const char* Name() const override { return "Builds"; }
    const char* Icon() const override { return ICON_FA_LIST; }

    void Initialize() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
    // Draw window(s) related to the "preferred skill order" feature
    void DrawPreferredSkillOrders(IDirect3DDevice9* pDevice);
    void DrawHelp() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

    void LoadFromFile();
    void SaveToFile();
    bool MoveOldBuilds(ToolboxIni* ini);

    void Send(unsigned int idx);
    const char* BuildName(unsigned int idx) const;
    inline unsigned int BuildCount() const { return teambuilds.size(); }
private:
    // Send a teambuild
    void Send(const TeamBuild& tbuild);
    // Send a specific build from a teambuild
    void Send(const TeamBuild& tbuild, unsigned int idx);
    void SendPcons(const TeamBuild& tbuild, unsigned int idx, bool include_build_name=true);
    // Load a specific build from a teambuild (and any applicable pcons)
    void Load(const TeamBuild& tbuild, unsigned int idx);
    // Toggle pcons for a specific build
    void LoadPcons(const TeamBuild& tbuild, unsigned int idx);
    // View a specific build from a teambuild
    void View(const TeamBuild& tbuild, unsigned int idx);
    // Load build by name or code, without specific teambuild assigned.
    void Load(const char* build_name);
    // Load build by teambuild name and build name
    void Load(const char* tbuild_name, const char* build_name);

    static bool BuildSkillTemplateString(const TeamBuild& tbuild, unsigned int idx, char* out, unsigned int out_len);

    void DrawBuildSection(TeamBuild& tbuild, unsigned int idx);

    bool builds_changed = false;
    std::vector<TeamBuild> teambuilds;
    bool order_by_name = false;
    bool order_by_index = !order_by_name;
    bool auto_load_pcons = true;
    bool auto_send_pcons = true;
    bool delete_builds_without_prompt = false;
    bool hide_when_entering_explorable = false;
    bool one_teambuild_at_a_time = false;


    clock_t send_timer = 0;
    std::queue<std::string> queue;

    ToolboxIni* inifile = nullptr;

    // Preferred skill orders
    bool preferred_skill_orders_visible = false;
    std::vector<Build> preferred_skill_order_builds;
    GuiUtils::EncString preferred_skill_order_tooltip;
    char preferred_skill_order_code[128] = { 0 };
    // Pass array of skills for a bar; if a preferred order is found, returns a new array of skills in order, otherwise nullptr.
    const GW::Constants::SkillID* GetPreferredSkillOrder(const GW::Constants::SkillID*, size_t* found_idx = nullptr);
    GW::HookEntry on_load_skills_entry;
    // Triggered when a set of skills is about to be loaded on player or hero
    static void OnSkillbarLoad(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*);
    // Attempt to add a preferred build by code and name
    const char* AddPreferredBuild(const char* code);

    bool GetCurrentSkillBar(char* out, size_t out_len);
};
