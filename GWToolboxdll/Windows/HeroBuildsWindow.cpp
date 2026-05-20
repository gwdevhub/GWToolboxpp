#include "stdafx.h"

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Hero.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Constants/Skills.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>
#include <Windows/HeroBuildsWindow.h>

#include <GWToolbox.h>
#include <Utils/TeamBuildEncoder.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Windows/BuildsWindow.h>

constexpr const wchar_t* INI_FILENAME = L"herobuilds.ini";

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    GW::HookEntry OnRecvWhisper_Entry;
    GW::HookEntry OnOpenTemplate_Entry;

    // Pool of received/detached teambuilds shown as standalone windows (not in the main list).
    // Entries are removed when their window is closed and no other owner holds the shared_ptr.
    std::vector<std::shared_ptr<TeamBuild>> detached_pool{};

    ToolboxIni* inifile = nullptr;

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

    void OnCreateChatLink(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*)
    {
        const auto packet = (GW::UI::UIPacket::kAddCustomChatLink*)wparam;

        if (!(packet && packet->url && *packet->url && TeamBuildEncoder::IsEncodedTeamBuild(packet->url)))
            return;

        TeamBuild tbuild("", TextUtils::WStringToString(packet->url).c_str());
        if (!TeamBuildEncoder::EncodedToTeamBuild(packet->url, tbuild)) return;
        tbuild.has_hero_slots = std::ranges::any_of(tbuild.builds, [](const Build& b) {
            return b.hero_id != GW::Constants::HeroID::NoHero;
        });

        status->blocked = true;

        wcscpy(packet->link_prefix, L"Teambuild: ");

        const auto new_name = std::format(L"{}'s Teambuild", packet->sender);

        wcscpy(packet->label, new_name.c_str());
    }

    void OnChatLinkClicked(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*)
    {
        const auto packet = (GW::UI::UIPacket::kChatLinkClicked*)wparam;
        if (!(packet && packet->url && *packet->url)) return;

        const auto ui_id = TextUtils::WStringToString(packet->url);

        const auto it = std::ranges::find_if(detached_pool, [&](const auto& ptr) {
            return ptr->ui_id == ui_id;
        });
        if (it != detached_pool.end()) {
            (*it)->edit_open = (*it)->focus_next_frame = true;
            status->blocked = true;
            return;
        }

        auto tbuild = std::make_shared<TeamBuild>(TextUtils::WStringToString(packet->label), ui_id);
        if (!TeamBuildEncoder::EncodedToTeamBuild(packet->url, *tbuild)) return;
        tbuild->has_hero_slots = std::ranges::any_of(tbuild->builds, [](const Build& b) {
            return b.hero_id != GW::Constants::HeroID::NoHero;
        });
        status->blocked = true;
        tbuild->edit_open = tbuild->focus_next_frame = true;
        detached_pool.push_back(std::move(tbuild));
    }

} // namespace

std::string HeroBuildsWindow::EncodeTeambuildToDaybreak(const TeamBuild& tbuild)
{
    return TeamBuildEncoder::TeamBuildToDaybreak(tbuild);
}

bool HeroBuildsWindow::DecodeTeambuildFromDaybreak(const std::string& code, TeamBuild& out)
{
    return TeamBuildEncoder::DaybreakToTeamBuild(code, out);
}
const GW::HeroFlag* HeroBuildsWindow::GetHeroFlagInfo(const uint32_t hero_id)
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

GW::HeroPartyMember* HeroBuildsWindow::GetPartyHeroByID(const GW::Constants::HeroID hero_id, size_t* out_hero_index)
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
        if (party_heros[i].owner_player_id == my_player_id && party_heros[i].hero_id == hero_id) {
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
    skill_toggle_sprite = GwDatTextureModule::LoadTextureFromFileId(0x268f6);
    TeamBuild::SetSkillToggleSprite(skill_toggle_sprite);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"heroteam", &CmdHeroTeamBuild);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"herobuild", &CmdHeroTeamBuild);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"loadteambuild", &CmdLoadTeamBuild);
    GW::UI::RegisterUIMessageCallback(&OnOpenTemplate_Entry, GW::UI::UIMessage::kChatLinkClicked, OnChatLinkClicked);
    GW::UI::RegisterUIMessageCallback(&OnOpenTemplate_Entry, GW::UI::UIMessage::kAddCustomChatLink, OnCreateChatLink);
}

void HeroBuildsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    teambuilds.clear();
    detached_pool.clear();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnOpenTemplate_Entry);
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
            std::vector<TeamBuild*> filtered;
            for (TeamBuild& tbuild : teambuilds) {
                if (filter_by_profession && player_profession != GW::Constants::Profession::None) {
                    if (!tbuild.builds.empty()) {
                        const auto& player_code = tbuild.builds[0].code;
                        if (!player_code.empty()) {
                            GW::SkillbarMgr::SkillTemplate t{};
                            if (GW::SkillbarMgr::DecodeSkillTemplate(t, player_code.c_str()) && t.primary != player_profession) {
                                continue;
                            }
                        }
                    }
                }
                filtered.push_back(&tbuild);
            }

            // Build ordered group list
            std::vector<std::string> group_order;
            std::unordered_map<std::string, std::vector<TeamBuild*>> by_group;
            for (TeamBuild* tbuild : filtered) {
                const std::string g(tbuild->group);
                if (!by_group.contains(g)) {
                    by_group[g] = {};
                    group_order.push_back(g);
                }
                by_group[g].push_back(tbuild);
            }

            auto draw_teambuild_row = [&](TeamBuild& tbuild) {
                ImGui::PushID(tbuild.ui_id.c_str());
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
                if (ImGui::Button(tbuild.name.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - item_spacing - btn_width, 0))) {
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
                            if (build.code.empty() && build.name.empty()) continue;
                            ImGui::Spacing();
                            std::string name;
                            HeroBuildName(build, &name);
                            ImGui::TextUnformatted(name.empty() ? build.name.c_str() : name.c_str());
                            GuiUtils::DrawSkillbar(build.code.c_str(), false);
                            ImGui::Spacing();
                        }
                    });
                }
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5f, 0.5f);
                ImGui::SameLine(0, item_spacing);
                const bool ctrl_held = ImGui::GetIO().KeyCtrl;
                const bool send_disabled = ctrl_held && tbuild.ChatCodeTooLong();
                if (send_disabled) ImGui::BeginDisabled();
                if (ImGui::Button(ctrl_held ? "Send" : "Load", ImVec2(btn_width, 0))) {
                    if (ctrl_held) {
                        tbuild.Send();
                    }
                    else {
                        tbuild.Load();
                    }
                }
                if (send_disabled) ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    if (send_disabled) {
                        ImGui::SetTooltip("Teambuild code is too long to send in chat.\n[TB;<code>] would exceed 120 characters.");
                    }
                    else {
                        ImGui::SetTooltip(ctrl_held ? "Click to send to team chat" : "Click to load builds to heroes and player. Ctrl + Click to send to chat.");
                    }
                }
                ImGui::PopID();
            };

            for (const auto& group_name : group_order) {
                auto& group_builds = by_group[group_name];
                if (group_name.empty()) {
                    for (TeamBuild* tbuild : group_builds) {
                        draw_teambuild_row(*tbuild);
                    }
                }
                else {
                    ImGui::PushID(group_name.c_str());
                    const bool open = ImGui::CollapsingHeader(group_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                    if (open) {
                        ImGui::Indent();
                        for (TeamBuild* tbuild : group_builds) {
                            draw_teambuild_row(*tbuild);
                        }
                        ImGui::Unindent();
                    }
                    ImGui::PopID();
                }
            }
            if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                TeamBuild tb(std::format("My New Hero Teambuild, {}", TextUtils::GetFormattedDateTime()));
                tb.has_hero_slots = true;
                tb.edit_open = true;
                GW::SkillbarMgr::SkillTemplate skill_template;
                for (auto i = 0u; i < 8; i++) {
                    Build build;
                    if (GW::SkillbarMgr::GetSkillTemplate(GW::PartyMgr::GetPartyMemberAgentId(i), skill_template)) {
                        char buf[64]{};
                        if (GW::SkillbarMgr::EncodeSkillTemplate(skill_template, buf, _countof(buf))) {
                            const auto party_info = GW::PartyMgr::GetPartyInfo();
                            const auto cur_hero_id = i > 0 && party_info && party_info->heroes.size() >= i ? party_info->heroes[i - 1].hero_id : HeroID::NoHero;
                            const GW::HeroFlag* flag = cur_hero_id != HeroID::NoHero ? GetHeroFlagInfo(cur_hero_id) : nullptr;
                            build = Build("", buf, cur_hero_id, 0, static_cast<uint32_t>(flag ? flag->hero_behavior : GW::HeroBehavior::Guard));
                        }
                    }
                    tb.builds.push_back(std::move(build));
                }

                builds_changed = true;
                teambuilds.push_back(std::move(tb));
            }
            /* Code for copying a teambuild */
            std::vector<const char*> names(teambuilds.size(), "\0");
            std::ranges::transform(teambuilds, names.begin(), [](const TeamBuild& tb) {
                return tb.name.c_str();
            });
            if (const auto num_elements = names.size()) {
                static int selected_teambuild = 0;
                selected_teambuild = std::clamp(selected_teambuild, 0, static_cast<int>(names.size()) - 1);
                ImGui::PushItemWidth(-60.0f - ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::Combo("###teamBuildCombo", &selected_teambuild, names.data(), num_elements);
                ImGui::PopItemWidth();
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Copy##1", ImVec2(60.0f, 0))) {
                    TeamBuild new_tb = teambuilds[selected_teambuild];
                    new_tb.name += " (Copy)";
                    builds_changed = true;
                    teambuilds.push_back(std::move(new_tb));
                }
            }
        }
        ImGui::End();
    }

    // Draw edit windows for each open teambuild using unified DrawEditWindow
    for (size_t i = 0; i < teambuilds.size(); i++) {
        if (!teambuilds[i].edit_open) continue;
        if (!teambuilds[i].DrawEditWindow(i, teambuilds, builds_changed)) {
            break; // teambuild was deleted; teambuilds vector modified
        }
    }

    // Draw detached teambuild windows (received builds not in the main list).
    // Build names are lazily populated with the elite skill from each slot.
    for (auto& tbuild_ptr : detached_pool) {
        TeamBuild& tbuild = *tbuild_ptr;
        if (!tbuild.edit_open) continue;

        const auto winname = std::format("{}###detached_{}", tbuild.name, tbuild.ui_id);
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
        if (tbuild.focus_next_frame) {
            ImGui::SetNextWindowCollapsed(false);
            ImGui::SetNextWindowFocus();
            tbuild.focus_next_frame = false;
        }
        if (ImGui::Begin(winname.c_str(), &tbuild.edit_open)) {
            for (size_t j = 0; j < tbuild.builds.size(); j++) {
                const auto& build = tbuild.builds[j];
                if (build.code.empty() && build.hero_id == HeroID::NoHero) continue;
                std::string disp_name;
                if (tbuild.has_hero_slots) {
                    HeroBuildName(build, &disp_name);
                } else {
                    const auto& bname = !build.name.empty() ? build.name : build.GetFallbackBuildName();
                    disp_name = std::format("#{} {}", j + 1, bname);
                }
                if (!disp_name.empty()) ImGui::TextUnformatted(disp_name.c_str());
                if (!build.code.empty()) GuiUtils::DrawSkillbar(build.code.c_str(), false);
                ImGui::Spacing();
            }
            ImGui::Separator();
            if (tbuild.has_hero_slots) {
                if (ImGui::Button("Load All")) {
                    tbuild.Load();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load all builds onto your heroes");
                ImGui::SameLine();
            }
            if (ImGui::Button("Add to My Builds")) {
                TeamBuild copy = tbuild;
                copy.edit_open = false;
                if (copy.has_hero_slots) {
                    teambuilds.push_back(std::move(copy));
                    builds_changed = true;
                } else {
                    BuildsWindow::Instance().AddTeambuild(std::move(copy));
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(tbuild.has_hero_slots
                    ? "Save this teambuild to your Hero Builds list"
                    : "Save this teambuild to your Builds list");
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) tbuild.edit_open = false;
        }
        ImGui::End();
    }
}

HeroBuildsWindow::HeroBuildsWindow()
{
    inifile = new ToolboxIni(false, false, false);
    show_menubutton = can_show_in_main_window;
}
HeroBuildsWindow::~HeroBuildsWindow() {
    delete inifile;
}

void HeroBuildsWindow::HeroBuildName(const Build& build, std::string* out)
{
    const auto& code = build.code;
    const auto id = build.hero_id;
    const auto hero_name = build.hero_id == GW::Constants::HeroID::NoHero ? "Player" : Resources::GetHeroName(id)->string();
    const auto name = !build.name.empty() ? build.name : build.GetFallbackBuildName();
    if (name.empty() && code.empty() && id == HeroID::NoHero) {
        return;
    }
    *out = std::format("{} ({})", name, hero_name);
}

const char* HeroBuildsWindow::BuildName(const size_t idx) const
{
    if (idx < teambuilds.size()) {
        return teambuilds[idx].name.c_str();
    }
    return nullptr;
}

void HeroBuildsWindow::Load(const size_t idx)
{
    if (idx < teambuilds.size())
        teambuilds[idx].Load();
}

void HeroBuildsWindow::Update(float)
{
    const GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();
    if (instance_type != last_instance_type) {
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            for (auto& hb : teambuilds) hb.edit_open = false;
            visible = false;
        }
        last_instance_type = instance_type;
    }

    // GC detached pool: remove closed entries with no external owners
    std::erase_if(detached_pool, [](const auto& ptr) {
        return !ptr->edit_open && ptr.use_count() == 1;
    });

    // if we open the window, load from file. If we close the window, save to file.
    static bool old_visible = false;
    bool cur_visible = visible;
    for (const TeamBuild& tbuild : teambuilds) cur_visible |= tbuild.edit_open;

    if (cur_visible != old_visible) {
        old_visible = cur_visible;
        if (cur_visible) LoadFromFile();
        else             SaveToFile();
    }
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
    const TeamBuild* found = Instance().GetTeambuildByName(argBuildName_s);
    if (!found) {
        Log::ErrorW(L"No hero build found for %s", argBuildname.c_str());
        return;
    }
    const TeamBuild& tbuild = *found;
    tbuild.Load();
}

void CHAT_CMD_FUNC(HeroBuildsWindow::CmdLoadTeamBuild)
{
    if (argc < 2) {
        Log::ErrorW(L"Syntax: /%s [hero_build_name|build_code]", argv[0]);
        return;
    }
    std::wstring arg = argv[1];
    for (auto i = 2; i < argc; i++) {
        arg.append(L" ");
        arg.append(argv[i]);
    }
    const std::string arg_s = TextUtils::WStringToString(arg);
    if (TeamBuildEncoder::IsDaybreakTeamBuild(arg_s)) {
        TeamBuild tbuild;
        if (!TeamBuildEncoder::DaybreakToTeamBuild(arg_s, tbuild)) {
            Log::ErrorW(L"Failed to decode team build code");
            return;
        }
        tbuild.has_hero_slots = true;
        tbuild.Load();
        return;
    }
    const TeamBuild* found = Instance().GetTeambuildByName(arg_s);
    if (!found) {
        Log::ErrorW(L"No hero build found for '%s'", arg.c_str());
        return;
    }
    found->Load();
}

void HeroBuildsWindow::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Hero Team Build Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    ImGui::Bullet();
    ImGui::Text("'/loadteambuild <name|code>' loads a hero team build by partial name or Daybreak build code.");
    ImGui::Bullet();
    ImGui::Text("'/heroteam <name>' or '/herobuild <name>' load a hero team build by partial name.");
    ImGui::TreePop();
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

        TeamBuild tb(inifile->GetValue(section, "buildname", ""));
        tb.has_hero_slots = true;
        tb.mode = inifile->GetLongValue(section, "mode", false);
        tb.group = inifile->GetValue(section, "group", "");

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
            tb.builds.push_back(Build(nameval, templateval, hero_id, show_panel == 1 ? 1 : 0, behavior, disabled_skills));
        }

        teambuilds.push_back(std::move(tb));
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
            const TeamBuild& tbuild = teambuilds[i];
            char section[buffer_size];
            snprintf(section, buffer_size, "builds%03d", i);
            inifile->SetValue(section, "buildname", tbuild.name.c_str());
            inifile->SetLongValue(section, "mode", tbuild.mode);
            if (!tbuild.group.empty())
                inifile->SetValue(section, "group", tbuild.group.c_str());
            for (size_t j = 0; j < tbuild.builds.size(); ++j) {
                const Build& build = tbuild.builds[j];

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
                inifile->SetValue(section, namekey, build.name.c_str());
                inifile->SetValue(section, templatekey, build.code.c_str());
                inifile->SetLongValue(section, heroidkey, static_cast<long>(build.hero_id));
                inifile->SetLongValue(section, showpanelkey, build.show_panel ? 1 : 0);
                inifile->SetLongValue(section, behaviorkey, build.behavior);
                inifile->SetLongValue(section, dskillskey, build.disabled_skills);
            }
        }

        ASSERT(inifile->SaveFile(Resources::GetSettingFile(INI_FILENAME).c_str()) == SI_OK);
    }
}

TeamBuild* HeroBuildsWindow::GetTeambuildByName(const std::string& build_name_search)
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
