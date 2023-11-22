#pragma once

#include <GWCA/Constants/Constants.h>

#include <Modules/HallOfMonumentsModule.h>
#include <ToolboxWindow.h>
#include <Color.h>


namespace Missions {

    class Mission {
    protected:
        static Color is_daily_bg_color;
        static Color has_quest_bg_color;

        GuiUtils::EncString name;

        GW::Constants::MapID outpost;
        GW::Constants::MapID map_to;
        GW::Constants::QuestID zm_quest;

        uint8_t mission_state = 0;
        // Array of placeholder pointers; the actual IDirect3DTexture9* is loaded from dx9
        IDirect3DTexture9** icons[4] = { nullptr };
        bool icons_loaded = false;
        ImVec2 icon_uv_offset[2] = { { .0f,.0f },{0.f,0.f} };

    public:
        virtual ~Mission() = default;
        Mission(GW::Constants::MapID, GW::Constants::QuestID = static_cast<GW::Constants::QuestID>(0));
        static ImVec2 icon_size;
        [[nodiscard]] GW::Constants::MapID GetOutpost() const;

        bool is_completed = false;
        bool bonus = false;
        bool map_unlocked = true;

        virtual size_t GetLoadedIcons(IDirect3DTexture9* icons_out[4]);

        virtual const char* Name();
        virtual bool Draw(IDirect3DDevice9*);
        virtual void OnClick();
        virtual bool IsDaily();  // True if this mission is ZM or ZB today
        virtual bool HasQuest(); // True if the ZM or ZB is in quest log
        virtual void CheckProgress(const std::wstring& player_name);
    };


    class PvESkill : public Mission {
    protected:
        GW::Constants::SkillID skill_id;

    public:
        uint32_t profession = 0;
        PvESkill(GW::Constants::SkillID _skill_id);
        bool IsDaily() override { return false; }
        bool HasQuest() override { return false; }

        size_t GetLoadedIcons(IDirect3DTexture9* icons_out[4]) override;

        bool Draw(IDirect3DDevice9*) override;
        void OnClick() override;

        void CheckProgress(const std::wstring& player_name) override;
    };

    class HeroUnlock : public PvESkill {
    public:
        HeroUnlock(GW::Constants::HeroID _hero_id);
        ~HeroUnlock();

        size_t GetLoadedIcons(IDirect3DTexture9* icons_out[4]) override;

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
        size_t GetLoadedIcons(IDirect3DTexture9* icons_out[4]) override;

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
        size_t GetLoadedIcons(IDirect3DTexture9* icons_out[4]) override;
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

    public:
        AchieventWithWikiFile(const size_t hom_achievement_index, const wchar_t* encoded_name, const char* _wiki_file_name = nullptr)
            : ItemAchievement(hom_achievement_index, encoded_name)
        {
            if (_wiki_file_name) {
                wiki_file_name = _wiki_file_name;
            }
        }

        size_t GetLoadedIcons(IDirect3DTexture9* icons_out[4]) override;
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

    class Vanquish : public Mission {
    public:

        Vanquish(const GW::Constants::MapID _outpost, const GW::Constants::QuestID _zm_quest = static_cast<GW::Constants::QuestID>(0))
            : Mission(_outpost, _zm_quest) { }

        void CheckProgress(const std::wstring& player_name) override;
    };

    class EotNMission : public Mission {
    public:
        EotNMission(const GW::Constants::MapID _outpost, const std::vector<GW::Constants::QuestID>& _zb_quests)
            : zb_quests(_zb_quests), Mission(_outpost)  { }

        EotNMission(const GW::Constants::MapID _outpost, GW::Constants::QuestID _zb_quest = static_cast<GW::Constants::QuestID>(0))
            : zb_quests({_zb_quest}), Mission(_outpost)  { }

        void CheckProgress(const std::wstring& player_name) override;
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
    static void DrawHallOfMonuments(IDirect3DDevice9* device);

    static CharacterCompletion* GetCharacterCompletion(const wchar_t* name, bool create_if_not_found = false);

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    // Check explicitly rather than every frame
    CompletionWindow* CheckProgress(bool fetch_hom = false);
};
