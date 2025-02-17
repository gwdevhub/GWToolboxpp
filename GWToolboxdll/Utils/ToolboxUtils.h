#pragma once
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>

class StoCCallback {
    GW::HookEntry* hook_entry = nullptr;
    const GW::StoC::PacketCallback callback;
    const uint32_t header;
    const int altitude;

public:
    StoCCallback(const uint32_t _header, const GW::StoC::PacketCallback _callback, const int _altitude = -0x8000)
        : header(_header), callback(_callback), altitude(_altitude) { }
    void detach() {
        if (!hook_entry) return;
        GW::StoC::RemoveCallback(header, hook_entry);
        delete hook_entry;
        hook_entry = nullptr;
    }
    void attach() {
        hook_entry = new GW::HookEntry;
        GW::StoC::RegisterPacketCallback(hook_entry, header, callback, altitude);
    }
    ~StoCCallback() {
        detach();
    }
};

struct ImRect;

namespace GW {
    struct MapProp;
    struct AreaInfo;
    struct Player;
    struct Agent;
    struct AgentLiving;
    struct HeroPartyMember;
    struct PlayerPartyMember;
    struct HenchmanPartyMember;
    struct PartyInfo;
    struct HeroInfo;
    struct Friend;
    struct Item;
    enum class FriendType : uint32_t;
    enum class FriendStatus : uint32_t;

    namespace Constants {
        enum class SkillID : uint32_t;
    }

    template <typename T>
    class Array;

    namespace Packet::StoC {
        struct PacketBase;
    }
    typedef Array<PlayerPartyMember> PlayerPartyMemberArray;
    struct AvailableCharacterInfo {
        /* + h0000 */
        uint32_t h0000[2];
        /* + h0008 */
        uint32_t uuid[4];
        /* + h0018 */
        wchar_t player_name[20];
        /* + h0040 */
        uint32_t props[17];

        GW::Constants::MapID map_id() const
        {
            return static_cast<GW::Constants::MapID>((props[0] >> 16) & 0xffff);
        }

        uint32_t primary() const
        {
            return ((props[2] >> 20) & 0xf);
        }
        uint32_t secondary() const
        {
            return ((props[7] >> 10) & 0xf);
        }

        uint32_t campaign() const
        {
            return (props[7] & 0xf);
        }

        uint32_t level() const
        {
            return ((props[7] >> 4)  & 0x3f);
        }

        bool is_pvp() const
        {
            return ((props[7] >> 9) & 0x1) == 0x1;
        }
    };
    static_assert(sizeof(AvailableCharacterInfo) == 0x84);

}
namespace GW {
    typedef void(__cdecl* OnGotFrame_Callback)(UI::Frame*);
    void WaitForFrame(const wchar_t* frame_label, OnGotFrame_Callback callback);
    namespace Map {
        GW::Array<GW::MapProp*>* GetMapProps();
        bool GetMapWorldMapBounds(GW::AreaInfo* map, ImRect* out);

        void PingCompass(const GW::GamePos& position);
    }
    namespace PartyMgr {
        GW::PlayerPartyMemberArray* GetPartyPlayers(uint32_t party_id = 0);
        size_t GetPlayerPartyIndex(uint32_t player_number, uint32_t party_id = 0);
        // Fetches agent id of party member (hero, player or henchman) by index. This is NOT player number! party_member_index is ZERO based
        uint32_t GetPartyMemberAgentId(uint32_t party_member_index, uint32_t party_id = 0);
    }
    namespace AccountMgr {
        // Return pointer to array containing list of playable characters from character select screen.
        GW::Array<AvailableCharacterInfo>* GetAvailableChars();

        const wchar_t* GetCurrentPlayerName();

        // Try not to be a dick with this info
        const wchar_t* GetAccountEmail();
        const UUID* GetPortalAccountUuid();

        AvailableCharacterInfo* GetAvailableCharacter(const wchar_t* name);
    }
    namespace MemoryMgr {
        bool GetPersonalDir(std::wstring& out);
    }
    namespace UI {
        void AsyncDecodeStr(const wchar_t* enc_str, std::wstring* out, GW::Constants::Language language_id = (GW::Constants::Language)0xff);
    }
    namespace Agents {
        void AsyncGetAgentName(const Agent* agent, std::wstring& out);
    }
}

namespace ToolboxUtils {

    bool ArrayBoolAt(const GW::Array<uint32_t>&, const uint32_t);
    // Map

    bool IsOutpost();
    bool IsExplorable();

    enum MissionState : uint8_t {
        Primary = 0x1,
        Expert = 0x2,
        Master = 0x4
    };

    // Returns bitwise state of MissionState enum
    uint8_t GetMissionState(GW::Constants::MapID map_id, const GW::Array<uint32_t>& missions_completed, const GW::Array<uint32_t>& missions_bonus);
    // Returns bitwise state of MissionState enum
    uint8_t GetMissionState(GW::Constants::MapID map_id, bool is_hard_mode = false);
    // Returns bitwise state of MissionState enum
    uint8_t GetMissionState();

    // Player

    // Return player name without guild tag or player number
    std::wstring GetPlayerName(uint32_t player_number = 0);
    // Looks for player whilst sanitising for comparison
    GW::Player* GetPlayerByName(const wchar_t* _name);
    // Pass GW::AgentLiving** to also grab the Agent info for this player
    GW::Player* GetPlayerByAgentId(uint32_t agent_id, GW::AgentLiving** info_out = nullptr);

    // Messages

    // Get buffer from world context for incoming chat message
    GW::Array<wchar_t>* GetMessageBuffer();
    // Get encoded message from world context for incoming chat message
    const wchar_t* GetMessageCore();
    // Clear incoming chat message buffer, usually needed when blocking an incoming message packet.
    bool ClearMessageCore();
    // Find the name of the player related to the incoming packet e.g. trade partner, chat message sender
    const std::wstring GetSenderFromPacket(GW::Packet::StoC::PacketBase* packet);

    // Heros
    bool IsHenchman(uint32_t agent_id);
    bool IsHero(uint32_t agent_id, GW::HeroInfo** info_out = nullptr);

    // Party related

    // Find HenchmanPartyMember by agent_id, pass GW::PartyInfo** to also grab the party this henchman belongs to
    const GW::HenchmanPartyMember* GetHenchmanPartyMember(uint32_t, GW::PartyInfo** = nullptr);
    bool IsHenchmanInParty(uint32_t);
    // Find HeroPartyMember by agent_id, pass GW::PartyInfo** to also grab the party this hero belongs to
    const GW::HeroPartyMember* GetHeroPartyMember(uint32_t, GW::PartyInfo** = nullptr);
    bool IsHeroInParty(uint32_t);
    // Find PlayerPartyMember by agent_id, pass GW::PartyInfo** to also grab the party this player belongs to
    const GW::PlayerPartyMember* GetPlayerPartyMember(uint32_t, GW::PartyInfo** = nullptr);
    bool IsPlayerInParty(uint32_t);
    bool IsAgentInParty(uint32_t);

    // Skills

    // Get skill range in gwinches
    float GetSkillRange(GW::Constants::SkillID skill_id);

    // Friends
    GW::Friend* GetFriend(const wchar_t* account, const wchar_t* playing, GW::FriendType type, GW::FriendStatus status);

    std::wstring ShorthandItemDescription(GW::Item* item);
};
