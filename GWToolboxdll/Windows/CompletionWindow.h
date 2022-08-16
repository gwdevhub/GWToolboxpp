#pragma once 

#include "ToolboxWindow.h"

#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hook.h>
#include <Modules/Resources.h>
#include <Color.h>

#include <Modules/HallOfMonumentsModule.h>

namespace Missions {


    struct MissionImage {
        const wchar_t* file_name;
        const int resource_id;
        IDirect3DTexture9* texture = nullptr;
        MissionImage(const wchar_t* _file_name, const int _resource_id) : file_name(_file_name), resource_id(_resource_id) {};
    };

    class Mission
    {
    protected:
        using MissionImageList = std::vector<MissionImage>;
        static Color is_daily_bg_color;
        static Color has_quest_bg_color;
        
        GuiUtils::EncString name;

        GW::Constants::MapID outpost;
        GW::Constants::MapID map_to;
        GW::Constants::QuestID zm_quest;
        const MissionImageList& normal_mode_textures;
        const MissionImageList& hard_mode_textures;



    public:
        Mission(GW::Constants::MapID, const MissionImageList&, const MissionImageList&, GW::Constants::QuestID = (GW::Constants::QuestID)0);
        static ImVec2 icon_size;
        GW::Constants::MapID GetOutpost();

        bool is_completed = false;
        bool bonus = false;
        bool map_unlocked = true;

        virtual const char* Name();


        virtual bool Draw(IDirect3DDevice9*);
        virtual void OnClick();
        virtual IDirect3DTexture9* GetMissionImage();
        virtual bool IsDaily(); // True if this mission is ZM or ZB today
        virtual bool HasQuest(); // True if the ZM or ZB is in quest log
        virtual void CheckProgress(const std::wstring& player_name);
        
    };


    class PvESkill : public Mission {
    protected:
        
        GW::Constants::SkillID skill_id;
        bool img_loaded = false;
        const wchar_t* image_url = 0;
        IDirect3DTexture9* skill_image = 0;
    public:
        uint32_t profession = 0;
        inline static MissionImageList dummy_var = {};
        PvESkill(GW::Constants::SkillID _skill_id, const wchar_t* _image_url = 0);
        virtual IDirect3DTexture9* GetMissionImage() override;
        bool IsDaily() override { return false; }
        bool HasQuest() override { return false; }
        
        virtual bool Draw(IDirect3DDevice9*) override;
        virtual void OnClick() override;

        virtual void CheckProgress(const std::wstring& player_name) override;
    };

    class HeroUnlock : public PvESkill {
    public:
        HeroUnlock(GW::Constants::HeroID _hero_id);
        IDirect3DTexture9* GetMissionImage() override;

        void OnClick() override;

        virtual void CheckProgress(const std::wstring& player_name) override;
        const char* Name() override;
    };

    class FactionsPvESkill : public PvESkill {
    public:
        FactionsPvESkill(GW::Constants::SkillID skill_id);
        bool Draw(IDirect3DDevice9*) override;
        
    };

    class PropheciesMission : public Mission
    {
    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        PropheciesMission(GW::Constants::MapID _outpost, GW::Constants::QuestID _zm_quest = (GW::Constants::QuestID)0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class FactionsMission : public Mission
    {
    private:


    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        FactionsMission(GW::Constants::MapID _outpost, GW::Constants::QuestID _zm_quest = (GW::Constants::QuestID)0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class NightfallMission : public Mission
    {
    private:


    protected:

        NightfallMission(GW::Constants::MapID _outpost,
            const MissionImageList& _normal_mode_images,
            const MissionImageList& _hard_mode_images,
            GW::Constants::QuestID _zm_quest)
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest) {}

    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        NightfallMission(GW::Constants::MapID _outpost, GW::Constants::QuestID _zm_quest = (GW::Constants::QuestID)0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class TormentMission : public NightfallMission
    {
    private:


    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        TormentMission(GW::Constants::MapID _outpost, GW::Constants::QuestID _zm_quest = (GW::Constants::QuestID)0)
            : NightfallMission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };

    class Vanquish : public Mission
    {
    public:
        static MissionImageList hard_mode_images;
        Vanquish(GW::Constants::MapID _outpost, GW::Constants::QuestID _zm_quest = (GW::Constants::QuestID)0)
            : Mission(_outpost, hard_mode_images, hard_mode_images, _zm_quest) {
        }

        
        IDirect3DTexture9* GetMissionImage();
        virtual void CheckProgress(const std::wstring& player_name) override;
    };


    class EotNMission : public Mission
    {
    private:

        std::string name;

    protected:
        EotNMission(GW::Constants::MapID _outpost,
            const MissionImageList& _normal_mode_images,
            const MissionImageList& _hard_mode_images,
            GW::Constants::QuestID _zm_quest = (GW::Constants::QuestID)0)
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest) {}
    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        EotNMission(GW::Constants::MapID _outpost, GW::Constants::QuestID _zm_quest = (GW::Constants::QuestID)0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}

        
        IDirect3DTexture9* GetMissionImage();
        virtual void CheckProgress(const std::wstring& player_name) override;
    };


    class Dungeon : public EotNMission
    {
    private:

        std::vector<GW::Constants::QuestID> zb_quests;

    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        Dungeon(GW::Constants::MapID _outpost, std::vector<GW::Constants::QuestID> _zb_quests)
            : EotNMission(_outpost, normal_mode_images, hard_mode_images), zb_quests(_zb_quests) {}
        Dungeon(GW::Constants::MapID _outpost, GW::Constants::QuestID _zb_quest = (GW::Constants::QuestID)0)
            : EotNMission(_outpost, normal_mode_images, hard_mode_images), zb_quests({ _zb_quest }) {}

        bool IsDaily() override;
        bool HasQuest() override;
    };
}


// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class CompletionWindow : public ToolboxWindow {
protected:
    bool hide_unlocked_skills = false;
    bool hide_completed_vanquishes = false;
    bool hide_completed_missions = false;
    bool pending_sort = true;
    const char* completion_ini_filename = "character_completion.ini";

    bool hard_mode = false;

    enum CompletionType : uint8_t {
        Skills,
        Mission,
        MissionBonus,
        MissionHM,
        MissionBonusHM,
        Vanquishes,
        Heroes,
        MapsUnlocked
    };
    struct Completion {
        GW::Constants::Profession profession;
        std::string name_str;
        std::vector<uint32_t> skills;
        std::vector<uint32_t> mission;
        std::vector<uint32_t> mission_bonus;
        std::vector<uint32_t> mission_hm;
        std::vector<uint32_t> mission_bonus_hm;
        std::vector<uint32_t> vanquishes;
        std::vector<uint32_t> heroes;
        std::vector<uint32_t> maps_unlocked;
        std::string hom_code;
        HallOfMonumentsAchievements* hom_achievements = 0;
    };

public:
	static CompletionWindow& Instance() {
		static CompletionWindow instance;
		return instance;
	}

	const char* Name() const override { return "Completion"; }
    const char8_t* Icon() const override { return ICON_FA_BOOK; }

    const bool IsHardMode() { return hard_mode; }

	void Initialize() override;
	void Initialize_Prophecies();
	void Initialize_Factions();
	void Initialize_Nightfall();
	void Initialize_EotN();
	void Initialize_Dungeons();
	void Terminate() override;
	void Draw(IDirect3DDevice9* pDevice) override;
    void DrawHallOfMonuments();
    
    std::unordered_map<std::wstring, Completion*> character_completion;

    Completion* GetCharacterCompletion(const wchar_t* name, bool create_if_not_found = false);

    // IF character_name is null, parse current logged in char.
    CompletionWindow* ParseCompletionBuffer(CompletionType type, wchar_t* character_name = 0, uint32_t* buffer = 0, size_t len = 0);

	void DrawSettingInternal() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
    // Check explicitly rather than every frame
    CompletionWindow* CheckProgress();


    GW::HookEntry skills_unlocked_stoc_entry;

    std::map<GW::Constants::Campaign, std::vector<Missions::Mission*>> missions;
    std::map<GW::Constants::Campaign, std::vector<Missions::Mission*>> vanquishes;
    std::map<GW::Constants::Campaign, std::vector<Missions::PvESkill*>> elite_skills;
    std::map<GW::Constants::Campaign, std::vector<Missions::PvESkill*>> pve_skills;
    std::map<GW::Constants::Campaign, std::vector<Missions::HeroUnlock*>> heros;
    HallOfMonumentsAchievements hom_achievements;
    int hom_achievements_status = 0xf;
};