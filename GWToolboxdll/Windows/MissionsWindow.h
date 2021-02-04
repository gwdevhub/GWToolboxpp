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
        Prophecies,
        Factions,
        Nightfall,
        EyeOfTheNorth,
        Dungeon,
    };
    class Mission
    {
    protected:
        using ImgPair = std::pair<const wchar_t*, const int>;
        using MissionImageList = std::vector<ImgPair>;
        static Color is_daily_bg_color;
        static Color has_quest_bg_color;
        
        wchar_t enc_mission_name[12] = { 0 };
        std::wstring mission_name;
        std::string mission_name_s;

        GW::Constants::MapID outpost;
        uint32_t zm_quest;
        std::vector<IDirect3DTexture9*> normal_mode_textures;
        std::vector<IDirect3DTexture9*> hard_mode_textures;

        Mission(GW::Constants::MapID, const MissionImageList&, const MissionImageList&, uint32_t);

    public:
        static float icon_size;

        void Draw(IDirect3DDevice9*);
        virtual IDirect3DTexture9* GetMissionImage();
        virtual bool IsDaily(); // True if this mission is ZM or ZB today
        virtual bool HasQuest(); // True if the ZM or ZB is in quest log
        virtual bool IsCompleted();
        std::string& Name();
    };


    class PropheciesMission : public Mission
    {
    private:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

    public:
        PropheciesMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class FactionsMission : public Mission
    {
    private:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

    public:
        FactionsMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class NightfallMission : public Mission
    {
    private:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

    protected:
        NightfallMission(GW::Constants::MapID _outpost,
            const MissionImageList& _normal_mode_images,
            const MissionImageList& _hard_mode_images,
            uint32_t _zm_quest)
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest) {}

    public:
        NightfallMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class TormentMission : public NightfallMission
    {
    private:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;

    public:
        TormentMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : NightfallMission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class EotNMission : public Mission
    {
    private:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        std::string name;

    protected:
        EotNMission(GW::Constants::MapID _outpost,
            const char* _name,
            const MissionImageList& _normal_mode_images,
            const MissionImageList& _hard_mode_images,
            uint32_t _zm_quest)
            : Mission(_outpost, _normal_mode_images, _hard_mode_images, _zm_quest), name(_name) {}

    public:
        EotNMission(GW::Constants::MapID _outpost, const char* _name, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest), name(_name) {}

        std::string Name();
        IDirect3DTexture9* GetMissionImage();
    };


    class Dungeon : public EotNMission
    {
    private:
        static MissionImageList normal_mode_images;
        static MissionImageList hard_mode_images;
        std::vector<uint32_t> zb_quests;

    public:
        Dungeon(GW::Constants::MapID _outpost, const char* _name, std::vector<uint32_t> _zb_quests)
            : EotNMission(_outpost, _name, normal_mode_images, hard_mode_images, 0), zb_quests(_zb_quests) {}
        Dungeon(GW::Constants::MapID _outpost, const char* _name, uint32_t _zb_quest = 0)
            : EotNMission(_outpost, _name, normal_mode_images, hard_mode_images, 0), zb_quests({ _zb_quest }) {}

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
};