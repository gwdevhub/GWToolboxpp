#pragma once

#include <GWCA/Constants/Constants.h>

#include <Modules/HallOfMonumentsModule.h>
#include <ToolboxWindow.h>
#include <Color.h>

namespace Missions {
    struct MissionImage {
        const wchar_t* file_name;
        int resource_id;
        IDirect3DTexture9* texture = nullptr;

        MissionImage(const wchar_t* _file_name, const int _resource_id)
            : file_name(_file_name), resource_id(_resource_id) {};
    };

    class Mission {
    protected:
        using MissionImageList = std::vector<MissionImage>;
        static Color is_daily_bg_color;
        static Color has_quest_bg_color;

        GuiUtils::EncString name;

        GW::Constants::MapID outpost;
        GW::Constants::MapID map_to;
        GW::Constants::QuestID zm_quest;
        MissionImageList normal_mode_textures;
        MissionImageList hard_mode_textures;

    public:
        virtual ~Mission() = default;
        Mission(GW::Constants::MapID, const MissionImageList&, const MissionImageList&, GW::Constants::QuestID = static_cast<GW::Constants::QuestID>(0));
        static ImVec2 icon_size;
        [[nodiscard]] GW::Constants::MapID GetOutpost() const;

        bool is_completed = false;
        bool bonus = false;
        bool map_unlocked = true;

        virtual const char* Name();
        virtual bool Draw(IDirect3DDevice9*);
        virtual void OnClick();
        virtual IDirect3DTexture9* GetMissionImage();
        virtual bool IsDaily();  // True if this mission is ZM or ZB today
        virtual bool HasQuest(); // True if the ZM or ZB is in quest log
        virtual void CheckProgress(const std::wstring& player_name);
    };


    class PvESkill : public Mission {
    protected:
        IDirect3DTexture9** image = nullptr;
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
        ~HeroUnlock() override { delete image; }
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
        ItemAchievement(size_t _encoded_name_index, const wchar_t* encoded_name);
        IDirect3DTexture9* GetMissionImage() override;

        void OnClick() override;
        const char* Name() override;
    };

    class FestivalHat : public ItemAchievement {
    public:
        FestivalHat(const size_t _encoded_name_index, const wchar_t* encoded_name)
            : ItemAchievement(_encoded_name_index, encoded_name) { }

        void CheckProgress(const std::wstring& player_name) override;
    };

    class UnlockedPvPItemUpgrade : public ItemAchievement {
    public:
        UnlockedPvPItemUpgrade(const size_t _encoded_name_index)
            : ItemAchievement(_encoded_name_index, nullptr) {}

        void CheckProgress(const std::wstring& player_name) override;
        IDirect3DTexture9* GetMissionImage() override;
        const char* Name() override;

        void OnClick() override;

    protected:
        void LoadStrings();
        GuiUtils::EncString single_item_name;
        bool sanitised_single_item_name = false;
    };

    class MinipetAchievement : public ItemAchievement {
    public:
        MinipetAchievement(const size_t hom_achievement_index, const wchar_t* encoded_name)
            : ItemAchievement(hom_achievement_index, encoded_name) { }

        void CheckProgress(const std::wstring& player_name) override;
    };

    class WeaponAchievement : public ItemAchievement {
    public:
        WeaponAchievement(const size_t _encoded_name_index, const wchar_t* encoded_name)
            : ItemAchievement(_encoded_name_index, encoded_name) { }

        void CheckProgress(const std::wstring& player_name) override;
    };

    class AchieventWithWikiFile : public ItemAchievement {
    protected:
        std::string wiki_file_name;
        IDirect3DTexture9** img = nullptr;

    public:
        AchieventWithWikiFile(const size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr)
            : ItemAchievement(hom_achievement_index, encoded_name)
        {
            if (_wiki_file_name) {
                wiki_file_name = _wiki_file_name;
            }
        }

        IDirect3DTexture9* GetMissionImage() override;
    };

    class ArmorAchievement : public AchieventWithWikiFile {
    public:
        ArmorAchievement(const size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr)
            : AchieventWithWikiFile(hom_achievement_index, encoded_name, _wiki_file_name) { };
        void CheckProgress(const std::wstring& player_name) override;
    };

    class CompanionAchievement : public AchieventWithWikiFile {
    public:
        CompanionAchievement(const size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr)
            : AchieventWithWikiFile(hom_achievement_index, encoded_name, _wiki_file_name) { };
        void CheckProgress(const std::wstring& player_name) override;
    };

    class HonorAchievement : public AchieventWithWikiFile {
    public:
        HonorAchievement(const size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr)
            : AchieventWithWikiFile(hom_achievement_index, encoded_name, _wiki_file_name) { };
        void CheckProgress(const std::wstring& player_name) override;
    };

    class FactionsPvESkill : public PvESkill {
    public:
        FactionsPvESkill(GW::Constants::SkillID skill_id);
        bool Draw(IDirect3DDevice9*) override;
    };

    class PropheciesMission : public Mission {
    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

        PropheciesMission(const GW::Constants::MapID _outpost, const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) { }
    };


    class FactionsMission : public Mission {
    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

        FactionsMission(const GW::Constants::MapID _outpost, const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) { }
    };


    class NightfallMission : public Mission {
    private:


    protected:
        NightfallMission(const GW::Constants::MapID _outpost,
                         const MissionImageList& _normal_mode_images,
                         const MissionImageList& _hard_mode_images,
                         const GW::Constants::QuestID _zm_quest)
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest) { }

    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

        NightfallMission(const GW::Constants::MapID _outpost, const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) { }
    };


    class TormentMission : public NightfallMission {
    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

        TormentMission(const GW::Constants::MapID _outpost, const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : NightfallMission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) { }
    };

    class Vanquish : public Mission {
    public:
        static MissionImageList hard_mode_images;

        Vanquish(const GW::Constants::MapID _outpost, const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : Mission(_outpost, hard_mode_images, hard_mode_images, _zm_quest) { }


        IDirect3DTexture9* GetMissionImage() override;
        void CheckProgress(const std::wstring& player_name) override;
    };


    class EotNMission : public Mission {
    protected:
        EotNMission(const GW::Constants::MapID _outpost,
                    const MissionImageList& _normal_mode_images,
                    const MissionImageList& _hard_mode_images,
                    const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest) { }

    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

        EotNMission(const GW::Constants::MapID _outpost, const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) { }


        IDirect3DTexture9* GetMissionImage() override;
        void CheckProgress(const std::wstring& player_name) override;

    private:
        std::string name;
    };


    class Dungeon : public EotNMission {
    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

        Dungeon(const GW::Constants::MapID _outpost, std::vector<GW::Constants::QuestID> _zb_quests)
            : EotNMission(_outpost, normal_mode_images, hard_mode_images), zb_quests(_zb_quests) { }

        Dungeon(const GW::Constants::MapID _outpost, GW::Constants::QuestID _zb_quest = static_cast<GW::Constants::QuestID>(0))
            : EotNMission(_outpost, normal_mode_images, hard_mode_images), zb_quests({_zb_quest}) { }

        bool IsDaily() override;
        bool HasQuest() override;

    private:
        std::vector<GW::Constants::QuestID> zb_quests{};
    };
} // namespace Missions

struct CharacterCompletion {
    GW::Constants::Profession profession = static_cast<GW::Constants::Profession>(0);
    std::wstring account;
    std::string name_str;
    std::vector<uint32_t> skills{};
    std::vector<uint32_t> mission{};
    std::vector<uint32_t> mission_bonus{};
    std::vector<uint32_t> mission_hm{};
    std::vector<uint32_t> mission_bonus_hm{};
    std::vector<uint32_t> vanquishes{};
    std::vector<uint32_t> heroes{};
    std::vector<uint32_t> maps_unlocked{};
    std::string hom_code;
    HallOfMonumentsAchievements hom_achievements;
    std::vector<uint32_t> minipets_unlocked{};
    std::vector<uint32_t> festival_hats{};
};

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class CompletionWindow : public ToolboxWindow {
public:
    static CompletionWindow& Instance()
    {
        static CompletionWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Completion"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BOOK; }


    void Initialize() override;
    static void Initialize_Prophecies();
    static void Initialize_Factions();
    static void Initialize_Nightfall();
    static void Initialize_EotN();
    static void Initialize_Dungeons();
    void Terminate() override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawHallOfMonuments(IDirect3DDevice9* device);

    static CharacterCompletion* GetCharacterCompletion(const wchar_t* name, bool create_if_not_found = false);

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    // Check explicitly rather than every frame
    CompletionWindow* CheckProgress(bool fetch_hom = false);
};
