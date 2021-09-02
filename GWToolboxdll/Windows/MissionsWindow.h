#pragma once 

#include "ToolboxWindow.h"

#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Constants/Constants.h>
#include <Modules/Resources.h>
#include <Color.h>

namespace Missions {

    enum class Campaign : uint8_t {
        Core,
        Prophecies,
        Factions,
        Nightfall,
        EyeOfTheNorth,
        Dungeon
    };

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
        uint32_t zm_quest;
        const MissionImageList& normal_mode_textures;
        const MissionImageList& hard_mode_textures;

        

    public:
        Mission(GW::Constants::MapID, const MissionImageList&, const MissionImageList&, uint32_t);
        static ImVec2 icon_size;
        GW::Constants::MapID GetOutpost();

        virtual void Draw(IDirect3DDevice9*);
        virtual void OnClick();
        virtual IDirect3DTexture9* GetMissionImage();
        virtual bool IsDaily(); // True if this mission is ZM or ZB today
        virtual bool HasQuest(); // True if the ZM or ZB is in quest log
        virtual bool IsCompleted();
    };

    class PvESkill : public Mission {
    protected:
        
        GW::Constants::SkillID skill_id;
        const wchar_t* image_url = 0;
        const wchar_t* image_name = 0;
        IDirect3DTexture9* skill_image = 0;
    public:
        uint32_t profession = 0;
        inline static MissionImageList dummy_var = {};
        PvESkill(GW::Constants::SkillID _skill_id, const wchar_t* _image_url);
        IDirect3DTexture9* GetMissionImage() override;
        bool IsDaily() override { return false; }
        bool HasQuest() override { return false; }
        bool IsCompleted() override;
        void Draw(IDirect3DDevice9*) override;
        void OnClick() override;
    };
    class FactionsPvESkill : public PvESkill {
    protected:
        GW::Constants::SkillID skill_id2;
    public:
        FactionsPvESkill(GW::Constants::SkillID kurzick_id, GW::Constants::SkillID luxon_id, const wchar_t* _image_url);
        void Draw(IDirect3DDevice9*) override;
        bool IsCompleted() override;
    };

    class PropheciesMission : public Mission
    {
    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        PropheciesMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class FactionsMission : public Mission
    {
    private:


    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        FactionsMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class NightfallMission : public Mission
    {
    private:


    protected:

        NightfallMission(GW::Constants::MapID _outpost,
            const MissionImageList& _normal_mode_images,
            const MissionImageList& _hard_mode_images,
            uint32_t _zm_quest)
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest) {}

    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        NightfallMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class TormentMission : public NightfallMission
    {
    private:


    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        TormentMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : NightfallMission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };

    class Vanquish : public Mission
    {
    public:
        static MissionImageList hard_mode_images;
        Vanquish(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, hard_mode_images, hard_mode_images, _zm_quest) {
        }

        bool IsCompleted();
        IDirect3DTexture9* GetMissionImage();
    };


    class EotNMission : public Mission
    {
    private:

        std::string name;

    protected:
        EotNMission(GW::Constants::MapID _outpost,
            const MissionImageList& _normal_mode_images,
            const MissionImageList& _hard_mode_images,
            uint32_t _zm_quest)
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest) {}

    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        EotNMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}

        bool IsCompleted();
        IDirect3DTexture9* GetMissionImage();
    };


    class Dungeon : public EotNMission
    {
    private:

        std::vector<uint32_t> zb_quests;

    public:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        Dungeon(GW::Constants::MapID _outpost, std::vector<uint32_t> _zb_quests)
            : EotNMission(_outpost, normal_mode_images, hard_mode_images, 0), zb_quests(_zb_quests) {}
        Dungeon(GW::Constants::MapID _outpost, uint32_t _zb_quest = 0)
            : EotNMission(_outpost, normal_mode_images, hard_mode_images, 0), zb_quests({ _zb_quest }) {}

        bool IsDaily() override;
        bool HasQuest() override;
    };
}


// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class MissionsWindow : public ToolboxWindow {
	MissionsWindow() {};
	~MissionsWindow() {};
public:
	static MissionsWindow& Instance() {
		static MissionsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Missions"; }
    const char* Icon() const override { return ICON_FA_BOOK; }

	void Initialize() override;
	void Initialize_Prophecies();
	void Initialize_Factions();
	void Initialize_Nightfall();
	void Initialize_EotN();
	void Initialize_Dungeons();
	void Terminate() override;
	void Draw(IDirect3DDevice9* pDevice) override;


	void DrawSettingInternal() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	std::map<Missions::Campaign, std::vector<Missions::Mission*>> missions{
		{ Missions::Campaign::Prophecies, {} },
		{ Missions::Campaign::Factions, {} },
		{ Missions::Campaign::Nightfall, {} },
		{ Missions::Campaign::EyeOfTheNorth, {} },
		{ Missions::Campaign::Dungeon, {} },
	};
    std::map<Missions::Campaign, std::vector<Missions::Mission*>> vanquishes{
        { Missions::Campaign::Prophecies, {} },
        { Missions::Campaign::Factions, {} },
        { Missions::Campaign::Nightfall, {} },
        { Missions::Campaign::EyeOfTheNorth, {} },
        { Missions::Campaign::Dungeon, {} },
    };
    std::map<Missions::Campaign, std::vector<Missions::PvESkill*>> pve_skills{
        { Missions::Campaign::Factions, {} },
        { Missions::Campaign::Nightfall, {} },
        { Missions::Campaign::EyeOfTheNorth, {} },
        { Missions::Campaign::Core, {} },
    };
protected:
    bool only_pve_skills_for_my_profession = false;
};