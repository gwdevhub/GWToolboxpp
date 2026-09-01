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

#include <Modules/GwDatModule.h>
#include <Modules/Resources.h>
#include <Windows/HeroBuildsWindow.h>

#include <GWToolbox.h>
#include <Utils/TeamBuildEncoder.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

constexpr const wchar_t* INI_FILENAME = L"herobuilds.ini"; // 旧版只读备选
constexpr const wchar_t* JSON_FILENAME = L"herobuilds.json";

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    GW::HookEntry OnRecvWhisper_Entry;
    GW::HookEntry OnOpenTemplate_Entry;

    // 收到的或分离的团队构建池，以独立窗口显示（不在主列表中）。
    // 当窗口关闭且没有其他所有者持有 shared_ptr 时，条目会被移除。
    std::vector<std::shared_ptr<TeamBuild>> detached_pool{};

    HeroBuildsWindow::Settings settings;

    // ----------------------------------------------------------------
    // 英雄构建分组排序
    // ----------------------------------------------------------------

    struct HerobuildGroup {
        std::string name{};
        size_t sort_order = SIZE_MAX;
    };

    std::unordered_map<std::string, HerobuildGroup> hero_build_groups;

    size_t GetGroupSortOrder(const std::string& name)
    {
        if (name.empty()) return SIZE_MAX;
        const auto it = hero_build_groups.find(name);
        return it != hero_build_groups.end() ? it->second.sort_order : SIZE_MAX;
    }

    HerobuildGroup& UpsertGroup(const std::string& name)
    {
        if (!hero_build_groups.contains(name)) {
            HerobuildGroup& grp = hero_build_groups[name];
            grp.name = name;
            grp.sort_order = hero_build_groups.size() - 1;
            return grp;
        }
        return hero_build_groups[name];
    }

    void SortTeambuilds(std::vector<TeamBuild>& teambuilds)
    {
        std::stable_sort(teambuilds.begin(), teambuilds.end(),
            [](const TeamBuild& a, const TeamBuild& b) {
                return GetGroupSortOrder(a.group) < GetGroupSortOrder(b.group);
            });
    }

    // ----------------------------------------------------------------

    // GW 文件 0x268f6: 2x2 精灵表（勾选/叉号叠加层）
    // 左下角精灵（列0，行1）= 半透明叉号 = “禁用”叠加层
    IDirect3DTexture9** skill_toggle_sprite = nullptr;

    using GW::Constants::HeroID;

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

        wcscpy(packet->link_prefix, L"团队构建: ");

        const auto new_name = std::format(L"{} 的团队构建", packet->sender);

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

        status->blocked = true;
        tbuild->edit_open = tbuild->focus_next_frame = true;
        detached_pool.push_back(std::move(tbuild));
    }

    TeamBuild FromCurrentTeam() {
        TeamBuild tb(std::format("{} 的团队构建, {}", TextUtils::WStringToString(GW::AccountMgr::GetCurrentPlayerName()),TextUtils::GetFormattedDateTime()));
        tb.has_hero_slots = true;
        tb.edit_open = true;
        GW::SkillbarMgr::SkillTemplate skill_template;
        for (auto i = 0u; i < 8; i++) {
            Build build;
            const uint32_t agent_id = GW::PartyMgr::GetPartyMemberAgentId(i);
            if (GW::SkillbarMgr::GetSkillTemplate(agent_id, skill_template)) {
                char buf[64]{};
                if (GW::SkillbarMgr::EncodeSkillTemplate(skill_template, buf, _countof(buf))) {
                    const auto party_info = GW::PartyMgr::GetPartyInfo();
                    const auto cur_hero_id = i > 0 && party_info && party_info->heroes.size() >= i ? party_info->heroes[i - 1].hero_id : HeroID::NoHero;
                    const GW::HeroFlag* flag = cur_hero_id != HeroID::NoHero ? HeroBuildsWindow::GetHeroFlagInfo(cur_hero_id) : nullptr;
                    uint8_t disabled_skills = 0;
                    const GW::Skillbar* skillbar = GW::SkillbarMgr::GetSkillbar(agent_id);
                    if (skillbar) {
                        disabled_skills = static_cast<uint8_t>(skillbar->disabled & 0xFF);
                    }
                    build = Build("", buf, cur_hero_id, 0, static_cast<uint32_t>(flag ? flag->hero_behavior : GW::HeroBehavior::Guard), disabled_skills);
                    build.name = build.GetFallbackBuildName();
                }
            }
            tb.builds.push_back(std::move(build));
        }
        tb.has_hero_slots = std::ranges::any_of(tb.builds, [](const Build& b) {
            return b.hero_id != GW::Constants::HeroID::NoHero;
        });
        return tb;
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
    SettingsRegistry::Register(this, settings);
    skill_toggle_sprite = GwDatModule::LoadTextureFromFileId(0x268f6);
    TeamBuild::SetSkillToggleSprite(skill_toggle_sprite);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"heroteam", &CmdHeroTeamBuild);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"herobuild", &CmdHeroTeamBuild);
    RegisterUIMessageCallback(&OnOpenTemplate_Entry, GW::UI::UIMessage::kChatLinkClicked, OnChatLinkClicked);
    RegisterUIMessageCallback(&OnOpenTemplate_Entry, GW::UI::UIMessage::kAddCustomChatLink, OnCreateChatLink);
}

void HeroBuildsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    teambuilds.clear();
    hero_build_groups.clear();
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
                snprintf(filter_label, sizeof(filter_label), "按 %s 过滤", ToolboxUtils::GetProfessionName(player_profession)->string().c_str());
                ImGui::Checkbox(filter_label, &settings.filter_by_profession);
            }
            const float btn_width = 60.0f * ImGui::FontScale();
            const float& item_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

            size_t signature = reinterpret_cast<size_t>(teambuilds.data());
            const auto mix_signature = [&signature](size_t v) {
                signature ^= v + 0x9e3779b97f4a7c15 + (signature << 6) + (signature >> 2);
            };
            mix_signature(teambuilds.size());
            mix_signature(settings.filter_by_profession);
            mix_signature(static_cast<size_t>(player_profession));
            for (const auto& tbuild : teambuilds) {
                mix_signature(std::hash<std::string>()(tbuild.group));
                mix_signature(std::hash<std::string>()(tbuild.name));
                mix_signature(tbuild.builds.empty() ? 0 : std::hash<std::string>()(tbuild.builds.front().code));
            }

            if (!grouping_cache.valid || builds_changed || grouping_cache.signature != signature) {
                grouping_cache.valid = true;
                grouping_cache.signature = signature;
                builds_changed = false;

                auto& filtered = grouping_cache.filtered;
                filtered.clear();
                for (TeamBuild& tbuild : teambuilds) {
                    if (settings.filter_by_profession && player_profession != GW::Constants::Profession::None) {
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

                auto& by_group = grouping_cache.by_group;
                by_group.clear();
                for (TeamBuild* tbuild : filtered) {
                    by_group[tbuild->group].push_back(tbuild);
                }

                for (const auto& [gname, _] : by_group) {
                    if (!gname.empty() && !hero_build_groups.contains(gname)) {
                        UpsertGroup(gname);
                    }
                }

                auto& group_order = grouping_cache.group_order;
                group_order.clear();
                {
                    std::vector<const HerobuildGroup*> sorted_grps;
                    for (const auto& [name, grp] : hero_build_groups) {
                        if (by_group.contains(name)) {
                            sorted_grps.push_back(&grp);
                        }
                    }
                    std::ranges::sort(sorted_grps, [](const HerobuildGroup* a, const HerobuildGroup* b) {
                        return a->sort_order < b->sort_order;
                    });
                    for (const auto* grp : sorted_grps) {
                        group_order.push_back(grp->name);
                    }
                }
                if (by_group.contains("")) {
                    group_order.push_back("");
                }
            }

            bool vector_invalidated = false;
            const auto& group_order = grouping_cache.group_order;

            auto draw_teambuild_row = [&](TeamBuild& tbuild) {
                ImGui::PushID(tbuild.ui_id.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
                if (ImGui::Button(tbuild.name.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - item_spacing - btn_width, 0))) {
                    if (settings.one_teambuild_at_a_time && !tbuild.edit_open) {
                        for (auto& tb : teambuilds) {
                            tb.edit_open = false;
                        }
                    }
                    tbuild.edit_open = !tbuild.edit_open;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip([&tbuild]() {
                        tbuild.DrawTooltip();
                    });
                }
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
                ImGui::SameLine(0, item_spacing);
                const bool ctrl_held = ImGui::GetIO().KeyCtrl;
                const bool send_disabled = ctrl_held && tbuild.ChatCodeTooLong();
                if (send_disabled) ImGui::BeginDisabled();
                if (ImGui::Button(ctrl_held ? "发送" : "加载", ImVec2(btn_width, 0))) {
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
                        ImGui::SetTooltip("团队构建代码太长，无法在聊天中发送。\n[TB;<code>] 将超过 120 个字符。");
                    }
                    else {
                        ImGui::SetTooltip(ctrl_held ? "点击发送到团队聊天" : "点击将构建加载到英雄和玩家。按住 Ctrl 点击则发送到聊天。");
                    }
                }


                ImGui::PopStyleVar(2);
                ImGui::PopID();
                return vector_invalidated;
            };

            for (size_t gi = 0; gi < group_order.size() && !vector_invalidated; gi++) {
                const auto& group_name = group_order[gi];
                auto& group_builds = grouping_cache.by_group[group_name];
                if (group_name.empty()) {
                    for (TeamBuild* tbuild : group_builds) {
                        draw_teambuild_row(*tbuild);
                    }
                }
                else {
                    ImGui::PushID(group_name.c_str());

                    // 当没有后续命名组时，该组被视为“最后”。
                    const bool is_first = (gi == 0);
                    const bool is_last = (gi + 1 >= group_order.size() || group_order[gi + 1].empty());

                    constexpr auto header_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap;
                    const float btn_y = ImGui::GetCursorPosY();
                    const bool open = ImGui::CollapsingHeader(group_name.c_str(), header_flags);

                    // 在标题行右侧覆盖显示 ↑/↓ 按钮。
                    {
                        const float btn_sz = ImGui::GetFrameHeight();
                        const float spacing = ImGui::GetStyle().ItemSpacing.x;

                        float btn_x = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();


                        if (!is_last) {
                            btn_x -= btn_sz;
                            ImGui::SetCursorPos({btn_x, btn_y});
                            if (ImGui::Button(ICON_FA_ARROW_DOWN "##gd", ImVec2(btn_sz, btn_sz))) {
                                std::swap(hero_build_groups[group_name].sort_order,
                                          hero_build_groups[group_order[gi + 1]].sort_order);
                                SortTeambuilds(teambuilds);
                                builds_changed = true;
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("将分组下移");
                            btn_x -= spacing;
                        }
                        if (!is_first) {
                            btn_x -= btn_sz;
                            ImGui::SetCursorPos({btn_x, btn_y});
                            if (ImGui::Button(ICON_FA_ARROW_UP "##gu", ImVec2(btn_sz, btn_sz))) {
                                std::swap(hero_build_groups[group_name].sort_order,
                                          hero_build_groups[group_order[gi - 1]].sort_order);
                                SortTeambuilds(teambuilds);
                                builds_changed = true;
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("将分组上移");
                        }
                    }

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
            if (ImGui::Button("添加团队构建", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                TeamBuild tb = FromCurrentTeam();
                tb.has_hero_slots = tb.edit_open = true;
                builds_changed = true;
                teambuilds.push_back(std::move(tb));
            }

        }
        ImGui::End();
    }

    for (size_t i = 0; i < teambuilds.size(); i++) {
        if (!teambuilds[i].edit_open) continue;
        if (!teambuilds[i].DrawEditWindow(i, teambuilds, builds_changed)) {
            break; // teambuild 被删除，teambuilds 向量被修改
        }
    }

    // 绘制分离的团队构建窗口（收到的构建，不在主列表中）
    for (auto& tbuild_ptr : detached_pool) {
        tbuild_ptr->DrawDetachedWindow(teambuilds, builds_changed);
    }
}

HeroBuildsWindow::HeroBuildsWindow()
{
    show_menubutton = can_show_in_main_window;
}
HeroBuildsWindow::~HeroBuildsWindow() = default;

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
        if (settings.hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            for (auto& hb : teambuilds) hb.edit_open = false;
            visible = false;
        }
        last_instance_type = instance_type;
    }

    // GC 分离池：移除已关闭且无外部所有者的条目
    std::erase_if(detached_pool, [](const auto& ptr) {
        return !ptr->edit_open && ptr.use_count() == 1;
    });

    // 如果打开窗口，从文件加载；如果关闭窗口，保存到文件。
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
        Log::ErrorW(L"语法: /%s [英雄构建名称|构建代码]", argv[0]);
        return;
    }
    std::wstring arg = argv[1];
    for (auto i = 2; i < argc; i++) {
        arg.append(L" ");
        arg.append(argv[i]);
    }

    if (TeamBuildEncoder::IsEncodedTeamBuild(arg)) {
        TeamBuild tbuild;
        if (!TeamBuildEncoder::EncodedToTeamBuild(arg, tbuild)) {
            Log::ErrorW(L"解码团队构建代码失败");
            return;
        }
        tbuild.has_hero_slots = true;
        tbuild.Load();
        return;
    }

    const std::string arg_s = TextUtils::WStringToString(arg);
    if (TeamBuildEncoder::IsDaybreakTeamBuild(arg_s)) {
        TeamBuild tbuild;
        if (!TeamBuildEncoder::DaybreakToTeamBuild(arg_s, tbuild)) {
            Log::ErrorW(L"解码团队构建代码失败");
            return;
        }
        tbuild.has_hero_slots = true;
        tbuild.Load();
        return;
    }

    const TeamBuild* found = Instance().GetTeambuildByName(arg_s);
    if (!found) {
        Log::ErrorW(L"未找到 '%s' 的英雄构建", arg.c_str());
        return;
    }
    found->Load();
}

void HeroBuildsWindow::DrawHelp()
{
    if (!ImGui::TreeNodeEx("英雄团队构建聊天命令", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    ImGui::Bullet();
    ImGui::Text("'/heroteam <名称|代码>' 或 '/herobuild <名称|代码>' 按部分名称、Daybreak 构建代码或加密字符串加载英雄团队构建。");
    ImGui::TreePop();
}

void HeroBuildsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    LoadFromFile();
}

void HeroBuildsWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("进入探索区域时隐藏英雄构建窗口", &settings.hide_when_entering_explorable);
    ImGui::CheckboxWithHelp("每次只显示一个团队构建窗口", &settings.one_teambuild_at_a_time, "打开新窗口时关闭其他团队构建窗口");
}

void HeroBuildsWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    SaveToFile();
}

void HeroBuildsWindow::LoadFromFile()
{
    teambuilds.clear();
    hero_build_groups.clear();

    const auto json_path = Resources::GetSettingFile(JSON_FILENAME);
    if (std::filesystem::exists(json_path)) {
        std::string buffer;
        HeroBuildsFile file;
        if (!Resources::ReadFile(json_path, buffer) || glz::read<glz::opts{.error_on_unknown_keys = false}>(file, buffer)) {
            Log::Warning("读取 %ls 失败", json_path.filename().c_str());
        }
        else {
            for (const auto& group : file.groups) {
                if (group.name.empty()) continue;
                auto& grp = hero_build_groups[group.name];
                grp.name = group.name;
                grp.sort_order = group.sort_order;
            }
            for (const auto& entry : file.teambuilds) {
                TeamBuild tb(entry.name, entry.ui_id);
                tb.has_hero_slots = true;
                tb.mode = entry.mode;
                tb.group = entry.group;
                // 仅被构建引用的组会获得临时的首次出现 sort_order。
                if (!tb.group.empty()) {
                    UpsertGroup(tb.group);
                }
                for (const auto& build : entry.builds) {
                    tb.builds.push_back(Build(build.name, build.code, build.hero_id, build.show_panel ? 1 : 0, build.behavior, build.disabled_skills));
                }
                // 旧版加载器总是产生 8 个槽位（玩家 + 7 个英雄）。
                while (tb.builds.size() < 8) {
                    tb.builds.emplace_back();
                }
                teambuilds.push_back(std::move(tb));
            }
        }
    }
    else {
        // 旧版 herobuilds.ini 解析器；仅在 herobuilds.json 不存在时使用。
        ToolboxIni inifile(false, false, false);
        inifile.LoadFile(Resources::GetLegacySettingFile(INI_FILENAME).c_str());

        TNamesDepend entries;
        inifile.GetAllSections(entries);
        for (const auto& entry : entries) {
            const char* section = entry.pItem;

            // herobuildgroup 节携带命名组的 sort_order 元数据。
            // 它们可能出现在引用它们的构建之前或之后。
            if (strncmp(section, "herobuildgroup", 14) == 0) {
                const char* name = inifile.GetValue(section, "name", "");
                if (!*name) continue;
                auto& grp = hero_build_groups[name];
                grp.name = name;
                grp.sort_order = static_cast<size_t>(inifile.GetLongValue(section, "sort_order", static_cast<long>(grp.sort_order)));
                continue;
            }

            if (strncmp(section, "builds", 6) != 0) continue;

            const char* saved_ui_id = inifile.GetValue(section, "uiid", "");
            TeamBuild tb(inifile.GetValue(section, "buildname", ""), saved_ui_id);
            tb.has_hero_slots = true;
            tb.mode = inifile.GetLongValue(section, "mode", false);
            tb.group = inifile.GetValue(section, "group", "");

            // 如果尚未见过该组，则创建并赋予临时 sort_order。
            // 显式的 herobuildgroup 节（稍后或之前处理）会覆盖它。
            if (!tb.group.empty()) {
                UpsertGroup(tb.group);
            }

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
                const char* nameval = inifile.GetValue(section, namekey, "");
                const char* templateval = inifile.GetValue(section, templatekey, "");
                // 首先尝试新的 heroid 键；若没有则回退到旧的 heroindex 键以兼容旧版
                HeroID hero_id = HeroID::NoHero;
                const long saved_hero_id = inifile.GetLongValue(section, heroidkey, -1);
                if (saved_hero_id >= 0) {
                    hero_id = static_cast<HeroID>(saved_hero_id);
                }
                else {
                    const long hero_index = inifile.GetLongValue(section, heroindexkey, -1);
                    if (hero_index > 0 && hero_index < static_cast<long>(HeroIndexToID.size())) {
                        hero_id = HeroIndexToID.at(hero_index);
                    }
                }
                const auto show_panel = inifile.GetLongValue(section, showpanelkey, 0);
                const uint32_t behavior = static_cast<uint32_t>(inifile.GetLongValue(section, behaviorkey, 1));
                const uint8_t disabled_skills = static_cast<uint8_t>(inifile.GetLongValue(section, dskillskey, 0));
                tb.builds.push_back(Build(nameval, templateval, hero_id, show_panel == 1 ? 1 : 0, behavior, disabled_skills));
            }

            teambuilds.push_back(std::move(tb));
        }
    }

    // 排序使得属于同一组的构建连续，且组按升序 sort_order 排列。组内相对顺序保持不变。
    SortTeambuilds(teambuilds);

    // 将共享计数器推进到所有已恢复 ID 之后，以便新创建的构建不会与持久化的冲突。
    for (const auto& tb : teambuilds) {
        uint32_t numeric_id = 0;
        if (std::from_chars(tb.ui_id.data(), tb.ui_id.data() + tb.ui_id.size(), numeric_id).ec == std::errc{})
            TeamBuild::s_cur_ui_id = std::max(TeamBuild::s_cur_ui_id, numeric_id);
    }

    builds_changed = false;
    grouping_cache.valid = false;
}

void HeroBuildsWindow::SaveToFile() const
{
    HeroBuildsFile file;

    for (const TeamBuild& tbuild : teambuilds) {
        auto& entry = file.teambuilds.emplace_back();
        entry.name = tbuild.name;
        entry.ui_id = tbuild.ui_id;
        entry.mode = tbuild.mode;
        entry.group = tbuild.group;
        for (const Build& build : tbuild.builds) {
            auto& build_entry = entry.builds.emplace_back();
            build_entry.name = build.name;
            build_entry.code = build.code;
            build_entry.hero_id = build.hero_id;
            build_entry.show_panel = build.show_panel;
            build_entry.behavior = build.behavior;
            build_entry.disabled_skills = build.disabled_skills;
        }
    }

    // 收集仍被至少一个构建引用的组。
    std::unordered_set<std::string> used_groups;
    for (const auto& tb : teambuilds) {
        if (!tb.group.empty()) used_groups.insert(tb.group);
    }

    // 构建已使用组的排序列表，写入归一化的 0 基 sort_order。
    std::vector<std::pair<std::string, size_t>> sorted_groups;
    for (const auto& [name, grp] : hero_build_groups) {
        if (used_groups.contains(name)) {
            sorted_groups.push_back({name, grp.sort_order});
        }
    }
    std::ranges::sort(sorted_groups, [](const auto& a, const auto& b) {
        return a.second < b.second;
    });
    for (size_t gi = 0; gi < sorted_groups.size(); gi++) {
        file.groups.push_back({sorted_groups[gi].first, gi});
    }

    std::string buffer;
    ASSERT(!glz::write<glz::opts{.prettify = true}>(file, buffer));
    ASSERT(Resources::WriteFile(Resources::GetSettingFile(JSON_FILENAME), buffer));
}

TeamBuild* HeroBuildsWindow::GetTeambuildByName(const std::string& build_name_search)
{
    const std::string compare = TextUtils::ToLower(TextUtils::RemovePunctuation(build_name_search));
    for (auto& tb : teambuilds) {
        std::string name = TextUtils::ToLower(TextUtils::RemovePunctuation(tb.name));
        if (name.length() < compare.length()) {
            continue; // 用户输入的字符串更长
        }
        if (name.rfind(compare) == 0) {
            return &tb;
        }
    }
    return nullptr;
}
