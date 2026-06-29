#pragma once

#include <memory>
#include <queue>
#include <vector>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/Managers/UIMgr.h>

#include <Timer.h>
#include <ToolboxWindow.h>
#include <Defines.h>
#include <Utils/TeamBuildEncoder.h>

class HeroBuildsWindow : public ToolboxWindow {

    HeroBuildsWindow();

    ~HeroBuildsWindow();

    GW::Constants::InstanceType last_instance_type = GW::Constants::InstanceType::Loading;

public:
    static HeroBuildsWindow& Instance()
    {
        static HeroBuildsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Hero Builds"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_USERS; }

    struct Settings {
        bool hide_when_entering_explorable = false;
        bool one_teambuild_at_a_time = false;
        bool filter_by_profession = false;
    };

    // On-disk schema of herobuilds.json
    struct BuildEntry {
        std::string name{};
        std::string code{};
        GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
        bool show_panel = false;
        uint32_t behavior = 1;
        uint8_t disabled_skills = 0;
    };
    struct TeamBuildEntry {
        std::string name{};
        std::string ui_id{};
        int mode = 0;
        std::string group{};
        std::vector<BuildEntry> builds{};
    };
    struct GroupEntry {
        std::string name{};
        size_t sort_order = 0;
    };
    struct HeroBuildsFile {
        std::vector<TeamBuildEntry> teambuilds{};
        std::vector<GroupEntry> groups{};
    };

    void Initialize() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    void LoadFromFile();
    void SaveToFile() const;

    void Load(size_t idx);
    [[nodiscard]] const char* BuildName(size_t idx) const;
    [[nodiscard]] unsigned int BuildCount() const { return static_cast<unsigned int>(teambuilds.size()); }

    void DrawHelp() override;

    static void CHAT_CMD_FUNC(CmdHeroTeamBuild);

    static const GW::HeroFlag* GetHeroFlagInfo(const uint32_t hero_id);
    // Returns ptr to party member of this hero, optionally fills out out_hero_index to be the index of this hero for the player.

    static GW::HeroPartyMember* GetPartyHeroByID(const GW::Constants::HeroID hero_id, size_t* out_hero_index);

private:
    TeamBuild* GetTeambuildByName(const std::string& argBuildname);

    // Encode a teambuild into a Daybreak party loadout base64 string (header=15, type=1, version=1).
    static std::string EncodeTeambuildToDaybreak(const TeamBuild& tbuild);
    // Decode a Daybreak party loadout base64 string into a teambuild.
    static bool DecodeTeambuildFromDaybreak(const std::string& code, TeamBuild& out);

    bool builds_changed = false;
    std::vector<TeamBuild> teambuilds{};
};
