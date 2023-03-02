#pragma once

#include "ToolboxWindow.h"

#include <d3d9.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hook.h>
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
        IDirect3DTexture9** image = 0;
        GW::Constants::SkillID skill_id;
    public:
        uint32_t profession = 0;
        inline static MissionImageList dummy_var = {};
        PvESkill(GW::Constants::SkillID _skill_id);
        IDirect3DTexture9* GetMissionImage() override;
        bool IsDaily() override { return false; }
        bool HasQuest() override { return false; }

        bool Draw(IDirect3DDevice9*) override;
        void OnClick() override;

        void CheckProgress(const std::wstring& player_name) override;
    };

    class HeroUnlock : public PvESkill {
    public:
        HeroUnlock(GW::Constants::HeroID _hero_id);
        ~HeroUnlock() { delete image; }
        IDirect3DTexture9* GetMissionImage() override;

        void OnClick() override;

        void CheckProgress(const std::wstring& player_name) override;
        const char* Name() override;
    };

    class ItemAchievement : public PvESkill {
    protected:
        std::string wiki_file_name;
        GuiUtils::EncString name;
        size_t encoded_name_index;
    public:
        ItemAchievement(size_t hom_achievement_index, const wchar_t* encoded_name);
        IDirect3DTexture9* GetMissionImage() override;

        void OnClick() override;
        const char* Name() override;
    };
    class FestivalHat : public ItemAchievement {
    public:
        FestivalHat(size_t _encoded_name_index, const wchar_t* encoded_name) :
            ItemAchievement(_encoded_name_index, encoded_name) {}
        void CheckProgress(const std::wstring& player_name) override;
    };

    class MinipetAchievement : public ItemAchievement {
    public:
        MinipetAchievement(size_t hom_achievement_index, const wchar_t* encoded_name) :
            ItemAchievement(hom_achievement_index, encoded_name) {}
        void CheckProgress(const std::wstring& player_name) override;
    };
    class WeaponAchievement : public ItemAchievement {
    public:
        WeaponAchievement(size_t _encoded_name_index, const wchar_t* encoded_name) :
            ItemAchievement(_encoded_name_index, encoded_name) {}
        void CheckProgress(const std::wstring& player_name) override;
    };
    class AchieventWithWikiFile : public ItemAchievement {
    protected:
        std::string wiki_file_name;
        IDirect3DTexture9** img = 0;
    public:
        AchieventWithWikiFile(size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr) :
            ItemAchievement(hom_achievement_index, encoded_name) {
            if (_wiki_file_name) {
                wiki_file_name = _wiki_file_name;
            }
        }
        IDirect3DTexture9* GetMissionImage() override;
    };

    class ArmorAchievement : public AchieventWithWikiFile {
    public:
        ArmorAchievement(size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr) :
            AchieventWithWikiFile( hom_achievement_index, encoded_name, _wiki_file_name) {};
         void CheckProgress(const std::wstring& player_name) override;
    };
    class CompanionAchievement : public AchieventWithWikiFile {
    public:
        CompanionAchievement(size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr) :
            AchieventWithWikiFile( hom_achievement_index, encoded_name, _wiki_file_name) {};
        void CheckProgress(const std::wstring& player_name) override;
    };
    class HonorAchievement : public AchieventWithWikiFile {
    public:
        HonorAchievement(size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr) :
            AchieventWithWikiFile( hom_achievement_index, encoded_name, _wiki_file_name) {};
        void CheckProgress(const std::wstring& player_name) override;
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


        IDirect3DTexture9* GetMissionImage() override;
        void CheckProgress(const std::wstring& player_name) override;
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


        IDirect3DTexture9* GetMissionImage() override;
        void CheckProgress(const std::wstring& player_name) override;
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
} // namespace Missions
struct CharacterCompletion {
    GW::Constants::Profession profession = (GW::Constants::Profession)0;
    std::wstring account;
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
    HallOfMonumentsAchievements hom_achievements;
    std::vector<uint32_t> minipets_unlocked;
    std::vector<uint32_t> festival_hats;
};

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class CompletionWindow : public ToolboxWindow {
public:
    static CompletionWindow& Instance() {
        static CompletionWindow instance;
        return instance;
    }

    const char* Name() const override { return "Completion"; }
    const char* Icon() const override { return ICON_FA_BOOK; }


    void Initialize() override;
    void Initialize_Prophecies();
    void Initialize_Factions();
    void Initialize_Nightfall();
    void Initialize_EotN();
    void Initialize_Dungeons();
    void Terminate() override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawHallOfMonuments(IDirect3DDevice9* device);

    CharacterCompletion* GetCharacterCompletion(const wchar_t* name, bool create_if_not_found = false);

    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    // Check explicitly rather than every frame
    CompletionWindow* CheckProgress(bool fetch_hom = false);


};
