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

#include <GWCA/Constants/UIMessages.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>
#include <Windows/HeroBuildsWindow.h>

#include <GWToolbox.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

constexpr const wchar_t* INI_FILENAME = L"herobuilds.ini";

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    GW::HookEntry OnRecvWhisper_Entry;
    GW::HookEntry OnOpenTemplate_Entry;

    // Each byte of the Daybreak bit stream is encoded as a single wchar_t using two allowed ranges:
    //   bytes 0x00–0x7F → U+0100–U+017F (Latin Extended-A)
    //   bytes 0x80–0xFF → U+0391–U+0410 (Greek / early Cyrillic)
    // GW whispers only pass through 0x100–0x17F and 0x391–0x45F, so 0x180–0x1FF are blocked.
    // First byte of any Daybreak party loadout stream (header=15, type=1, LSB-first packed):
    // bits 0-3 = 0xF, bits 4-7 = low nibble of type(1) = 0x1 → byte = 0x1F → wchar = U+011F
    constexpr wchar_t PARTY_LOADOUT_MAGIC_WCHAR = 0x011Fu;

    inline wchar_t ByteToWChar(const uint8_t b) {
        return b < 0x80u
            ? static_cast<wchar_t>(0x100u + b)
            : static_cast<wchar_t>(0x391u + (b - 0x80u));
    }

    inline int WCharToByte(const wchar_t wc) {
        if (wc >= 0x100 && wc <= 0x17F) return wc - 0x100;
        if (wc >= 0x391 && wc <= 0x410) return 0x80 + (wc - 0x391);
        return -1;
    }

    struct BitWriter {
        std::vector<uint8_t> buf;
        int pos = 0;
        void write(const uint32_t val, const int n) {
            for (int i = 0; i < n; i++) {
                if (pos / 8 >= static_cast<int>(buf.size())) buf.push_back(0);
                if ((val >> i) & 1u) buf[pos / 8] |= static_cast<uint8_t>(1u << (pos % 8));
                pos++;
            }
        }
    };

    struct BitReader {
        const uint8_t* data;
        int byte_len;
        int pos = 0;
        bool ok = true;
        BitReader(const uint8_t* d, const int len) : data(d), byte_len(len) {}
        uint32_t read(const int n) {
            uint32_t v = 0;
            for (int i = 0; i < n; i++) {
                if (pos / 8 >= byte_len) { ok = false; return 0; }
                if ((data[pos / 8] >> (pos % 8)) & 1u) v |= 1u << i;
                pos++;
            }
            return v;
        }
    };

    void WriteEmbedded(const GW::SkillbarMgr::SkillTemplate& t, BitWriter& bw) {
        const auto max_prof = std::max(static_cast<uint32_t>(t.primary), static_cast<uint32_t>(t.secondary));
        const uint32_t prof_code = max_prof > 15 ? 1u : 0u;
        const int p = static_cast<int>(prof_code) * 2 + 4;
        bw.write(prof_code, 2);
        bw.write(static_cast<uint32_t>(t.primary), p);
        bw.write(static_cast<uint32_t>(t.secondary), p);
        bw.write(t.attributes_count, 4);
        uint32_t attr_code = 0;
        for (uint32_t i = 0; i < t.attributes_count && i < 12; i++) {
            if (static_cast<uint32_t>(t.attribute_ids[i]) > 15) { attr_code = 2; break; }
        }
        const int a = static_cast<int>(attr_code) + 4;
        bw.write(attr_code, 4);
        for (uint32_t i = 0; i < t.attributes_count && i < 12; i++) {
            bw.write(static_cast<uint32_t>(t.attribute_ids[i]), a);
            bw.write(t.attribute_values[i], 4);
        }
        uint32_t skill_code = 0;
        for (int i = 0; i < 8; i++) {
            if (static_cast<uint32_t>(t.skills[i]) > 255) { skill_code = 1; break; }
        }
        const int s = static_cast<int>(skill_code) + 8;
        bw.write(skill_code, 4);
        for (int i = 0; i < 8; i++) {
            bw.write(static_cast<uint32_t>(t.skills[i]), s);
        }
    }

    bool ReadEmbedded(BitReader& br, GW::SkillbarMgr::SkillTemplate& t) {
        const uint32_t prof_code = br.read(2);
        const int p = static_cast<int>(prof_code) * 2 + 4;
        t.primary = static_cast<GW::Constants::Profession>(br.read(p));
        t.secondary = static_cast<GW::Constants::Profession>(br.read(p));
        t.attributes_count = br.read(4);
        if (t.attributes_count > 12) return false;
        const uint32_t attr_code = br.read(4);
        const int a = static_cast<int>(attr_code) + 4;
        for (uint32_t i = 0; i < t.attributes_count; i++) {
            t.attribute_ids[i] = static_cast<GW::Constants::Attribute>(br.read(a));
            t.attribute_values[i] = br.read(4);
        }
        const uint32_t skill_code = br.read(4);
        const int s = static_cast<int>(skill_code) + 8;
        for (int i = 0; i < 8; i++) {
            t.skills[i] = static_cast<GW::Constants::SkillID>(br.read(s));
        }
        return br.ok;
    }


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

bool HeroBuildsWindow::EncodePartyLoadout(const TeamHeroBuild& tbuild, std::wstring& out)
{
    int party_size = 0;
    for (const auto& b : tbuild.builds) {
        if (b.code[0] || b.hero_id != HeroID::NoHero) party_size++;
        else break;
    }
    if (!party_size) return false;

    BitWriter bw;
    bw.write(15u, 4); // extended template header
    bw.write(1u, 8);  // type = party loadout
    bw.write(1u, 4);  // version = 1
    bw.write(static_cast<uint32_t>(party_size), 4);

    for (int i = 0; i < party_size; i++) {
        const auto& build = tbuild.builds[i];
        const uint32_t member_type = i == 0 ? 0u : 2u; // 0 = player, 2 = hero
        bw.write(member_type, 2);
        if (i > 0) bw.write(static_cast<uint32_t>(build.hero_id), 6);
        bw.write(build.behavior, 2);
        GW::SkillbarMgr::SkillTemplate t{};
        if (build.code[0]) GW::SkillbarMgr::DecodeSkillTemplate(t, build.code);
        WriteEmbedded(t, bw);
    }

    out.clear();
    out.reserve(bw.buf.size());
    for (const uint8_t byte : bw.buf)
        out += ByteToWChar(byte);
    return !out.empty();
}

bool HeroBuildsWindow::DecodePartyLoadout(const std::wstring& in, TeamHeroBuild& out)
{
    if (in.empty() || in[0] != PARTY_LOADOUT_MAGIC_WCHAR) return false;

    std::vector<uint8_t> bytes;
    bytes.reserve(in.size());
    for (const wchar_t wc : in) {
        const int b = WCharToByte(wc);
        if (b < 0) return false;
        bytes.push_back(static_cast<uint8_t>(b));
    }

    BitReader br(bytes.data(), static_cast<int>(bytes.size()));
    if (br.read(4) != 15u || br.read(8) != 1u || br.read(4) != 1u) return false;
    const int party_size = static_cast<int>(br.read(4));
    if (party_size < 1 || party_size > 8) return false;

    for (int i = 0; i < party_size; i++) {
        auto& build = out.builds[i];
        const uint32_t member_type = br.read(2);
        if (member_type == 2u) build.hero_id = static_cast<HeroID>(br.read(6));
        build.behavior = br.read(2);
        GW::SkillbarMgr::SkillTemplate t{};
        if (!ReadEmbedded(br, t)) return false;
        if (!GW::SkillbarMgr::EncodeSkillTemplate(t, build.code, BUFFER_SIZE)) return false;
    }
    return br.ok;
}

void HeroBuildsWindow::OnRecvWhisper(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*)
{
    const auto packet = static_cast<GW::UI::UIPacket::kRecvWhisper*>(wparam);
    if (!packet || !packet->message || !packet->from) return;
    const std::wstring msg(packet->message);
    if (msg.empty() || msg[0] != PARTY_LOADOUT_MAGIC_WCHAR) return;
    auto tbuild = std::make_unique<TeamHeroBuild>("");
    if (!DecodePartyLoadout(msg, *tbuild)) return;
    status->blocked = true;

    auto& self = Instance();
    const uint32_t build_id = ++self.received_build_counter;
    const std::wstring sender(packet->from);

    // Use the player's skill template as the link code so GW renders it as a clickable link
    wchar_t player_code_w[BUFFER_SIZE]{};
    if (tbuild->builds[0].code[0])
        MultiByteToWideChar(CP_UTF8, 0, tbuild->builds[0].code, -1, player_code_w, BUFFER_SIZE);

    wchar_t id_hex[16];
    swprintf(id_hex, 16, L"%08X", build_id);
    std::wstring link = L"[Teambuild:0x";
    link += id_hex;
    if (player_code_w[0]) {
        link += L";";
        link += player_code_w;
    }
    link += L"]";

    self.received_teambuilds[build_id] = ReceivedBuild{sender, std::move(tbuild)};

    // Inject a new whisper with the template link so GW renders it in chat
    GW::GameThread::Enqueue([sender, link]() mutable {
        GW::UI::UIPacket::kRecvWhisper new_packet{};
        new_packet.from = sender.data();
        new_packet.message = link.data();
        GW::UI::SendUIMessage(GW::UI::UIMessage::kRecvWhisper, &new_packet);
    });
}

void HeroBuildsWindow::OnOpenTemplate(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*)
{
    const auto t = static_cast<GW::UI::ChatTemplate*>(wparam);
    if (!t || !t->name) return;
    const std::wstring name(t->name);
    constexpr std::wstring_view prefix = L"Teambuild:0x";
    if (name.size() <= prefix.size() || name.substr(0, prefix.size()) != prefix) return;

    const std::wstring hex_str = name.substr(prefix.size());
    uint32_t build_id = 0;
    try {
        build_id = static_cast<uint32_t>(std::stoul(hex_str, nullptr, 16));
    }
    catch (...) {
        return;
    }

    auto& self = Instance();
    auto it = self.received_teambuilds.find(build_id);
    if (it == self.received_teambuilds.end()) return;

    status->blocked = true;
    self.pending_import.sender = std::move(it->second.sender);
    self.pending_import.build = std::move(it->second.build);
    self.received_teambuilds.erase(it);
    self.pending_import.show_dialog = true;
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
    GW::UI::RegisterUIMessageCallback(&OnRecvWhisper_Entry, GW::UI::UIMessage::kRecvWhisper, OnRecvWhisper);
    GW::UI::RegisterUIMessageCallback(&OnOpenTemplate_Entry, GW::UI::UIMessage::kOpenTemplate, OnOpenTemplate);
}

void HeroBuildsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    teambuilds.clear();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnRecvWhisper_Entry);
    GW::UI::RemoveUIMessageCallback(&OnOpenTemplate_Entry);
}

void HeroBuildsWindow::Draw(IDirect3DDevice9*)
{
    // Force the main window open when an import is pending (popup needs a parent window)
    if (pending_import.show_dialog && pending_import.build) {
        visible = true;
    }
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

            // Import dialog shown when a party loadout whisper is received
            if (pending_import.show_dialog && pending_import.build) {
                ImGui::OpenPopup("Import Teambuild?");
                pending_import.show_dialog = false;
            }
            if (ImGui::BeginPopupModal("Import Teambuild?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (pending_import.build) {
                    const auto from_str = TextUtils::WStringToString(pending_import.sender);
                    ImGui::Text("Received a teambuild from '%s'.", from_str.c_str());
                    ImGui::Text("Import it into your Hero Builds list?");
                    ImGui::Spacing();
                    if (ImGui::Button("Import", ImVec2(120, 0))) {
                        pending_import.build->edit_open = true;
                        teambuilds.push_back(*pending_import.build);
                        builds_changed = true;
                        pending_import.build.reset();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Ignore", ImVec2(120, 0))) {
                        pending_import.build.reset();
                        ImGui::CloseCurrentPopup();
                    }
                }
                else {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
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

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Send as Whisper:");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("Player name##whisper_to", whisper_target, BUFFER_SIZE);
            ImGui::SameLine();
            if (ImGui::Button("Send##whisper_send")) {
                std::wstring encoded;
                if (whisper_target[0] && EncodePartyLoadout(tbuild, encoded)) {
                    pending_whisper_to = TextUtils::StringToWString(std::string(whisper_target));
                    pending_whisper_msg = std::move(encoded);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Send teambuild to another Toolbox user via whisper.\n"
                                  "Recipient must have GWToolbox installed to import it.");
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
    if (!pending_whisper_to.empty() && !pending_whisper_msg.empty() && TIMER_DIFF(send_timer) > 600) {
        GW::Chat::SendChat(pending_whisper_to.c_str(), pending_whisper_msg.c_str());
        pending_whisper_to.clear();
        pending_whisper_msg.clear();
        send_timer = TIMER_INIT();
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
