#pragma once

namespace GW {
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
}

namespace ToolboxUtils {
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

    GW::HeroInfo* GetHeroInfo(uint32_t hero_id);
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
