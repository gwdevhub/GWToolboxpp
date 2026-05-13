#include "stdafx.h"

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Hero.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Constants/Skills.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>
#include <Windows/HeroBuildsWindow.h>

#include <GWToolbox.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

constexpr const wchar_t* INI_FILENAME = L"herobuilds.ini";

// Party loadout encoding helpers (Daybreak extended template, header=15, type=1, version=1).
// Bit stream uses LSB-first ordering throughout.
namespace PartyLoadout {

    // GW's base64 alphabet (same as standard base64)
    constexpr char GW_B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Reverse lookup: ASCII char → 6-bit index (0 for invalid chars)
    constexpr uint8_t GW_B64_REV[128] = {
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-15
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 16-31
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,62, 0, 0, 0,63,  // 32-47  ('+', '/')
        52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0,  // 48-63  ('0'-'9')
         0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  // 64-79  ('A'-'O')
        15,16,17,18,19,20,21,22,23,24,25, 0, 0, 0, 0, 0,  // 80-95  ('P'-'Z')
         0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  // 96-111 ('a'-'o')
        41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0,  // 112-127 ('p'-'z')
    };

    // LSB-first bit stream for encoding and decoding
    struct BitStream {
        std::vector<uint8_t> data;
        size_t bit_count = 0;
        size_t read_pos  = 0;

        void write(uint32_t val, int n) {
            for (int i = 0; i < n; i++) {
                if (bit_count % 8 == 0)
                    data.push_back(0);
                if ((val >> i) & 1)
                    data.back() |= static_cast<uint8_t>(1u << (bit_count % 8));
                ++bit_count;
            }
        }

        uint32_t read(int n) {
            uint32_t val = 0;
            for (int i = 0; i < n && read_pos < bit_count; i++) {
                if (data[read_pos / 8] & (1u << (read_pos % 8)))
                    val |= (1u << i);
                ++read_pos;
            }
            return val;
        }

        bool ok() const { return read_pos <= bit_count; }

        void to_base64(std::string& out) const {
            out.clear();
            size_t pos = 0;
            while (pos < bit_count) {
                const int bits = static_cast<int>(std::min<size_t>(6, bit_count - pos));
                uint8_t val = 0;
                for (int i = 0; i < bits; i++) {
                    if (data[(pos + i) / 8] & (1u << ((pos + i) % 8)))
                        val |= static_cast<uint8_t>(1u << i);
                }
                out += GW_B64[val];
                pos += static_cast<size_t>(bits);
            }
        }

        void from_base64(const std::string& s) {
            data.clear();
            bit_count = 0;
            read_pos  = 0;
            for (const char c : s) {
                if (static_cast<unsigned char>(c) >= 128) continue;
                const uint8_t v = GW_B64_REV[static_cast<unsigned char>(c)];
                for (int i = 0; i < 6; i++) {
                    if (bit_count % 8 == 0)
                        data.push_back(0);
                    if ((v >> i) & 1)
                        data.back() |= static_cast<uint8_t>(1u << (bit_count % 8));
                    ++bit_count;
                }
            }
        }
    };

    // Encode a SkillTemplate (decoded from an existing code string) into the bit stream
    // as an embedded build template (without standalone header/version bytes).
    bool EncodeEmbeddedTemplate(BitStream& bs, const GW::SkillbarMgr::SkillTemplate& t) {
        // Choose minimum profession bit width
        const uint32_t max_prof = std::max(static_cast<uint32_t>(t.primary), static_cast<uint32_t>(t.secondary));
        const uint8_t pl = (max_prof >= 16) ? 1 : 0;
        const int p = pl * 2 + 4;

        // Choose minimum attribute bit width
        uint32_t max_attr = 0;
        for (uint32_t i = 0; i < t.attributes_count && i < 12; i++)
            max_attr = std::max(max_attr, static_cast<uint32_t>(t.attribute_ids[i]));
        uint8_t al = 0;
        while ((1u << (al + 4)) <= max_attr && al < 15) al++;
        const int a = al + 4;

        // Choose minimum skill bit width
        uint32_t max_skill = 0;
        for (int i = 0; i < 8; i++)
            max_skill = std::max(max_skill, static_cast<uint32_t>(t.skills[i]));
        uint8_t sl = 0;
        while ((1u << (sl + 8)) <= max_skill && sl < 15) sl++;
        const int s = sl + 8;

        bs.write(pl, 2);
        bs.write(static_cast<uint32_t>(t.primary), p);
        bs.write(static_cast<uint32_t>(t.secondary), p);

        const uint32_t ac = std::min<uint32_t>(t.attributes_count, 12);
        bs.write(ac, 4);
        bs.write(al, 4);
        for (uint32_t i = 0; i < ac; i++) {
            bs.write(static_cast<uint32_t>(t.attribute_ids[i]), a);
            bs.write(t.attribute_values[i], 4);
        }

        bs.write(sl, 4);
        for (int i = 0; i < 8; i++)
            bs.write(static_cast<uint32_t>(t.skills[i]), s);

        return true;
    }

    // Decode an embedded build template from the bit stream into a SkillTemplate.
    bool DecodeEmbeddedTemplate(BitStream& bs, GW::SkillbarMgr::SkillTemplate& t) {
        t = {};

        const uint8_t pl = static_cast<uint8_t>(bs.read(2));
        const int p = pl * 2 + 4;

        t.primary   = static_cast<GW::Constants::Profession>(bs.read(p));
        t.secondary = static_cast<GW::Constants::Profession>(bs.read(p));

        const uint32_t ac = bs.read(4);
        const uint8_t al  = static_cast<uint8_t>(bs.read(4));
        const int a       = al + 4;
        t.attributes_count = std::min<uint32_t>(ac, 12);

        for (uint32_t i = 0; i < t.attributes_count; i++) {
            t.attribute_ids[i]    = static_cast<GW::Constants::Attribute>(bs.read(a));
            t.attribute_values[i] = bs.read(4);
        }

        const uint8_t sl = static_cast<uint8_t>(bs.read(4));
        const int s = sl + 8;
        for (int i = 0; i < 8; i++)
            t.skills[i] = static_cast<GW::Constants::SkillID>(bs.read(s));

        return bs.ok();
    }

    constexpr uint8_t MEMBER_PLAYER   = 0;
    constexpr uint8_t MEMBER_HENCHMAN = 1;
    constexpr uint8_t MEMBER_HERO     = 2;

    // Detect the party loadout v1 magic prefix ("fA") in an ASCII string
    inline bool IsPartyLoadout(const std::string& s) {
        return s.size() >= 2 && s[0] == 'f' && s[1] == 'A';
    }

} // namespace PartyLoadout

namespace {
    GW::HookEntry ChatCmd_HookEntry;

    // GW file 0x268f6: 2x2 sprite sheet (tick/cross overlays)
    // Bottom-left sprite (col 0, row 1) = semi-transparent cross = "disabled" overlay
    IDirect3DTexture9** skill_toggle_sprite = nullptr;

    using GW::Constants::HeroID;

    // hero index is an arbitrary index.
    // We aim to have the same order as in the gw client.
    // Razah is after the mesmers because all players that don't have mercenaries have it set as mesmer.
    constexpr std::array HeroIndexToID = {
        HeroID::NoHero,
        HeroID::Goren,
        HeroID::Koss,
        HeroID::Jora,
        HeroID::AcolyteJin,
        HeroID::MargridTheSly,
        HeroID::PyreFierceshot,
        HeroID::Tahlkora,
        HeroID::Dunkoro,
        HeroID::Ogden,
        HeroID::MasterOfWhispers,
        HeroID::Olias,
        HeroID::Livia,
        HeroID::Norgu,
        HeroID::Razah,
        HeroID::Gwen,
        HeroID::AcolyteSousuke,
        HeroID::ZhedShadowhoof,
        HeroID::Vekk,
        HeroID::Zenmai,
        HeroID::Anton,
        HeroID::Miku,
        HeroID::Xandra,
        HeroID::ZeiRi,
        HeroID::GeneralMorgahn,
        HeroID::KeiranThackeray,
        HeroID::Hayda,
        HeroID::Melonni,
        HeroID::MOX,
        HeroID::Kahmu,
        HeroID::Merc1,
        HeroID::Merc2,
        HeroID::Merc3,
        HeroID::Merc4,
        HeroID::Merc5,
        HeroID::Merc6,
        HeroID::Merc7,
        HeroID::Merc8,
        HeroID::Devona,
        HeroID::GhostOfAlthea
    };

    // Returns hero IDs sorted by name; re-sorts each frame until all names are decoded.
    const std::vector<HeroID>& SortedHeroIDs() {
        static std::vector<HeroID> sorted;
        static bool is_sorted = false;
        if (!is_sorted) {
            sorted.clear();
            for (const auto id : HeroIndexToID) {
                if (id != HeroID::NoHero) sorted.push_back(id);
            }
            bool all_decoded = true;
            for (const auto id : sorted) {
                if (Resources::GetHeroName(id)->string().empty()) {
                    all_decoded = false;
                    break;
                }
            }
            std::ranges::sort(sorted, [](const HeroID a, const HeroID b) {
                return _stricmp(Resources::GetHeroName(a)->string().c_str(), Resources::GetHeroName(b)->string().c_str()) < 0;
            });
            is_sorted = all_decoded;
        }
        return sorted;
    }

    const size_t GetPlayerHeroCount()
    {
        size_t ret = 0;
        const GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
        if (!party_info) {
            return ret;
        }
        const GW::HeroPartyMemberArray& party_heros = party_info->heroes;
        if (!party_heros.valid()) {
            return ret;
        }
        const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
        if (!me) {
            return ret;
        }
        const uint32_t my_player_id = me->login_number;
        for (size_t i = 0; i < party_heros.size(); i++) {
            if (party_heros[i].owner_player_id == my_player_id) {
                ret++;
            }
        }
        return ret;
    }

    const GW::HeroFlag* GetHeroFlagInfo(const uint32_t hero_id)
    {
        const GW::GameContext* g = GW::GetGameContext();
        if (!g || !g->world) {
            return nullptr;
        }
        const GW::HeroFlagArray& flags = g->world->hero_flags;
        for (const GW::HeroFlag& flag : flags) {
            if (flag.hero_id == hero_id) {
                return &flag;
            }
        }
        return nullptr;
    }
}

unsigned int HeroBuildsWindow::TeamHeroBuild::cur_ui_id = 0;
GW::HookEntry HeroBuildsWindow::OnWhisper_Entry;

bool HeroBuildsWindow::EncodePartyLoadout(const TeamHeroBuild& tbuild, std::string& out)
{
    using namespace PartyLoadout;

    // Count non-empty slots
    int party_size = 0;
    for (int i = 0; i < 8; i++) {
        if (tbuild.builds[i].code[0] != 0)
            ++party_size;
    }
    if (party_size == 0)
        return false;

    BitStream bs;
    // Extended template header: 4 bits = 15, type: 8 bits = 1, version: 4 bits = 1
    bs.write(15, 4);
    bs.write(1, 8);
    bs.write(1, 4);
    bs.write(static_cast<uint32_t>(party_size), 4);

    for (int i = 0; i < 8; i++) {
        const HeroBuild& build = tbuild.builds[i];
        if (build.code[0] == 0)
            continue;

        const uint8_t member_type = (i == 0) ? MEMBER_PLAYER
            : (build.hero_id != HeroID::NoHero ? MEMBER_HERO : MEMBER_HENCHMAN);
        bs.write(member_type, 2);
        if (member_type == MEMBER_HERO)
            bs.write(static_cast<uint32_t>(build.hero_id), 6);
        bs.write(build.behavior, 2);

        // Decode the existing code string to get the SkillTemplate fields
        GW::SkillbarMgr::SkillTemplate tmpl{};
        if (build.code[0])
            GW::SkillbarMgr::DecodeSkillTemplate(tmpl, build.code);
        EncodeEmbeddedTemplate(bs, tmpl);
    }

    bs.to_base64(out);
    return !out.empty();
}

bool HeroBuildsWindow::DecodePartyLoadout(const std::string& encoded, TeamHeroBuild& out)
{
    using namespace PartyLoadout;

    if (!IsPartyLoadout(encoded))
        return false;

    BitStream bs;
    bs.from_base64(encoded);

    // Validate header
    if (bs.read(4) != 15) return false;  // extended template header
    if (bs.read(8) != 1)  return false;  // type = party loadout
    if (bs.read(4) != 1)  return false;  // version = 1

    const uint32_t party_size = bs.read(4);
    if (party_size == 0 || party_size > 8) return false;
    if (!bs.ok()) return false;

    int slot = 0;
    for (uint32_t m = 0; m < party_size && slot < 8; m++) {
        const auto member_type = static_cast<uint8_t>(bs.read(2));
        uint32_t hero_id_val = 0;
        if (member_type == MEMBER_HERO)
            hero_id_val = bs.read(6);
        const uint32_t behavior = bs.read(2);

        GW::SkillbarMgr::SkillTemplate tmpl{};
        if (!DecodeEmbeddedTemplate(bs, tmpl)) return false;

        // Re-encode the skill template to a code string
        char code[BUFFER_SIZE]{};
        GW::SkillbarMgr::EncodeSkillTemplate(tmpl, code, sizeof(code));

        const auto hero_id = static_cast<GW::Constants::HeroID>(hero_id_val);
        out.builds[slot] = HeroBuild("", code, hero_id, 0, behavior, 0);
        ++slot;
    }

    return bs.ok();
}

// Incoming whisper callback — detects party loadout templates and auto-imports them.
static void OnIncomingWhisper(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*)
{
    const auto packet = static_cast<GW::UI::UIPacket::kWriteToChatLog*>(wparam);
    if (packet->channel != GW::Chat::Channel::CHANNEL_WHISPER)
        return;
    const wchar_t* msg = packet->message;
    if (!msg || static_cast<uint16_t>(msg[0]) != 0x76d)
        return; // Not an incoming whisper message type

    // Sender name is in the first \x107 ... \x1 segment
    const wchar_t* name_start = wcschr(msg, 0x107);
    if (!name_start) return;
    name_start++;
    const wchar_t* name_end = wcschr(name_start, 0x1);
    if (!name_end) return;

    // Message text is in the LAST \x107 ... \x1 segment
    const wchar_t* text_start = wcsrchr(msg, 0x107);
    if (!text_start || text_start <= name_end) return;
    text_start++;
    const wchar_t* text_end = wcschr(text_start, 0x1);
    if (!text_end) return;

    // Convert to ASCII; bail on any non-ASCII char (not a base64 template)
    std::string encoded;
    encoded.reserve(static_cast<size_t>(text_end - text_start));
    for (const wchar_t* p = text_start; p < text_end; p++) {
        if (*p >= 128) return;
        encoded += static_cast<char>(*p);
    }

    if (!PartyLoadout::IsPartyLoadout(encoded))
        return;

    const std::wstring sender(name_start, name_end);
    HeroBuildsWindow::Instance().ImportTeamBuildFromWhisper(encoded, sender);
}

void HeroBuildsWindow::ImportTeamBuildFromWhisper(const std::string& encoded, const std::wstring& sender)
{
    const auto sender_str = TextUtils::WStringToString(sender);
    TeamHeroBuild received(std::format("Build from {}", sender_str));
    if (!DecodePartyLoadout(encoded, received))
        return;
    received.edit_open = true;
    teambuilds.push_back(std::move(received));
    builds_changed = true;
    Log::Flash("Received teambuild from %s!", sender_str.c_str());
}

GW::HeroPartyMember* HeroBuildsWindow::GetPartyHeroByID(const HeroID hero_id, size_t* out_hero_index)
{
    GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
    if (!party_info) {
        return nullptr;
    }
    GW::HeroPartyMemberArray& party_heros = party_info->heroes;
    if (!party_heros.valid()) {
        return nullptr;
    }
    const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
    if (!me) {
        return nullptr;
    }
    const uint32_t my_player_id = me->login_number;
    for (size_t i = 0; i < party_heros.size(); i++) {
        if (party_heros[i].owner_player_id == my_player_id &&
            party_heros[i].hero_id == hero_id) {
            if (out_hero_index) {
                *out_hero_index = i + 1;
            }
            return &party_heros[i];
        }
    }
    return nullptr;
}

void HeroBuildsWindow::Initialize()
{
    ToolboxWindow::Initialize();
    send_timer = TIMER_INIT();
    skill_toggle_sprite = GwDatTextureModule::LoadTextureFromFileId(0x268f6);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"heroteam", &CmdHeroTeamBuild);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"herobuild", &CmdHeroTeamBuild);
    GW::UI::RegisterUIMessageCallback(&OnWhisper_Entry, GW::UI::UIMessage::kWriteToChatLog, OnIncomingWhisper);
}

void HeroBuildsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    teambuilds.clear();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnWhisper_Entry);
}

void HeroBuildsWindow::Draw(IDirect3DDevice9*)
{
    if (visible) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            const auto* me = GW::Agents::GetControlledCharacter();
            const auto player_profession = me ? static_cast<GW::Constants::Profession>(me->primary) : GW::Constants::Profession::None;
            if (player_profession != GW::Constants::Profession::None) {
                char filter_label[64];
                snprintf(filter_label, sizeof(filter_label), "Filter by %s", ToolboxUtils::GetProfessionName(player_profession)->string().c_str());
                ImGui::Checkbox(filter_label, &filter_by_profession);
            }
            const float btn_width = 60.0f * ImGui::FontScale();
            const float& item_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

            // Collect filtered builds, preserving order
            std::vector<TeamHeroBuild*> filtered;
            for (TeamHeroBuild& tbuild : teambuilds) {
                if (filter_by_profession && player_profession != GW::Constants::Profession::None) {
                    const char* player_code = tbuild.builds[0].code;
                    if (player_code && player_code[0] != '\0') {
                        GW::SkillbarMgr::SkillTemplate t{};
                        if (GW::SkillbarMgr::DecodeSkillTemplate(t, player_code) && t.primary != player_profession) {
                            continue;
                        }
                    }
                }
                filtered.push_back(&tbuild);
            }

            // Build ordered group list
            std::vector<std::string> group_order;
            std::unordered_map<std::string, std::vector<TeamHeroBuild*>> by_group;
            for (TeamHeroBuild* tbuild : filtered) {
                const std::string g(tbuild->group);
                if (!by_group.contains(g)) {
                    by_group[g] = {};
                    group_order.push_back(g);
                }
                by_group[g].push_back(tbuild);
            }

            auto draw_teambuild_row = [&](TeamHeroBuild& tbuild) {
                ImGui::PushID(static_cast<int>(tbuild.ui_id));
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
                if (ImGui::Button(tbuild.name, ImVec2(ImGui::GetContentRegionAvail().x - item_spacing - btn_width, 0))) {
                    if (one_teambuild_at_a_time && !tbuild.edit_open) {
                        for (auto& tb : teambuilds) {
                            tb.edit_open = false;
                        }
                    }
                    tbuild.edit_open = !tbuild.edit_open;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip([&tbuild, this]() {
                        for (size_t ti = 0; ti < tbuild.builds.size(); ti++) {
                            const auto& build = tbuild.builds[ti];
                            if (build.code[0] == 0 && build.name[0] == 0) continue;
                            ImGui::Spacing();
                            std::string name;
                            HeroBuildName(tbuild, ti, &name);
                            ImGui::TextUnformatted(name.empty() ? build.name : name.c_str());
                            GuiUtils::DrawSkillbar(build.code, false);
                            ImGui::Spacing();
                        }
                    });
                }
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5f, 0.5f);
                ImGui::SameLine(0, item_spacing);
                if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "Load", ImVec2(btn_width, 0))) {
                    if (ImGui::GetIO().KeyCtrl) {
                        Send(tbuild);
                    }
                    else {
                        Load(tbuild);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to load builds to heroes and player. Ctrl + Click to send to chat.");
                }
                ImGui::PopID();
            };

            for (const auto& group_name : group_order) {
                auto& group_builds = by_group[group_name];
                if (group_name.empty()) {
                    for (TeamHeroBuild* tbuild : group_builds) {
                        draw_teambuild_row(*tbuild);
                    }
                }
                else {
                    ImGui::PushID(group_name.c_str());
                    const bool open = ImGui::CollapsingHeader(group_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                    if (open) {
                        ImGui::Indent();
                        for (TeamHeroBuild* tbuild : group_builds) {
                            draw_teambuild_row(*tbuild);
                        }
                        ImGui::Unindent();
                    }
                    ImGui::PopID();
                }
            }
            if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                auto tb = TeamHeroBuild(std::format("My New Hero Teambuild, {}", TextUtils::GetFormattedDateTime()));
                tb.edit_open = true;
                GW::SkillbarMgr::SkillTemplate skill_template;
                for (auto i = 0u; i < 8; i++) {
                    if (!GW::SkillbarMgr::GetSkillTemplate(GW::PartyMgr::GetPartyMemberAgentId(i), skill_template)) continue;
                    char buf[BUFFER_SIZE]{};
                    if (!GW::SkillbarMgr::EncodeSkillTemplate(skill_template, buf, BUFFER_SIZE)) continue;
                    const auto party_info = GW::PartyMgr::GetPartyInfo();
                    const auto cur_hero_id = i > 0 && party_info && party_info->heroes.size() >= i
                        ? party_info->heroes[i - 1].hero_id
                        : HeroID::NoHero;
                    const GW::HeroFlag* flag = cur_hero_id != HeroID::NoHero ? GetHeroFlagInfo(cur_hero_id) : nullptr;
                    tb.builds[i] = HeroBuild("", buf, cur_hero_id, 0, static_cast<uint32_t>(flag ? flag->hero_behavior : GW::HeroBehavior::Guard));
                }

                builds_changed = true;
                teambuilds.push_back(tb);
            }
            /* Code for copying a teambuild */
            std::vector<const char*> names(teambuilds.size(), "\0");
            std::ranges::transform(
                teambuilds,
                names.begin(),
                [](const TeamHeroBuild& tb) { return tb.name; }
            );
            if (const auto num_elements = names.size()) {
                static int selected_teambuild = 0;
                ImGui::PushItemWidth(-60.0f - ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::Combo("###teamBuildCombo", &selected_teambuild, names.data(), num_elements);
                ImGui::PopItemWidth();
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Copy##1", ImVec2(60.0f, 0))) {
                    TeamHeroBuild new_tb = teambuilds[selected_teambuild];
                    std::snprintf(new_tb.name, sizeof(new_tb.name), "%s (Copy)", new_tb.name);
                    builds_changed = true;
                    teambuilds.push_back(new_tb);
                }
            }
        }
        ImGui::End();
    }

    for (size_t i = 0; i < teambuilds.size(); i++) {
        if (!teambuilds[i].edit_open) {
            continue;
        }
        TeamHeroBuild& tbuild = teambuilds[i];
        constexpr size_t winname_buffer_size = 256;
        char winname[winname_buffer_size];
        snprintf(winname, winname_buffer_size, "%s###herobuild%d", tbuild.name, tbuild.ui_id);
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winname, &tbuild.edit_open)) {
            constexpr size_t name_buffer_size = 128;
            builds_changed |= ImGui::InputText("Hero Build Name", tbuild.name, name_buffer_size);
            builds_changed |= ImGui::InputText("Group", tbuild.group, name_buffer_size);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Assign to a group. Builds sharing a group name are shown together under a collapsible header.");
            }
            const float btn_width = 50.0f * ImGui::FontScale();
            const float icon_btn_width = btn_width / 1.75f;
            const float panel_width = btn_width + 12.0f;
            const float item_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            const float text_item_width = (ImGui::GetContentRegionAvail().x - btn_width - btn_width - btn_width - panel_width - icon_btn_width - item_spacing * 4) / 3;
            float offset = btn_width;
            ImGui::SetCursorPosX(offset);
            ImGui::Text("Name");
            ImGui::SameLine(offset += text_item_width + item_spacing);
            ImGui::Text("Template");
            for (size_t j = 0; j < tbuild.builds.size(); ++j) {
                offset = btn_width;
                HeroBuild& build = tbuild.builds[j];
                ImGui::PushID(static_cast<int>(j));
                if (j == 0) {
                    ImGui::Text("P");
                }
                else {
                    ImGui::Text("H#%d", j);
                }
                ImGui::SameLine(offset);
                ImGui::PushItemWidth(text_item_width);
                builds_changed |= ImGui::InputText("###name", build.name, name_buffer_size);
                if (ImGui::IsItemHovered() && *build.name) {
                    ImGui::SetTooltip("%s", build.name);
                }
                ImGui::SameLine(offset += text_item_width + item_spacing);
                builds_changed |= ImGui::InputText("###code", build.code, name_buffer_size);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip([&]() {
                        GuiUtils::DrawSkillbar(build.code);
                        });
                }
                ImGui::SameLine(offset += text_item_width + item_spacing);
                if (j == 0) {
                    ImGui::TextDisabled("Player");
                    ImGui::SameLine(offset += text_item_width + item_spacing + btn_width + 10.0f + icon_btn_width + item_spacing * 2);
                    ImGui::PopItemWidth();
                }
                else {
                    {
                        const auto& sorted_heroes = SortedHeroIDs();
                        const auto hero_it = std::ranges::find(sorted_heroes, build.hero_id);
                        int combo_idx = hero_it != sorted_heroes.end()
                            ? static_cast<int>(std::distance(sorted_heroes.begin(), hero_it))
                            : -1;
                        if (ImGui::MyCombo(
                            "###heroid", "Choose Hero", &combo_idx,
                            [](void*, const int idx, const char** out_text) -> bool {
                                const auto& heroes = SortedHeroIDs();
                                if (idx < 0 || idx >= static_cast<int>(heroes.size())) return false;
                                *out_text = Resources::GetHeroName(heroes[idx])->string().c_str();
                                return true;
                            },
                            nullptr, static_cast<int>(sorted_heroes.size()))) {
                            build.hero_id = combo_idx >= 0 && combo_idx < static_cast<int>(sorted_heroes.size())
                                ? sorted_heroes[combo_idx]
                                : HeroID::NoHero;
                            builds_changed = true;
                        }
                    }
                    ImGui::PopItemWidth();
                    ImGui::SameLine(offset += text_item_width + item_spacing);
                    const auto* icon = reinterpret_cast<const char*>(build.show_panel ? ICON_FA_EYE : ICON_FA_EYE_SLASH);
                    if (ImGui::Button(icon, ImVec2(icon_btn_width, 0))) {
                        build.show_panel = !build.show_panel;
                        builds_changed = true;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(build.show_panel ? "Hero panel: Show" : "Hero panel: Hide");
                    }
                    ImGui::SameLine(offset += icon_btn_width + item_spacing);
                    auto hero_stance_icon = ICON_FA_SHIELD_ALT;
                    auto hero_stance_tooltip = "Hero behaviour: Guard";
                    switch (build.behavior) {
                        case 2:
                            hero_stance_icon = reinterpret_cast<const char*>(ICON_FA_DOVE);
                            hero_stance_tooltip = "Hero behaviour: Avoid Combat";
                            break;
                        case 0:
                            hero_stance_icon = reinterpret_cast<const char*>(ICON_FA_FIST_RAISED);
                            hero_stance_tooltip = "Hero behaviour: Fight";
                            break;
                    }
                    if (ImGui::Button(hero_stance_icon, ImVec2(icon_btn_width, 0))) {
                        build.behavior++;
                        if (build.behavior > 2) {
                            build.behavior = 0;
                        }
                        builds_changed = true;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(hero_stance_tooltip);
                    }
                    ImGui::SameLine(offset += icon_btn_width + item_spacing);

                    int enabled_count = 8;
                    for (int k = 0; k < 8; k++) {
                        if ((build.disabled_skills >> k) & 1) {
                            enabled_count--;
                        }
                    }
                    char skills_label[8];
                    snprintf(skills_label, sizeof(skills_label), "%d/8", enabled_count);
                    if (ImGui::Button(skills_label, ImVec2(icon_btn_width, 0))) {
                        ImGui::OpenPopup("SkillsPopup");
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Click to toggle which skills are disabled when loading this build");
                    }
                    if (ImGui::BeginPopup("SkillsPopup")) {
                        GW::SkillbarMgr::SkillTemplate skill_template{};
                        const bool decoded = build.code[0] && GW::SkillbarMgr::DecodeSkillTemplate(skill_template, build.code);

                        constexpr float skill_btn_px = 48.0f;
                        const ImVec2 btn_size(skill_btn_px, skill_btn_px);

                        for (int k = 0; k < 8; k++) {
                            if (k > 0) {
                                ImGui::SameLine(0, 0);
                            }

                            const bool is_disabled = (build.disabled_skills >> k) & 1;
                            const auto skill_id = decoded ? skill_template.skills[k] : GW::Constants::SkillID::No_Skill;
                            auto* skill_tex = *Resources::GetSkillImage(skill_id);

                            ImGui::PushID(k);
                            const ImVec2 pos = ImGui::GetCursorScreenPos();

                            if (ImGui::InvisibleButton("##skill_slot", btn_size)) {
                                if (is_disabled) {
                                    build.disabled_skills &= static_cast<uint8_t>(~(1u << k));
                                }
                                else {
                                    build.disabled_skills |= static_cast<uint8_t>(1u << k);
                                }
                                builds_changed = true;
                            }
                            const bool hovered = ImGui::IsItemHovered();

                            const ImVec2 p_max(pos.x + skill_btn_px, pos.y + skill_btn_px);
                            auto* draw_list = ImGui::GetWindowDrawList();

                            if (skill_id != GW::Constants::SkillID::No_Skill && skill_tex) {
                                draw_list->AddImage(reinterpret_cast<ImTextureID>(skill_tex), pos, p_max);
                            }
                            else {
                                draw_list->AddRectFilled(pos, p_max, IM_COL32(50, 50, 50, 255));
                            }

                            // Dark overlay then semi-transparent cross to clearly show disabled state
                            if (is_disabled) {
                                draw_list->AddRectFilled(pos, p_max, IM_COL32(0, 0, 0, 140));
                                if (skill_toggle_sprite && *skill_toggle_sprite) {
                                    // Bottom-left of 0x268f6 sprite sheet (col 0, row 1): semi-transparent cross
                                    draw_list->AddImage(reinterpret_cast<ImTextureID>(*skill_toggle_sprite), pos, p_max,
                                        ImVec2(0.0f, 0.5f), ImVec2(0.5f, 1.0f));
                                }
                            }

                            if (hovered) {
                                draw_list->AddRect(pos, p_max, IM_COL32(255, 255, 255, 200), 0.0f, 0, 2.0f);
                            }

                            ImGui::PopID();
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine(offset += icon_btn_width + item_spacing);
                }

                if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "View", ImVec2(btn_width, 0))) {
                    if (ImGui::GetIO().KeyCtrl) {
                        Send(tbuild, j);
                    }
                    else {
                        View(tbuild, j);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to view build. Ctrl + Click to send to chat.");
                }
                ImGui::SameLine(offset += btn_width + item_spacing);
                if (ImGui::Button("Load", ImVec2(btn_width, 0))) {
                    Load(tbuild, j);
                }
                if (j == 0) {
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Load Build on Player");
                    }
                }
                else {
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Load Build on Hero");
                    }
                }
                ImGui::PopID();
            }
            ImGui::Spacing();

            if (ImGui::SmallButton("Up") && i > 0) {
                std::swap(teambuilds[i - 1], teambuilds[i]);
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the teambuild up in the list");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Down") && i + 1 < static_cast<int>(teambuilds.size())) {
                std::swap(teambuilds[i], teambuilds[i + 1]);
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the teambuild down in the list");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                ImGui::OpenPopup("Delete Teambuild?");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Delete the teambuild");
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(110.0f);
            const static char* modes[] = {"Don't change", "Normal Mode", "Hard Mode"};
            ImGui::Combo("Mode", &tbuild.mode, modes, 3);
            ImGui::PopItemWidth();
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
            if (ImGui::Button("Close", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                tbuild.edit_open = false;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Close this window");
            }

            // Whisper send section
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            static char whisper_target[64] = {};
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 110.0f - ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::InputText("###whisper_target", whisper_target, sizeof(whisper_target));
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enter the recipient's character name");
            }
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Button("Send Whisper", ImVec2(110.0f, 0))) {
                if (whisper_target[0]) {
                    std::string encoded;
                    if (EncodePartyLoadout(tbuild, encoded)) {
                        pending_whisper_send.recipient = TextUtils::StringToWString(whisper_target);
                        pending_whisper_send.message   = TextUtils::StringToWString(encoded);
                        pending_whisper_send.active    = true;
                    }
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Send this teambuild to another Toolbox user via whisper.\nEnter the recipient's character name in the field to the left.");
            }

            if (ImGui::BeginPopupModal("Delete Teambuild?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure?\nThis operation cannot be undone.\n\n");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    teambuilds.erase(teambuilds.begin() + static_cast<int>(i));
                    builds_changed = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}

void HeroBuildsWindow::View(const TeamHeroBuild& tbuild, const size_t idx)
{
    if (idx >= tbuild.builds.size()) {
        return;
    }
    const HeroBuild& build = tbuild.builds[idx];

    std::string build_name;
    HeroBuildName(tbuild, idx, &build_name);
    if (build_name.empty()) {
        return; // No name = no build.
    }

    auto t = new GW::UI::ChatTemplate();
    constexpr size_t buffer_size = 128;
    t->code.m_buffer = new wchar_t[buffer_size];
    MultiByteToWideChar(CP_UTF8, 0, build.code, -1, t->code.m_buffer, buffer_size);
    t->code.m_size = t->code.m_capacity = wcslen(t->code.m_buffer);
    t->name = new wchar_t[buffer_size];
    MultiByteToWideChar(CP_UTF8, 0, build_name.c_str(), -1, t->name, buffer_size);
    GW::GameThread::Enqueue([t] {
        SendUIMessage(GW::UI::UIMessage::kOpenTemplate, t);
        delete[] t->code.m_buffer;
        delete[] t->name;
        delete t;
    });
}

void HeroBuildsWindow::Send(const TeamHeroBuild& tbuild)
{
    if (!std::string(tbuild.name).empty()) {
        send_queue.push(tbuild.name);
    }
    for (size_t i = 0; i < tbuild.builds.size(); i++) {
        if (i == 0) {
            const HeroBuild& build = tbuild.builds[i];
            if (build.code[0] == 0 && build.name[0] == 0) {
                continue; // Player build is empty.
            }
        }
        Send(tbuild, i);
    }
}

void HeroBuildsWindow::Send(const TeamHeroBuild& tbuild, const size_t idx)
{
    if (idx >= tbuild.builds.size()) {
        return;
    }
    const HeroBuild& build = tbuild.builds[idx];
    const std::string name(build.name);
    const std::string code(build.code);

    constexpr int buffer_size = 139;
    char buffer[buffer_size];
    std::string build_name;
    HeroBuildName(tbuild, idx, &build_name);
    if (build_name.empty()) {
        return; // No name = no build.
    }

    if (code.empty()) {
        snprintf(buffer, buffer_size, "%s", build_name.c_str());
    }
    else {
        snprintf(buffer, buffer_size, "[%s;%s]", build_name.c_str(), build.code);
    }
    if (buffer[0]) {
        send_queue.push(buffer);
    }
}

void HeroBuildsWindow::HeroBuildName(const TeamHeroBuild& tbuild, const size_t idx, std::string* out)
{
    if (idx >= tbuild.builds.size()) {
        return;
    }
    const HeroBuild& build = tbuild.builds[idx];
    const std::string name(build.name);
    const std::string code(build.code);
    constexpr int buffer_size = 128;
    char buffer[buffer_size] = {0};
    const auto id = idx > 0 ? build.hero_id : HeroID::NoHero;
    if (name.empty() && code.empty() && id == HeroID::NoHero) {
        return; // nothing to do here
    }
    const char* c = Resources::GetHeroName(id)->string().c_str();

    if (name.empty()) {
        if (idx > 0) {
            snprintf(buffer, buffer_size, "%s", c);
        }
    }
    else {
        snprintf(buffer, buffer_size, "%s (%s)", name.c_str(), idx == 0 ? "Player" : c);
    }
    if (buffer[0]) {
        out->assign(buffer);
    }
}

const char* HeroBuildsWindow::BuildName(const size_t idx) const
{
    if (idx < teambuilds.size()) {
        return teambuilds[idx].name;
    }
    return nullptr;
}

void HeroBuildsWindow::Load(const size_t idx)
{
    if (idx < teambuilds.size()) {
        Load(teambuilds[idx]);
    }
}

void HeroBuildsWindow::Load(const TeamHeroBuild& tbuild)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        return;
    }

    GW::PartyMgr::KickAllHeroes();
    kickall_timer = TIMER_INIT();
    pending_hero_loads.clear();
    if (tbuild.mode > 0) {
        GW::PartyMgr::SetHardMode(tbuild.mode == 2);
    }
    for (size_t i = 0; i < tbuild.builds.size(); i++) {
        Load(tbuild, i);
    }
    send_timer = TIMER_INIT(); // give GW time to update the hero structs after adding them.
}

void HeroBuildsWindow::Load(const TeamHeroBuild& tbuild, const size_t idx)
{
    if (idx >= tbuild.builds.size()) {
        return;
    }
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        return;
    }
    const HeroBuild& build = tbuild.builds[idx];
    const std::string code(build.code);

    if (idx == 0) {
        // Player
        if (!code.empty()) {
            GW::SkillbarMgr::LoadSkillTemplate(GW::Agents::GetControlledCharacterId(),build.code);
        }
    }
    else if (build.hero_id != HeroID::NoHero) {
        pending_hero_loads.push_back({code.c_str(), build.hero_id, build.show_panel, build.behavior, build.disabled_skills});
    }
}

void HeroBuildsWindow::Update(float)
{
    const GW::Constants::InstanceType& instance_type = GW::Map::GetInstanceType();
    if (instance_type != last_instance_type) {
        // Run tasks on map change without an StoC hook
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            for (auto& hb : teambuilds) {
                hb.edit_open = false;
            }
            visible = false;
        }
        if (instance_type == GW::Constants::InstanceType::Loading) {
            while (send_queue.size()) {
                send_queue.pop();
            }
            kickall_timer = 0;
            pending_hero_loads.clear();
        }
        last_instance_type = instance_type;
    }

    if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
        GW::Chat::SendChat('#', send_queue.front().c_str());
        send_queue.pop();
        send_timer = TIMER_INIT();
    }
    if (pending_whisper_send.active) {
        GW::Chat::SendChat(pending_whisper_send.recipient.c_str(), pending_whisper_send.message.c_str());
        pending_whisper_send.active = false;
    }
    if (kickall_timer) {
        if (TIMER_DIFF(kickall_timer) > 500 || instance_type != GW::Constants::InstanceType::Outpost || !GetPlayerHeroCount()) {
            kickall_timer = 0;
        }
    }
    for (size_t i = 0; !kickall_timer && i < pending_hero_loads.size(); i++) {
        if (instance_type != GW::Constants::InstanceType::Outpost) {
            pending_hero_loads.clear();
            break;
        }
        if (pending_hero_loads[i].Process()) {
            pending_hero_loads.erase(pending_hero_loads.begin() + static_cast<int>(i));
            break; // Continue loop on next frame
        }
    }

    // if we open the window, load from file. If we close the window, save to file.
    static bool old_visible = false;
    bool cur_visible = false;
    cur_visible |= visible;
    for (const TeamHeroBuild& tbuild : teambuilds) {
        cur_visible |= tbuild.edit_open;
    }

    if (cur_visible != old_visible) {
        old_visible = cur_visible;
        if (cur_visible) {
            LoadFromFile();
        }
        else {
            SaveToFile();
        }
    }
    last_instance_type = instance_type;
}

void CHAT_CMD_FUNC(HeroBuildsWindow::CmdHeroTeamBuild)
{
    if (argc < 2) {
        Log::ErrorW(L"Syntax: /%s [hero_build_name]", argv[0]);
        return;
    }
    std::wstring argBuildname = argv[1];
    for (auto i = 2; i < argc; i++) {
        argBuildname.append(L" ");
        argBuildname.append(argv[i]);
    }
    const std::string argBuildName_s = TextUtils::WStringToString(argBuildname);
    const TeamHeroBuild* found = Instance().GetTeambuildByName(argBuildName_s);
    if (!found) {
        Log::ErrorW(L"No hero build found for %s", argBuildname.c_str());
        return;
    }
    const TeamHeroBuild& tbuild = *found;
    Instance().Load(tbuild);
}

void HeroBuildsWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(hide_when_entering_explorable);
    LOAD_BOOL(one_teambuild_at_a_time);
    LOAD_BOOL(filter_by_profession);
    LoadFromFile();
}

void HeroBuildsWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Hide Hero Build windows when entering explorable area", &hide_when_entering_explorable);
    ImGui::CheckboxWithHelp("Only show one teambuild window at a time", &one_teambuild_at_a_time, "Close other teambuild windows when you open a new one");
}

void HeroBuildsWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(hide_when_entering_explorable);
    SAVE_BOOL(one_teambuild_at_a_time);
    SAVE_BOOL(filter_by_profession);
    SaveToFile();
}

void HeroBuildsWindow::LoadFromFile()
{
    // clear builds from toolbox
    teambuilds.clear();

    inifile->LoadFile(Resources::GetSettingFile(INI_FILENAME).c_str());

    // then load
    ToolboxIni::TNamesDepend entries;
    inifile->GetAllSections(entries);
    for (const ToolboxIni::Entry& entry : entries) {
        const char* section = entry.pItem;

        TeamHeroBuild tb(inifile->GetValue(section, "buildname", ""));
        tb.mode = inifile->GetLongValue(section, "mode", false);
        std::snprintf(tb.group, sizeof(tb.group), "%s", inifile->GetValue(section, "group", ""));

        for (auto i = 0; i < 8; i++) {
            constexpr size_t buffer_size = 16;
            char namekey[buffer_size];
            char templatekey[buffer_size];
            char heroidkey[buffer_size];
            char heroindexkey[buffer_size];
            char showpanelkey[buffer_size];
            char behaviorkey[buffer_size];
            char dskillskey[buffer_size];
            snprintf(namekey, buffer_size, "name%d", i);
            snprintf(templatekey, buffer_size, "template%d", i);
            snprintf(heroidkey, buffer_size, "heroid%d", i);
            snprintf(heroindexkey, buffer_size, "heroindex%d", i);
            snprintf(showpanelkey, buffer_size, "panel%d", i);
            snprintf(behaviorkey, buffer_size, "behavior%d", i);
            snprintf(dskillskey, buffer_size, "dskills%d", i);
            const char* nameval = inifile->GetValue(section, namekey, "");
            const char* templateval = inifile->GetValue(section, templatekey, "");
            // Try new heroid key first; fall back to old heroindex key for backward compat
            HeroID hero_id = HeroID::NoHero;
            const long saved_hero_id = inifile->GetLongValue(section, heroidkey, -1);
            if (saved_hero_id >= 0) {
                hero_id = static_cast<HeroID>(saved_hero_id);
            }
            else {
                const long hero_index = inifile->GetLongValue(section, heroindexkey, -1);
                if (hero_index > 0 && hero_index < static_cast<long>(HeroIndexToID.size())) {
                    hero_id = HeroIndexToID.at(hero_index);
                }
            }
            const auto show_panel = inifile->GetLongValue(section, showpanelkey, 0);
            const uint32_t behavior = static_cast<uint32_t>(inifile->GetLongValue(section, behaviorkey, 1));
            const uint8_t disabled_skills = static_cast<uint8_t>(inifile->GetLongValue(section, dskillskey, 0));
            tb.builds[i] = HeroBuild(nameval, templateval, hero_id, show_panel == 1 ? 1 : 0, behavior, disabled_skills);
        }

        // Check the binary to see if we should instead take the ptr to read everything to avoid the copy
        // But this might not matter at all, because we would do copy anyway for each element and hence
        // as much copy.
        teambuilds.push_back(tb);
    }

    builds_changed = false;
}

void HeroBuildsWindow::SaveToFile() const
{
    constexpr size_t buffer_size = 16;
    if (builds_changed || GWToolbox::SettingsFolderChanged()) {
        // clear builds from ini
        inifile->Reset();

        // then save
        for (size_t i = 0; i < teambuilds.size(); i++) {
            const TeamHeroBuild& tbuild = teambuilds[i];
            char section[buffer_size];
            snprintf(section, buffer_size, "builds%03d", i);
            inifile->SetValue(section, "buildname", tbuild.name);
            inifile->SetLongValue(section, "mode", tbuild.mode);
            if (tbuild.group[0])
                inifile->SetValue(section, "group", tbuild.group);
            for (size_t j = 0; j < tbuild.builds.size(); ++j) {
                const HeroBuild& build = tbuild.builds[j];

                char namekey[buffer_size];
                char templatekey[buffer_size];
                char heroidkey[buffer_size];
                char showpanelkey[buffer_size];
                char behaviorkey[buffer_size];
                char dskillskey[buffer_size];
                snprintf(namekey, buffer_size, "name%d", j);
                snprintf(templatekey, buffer_size, "template%d", j);
                snprintf(heroidkey, buffer_size, "heroid%d", j);
                snprintf(showpanelkey, buffer_size, "panel%d", j);
                snprintf(behaviorkey, buffer_size, "behavior%d", j);
                snprintf(dskillskey, buffer_size, "dskills%d", j);
                inifile->SetValue(section, namekey, build.name);
                inifile->SetValue(section, templatekey, build.code);
                inifile->SetLongValue(section, heroidkey, static_cast<long>(build.hero_id));
                inifile->SetLongValue(section, showpanelkey, build.show_panel);
                inifile->SetLongValue(section, behaviorkey, build.behavior);
                inifile->SetLongValue(section, dskillskey, build.disabled_skills);
            }
        }

        ASSERT(inifile->SaveFile(Resources::GetSettingFile(INI_FILENAME).c_str()) == SI_OK);
    }
}

bool HeroBuildsWindow::CodeOnHero::Process()
{
    if (!started) {
        started = TIMER_INIT();
    }
    if (TIMER_DIFF(started) > 1000) {
        return true; // Consume, timeout.
    }
    switch (stage) {
        case Add: // Need to add hero to party
            GW::PartyMgr::AddHero(heroid);
            stage = Load;
        case Load: // Waiting for hero to be added to party
        {
            const GW::HeroPartyMember* hero = GetPartyHeroByID(heroid, &party_hero_index);
            if (!hero) {
                break;
            }
            const GW::HeroFlag* flag = GetHeroFlagInfo(heroid);
            if (!flag) {
                break;
            }
            if (code[0]) // Build optional
            {
                GW::SkillbarMgr::LoadSkillTemplate(hero->agent_id, code);
            }
            for (uint32_t k = 0; k < 8; k++) {
                GW::PartyMgr::SetHeroSkillDisabled(hero->agent_id, k, ((disabled_skills >> k) & 1) != false);
            }
            if (show_panel) {
                SendUIMessage(GW::UI::UIMessage::kShowHeroPanel, (void*)heroid);
            }
            else {
                SendUIMessage(GW::UI::UIMessage::kHideHeroPanel, (void*)heroid);
            }
            if (flag->hero_behavior != behavior) {
                GW::PartyMgr::SetHeroBehavior(hero->agent_id, behavior);
            }
        }
            stage = Finished;
        case Finished: // Success, hero added and build loaded.
            return true;
    }
    return false;
}

HeroBuildsWindow::TeamHeroBuild* HeroBuildsWindow::GetTeambuildByName(const std::string& build_name_search)
{
    const std::string compare = TextUtils::ToLower(TextUtils::RemovePunctuation(build_name_search));
    for (auto& tb : teambuilds) {
        std::string name = TextUtils::ToLower(TextUtils::RemovePunctuation(tb.name));
        if (name.length() < compare.length()) {
            continue; // String entered by user is longer
        }
        if (name.rfind(compare) == 0) {
            return &tb;
        }
    }
    return nullptr;
}
