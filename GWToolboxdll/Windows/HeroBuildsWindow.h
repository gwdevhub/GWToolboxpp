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

    void Load(size_t idx);
    [[nodiscard]] const char* BuildName(size_t idx) const;
    [[nodiscard]] unsigned int BuildCount() const { return static_cast<unsigned int>(teambuilds.size()); }

    static void CHAT_CMD_FUNC(CmdHeroTeamBuild);

    static const GW::HeroFlag* GetHeroFlagInfo(const uint32_t hero_id);
    // Returns ptr to party member of this hero, optionally fills out out_hero_index to be the index of this hero for the player.

    static GW::HeroPartyMember* GetPartyHeroByID(const GW::Constants::HeroID hero_id, size_t* out_hero_index);

private:
    bool hide_when_entering_explorable = false;
    bool one_teambuild_at_a_time = false;
    bool filter_by_profession = false;

    // Load a teambuild
    void Load(const TeamBuild& tbuild);
    // Load a specific build from a teambuild
    void Load(const TeamBuild& tbuild, size_t idx);
    void Send(const TeamBuild& tbuild, size_t idx);
    void Send(const TeamBuild& tbuild);
    static void View(const TeamBuild& tbuild, size_t idx);
    static void HeroBuildName(const TeamBuild& tbuild, size_t idx, std::string* out);
    TeamBuild* GetTeambuildByName(const std::string& argBuildname);

    // Encode a teambuild into a Daybreak party loadout base64 string (header=15, type=1, version=1).
    static std::string EncodeTeambuildToDaybreak(const TeamBuild& tbuild);
    // Decode a Daybreak party loadout base64 string into a teambuild.
    static bool DecodeTeambuildFromDaybreak(const std::string& code, TeamBuild& out);

    bool builds_changed = false;
    std::vector<TeamBuild> teambuilds{};
};
