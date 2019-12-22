#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\WorldContext.h>
#include <GWCA\Constants\Constants.h>
#include <Modules\Resources.h>

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
        GW::Constants::MapID outpost;
        std::vector<IDirect3DTexture9*> normal_mode_textures;
        std::vector<IDirect3DTexture9*> hard_mode_textures;

    public:
        using ImgPair = std::pair<const wchar_t*, const WORD>;
        using MissionImageList = std::vector<ImgPair>;

        Mission(GW::Constants::MapID, const MissionImageList&, const MissionImageList&);

        void Draw(IDirect3DDevice9*, float);
        virtual const std::string Name() { return std::string(GW::Constants::NAME_FROM_ID[static_cast<int>(outpost)]); }
        virtual IDirect3DTexture9* GetMissionImage();
        //TODO: highlight dailies
        //TODO: highlight if you have quest
    };


    class PropheciesMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        PropheciesMission(GW::Constants::MapID _outpost)
            : Mission(_outpost, normal_mode_images, hard_mode_images) {}
    };


    class FactionsMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        FactionsMission(GW::Constants::MapID _outpost)
            : Mission(_outpost, normal_mode_images, hard_mode_images) {}
    };


    class NightfallMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    protected:
        NightfallMission(GW::Constants::MapID _outpost,
            const MissionImageList& normal_mode_images, const MissionImageList& hard_mode_images)
            : Mission(_outpost, normal_mode_images, hard_mode_images) {}

    public:
        NightfallMission(GW::Constants::MapID _outpost)
            : Mission(_outpost, normal_mode_images, hard_mode_images) {}
    };


    class TormentMission : public NightfallMission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        TormentMission(GW::Constants::MapID _outpost)
            : NightfallMission(_outpost, normal_mode_images, hard_mode_images) {}
    };


    class EotNMission : public Mission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;
        const char* name;

    protected:
        EotNMission(GW::Constants::MapID _outpost, const char* _name,
            const MissionImageList& normal_mode_images, const MissionImageList& hard_mode_images)
            : Mission(_outpost, normal_mode_images, hard_mode_images), name(_name) {}

    public:
        EotNMission(GW::Constants::MapID _outpost, const char* _name)
            : Mission(_outpost, normal_mode_images, hard_mode_images), name(_name) {}

        const std::string Name() override;
        IDirect3DTexture9* GetMissionImage();
    };


    class Dungeon : public EotNMission
    {
    private:
        static const MissionImageList normal_mode_images;
        static const MissionImageList hard_mode_images;

    public:
        Dungeon(GW::Constants::MapID _outpost, const char* _name)
            : EotNMission(_outpost, _name, normal_mode_images, hard_mode_images) {}
    };
}