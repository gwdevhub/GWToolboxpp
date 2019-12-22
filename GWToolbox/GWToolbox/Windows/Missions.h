#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Constants/Constants.h>
#include <Modules/Resources.h>
#include <Color.h>

namespace Missions {
    enum class Campaign {
        Prophecies,
        Factions,
        Nightfall,
        EyeOfTheNorth,
        Dungeon,
    };

    const char* const CampaignName(const Campaign camp);

    class Mission
    {
    protected:
        using ImgPair = std::pair<const wchar_t*, const WORD>;
        using MissionImageList = std::vector<ImgPair>;
        static Color has_quest_bg_color;
        static float icon_size;

        GW::Constants::MapID outpost;
        uint32_t zm_quest;
        std::vector<IDirect3DTexture9*> normal_mode_textures;
        std::vector<IDirect3DTexture9*> hard_mode_textures;

        Mission(GW::Constants::MapID, const MissionImageList&, const MissionImageList&, uint32_t);

    public:


        void Draw(IDirect3DDevice9*);
        virtual const std::string Name() { return std::string(GW::Constants::NAME_FROM_ID[static_cast<uint32_t>(outpost)]); }
        virtual IDirect3DTexture9* GetMissionImage();
        bool IsDaily(); // True if this mission is ZM or ZB today
        bool HasQuest(); // True if the ZM or ZB is in quest log
    };


    class PropheciesMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        PropheciesMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class FactionsMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        FactionsMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class NightfallMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    protected:
        NightfallMission(GW::Constants::MapID _outpost,
            const MissionImageList& normal_mode_images,
            const MissionImageList& hard_mode_images,
            uint32_t _zm_quest)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}

    public:
        NightfallMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class TormentMission : public NightfallMission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        TormentMission(GW::Constants::MapID _outpost, uint32_t _zm_quest = 0)
            : NightfallMission(_outpost, normal_mode_images, hard_mode_images, _zm_quest) {}
    };


    class EotNMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;
        const char* name;

    protected:
        EotNMission(GW::Constants::MapID _outpost,
            const char* _name,
            const MissionImageList& normal_mode_images,
            const MissionImageList& hard_mode_images,
            uint32_t _zm_quest)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest), name(_name) {}

    public:
        EotNMission(GW::Constants::MapID _outpost, const char* _name, uint32_t _zm_quest = 0)
            : Mission(_outpost, normal_mode_images, hard_mode_images, _zm_quest), name(_name) {}

        const std::string Name() override;
        IDirect3DTexture9* GetMissionImage();
    };


    class Dungeon : public EotNMission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        Dungeon(GW::Constants::MapID _outpost, const char* _name, uint32_t _zm_quest = 0)
            : EotNMission(_outpost, _name, normal_mode_images, hard_mode_images, _zm_quest) {}
    };
}