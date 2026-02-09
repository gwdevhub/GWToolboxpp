#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Hero.h>

#include <Timer.h>
#include <ToolboxWindow.h>
#include <Defines.h>

constexpr size_t BUFFER_SIZE = 128;

class HeroBuildsWindow : public ToolboxWindow {
    // hero_index is:
    // -2 for player (although it doesn't really matter),
    // -1 for 'choose hero',
    // 0 for 'no hero',
    // and 1+ for heroes, order is in HeroIndexToID array
    struct HeroBuild {
        HeroBuild() = default;
        HeroBuild(const std::string_view n, const std::string_view c, const int index = -1, const int panel = 0, const uint32_t _behavior = 1)
            : hero_index(index)
            , behavior(_behavior)
            , show_panel(panel)
        {
            std::snprintf(name, _countof(name), "%s", n.data());
            std::snprintf(code, _countof(code), "%s", c.data());
        }

        char name[BUFFER_SIZE]{};
        char code[BUFFER_SIZE]{};
        int hero_index{};
        uint32_t behavior = 1;
        bool show_panel = false;
    };

    struct TeamHeroBuild {
        static unsigned int cur_ui_id;

        TeamHeroBuild(const std::string_view n)
            : ui_id(++cur_ui_id)
        {
            std::snprintf(name, sizeof(name), "%s", n.data());
        }

        bool edit_open = false;
        int mode = 0; // 0=don't change, 1=normal mode, 2=hard mode
        char name[BUFFER_SIZE]{};
        std::array<HeroBuild, 8> builds{};
        unsigned int ui_id; // should be const but then assignment operator doesn't get created automatically, and I'm too lazy to redefine it, so just don't change this value, okay?
    };

    HeroBuildsWindow()
    {
        inifile = new ToolboxIni(false, false, false);
        show_menubutton = can_show_in_main_window;
    }

    ~HeroBuildsWindow() override
    {
        delete inifile;
    }

    GW::Constants::InstanceType last_instance_type = GW::Constants::InstanceType::Loading;

public:
    static HeroBuildsWindow& Instance()
    {
        static HeroBuildsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Hero Builds"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_USERS; }

    void Initialize() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    void LoadFromFile();
    void SaveToFile() const;

    void Load(unsigned int idx);
    [[nodiscard]] const char* BuildName(unsigned int idx) const;
    [[nodiscard]] unsigned int BuildCount() const { return teambuilds.size(); }

    static void CHAT_CMD_FUNC(CmdHeroTeamBuild);

    // Returns ptr to party member of this hero, optionally fills out out_hero_index to be the index of this hero for the player.
    static GW::HeroPartyMember* GetPartyHeroByID(GW::Constants::HeroID hero_id, size_t* out_hero_index);
private:
    bool hide_when_entering_explorable = false;
    bool one_teambuild_at_a_time = false;

    // Load a teambuild
    void Load(const TeamHeroBuild& tbuild);
    // Load a specific build from a teambuild
    void Load(const TeamHeroBuild& tbuild, unsigned int idx);
    void Send(const TeamHeroBuild& tbuild, size_t idx);
    void Send(const TeamHeroBuild& tbuild);
    static void View(const TeamHeroBuild& tbuild, unsigned int idx);
    static void HeroBuildName(const TeamHeroBuild& tbuild, unsigned int idx, std::string* out);
    TeamHeroBuild* GetTeambuildByName(const std::string& argBuildname);

    bool builds_changed = false;
    std::vector<TeamHeroBuild> teambuilds{};

    struct CodeOnHero {
        enum Stage : uint8_t {
            Add,
            Load,
            Finished
        } stage = Add;

        char code[BUFFER_SIZE]{};
        size_t party_hero_index = 0xFFFFFFFF;
        GW::Constants::HeroID heroid = GW::Constants::HeroID::NoHero;
        int show_panel = 0;
        GW::HeroBehavior behavior = GW::HeroBehavior::Guard;
        clock_t started = 0;

        CodeOnHero(const char* c = "", const GW::Constants::HeroID i = GW::Constants::HeroID::NoHero, const int _show_panel = 0, uint32_t _behavior = 1)
            : heroid(i)
            , show_panel(_show_panel)
            , behavior(static_cast<GW::HeroBehavior>(_behavior))
        {
            snprintf(code, BUFFER_SIZE, "%s", c);
            if (behavior > GW::HeroBehavior::AvoidCombat) {
                behavior = GW::HeroBehavior::Guard;
            }
        }

        // True when processing is done
        bool Process();
    };


    clock_t send_timer = 0;
    clock_t kickall_timer = 0;
    std::vector<CodeOnHero> pending_hero_loads{};
    std::queue<std::string> send_queue{};

    ToolboxIni* inifile = nullptr;
};
