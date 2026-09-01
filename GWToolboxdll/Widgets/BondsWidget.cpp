#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Color.h>
#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Modules/Resources.h>
#include <Widgets/BondsWidget.h>
#include <Windows/FriendListWindow.h>
#include <Utils/ToolboxUtils.h>

namespace {
    GW::HookEntry ChatCmd_HookEntry;

    struct AvailableBond {
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        GuiUtils::EncString skill_name;
        bool enabled = true;
        const char* help_text = nullptr;

        AvailableBond(const GW::Constants::SkillID _skill_id, const bool _enabled = true, const char* _help_text = nullptr)
            : skill_id(_skill_id), enabled(_enabled), help_text(_help_text) { };

        void Initialize()
        {
            // 因为 AvailableBond 在工具箱中静态使用，我们需要在渲染循环中显式调用此函数
            // - 否则 GetSkillConstantData 不会在正确的时间被调用。
            if (const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id)) {
                skill_name.reset(skill->name);
            }
        }
    };

    // Refrains are read from the party effects array, which the game only populates for you and your own heroes.
    const char* refrain_help_text =
        "Only shown on yourself and your own heroes.\n"
        "Guild Wars doesn't tell your client about refrains on other players, so a refrain you maintain on them can't be displayed.";

    // Skill ID => enabled by default
    AvailableBond available_bonds[] = {
        {GW::Constants::SkillID::Balthazars_Spirit, true},
        {GW::Constants::SkillID::Essence_Bond, true},
        {GW::Constants::SkillID::Holy_Veil, true},
        {GW::Constants::SkillID::Life_Attunement, true},
        {GW::Constants::SkillID::Life_Barrier, true},
        {GW::Constants::SkillID::Life_Bond, true},
        {GW::Constants::SkillID::Live_Vicariously, true},
        {GW::Constants::SkillID::Mending, true},
        {GW::Constants::SkillID::Protective_Bond, true},
        {GW::Constants::SkillID::Purifying_Veil, true},
        {GW::Constants::SkillID::Retribution, true},
        {GW::Constants::SkillID::Strength_of_Honor, true},
        {GW::Constants::SkillID::Succor, true},
        {GW::Constants::SkillID::Vital_Blessing, true},
        {GW::Constants::SkillID::Watchful_Spirit, true},
        {GW::Constants::SkillID::Watchful_Intervention, false},
        {GW::Constants::SkillID::Heroic_Refrain, true, refrain_help_text},
        {GW::Constants::SkillID::Burning_Refrain, true, refrain_help_text},
        {GW::Constants::SkillID::Mending_Refrain, true, refrain_help_text},
        {GW::Constants::SkillID::Bladeturn_Refrain, true, refrain_help_text},
        {GW::Constants::SkillID::Hasty_Refrain, true, refrain_help_text},
        {GW::Constants::SkillID::Aggressive_Refrain, false, refrain_help_text}
    };

    AvailableBond* GetAvailableBond(const GW::Constants::SkillID skill_id)
    {
        for (auto& b : available_bonds) {
            if (b.skill_id == skill_id) {
                return &b;
            }
        }
        return nullptr;
    }


    BondsWidget::Settings settings;

    std::vector<GW::Constants::SkillID> bond_list{};               // index to skill id
    std::unordered_map<GW::Constants::SkillID, size_t> bond_map{}; // skill id to index
    std::array<uint32_t, 8> fetched_skill_ids{};
    bool bond_skills_dirty = true;

    bool UseBuff(GW::AgentID agent_id, GW::Constants::SkillID skill_id)
    {
        if (!(GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && !GW::Map::GetIsObserving())) {
            return false;
        }
        const auto target = GW::Agents::GetAgentByID(agent_id);
        if (!target) {
            return false;
        }

        const auto islot = GW::SkillbarMgr::GetSkillSlot(skill_id);
        if (islot < 0) {
            return false;
        }
        const auto slot = static_cast<uint32_t>(islot);
        const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!skillbar || !skillbar->IsValid()) {
            return false;
        }
        if (skillbar->skills[slot].recharge != 0) {
            return false;
        }

        // 按值捕获！
        GW::GameThread::Enqueue([slot, agent_id] {
            GW::SkillbarMgr::UseSkill(slot, agent_id);
        });
        return true;
    }

    bool FetchBondSkills()
    {
        const GW::Skillbar* bar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!bar || !bar->IsValid()) {
            bond_skills_dirty = true;
            return false;
        }
        std::array<uint32_t, 8> skill_ids{};
        for (size_t i = 0; i < skill_ids.size(); i++) {
            skill_ids[i] = std::to_underlying(bar->skills[i].skill_id);
        }
        if (!bond_skills_dirty && skill_ids == fetched_skill_ids)
            return true;
        bond_skills_dirty = false;
        fetched_skill_ids = skill_ids;
        bond_list.clear();
        bond_map.clear();
        for (const auto& skill : bar->skills) {
            auto skill_id = static_cast<GW::Constants::SkillID>(skill.skill_id);
            if (const auto found = GetAvailableBond(skill_id); found && found->enabled) {
                bond_map[skill_id] = bond_list.size();
                bond_list.push_back(skill_id);
            }
        }
        return true;
    }

    bool DropBuffs(GW::AgentID targetId, GW::Constants::SkillID skill_id) {
        const auto buffs = GW::Effects::GetPlayerBuffs();
        if (!buffs)
            return false;
        size_t dropped = 0;
        for (const auto& buff : *buffs) {
            if (!(skill_id == (GW::Constants::SkillID)0 || buff.skill_id == skill_id))
                continue;
            if (!(targetId == (GW::AgentID)0 || buff.target_agent_id == targetId))
                continue;
            const auto buff_id = buff.buff_id;
            GW::GameThread::Enqueue([buff_id] {
                GW::Effects::DropBuff(buff_id);
                });
            dropped++;
        }
        return dropped > 0;
    }

    bool ToggleBuff(GW::AgentID agent_id, GW::Constants::SkillID skill_id) {
        return DropBuffs(agent_id, skill_id) || UseBuff(agent_id, skill_id);
    }

    const char* cmd_bonds_syntax = "'/bonds [remove|add] [队员索引|all] [all|技能ID]' 从单个队员或所有队员移除或添加增益";

    void CHAT_CMD_FUNC(CmdBondsAddRemove) {

        const auto syntax_err = [argc, argv] {
            Log::WarningW(L"/%s 语法无效；正确语法：\n%S", argc ? argv[0] : L"未知", cmd_bonds_syntax);
            };

        if (argc < 4) {
            syntax_err();
            return;
        }
        bool add_bond = true;
        uint32_t agent_id = 0;
        uint32_t skill_id = 0;

        if (wcscmp(argv[1], L"add") == 0) {
            add_bond = true;
        }
        else if (wcscmp(argv[1], L"remove") == 0) {
            add_bond = false;
        }
        else {
            syntax_err();
            return;
        }
        // 队员（或全部）
        if (wcscmp(argv[2], L"all") != 0) {
            uint32_t party_member_idx = 0;
            if (!TextUtils::ParseUInt(argv[2], &party_member_idx)) {
                syntax_err();
                return;
            }
            agent_id = GW::PartyMgr::GetPartyMemberAgentId(party_member_idx);
            if (!agent_id) {
                return; // 未找到队员
            }
        }
        // 技能
        if (wcscmp(argv[3], L"all") != 0) {
            if (!TextUtils::ParseUInt(argv[3], &skill_id)) {
                syntax_err();
                return;
            }
        }
        if (add_bond && !skill_id) {
            Log::WarningW(L"/%s：添加增益时需要技能 ID", argv[0]);
            syntax_err();
            return;
        }
        if (skill_id >= GW::SkillbarMgr::GetSkillCount()) {
            Log::WarningW(L"%d: is not a valid skill id", skill_id);
            syntax_err();
            return;
        }
        if (add_bond && !agent_id) {
            Log::WarningW(L"/%s：添加增益时需要队员索引", argv[0]);
            syntax_err();
            return;
        }

        if (add_bond) {
            UseBuff(agent_id, static_cast<GW::Constants::SkillID>(skill_id));
        }
        else {
            DropBuffs(agent_id, static_cast<GW::Constants::SkillID>(skill_id));
        }

    }

}

bool BondsWidget::IsBondLikeSkill(GW::Constants::SkillID skill_id) {
    return GetAvailableBond(skill_id) != nullptr;
}

void BondsWidget::Initialize()
{
    SnapsToPartyWindow::Initialize();
    SettingsRegistry::Register(this, settings);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry,L"bonds", CmdBondsAddRemove);
    for (auto& b : available_bonds) {
        b.Initialize();
    }
}
void BondsWidget::Terminate()
{
    SnapsToPartyWindow::Terminate();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

bool BondsWidget::DrawBondImage(uint32_t agent_id, GW::Constants::SkillID skill_id, ImVec2* top_left_out, ImVec2* bottom_right_out) {
    if (!GetBondPosition(agent_id, skill_id, top_left_out, bottom_right_out))
        return false;
    const auto texture = *Resources::GetSkillImage(skill_id);
    if (texture) {
        ImGui::AddImageCropped(texture, *top_left_out, *bottom_right_out);
        return true;
    }
    return false;
}

bool BondsWidget::GetBondPosition(uint32_t agent_id, GW::Constants::SkillID skill_id, ImVec2* top_left_out, ImVec2* bottom_right_out) {

    const auto health_bar_pos = GetAgentHealthBarPosition(agent_id);
    if (!health_bar_pos)
        return false;

    const auto party_slot_it = party_indeces_by_agent_id.find(agent_id);
    if (party_slot_it == party_indeces_by_agent_id.end())
        return false;
    const auto party_slot = party_slot_it->second;
    if (party_slot >= allies_start_idx && !settings.show_allies)
        return false;

    const auto bond_it = bond_map.find(skill_id);
    if (bond_it == bond_map.end()) {
        return false; // bond with a skill not in skillbar
    }

    const auto img_width = health_bar_pos->bottom_right.y - health_bar_pos->top_left.y;
    const auto y = health_bar_pos->top_left.y;
    const auto x = ImGui::GetCurrentWindow()->Pos.x + (img_width * bond_it->second);

    *top_left_out = { x, y };
    *bottom_right_out = { x + img_width, y + img_width };
    return true;
}

void BondsWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (settings.hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }
    const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    const GW::PlayerArray* players = info ? GW::Agents::GetPlayerArray() : nullptr;
    if (!players) {
        return;
    }
    // 注意：info->heroes、->henchmen 和 ->others 在正常使用期间可能无效。

    if (!FetchBondSkills()) {
        return;
    }
    if (bond_list.empty()) {
        return; // 如果技能栏中没有增益技能，则不显示增益小部件
    }
    if (!(FetchPartyInfo() && RecalculatePartyPositions())) {
        return;
    }
    if (agent_health_bar_positions.empty()) {
        return;
    }

    // ==== 绘制 ====

    const auto& first_health_bar_position = agent_health_bar_positions.begin()->second;

    const auto img_width = (first_health_bar_position.bottom_right.y - first_health_bar_position.top_left.y);

    const float width = bond_list.size() * img_width;

    const auto user_offset_x = abs(static_cast<float>(settings.user_offset));
    float window_x = .0f;
    if (settings.overlay_party_window) {
        window_x = party_health_bars_position.top_left.x + user_offset_x;
        if (settings.user_offset < 0) {
            window_x = party_health_bars_position.bottom_right.x - user_offset_x - width;
        }

    }
    else {
        window_x = party_health_bars_position.top_left.x - user_offset_x - width;
        if (window_x < 0 || settings.user_offset < 0) {
            // 右侧放置
            window_x = party_health_bars_position.bottom_right.x + user_offset_x;
        }
    }
    // 添加一个窗口来捕获鼠标点击。
    ImGui::SetNextWindowPos({ window_x,party_health_bars_position.top_left.y });
    ImGui::SetNextWindowSize({ width, party_health_bars_position.bottom_right.y - party_health_bars_position.top_left.y });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(settings.click_to_cast || settings.click_to_drop)))) {
        const auto draw_list = ImGui::GetWindowDrawList();
        bool handled_click = false;
        ImVec2 bond_top_left;
        ImVec2 bond_bottom_right;

        for (auto& [agent_id, party_slot] : party_indeces_by_agent_id) {
            if (!GetBondPosition(agent_id, bond_list[0], &bond_top_left, &bond_bottom_right))
                continue;
            draw_list->AddRectFilled({ window_x , bond_top_left.y}, { window_x + width, bond_bottom_right.y }, settings.background);
        }

        if (GW::BuffArray* buffs = GW::Effects::GetPlayerBuffs()) {
            for (const auto& buff : *buffs) {
                DrawBondImage(buff.target_agent_id, buff.skill_id, &bond_top_left, &bond_bottom_right);
                if (!handled_click && ImGui::IsMouseHoveringRect(bond_top_left, bond_bottom_right, false) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if(settings.click_to_drop)
                        DropBuffs(buff.target_agent_id, buff.skill_id);
                    handled_click = true;
                }
            }
        }

        // 玩家和英雄的非增益效果
        if (const GW::AgentEffectsArray* agent_effects_array = GW::Effects::GetPartyEffectsArray(); agent_effects_array != nullptr) {
            for (auto& agent_effects_it : *agent_effects_array) {
                auto& agent_effects = agent_effects_it.effects;
                if (!agent_effects.valid()) {
                    continue;
                }
                const auto agent_id = agent_effects_it.agent_id;
                for (const GW::Effect& effect : agent_effects) {
                    const auto skill_id = static_cast<GW::Constants::SkillID>(effect.skill_id);

                    const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                    if (!skill_data || skill_data->duration0 == 0x20000) {
                        continue; // 维持技能/增益
                    }

                    if (!DrawBondImage(agent_id, skill_id, &bond_top_left, &bond_bottom_right))
                        continue;

                    const GW::Attribute* agentAttributes = GW::PartyMgr::GetAgentAttributes(agent_id);
                    ASSERT(agentAttributes);
                    agentAttributes = &agentAttributes[static_cast<size_t>(skill_data->attribute)];
                    const bool overlay = effect.attribute_level < agentAttributes->level;

                    if (overlay) {
                        draw_list->AddRectFilled(bond_top_left, bond_bottom_right, settings.low_attribute_overlay);
                    }
                }
            }
        }

        if (!handled_click) {
            for (auto agent_id : party_agent_ids_by_index) {
                for (auto skill_id : bond_list) {
                    if (!GetBondPosition(agent_id, skill_id, &bond_top_left, &bond_bottom_right))
                        continue;
                    if (!ImGui::IsMouseHoveringRect(bond_top_left, bond_bottom_right, false))
                        continue;
                    draw_list->AddRect(bond_top_left, bond_bottom_right, IM_COL32(255, 255, 255, 255));
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        if(settings.click_to_cast)
                            UseBuff(agent_id, skill_id);
                        handled_click = true;
                    }
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(3);
}

void BondsWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    SnapsToPartyWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    for (auto& b : available_bonds) {
        char buf[128];
        const int written = snprintf(buf, sizeof(buf), "bond_enabled_%d", b.skill_id);
        ASSERT(written != -1);
        if (!doc.Get(Name(), buf, b.enabled) && legacy) {
            b.enabled = legacy->GetBoolValue(Name(), buf, b.enabled);
        }
    }
}

void BondsWidget::SaveSettings(SettingsDoc& doc)
{
    SnapsToPartyWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    for (const auto& b : available_bonds) {
        char buf[128];
        const int written = snprintf(buf, sizeof(buf), "bond_enabled_%d", b.skill_id);
        ASSERT(written != -1);
        doc.Set(Name(), buf, b.enabled);
    }
}

void BondsWidget::DrawSettingsInternal()
{
    ImGui::SameLine();
    ImGui::Checkbox("在前哨站隐藏", &settings.hide_in_outpost);
    if (bond_list.empty()) {
        ImGui::TextColored(ImVec4(0xFF, 0, 0, 0xFF), "装备可维持的增益或副歌以在屏幕上显示增益小部件");
    }
    ImGui::StartSpacedElements(292.f);
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("在生命条上方显示", &settings.overlay_party_window, "取消勾选以在队伍窗口左侧（或右侧）显示此小部件。\n勾选以在队伍窗口内队伍生命条上方显示此小部件。");
    ImGui::NextSpacedElement();
    ImGui::PushItemWidth(120.f);
    ImGui::DragInt("队伍窗口偏移", &settings.user_offset);
    ImGui::TextUnformatted("增益监视器启用的技能：");
    ImGui::Indent();
    ImGui::StartSpacedElements(180.f);
    for (auto& bond : available_bonds) {
        char label_buf[128];
        ImGui::NextSpacedElement();
        const auto written = snprintf(label_buf, sizeof(label_buf), "%s##available_bond_%p", bond.skill_name.string().c_str(), &bond);
        ASSERT(written != -1);
        const bool changed = bond.help_text
                                 ? ImGui::CheckboxWithHelp(label_buf, &bond.enabled, bond.help_text)
                                 : ImGui::Checkbox(label_buf, &bond.enabled);
        if (changed) {
            bond_skills_dirty = true;
            FetchBondSkills();
        }
    }
    ImGui::Unindent();

    Colors::DrawSettingHueWheel("背景", &settings.background.value, 0);
    ImGui::Checkbox("点击施放增益", &settings.click_to_cast);
    ImGui::Checkbox("点击取消增益", &settings.click_to_drop);
    ImGui::CheckboxWithHelp("显示盟友的增益", &settings.show_allies, "'盟友' 指队伍窗口中显示的单位，如召唤石");
    ImGui::CheckboxWithHelp("翻转增益顺序（左/右）", &settings.flip_bonds, "增益顺序基于你的配装。勾选以左右翻转");
    Colors::DrawSetting("低属性覆盖层", &settings.low_attribute_overlay.value);
    ImGui::ShowHelp(
        "覆盖以低于当前属性等级施放的效果。\n仅适用于你自己和你的英雄，不包括增益。"
    );
}
