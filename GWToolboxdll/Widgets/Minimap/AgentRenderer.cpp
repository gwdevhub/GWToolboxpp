#include "stdafx.h"

#include <GWCA/Context/MapContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Constants/EncStrings.h>

#include <Modules/Resources.h>
#include <Widgets/Minimap/AgentRenderer.h>
#include <Widgets/Minimap/Minimap.h>

#include "GWToolbox.h"
#include <Utils/ToolboxUtils.h>

constexpr auto AGENTCOLOR_INIFILENAME = L"AgentColors.ini";
constexpr auto AGENTCOLOR_JSONFILENAME = L"AgentColors.json";

namespace {

    GW::HookEntry ChatCmd_HookEntry;
    uint32_t GetAgentProfession(const GW::AgentLiving* agent)
    {
        if (!agent) {
            return 0;
        }
        if (agent->primary != GW::Constants::ProfessionByte::None) {
            return (uint32_t)agent->primary;
        }
        const GW::NPC* npc = GW::Agents::GetNPCByID(agent->player_number);
        if (!npc) {
            return 0;
        }
        return (uint32_t)npc->primary;
    }

    bool show_props_on_minimap = false;

    bool target_drawn = false;

    bool IsLockedChest(const GW::Agent* agent)
    {
        return agent && agent->GetIsGadgetType() && wcseq(GW::Agents::GetAgentEncName(agent->agent_id), GW::EncStrings::LockedChest);
    }

    bool IsOpenedLockedChest(const GW::Agent* agent)
    {
        return IsLockedChest(agent) && !GW::Agents::GetAgentMatchesFlags(agent, GW::TargetFilter::Gadgets);
    }

    struct MarkedTarget {
        uint32_t type = 0;
        uint32_t identifier = 0;
        wchar_t* agent_name = nullptr;
        MarkedTarget& operator=(const MarkedTarget&) = delete;
        MarkedTarget(MarkedTarget&&) = delete;
        MarkedTarget& operator=(MarkedTarget&& other) = delete;

        explicit MarkedTarget(const GW::Agent* agent)
        {
            ASSERT(agent);
            type = agent->type;
            identifier = GetIdentifier(agent);
            if (const auto found = GW::Agents::GetAgentEncName(agent)) {
                agent_name = new wchar_t[wcslen(found) + 1];
                wcscpy(agent_name, found);
            }
        }

        static uint32_t GetIdentifier(const GW::Agent* agent)
        {
            ASSERT(agent);
            switch (agent->type) {
                case 0xdb:
                    return agent->GetAsAgentLiving()->player_number;
                case 0x200:
                    return agent->GetAsAgentGadget()->extra_type;
                case 0x400:
                    return static_cast<uint32_t>(agent->GetAsAgentItem()->x);
            }
            return 0;
        }

        bool Matches(const GW::Agent* agent) const
        {
            if (!agent || agent->type != type || GetIdentifier(agent) != identifier) {
                return false;
            }
            if (const auto found = GW::Agents::GetAgentEncName(agent)) {
                return agent_name && wcscmp(found, agent_name) == 0;
            }
            return agent_name == nullptr;
        }

        ~MarkedTarget()
        {
            delete[] agent_name;
            agent_name = nullptr;
        }
    };

    std::map<uint32_t, MarkedTarget*> marked_targets; // {单位 ID, MarkedTarget}
    MarkedTarget* GetMarkedTarget(const uint32_t agent_id)
    {
        const auto found = agent_id ? marked_targets.find(agent_id) : marked_targets.end();
        return found != marked_targets.end() ? found->second : nullptr;
    }

    bool RemoveMarkedTarget(const uint32_t agent_id = 0)
    {
        if (!agent_id) {
            while (marked_targets.size()) {
                RemoveMarkedTarget(marked_targets.begin()->first);
            }
            return true;
        }
        const auto found = marked_targets.find(agent_id);
        if (found == marked_targets.end()) {
            return false;
        }
        delete found->second;
        marked_targets.erase(found);
        return true;
    }

    void CHAT_CMD_FUNC(CmdMarkTarget)
    {
        if (argc > 1 && wcscmp(argv[1], L"clearall") == 0) {
            RemoveMarkedTarget();
            return;
        }
        const auto agent = GW::Agents::GetTarget();
        if (!agent) {
            return;
        }
        if (argc > 1 && (wcscmp(argv[1], L"clear") == 0 || wcscmp(argv[1], L"remove") == 0)) {
            RemoveMarkedTarget(agent->agent_id);
            return;
        }
        marked_targets.emplace(agent->agent_id, new MarkedTarget(agent));
    }

    void CHAT_CMD_FUNC(CmdClearMarkTarget)
    {
        RemoveMarkedTarget();
    }

    GW::HookEntry OnAgentAdded_HookEntry;

    bool hooks_added = false;

    void OnAgentAdded(GW::HookStatus*, const GW::Packet::StoC::AgentAdd* packet)
    {
        const auto agent_id = packet->agent_id;
        const auto marked_target = GetMarkedTarget(agent_id);
        if (!marked_target) {
            return;
        }
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        if (!marked_target->Matches(agent)) {
            // 单位 ID 已被回收
            RemoveMarkedTarget(agent_id);
        }
    }
}

AgentRenderer* AgentRenderer::instance = nullptr;

unsigned int AgentRenderer::CustomAgent::cur_ui_id = 0;

void AgentRenderer::RegisterSettings(ToolboxModule* module)
{
    const std::pair<const char*, Color*> colors[] = {
        {"color_agent_modifier", &color_agent_modifier},
        {"color_agent_damaged_modifier", &color_agent_damaged_modifier},
        {"color_eoe", &color_eoe},
        {"color_qz", &color_qz},
        {"color_winnowing", &color_winnowing},
        {"color_frozen_soil", &color_frozen_soil},
        {"color_symbiosis", &color_symbiosis},
        {"color_target", &color_target},
        {"color_player", &color_player},
        {"color_player_dead", &color_player_dead},
        {"color_signpost", &color_signpost},
        {"color_locked_chest", &color_locked_chest},
        {"color_locked_chest_open", &color_locked_chest_open},
        {"color_item", &color_item},
        {"color_hostile", &color_hostile},
        {"color_hostile_dead", &color_hostile_dead},
        {"color_neutral", &color_neutral},
        {"color_ally", &color_ally},
        {"color_ally_npc", &color_ally_npc},
        {"color_ally_npc_quest", &color_ally_npc_quest},
        {"color_ally_spirit", &color_ally_spirit},
        {"color_ally_minion", &color_ally_minion},
        {"color_ally_dead", &color_ally_dead},
        {"color_marked_target", &color_marked_target},
        {"color_profession_warrior", &profession_colors[1]},
        {"color_profession_ranger", &profession_colors[2]},
        {"color_profession_monk", &profession_colors[3]},
        {"color_profession_necromancer", &profession_colors[4]},
        {"color_profession_mesmer", &profession_colors[5]},
        {"color_profession_elementalist", &profession_colors[6]},
        {"color_profession_assassin", &profession_colors[7]},
        {"color_profession_ritualist", &profession_colors[8]},
        {"color_profession_paragon", &profession_colors[9]},
        {"color_profession_dervish", &profession_colors[10]},
    };
    for (const auto& [key, color] : colors) {
        // SettingColor 与 Color 布局兼容；强制转换使注册表能将其持久化为十六进制字符串
        SettingsRegistry::RegisterField(module, key, reinterpret_cast<Colors::SettingColor*>(color));
    }
    SettingsRegistry::RegisterField(module, "enemies_colors_by_profession", &enemies_colors_by_profession);
    SettingsRegistry::RegisterField(module, "only_color_bosses", &only_color_bosses);
    SettingsRegistry::RegisterField(module, "show_quest_npcs_on_minimap", &show_quest_npcs_on_minimap);
    SettingsRegistry::RegisterField(module, "show_hidden_npcs", &show_hidden_npcs);
    SettingsRegistry::RegisterField(module, "marked_target_inherit_custom_agents", &marked_target_inherit_custom_agents);
    SettingsRegistry::RegisterField(module, "custom_agent_defaults_seeded", &custom_agent_defaults_seeded);
#ifdef _DEBUG
    SettingsRegistry::RegisterField(module, "show_props_on_minimap", &show_props_on_minimap);
#endif
    SettingsRegistry::RegisterField(module, "size_default", &size_default);
    SettingsRegistry::RegisterField(module, "size_player", &size_player);
    SettingsRegistry::RegisterField(module, "size_signpost", &size_signpost);
    SettingsRegistry::RegisterField(module, "size_locked_chest", &size_locked_chest);
    SettingsRegistry::RegisterField(module, "size_locked_chest_open", &size_locked_chest_open);
    SettingsRegistry::RegisterField(module, "size_item", &size_item);
    SettingsRegistry::RegisterField(module, "size_boss", &size_boss);
    SettingsRegistry::RegisterField(module, "size_minion", &size_minion);
    SettingsRegistry::RegisterField(module, "size_marked_target", &size_marked_target);
    SettingsRegistry::RegisterField(module, "size_hostile", &size_hostile);
    SettingsRegistry::RegisterField(module, "size_neutral", &size_neutral);
    SettingsRegistry::RegisterField(module, "size_ally", &size_ally);
    SettingsRegistry::RegisterField(module, "size_ally_npc", &size_ally_npc);
    SettingsRegistry::RegisterField(module, "size_ally_npc_quest", &size_ally_npc_quest);
    SettingsRegistry::RegisterField(module, "size_ally_spirit", &size_ally_spirit);
    SettingsRegistry::RegisterField(module, "agent_border_thickness", &agent_border_thickness);
    SettingsRegistry::RegisterField(module, "target_border_thickness", &target_border_thickness);
    SettingsRegistry::RegisterField(module, "default_shape", reinterpret_cast<int*>(&default_shape));
    SettingsRegistry::RegisterField(module, "shape_player", reinterpret_cast<int*>(&shape_player));
    SettingsRegistry::RegisterField(module, "shape_players", reinterpret_cast<int*>(&shape_players));
}

void AgentRenderer::LoadCustomAgents()
{
    custom_agents_map.clear();
    for (const CustomAgent* ca : custom_agents) {
        delete ca;
    }
    custom_agents.clear();

    bool migrated_from_ini = false;
    const auto json_path = Resources::GetSettingFile(AGENTCOLOR_JSONFILENAME);
    std::error_code ec;
    if (std::filesystem::exists(json_path, ec)) {
        std::ifstream file(json_path, std::ios::binary);
        const std::string json_buf{std::istreambuf_iterator(file), {}};
        std::vector<CustomAgent::Settings> entries;
        if (!file || glz::read<glz::opts{.error_on_unknown_keys = false}>(entries, json_buf)) {
            // 保持 custom_agents_loaded 未设置，以便保存不会覆盖不可读的文件
            Log::Error("解析 AgentColors.json 失败");
            return;
        }
        for (const auto& entry : entries) {
            const auto custom_agent = new CustomAgent(entry);
            custom_agent->index = custom_agents.size();
            custom_agents.push_back(custom_agent);
        }
    }
    else {
        // 旧版回退；AgentColors.ini 从此处读取，下次保存时写入 json
        ToolboxIni inifile;
        ASSERT(inifile.LoadIfExists(Resources::GetLegacySettingFile(AGENTCOLOR_INIFILENAME)) == SI_OK);

        TNamesDepend entries;
        inifile.GetAllSections(entries);

        for (const auto& entry : entries) {
            // 我们知道所有节都是单位颜色，不检查节名称
            auto* custom_agent = new CustomAgent(&inifile, entry.pItem);
            custom_agent->index = custom_agents.size();
            custom_agents.push_back(custom_agent);
        }
        migrated_from_ini = true;
    }
    BuildCustomAgentsMap();
    // 旧版 .ini 加载后磁盘上没有 .json；标记已更改以便下次保存时迁移
    agentcolors_changed = migrated_from_ini;
    custom_agents_loaded = true;

    if (!custom_agent_defaults_seeded) {
        SeedDefaultCustomAgents();
        custom_agent_defaults_seeded = true;
    }
}

void AgentRenderer::SeedDefaultCustomAgents()
{
    struct DefaultRow {
        const char* label;
        GW::Constants::Allegiance allegiance;
        QuestState quest_state;
        Color* color;
        float* size;
    };
    const DefaultRow rows[] = {
        {"中立", GW::Constants::Allegiance::Neutral, EitherQuestState, &color_neutral, &size_neutral},
        {"盟友", GW::Constants::Allegiance::Ally_NonAttackable, NotQuestGiver, &color_ally, &size_ally},
        {"盟友（NPC）", GW::Constants::Allegiance::Npc_Minipet, NotQuestGiver, &color_ally_npc, &size_ally_npc},
        {"盟友（灵/宠物）", GW::Constants::Allegiance::Spirit_Pet, NotQuestGiver, &color_ally_spirit, &size_ally_spirit},
        {"盟友（仆从）", GW::Constants::Allegiance::Minion, NotQuestGiver, &color_ally_minion, &size_minion},
    };
    for (const auto& row : rows) {
        auto* ca = new CustomAgent(0, *row.color, row.label);
        ca->allegiance = static_cast<int>(row.allegiance);
        ca->quest_state = row.quest_state;
        ca->size = *row.size;
        ca->size_active = true;
        ca->is_default = true;
        std::snprintf(ca->group, sizeof(ca->group), "默认");
        ca->index = custom_agents.size();
        custom_agents.push_back(ca);
    }
    BuildCustomAgentsMap();
    agentcolors_changed = true;
}

void AgentRenderer::SyncSeededDefaultsFromLegacyFields()
{
    // 当其他地方点击“恢复默认”时，保持种子 Custom Agents 行同步。
    const std::pair<GW::Constants::Allegiance, std::pair<Color, float>> legacy_values[] = {
        {GW::Constants::Allegiance::Neutral, {color_neutral, size_neutral}},
        {GW::Constants::Allegiance::Ally_NonAttackable, {color_ally, size_ally}},
        {GW::Constants::Allegiance::Npc_Minipet, {color_ally_npc, size_ally_npc}},
        {GW::Constants::Allegiance::Spirit_Pet, {color_ally_spirit, size_ally_spirit}},
        {GW::Constants::Allegiance::Minion, {color_ally_minion, size_minion}},
    };
    for (const auto& [allegiance, values] : legacy_values) {
        for (CustomAgent* ca : custom_agents) {
            if (!ca->is_default || ca->allegiance != static_cast<int>(allegiance)) {
                continue;
            }
            ca->color = values.first;
            ca->size = values.second;
        }
    }
    agentcolors_changed = true;
}

void AgentRenderer::SaveCustomAgents() const
{
    if ((agentcolors_changed || GWToolbox::SettingsFolderChanged()) && custom_agents_loaded) {
        std::vector<CustomAgent::Settings> entries;
        entries.reserve(custom_agents.size());
        for (const CustomAgent* ca : custom_agents) {
            entries.push_back(ca->ToSettings());
        }
        std::string json_buf;
        ASSERT(!glz::write<glz::opts{.prettify = true}>(entries, json_buf));
        std::ofstream file(Resources::GetSettingFile(AGENTCOLOR_JSONFILENAME), std::ios::binary | std::ios::trunc);
        file.write(json_buf.data(), static_cast<std::streamsize>(json_buf.size()));
        ASSERT(file.good());
    }
}

void AgentRenderer::LoadDefaultSizes()
{
    size_default = 100.0f;
    size_player = size_default;
    size_signpost = size_default * .5f;
    size_locked_chest = size_signpost;
    size_locked_chest_open = size_signpost;
    size_item = size_default * .25f;
    size_boss = size_default * 1.25f;
    size_minion = size_default * .5f;
    size_marked_target = size_default;
    size_hostile = size_default;
    size_neutral = size_default;
    size_ally = size_default;
    size_ally_npc = size_default;
    size_ally_npc_quest = size_default;
    size_ally_spirit = size_default;
    agent_border_thickness = 0.f;
    target_border_thickness = 50.0f;
}

void AgentRenderer::LoadDefaultColors()
{
    color_marked_target = 0xFFFFFC00;
    color_agent_modifier = 0x001E1E1E;
    color_agent_damaged_modifier = 0x00505050;
    color_eoe = 0x3200FF00;
    color_qz = 0x320000FF;
    color_winnowing = 0x3200FFFF;
    color_frozen_soil = 0x00FEFFFF;
    color_symbiosis = 0x00FF00FF;
    color_target = 0xFFFFFF00;
    color_player = 0xFFFF8000;
    color_player_dead = 0x64FF8000;
    color_signpost = 0xFF0000C8;
    color_locked_chest = 0xFF0000C8;
    color_locked_chest_open = 0xFF0000C8;
    color_item = 0xFF0000F0;
    color_hostile = 0xFFF00000;
    color_hostile_dead = 0xFF320000;
    color_neutral = 0xFF0000DC;
    color_ally = 0xFF00B300;
    color_ally_npc = 0xFF99FF99;
    color_ally_npc_quest = 0xFF99FF99;
    color_ally_spirit = 0xFF608000;
    color_ally_minion = 0xFF008060;
    color_ally_dead = 0x64006400;
    enemies_colors_by_profession = true;
    only_color_bosses = true;
}

void AgentRenderer::DrawSettings()
{
#ifdef _DEBUG
    ImGui::Checkbox("在小地图上显示物品", &show_props_on_minimap);
#endif
    if (ImGui::TreeNodeEx("单位颜色", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::SmallConfirmButton("恢复默认", "确定吗？\n这将重置所有单位大小为默认值。\n此操作不可撤销。\n\n", [&](bool result, void*) {
            if (result) {
                LoadDefaultColors();
                LoadDefaultSizes();
                SyncSeededDefaultsFromLegacyFields();
            }
        });

        struct AgentColorRow {
            const char* label;
            Color* color;
            float* size;
            const char* color_tooltip;
            const char* size_tooltip;
        };

        const AgentColorRow rows[] = {
            {"灭绝之刃", &color_eoe, nullptr, nullptr, "这是边缘的颜色，中心的颜色相同，Alpha-50"},
            {"疾风", &color_qz, nullptr, nullptr, "这是边缘的颜色，中心的颜色相同，Alpha-50"},
            {"筛选", &color_winnowing, nullptr, nullptr, "这是边缘的颜色，中心的颜色相同，Alpha-50"},
            {"冻土", &color_frozen_soil, nullptr, nullptr, "这是边缘的颜色，中心的颜色相同，Alpha-50"},
            {"共生", &color_symbiosis, nullptr, nullptr, "这是边缘的颜色，中心的颜色相同，Alpha-50"},
            {"目标", &color_target, nullptr, nullptr, nullptr},
            {"玩家（存活）", &color_player, nullptr, nullptr, nullptr},
            {"玩家（死亡）", &color_player_dead, nullptr, nullptr, nullptr},
            {"路标", &color_signpost, nullptr, nullptr, nullptr},
            {"锁箱（未开）", &color_locked_chest, &size_locked_chest, nullptr, "未开启锁箱大小"},
            {"锁箱（已开）", &color_locked_chest_open, &size_locked_chest_open, nullptr, "已开启锁箱大小"},
            {"物品", &color_item, nullptr, nullptr, nullptr},
            {"敌对（>90% 生命）", &color_hostile, &size_hostile, nullptr, "敌对单位大小"},
            {"敌对（死亡）", &color_hostile_dead, nullptr, nullptr, nullptr},
            // 中立/盟友/盟友（NPC）/盟友（灵）/盟友（仆从）已迁移到种子自定义单位行（见 SeedDefaultCustomAgents()）。
            // 盟友（NPC 任务给予者）/盟友（死亡）保留在此：这些颜色适用于所有友好阵营。
            {"盟友（NPC 任务给予者）", &color_ally_npc_quest, &size_ally_npc_quest, nullptr, "盟友（NPC 任务给予者）大小"},
            {"盟友（死亡）", &color_ally_dead, nullptr, nullptr, nullptr},
            {"单位修饰符", &color_agent_modifier, nullptr, nullptr, "每个单位的边框减去此值，中心加上此值\n零使单位颜色纯色，高值使阴影更明显。"},
            {"单位受伤修饰符", &color_agent_damaged_modifier, nullptr, nullptr, "每个敌对单位在生命低于 90% 时减去此值。"},
            {"标记目标", &color_marked_target, nullptr, nullptr, "通过 /marktarget 命令高亮的单位"},
        };
        const auto color_w = (ImGui::GetContentRegionAvail().x - 260.f) * 0.6f;
        const auto size_w = ImGui::GetTextLineHeight() * 4.f;

        for (const auto& row : rows) {
            ImGui::SetNextItemWidth(color_w);
            Colors::DrawSettingHueWheel(row.label, row.color);
            if (row.color_tooltip) {
                ImGui::ShowHelp(row.color_tooltip);
            }
            if (row.size) {
                ImGui::SameLine(color_w + 260.f);
                ImGui::SetNextItemWidth(size_w);
                ImGui::PushID(row.label);
                ImGui::DragFloat("大小##size", row.size, 1.0f, 1.0f, 0.0f, "%.0f");
                ImGui::PopID();
                if (row.size_tooltip && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", row.size_tooltip);
                }
            }
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("单位大小", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::SmallConfirmButton("恢复默认", "确定吗？\n这将重置所有单位大小为默认值。\n此操作不可撤销。\n\n",
            [&](const bool result, void*) {
                if (result) {
                    LoadDefaultSizes();
                    SyncSeededDefaultsFromLegacyFields();
                }
            });
        {
            struct SizeEntry { const char* label; float* size; const char* help; };
            const SizeEntry entries[] = {
                {"默认大小",       &size_default,       nullptr},
                {"玩家大小",        &size_player,        nullptr},
                {"路标大小",      &size_signpost,      nullptr},
                {"物品大小",          &size_item,          nullptr},
                {"首领大小",          &size_boss,          nullptr},
                // 仆从大小已迁移到自定义单位中的种子“盟友（仆从）”行。
                {"标记目标大小", &size_marked_target, "通过 /marktarget 命令高亮的单位"},
            };
            for (const auto& [label, sz, help] : entries) {
                ImGui::DragFloat(label, sz, 1.0f, 1.0f, 0.0f, "%.0f");
                if (help) {
                    ImGui::ShowHelp(help);
                }
            }
        }
        // 右对齐复选框
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::CalcItemWidth() - ImGui::GetFrameHeight());
        ImGui::CheckboxWithHelp("标记目标继承自定义大小/形状", &marked_target_inherit_custom_agents, "启用后，通过 /marktarget 高亮的单位如果定义了自定义单位配置，则使用其大小和形状");
        static std::array items = {"泪滴", "圆形", "方形", "大圆形"};
        ImGui::Combo("默认形状", reinterpret_cast<int*>(&default_shape), items.data(), items.size());
        ImGui::Combo("玩家形状", reinterpret_cast<int*>(&shape_player), items.data(), items.size());
        ImGui::Combo("其他玩家形状", reinterpret_cast<int*>(&shape_players), items.data(), items.size());
        ImGui::ShowHelp("单位的默认形状。");

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("自定义单位", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        static char group_filter[64] = "";
        ImGui::InputTextWithHint("筛选", "按名称或组筛选...", group_filter, sizeof(group_filter));
        ImGui::ShowHelp("仅影响此处显示的内容；列表顺序（因此小地图绘制顺序）不变。");

        const auto matches_filter = [](const CustomAgent* ca) {
            if (!group_filter[0]) {
                return true;
            }
            const auto to_lower = [](std::string s) {
                std::ranges::transform(s, s.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return s;
            };
            const auto needle = to_lower(group_filter);
            return to_lower(ca->name).find(needle) != std::string::npos || to_lower(ca->group).find(needle) != std::string::npos;
        };

        bool changed = false;
        ImGui::BeginChild("##custom_agents_scroll", ImVec2(0.f, 400.f), true);
        for (unsigned i = 0; i < custom_agents.size(); ++i) {
            CustomAgent* custom = custom_agents[i];
            if (!custom) {
                continue;
            }
            if (!matches_filter(custom)) {
                continue;
            }

            ImGui::PushID(static_cast<int>(custom->ui_id));

            auto op = CustomAgent::Operation::None;
            if (custom->DrawSettings(op)) {
                changed = true;
            }

            ImGui::PopID();

            switch (op) {
                case CustomAgent::Operation::None:
                    break;
                case CustomAgent::Operation::MoveUp:
                    if (i > 0) {
                        std::swap(custom_agents[i], custom_agents[i - 1]);
                    }
                    break;
                case CustomAgent::Operation::MoveDown:
                    if (i < custom_agents.size() - 1) {
                        std::swap(custom_agents[i], custom_agents[i + 1]);
                        // 渲染移动后的项并增加 i
                        ++i;
                        ImGui::PushID(static_cast<int>(custom_agents[i]->ui_id));
                        auto op2 = CustomAgent::Operation::None;
                        custom_agents[i]->DrawSettings(op2);
                        ImGui::PopID();
                    }
                    break;
                case CustomAgent::Operation::Delete:
                    custom_agents.erase(custom_agents.begin() + static_cast<int>(i));
                    delete custom;
                    --i;
                    break;
                case CustomAgent::Operation::ModelIdChange: {
                    changed = true;
                    break;
                }
                default:
                    break;
            }

            switch (op) {
                case CustomAgent::Operation::MoveUp:
                case CustomAgent::Operation::MoveDown:
                case CustomAgent::Operation::Delete:
                    for (size_t j = 0; j < custom_agents.size(); ++j) {
                        custom_agents[j]->index = j;
                    }
                    changed = true;
                default:
                    break;
            }
        }
        ImGui::EndChild();
        if (changed) {
            agentcolors_changed = true;
            BuildCustomAgentsMap();
        }
        if (ImGui::Button("添加自定义单位颜色")) {
            custom_agents.push_back(new CustomAgent(0, color_hostile, "<名称>"));
            custom_agents.back()->index = custom_agents.size() - 1;
            agentcolors_changed = true;
        }
        ImGui::TreePop();
    }
}

void AgentRenderer::Terminate()
{
    D3DVertexBuffer::Terminate();
    custom_agents_map.clear();
    for (const CustomAgent* ca : custom_agents) {
        delete ca;
    }
    custom_agents.clear();
    RemoveMarkedTarget();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

AgentRenderer& AgentRenderer::Instance() { return *instance; }

AgentRenderer::AgentRenderer()
{
    instance = this;
    last_check = TIMER_INIT();
    shapes[Tear].AddVertex(1.8f, 0, Dark);      // A
    shapes[Tear].AddVertex(0.7f, 0.7f, Dark);   // B
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.7f, 0.7f, Dark);   // B
    shapes[Tear].AddVertex(0.0f, 1.0f, Dark);   // C
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.0f, 1.0f, Dark);   // C
    shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);  // D
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);  // D
    shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);  // E
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);  // E
    shapes[Tear].AddVertex(-0.7f, -0.7f, Dark); // F
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(-0.7f, -0.7f, Dark); // F
    shapes[Tear].AddVertex(0.0f, -1.0f, Dark);  // G
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.0f, -1.0f, Dark);  // G
    shapes[Tear].AddVertex(0.7f, -0.7f, Dark);  // H
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.7f, -0.7f, Dark);  // H
    shapes[Tear].AddVertex(1.8f, 0.0f, Dark);   // A
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O

    constexpr auto pi = DirectX::XM_PI;
    for (int i = 0; i < num_triangles; ++i) {
        const float angle1 = 2 * (i + 0) * pi / num_triangles;
        const float angle2 = 2 * (i + 1) * pi / num_triangles;
        shapes[Circle].AddVertex(std::cos(angle1), std::sin(angle1), Dark);
        shapes[Circle].AddVertex(std::cos(angle2), std::sin(angle2), Dark);
        shapes[Circle].AddVertex(0.0f, 0.0f, Light);
    }

    for (int i = 0; i < num_triangles; ++i) {
        const float angle1 = 2 * (i + 0) * pi / num_triangles;
        const float angle2 = 2 * (i + 1) * pi / num_triangles;
        shapes[BigCircle].AddVertex(std::cos(angle1), std::sin(angle1), None);
        shapes[BigCircle].AddVertex(std::cos(angle2), std::sin(angle2), None);
        shapes[BigCircle].AddVertex(0.0f, 0.0f, CircleCenter);
    }

    shapes[Quad].AddVertex(1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(-1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(-1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(-1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(-1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);

    constexpr size_t star_ntriangles = 16;
    constexpr float star_size_small = 1.f;
    constexpr float star_size_big = 1.5f;
    for (unsigned int i = 0; i < star_ntriangles; ++i) {
        const float angle1 = 2 * (i + 0) * pi / star_ntriangles;
        const float angle2 = 2 * (i + 1) * pi / star_ntriangles;

        const float size1 = (i + 0) % 2 == 0 ? star_size_small : star_size_big;
        const float size2 = (i + 1) % 2 == 0 ? star_size_small : star_size_big;
        shapes[Star].AddVertex(std::cos(angle1) * size1, std::sin(angle1) * size1, None);
        shapes[Star].AddVertex(std::cos(angle2) * size2, std::sin(angle2) * size2, None);
        shapes[Star].AddVertex(0.0f, 0.0f, CircleCenter);
    }
}

void AgentRenderer::OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage msgid, void* wParam, void*)
{
    switch (msgid) {
        case GW::UI::UIMessage::kMapLoaded:
            RemoveMarkedTarget();
            break;
        case GW::UI::UIMessage::kShowAgentNameTag:
        case GW::UI::UIMessage::kSetAgentNameTagAttribs: {
            const auto msg = static_cast<GW::UI::AgentNameTagInfo*>(wParam);

            GW::Agent* agent = GW::Agents::GetAgentByID(msg->agent_id);
            if (!agent) {
                return;
            }
            const GW::AgentLiving* living = agent->GetAsAgentLiving();
            if (!living) {
                return;
            }
            auto& custom_agents_map = Instance().custom_agents_map;
            const auto it = custom_agents_map.find(living->player_number);
            if (it != custom_agents_map.end()) {
                for (const CustomAgent* ca : it->second) {
                    if (!ca->active) {
                        continue;
                    }
                    if (!ca->color_text_active) {
                        continue;
                    }
                    if (ca->mapId > 0 && ca->mapId != static_cast<DWORD>(GW::Map::GetMapID())) {
                        continue;
                    }
                    if (ca->combat_state == CombatState::InCombat && !living->GetInCombatStance()) {
                        continue;
                    }
                    if (ca->combat_state == CombatState::NotInCombat && living->GetInCombatStance()) {
                        continue;
                    }
                    if (ca->weapon_state == WeaponState::HasWeapon && (living->weapon_type == 0 || living->weapon_type == 512)) {
                        continue;
                    }
                    if (ca->weapon_state == WeaponState::NoWeapon && !(living->weapon_type == 0 || living->weapon_type == 512)) {
                        continue;
                    }
                    msg->text_color = ca->color_text;
                }
            }
            break;
        }
    }
}

void AgentRenderer::Shape_t::AddVertex(const float x, const float y, const Color_Modifier mod)
{
    vertices.push_back(Shape_Vertex(x, y, mod));
}

void AgentRenderer::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLELIST;
    D3DVertexBuffer::Initialize(device);

    if (!hooks_added) {
        hooks_added = true;
        constexpr GW::UI::UIMessage hook_messages[] = {GW::UI::UIMessage::kShowAgentNameTag, GW::UI::UIMessage::kSetAgentNameTagAttribs, GW::UI::UIMessage::kMapLoaded};
        for (const auto message_id : hook_messages) {
            RegisterUIMessageCallback(&UIMsg_Entry, message_id, OnUIMessage);
        }
        GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdded_HookEntry, OnAgentAdded);

        GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"marktarget", CmdMarkTarget);
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"clearmarktarget", CmdClearMarkTarget);
    }

}

std::vector<const AgentRenderer::CustomAgent*>* AgentRenderer::GetCustomAgentsToDraw(const GW::Agent* agent)
{
    if (!agent) {
        return nullptr;
    }
    const auto living = agent->GetAsAgentLiving();
    const auto agent_identifier = living ? living->player_number : (agent->GetIsGadgetType() ? agent->GetAsAgentGadget()->gadget_id : 0);

    static std::vector<const CustomAgent*> out;
    out.clear();

    const auto try_add = [&](const CustomAgent* ca) {
        if (!ca->active) {
            return;
        }
        if (ca->mapId > 0 && ca->mapId != static_cast<DWORD>(GW::Map::GetMapID())) {
            return;
        }
        if (living) {
            if (ca->combat_state == CombatState::InCombat && !living->GetInCombatStance()) {
                return;
            }
            if (ca->combat_state == CombatState::NotInCombat && living->GetInCombatStance()) {
                return;
            }
            if (ca->weapon_state == WeaponState::HasWeapon && (living->weapon_type == 0 || living->weapon_type == 512)) {
                return;
            }
            if (ca->weapon_state == WeaponState::NoWeapon && !(living->weapon_type == 0 || living->weapon_type == 512)) {
                return;
            }
            if (ca->dead_state == DeadState::Dead && !living->GetIsDead()) {
                return;
            }
            if (ca->dead_state == DeadState::Alive && living->GetIsDead()) {
                return;
            }
            if (ca->quest_state == QuestState::QuestGiver && !living->GetHasQuest()) {
                return;
            }
            if (ca->quest_state == QuestState::NotQuestGiver && living->GetHasQuest()) {
                return;
            }
        }
        out.push_back(ca);
    };

    if (const auto it = custom_agents_map.find(agent_identifier); it != custom_agents_map.end()) {
        for (const CustomAgent* ca : it->second) {
            try_add(ca);
        }
    }
    // 按类型的默认行匹配该阵营的每个活体单位，此外还有上面的 modelId 匹配。
    if (living && !custom_agents_by_allegiance.empty()) {
        if (const auto it = custom_agents_by_allegiance.find(static_cast<int>(living->allegiance)); it != custom_agents_by_allegiance.end()) {
            for (const CustomAgent* ca : it->second) {
                try_add(ca);
            }
        }
    }

    if (out.empty()) {
        return nullptr;
    }
    std::ranges::sort(
        out,
        [&](const CustomAgent* pA,
            const CustomAgent* pB) -> bool {
            return pA->index > pB->index;
        });
    return &out;
};

void AgentRenderer::Render(IDirect3DDevice9* device)
{
    const auto now = TIMER_INIT();
    // 每 30 帧更新一次，减少 CPU 负载
    if (now - last_check > 33) {
        last_check = now;
        clear();

        if (show_props_on_minimap) {
            const auto& props = GW::GetMapContext()->props->propArray;
            for (size_t i = 0; i < props.size(); i++) {
                Enqueue(Quad, props[i], size_item, color_signpost);
            }
        }

        GW::AgentArray* agents = GW::Agents::GetAgentArray();
        if (!agents) {
            return;
        }

        RefreshRelevantPolys();

        const GW::AgentLiving* player = GW::Agents::GetControlledCharacter();
        const GW::Agent* target = GW::Agents::GetTarget();
        if (target) {
            auto_target_id = 0;
        }
        else if (auto_target_id) {
            auto* const target_ = GW::Agents::GetAgentByID(auto_target_id);
            target = target_ ? target_->GetAsAgentLiving() : nullptr;
        }

        // 1. 范围效果
        for (GW::Agent* agent_ptr : *agents) {
            if (!agent_ptr) {
                continue;
            }
            const GW::AgentLiving* agent = agent_ptr->GetAsAgentLiving();
            if (!agent) {
                continue;
            }
            if (agent->GetIsDead()) {
                continue;
            }
            switch (agent->player_number) {
                case GW::Constants::ModelID::EoE:
                    Enqueue(BigCircle, agent, GW::Constants::Range::SpiritExtended, color_eoe);
                    break;
                case GW::Constants::ModelID::QZ:
                    Enqueue(BigCircle, agent, GW::Constants::Range::SpiritExtended, color_qz);
                    break;
                case GW::Constants::ModelID::Winnowing:
                    Enqueue(BigCircle, agent, GW::Constants::Range::SpiritExtended, color_winnowing);
                    break;
                case GW::Constants::ModelID::FrozenSoil:
                    Enqueue(BigCircle, agent, GW::Constants::Range::SpiritExtended, color_frozen_soil);
                    break;
                case GW::Constants::ModelID::Symbiosis:
                    Enqueue(BigCircle, agent, GW::Constants::Range::SpiritExtended, color_symbiosis);
                    break;
                default:
                    break;
            }
        }
        // 2. 非玩家单位
        static std::vector<std::pair<const GW::Agent*, const CustomAgent*>> custom_agents_to_draw;
        custom_agents_to_draw.clear();

        static std::vector<const GW::AgentLiving*> marked_targets_to_draw;
        marked_targets_to_draw.clear();
        static std::vector<const GW::AgentLiving*> players_to_draw;
        players_to_draw.clear();
        static std::vector<const GW::AgentLiving*> dead_agents_to_draw;
        dead_agents_to_draw.clear();
        static std::vector<const GW::Agent*> other_agents_to_draw;
        other_agents_to_draw.clear();

        target_drawn = false;


        const auto add_custom_agents_to_draw = [this](const GW::Agent* agent) -> bool {
            const auto custom_agents_for_this_agent = GetCustomAgentsToDraw(agent);
            if (!custom_agents_for_this_agent) {
                return false;
            }
            for (auto ca : *custom_agents_for_this_agent) {
                custom_agents_to_draw.push_back({agent, ca});
            }
            return true;
        };

        const auto add_marked_target = [](const GW::AgentLiving* agent) -> bool {
            if (!GetMarkedTarget(agent ? agent->agent_id : 0)) {
                return false;
            }
            marked_targets_to_draw.push_back(agent);
            return true;
        };

        const auto add_other_players_to_draw = [](const GW::AgentLiving* agent) -> bool {
            if (!agent || !agent->IsPlayer() || agent == GW::Agents::GetObservingAgent()) {
                return false;
            }
            players_to_draw.push_back(agent);
            return true;
        };

        const auto add_dead_agent_to_draw = [](const GW::AgentLiving* agent) -> bool {
            if (!agent || !agent->GetIsDead()) {
                return false;
            }
            dead_agents_to_draw.push_back(agent);
            return true;
        };

        auto sort_custom_agents_to_draw = [] {
            std::ranges::sort(custom_agents_to_draw, [&](const std::pair<const GW::Agent*, const CustomAgent*>& pA, const std::pair<const GW::Agent*, const CustomAgent*>& pB) {
                return pA.second->index > pB.second->index;
            });
        };

        for (const auto agent : *agents) {
            if (!agent) {
                continue;
            }
            if (agent == player) {
                continue; //  7. 玩家
            }
            if (agent == target) {
                continue; // 4. 非玩家目标，6. 玩家目标
            }
            if (agent->GetIsGadgetType()) {
                const auto gadget = agent->GetAsAgentGadget();
                if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && gadget->extra_type == 7602) {
                    continue;
                }
                add_custom_agents_to_draw(gadget);
            }
            else if (agent->GetIsLivingType()) {
                const auto living = agent->GetAsAgentLiving();
                if (!show_hidden_npcs && !GW::Agents::GetAgentMatchesFlags(living)) {
                    continue;
                }
                if (add_marked_target(living)) {
                    continue; // 8. 标记目标
                }
                if (add_other_players_to_draw(living)) {
                    continue; // 5. 玩家
                }
                if (add_dead_agent_to_draw(living)) {
                    continue;
                }
                if (add_custom_agents_to_draw(living)) {
                    continue; // 3. 自定义颜色模型
                }
            }
            other_agents_to_draw.push_back(agent);
        }

        // 死亡单位
        for (const auto agent : dead_agents_to_draw) {
            Enqueue(agent);
        }

        // 2. 通用单位
        for (const auto agent : other_agents_to_draw) {
            Enqueue(agent);
        }

        // 3. 自定义颜色模型
        sort_custom_agents_to_draw();
        for (const auto& [fst, snd] : custom_agents_to_draw) {
            Enqueue(fst, snd);
        }

        // 8. 标记目标
        for (const auto agent : marked_targets_to_draw) {
            if (!agent->GetIsAlive()) {
                continue;
            }
            const auto* cas = GetCustomAgentsToDraw(agent);
            const auto* ca = cas && !cas->empty() ? cas->front() : nullptr;
            const auto size = marked_target_inherit_custom_agents && ca && ca->size_active && ca->size >= 0 ? ca->size : size_marked_target;
            const auto shape = marked_target_inherit_custom_agents && ca && ca->shape_active ? ca->shape : default_shape;
            Enqueue(shape, agent, size, color_marked_target);
        }

        // 4. 非玩家目标
        if (target && (!target->GetAsAgentLiving() || !target->GetAsAgentLiving()->IsPlayer())) {
            const auto marked = GetMarkedTarget(target->agent_id);
            const auto custom_agents_for_this_agent = GetCustomAgentsToDraw(target);
            if (custom_agents_for_this_agent) {
                for (const auto ca : *custom_agents_for_this_agent) {
                    Enqueue(target, ca);
                }
            }
            if (marked) {
                const auto* ca = custom_agents_for_this_agent && !custom_agents_for_this_agent->empty() ? custom_agents_for_this_agent->front() : nullptr;
                const auto size = marked_target_inherit_custom_agents && ca && ca->size_active && ca->size >= 0 ? ca->size : size_marked_target;
                const auto shape = marked_target_inherit_custom_agents && ca && ca->shape_active ? ca->shape : default_shape;
                Enqueue(shape, target, size, color_marked_target);
            }

            if (!marked && !custom_agents_for_this_agent) {
                Enqueue(target);
            }
        }

        // 注意：我们不支持玩家的自定义单位

        // 5. 玩家
        for (const auto agent : players_to_draw) {
            Enqueue(agent);
        }

        // 6. 玩家目标
        if (target && target != player && target->GetAsAgentLiving() && target->GetAsAgentLiving()->IsPlayer()) {
            Enqueue(target);
        }

        // 7. 玩家
        if (player) {
            Enqueue(player);
        }
    }

    D3DVertexBuffer::Render(device);
}

void AgentRenderer::Enqueue(const GW::Agent* agent, const CustomAgent* ca)
{
    const auto color = GetColor(agent, ca);
    const auto size = GetSize(agent, ca);
    const auto shape = GetShape(agent, ca);
    return Enqueue(shape, agent, size, color);
}

void AgentRenderer::RefreshRelevantPolys()
{
    const auto map_id = GW::Map::GetMapID();
    relevant_polygons.clear();
    relevant_markers.clear();
    for (const CustomRenderer::CustomPolygon& polygon : Minimap::Instance().custom_renderer.polygons) {
        if (!((polygon.visible && polygon.map == GW::Constants::MapID::None) || polygon.map == map_id)) {
            continue;
        }
        if (polygon.points.empty() || !(polygon.color_sub & IM_COL32_A_MASK)) {
            continue;
        }
        auto& cached = relevant_polygons.emplace_back();
        cached.polygon = &polygon;
        cached.min_x = cached.max_x = polygon.points[0].x;
        cached.min_y = cached.max_y = polygon.points[0].y;
        for (const GW::GamePos& point : polygon.points) {
            cached.min_x = std::min(cached.min_x, point.x);
            cached.max_x = std::max(cached.max_x, point.x);
            cached.min_y = std::min(cached.min_y, point.y);
            cached.max_y = std::max(cached.max_y, point.y);
        }
    }
    for (const CustomRenderer::CustomMarker& marker : Minimap::Instance().custom_renderer.markers) {
        if (!((marker.visible && marker.map == GW::Constants::MapID::None) || marker.map == map_id)) {
            continue;
        }
        if (!(marker.color_sub & IM_COL32_A_MASK)) {
            continue;
        }
        auto& cached = relevant_markers.emplace_back();
        cached.marker = &marker;
        cached.radius_squared = marker.size * marker.size;
    }
}

Color AgentRenderer::GetColor(const GW::Agent* agent, const CustomAgent* ca) const
{
    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    const auto is_dead = living ? living->GetIsDead() : false;
    if (ca && ca->color_active && !is_dead) {
        if (living && living->allegiance == GW::Constants::Allegiance::Enemy && living->hp <= 0.9f) {
            return Colors::Sub(ca->color, color_agent_damaged_modifier);
        }
        return ca->color;
    }

    if (agent->agent_id == GW::Agents::GetControlledCharacterId()) {
        if (agent->GetAsAgentLiving()->GetIsDead()) {
            return color_player_dead;
        }
        return color_player;
    }

    if (agent->GetIsGadgetType()) {
        if (IsLockedChest(agent)) {
            return IsOpenedLockedChest(agent) ? color_locked_chest_open : color_locked_chest;
        }
        return color_signpost;
    }
    if (agent->GetIsItemType()) {
        return color_item;
    }
    if (!agent->GetIsLivingType()) {
        return color_item;
    }

    // 不绘制死亡灵体

    const auto* npc = living->GetIsDead() && living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
    if (npc) {
        switch (npc->model_file_id) {
            case 0x22A34: // 自然仪式
            case 0x2D0E4: // 防御束缚仪式
            case 0x2D07E: // 攻击束缚仪式
                return IM_COL32(0, 0, 0, 0);
            default:
                break;
        }
    }

    if (living->allegiance == GW::Constants::Allegiance::Enemy) {
        if (living->GetIsDead()) {
            return color_hostile_dead;
        }
        const Color* c = &color_hostile;
        if (enemies_colors_by_profession) {
            if ((only_color_bosses && living->GetHasBossGlow()) || !only_color_bosses) {
                const auto prof = GetAgentProfession(living);
                if (prof) {
                    c = &profession_colors[prof];
                }
            }
        }
        constexpr auto relevance_range_squared = 2500.f * 2500.f;
        const auto is_inside = [](const GW::GamePos pos, const std::vector<GW::GamePos>& points) -> bool {
            bool b = false;
            // TODO：可能需要调整以考虑 z 平面
            for (auto i = 0u, j = points.size() - 1; i < points.size(); j = i++) {
                if (points[i].y >= pos.y != points[j].y >= pos.y &&
                    pos.x <= (points[j].x - points[i].x) * (pos.y - points[i].y) / (points[j].y - points[i].y) +
                    points[i].x) {
                    b = !b;
                }
            }
            return b;
        };

        for (const auto& cached : relevant_polygons) {
            const auto& polygon = *cached.polygon;
            if (living->pos.x < cached.min_x || living->pos.x > cached.max_x || living->pos.y < cached.min_y || living->pos.y > cached.max_y) {
                continue;
            }
            const auto& origin = polygon.points[0];
            const float dx = living->pos.x - origin.x;
            const float dy = living->pos.y - origin.y;
            if (dx * dx + dy * dy >= relevance_range_squared) {
                continue;
            }
            if (is_inside(living->pos, polygon.points)) {
                c = &polygon.color_sub;
            }
        }
        for (const auto& cached : relevant_markers) {
            const auto& marker = *cached.marker;
            const float dx = living->pos.x - marker.pos.x;
            const float dy = living->pos.y - marker.pos.y;
            const float dist_squared = dx * dx + dy * dy;
            if (dist_squared >= relevance_range_squared || dist_squared > cached.radius_squared) {
                continue;
            }
            c = &marker.color_sub;
        }
        if (living->hp > 0.9f) {
            return *c;
        }
        return Colors::Sub(*c, color_agent_damaged_modifier);
    }

    if (living->allegiance == GW::Constants::Allegiance::Neutral) {
        return color_neutral;
    }

    // 友好
    if (living->GetIsDead()) {
        return color_ally_dead;
    }
    if (living->GetHasQuest()) {
        return color_ally_npc_quest;
    }
    switch (living->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
            return color_ally; // 盟友
        case GW::Constants::Allegiance::Npc_Minipet:
            return color_ally_npc; // NPC / 小宠物
        case GW::Constants::Allegiance::Spirit_Pet:
            return color_ally_spirit; // 灵 / 宠物
        case GW::Constants::Allegiance::Minion:
            return color_ally_minion; // 仆从
        default:
            break;
    }

    return IM_COL32(0, 0, 0, 0);
}

float AgentRenderer::GetSeededDefaultSize(const GW::Constants::Allegiance allegiance, const float fallback) const
{
    const auto it = custom_agents_by_allegiance.find(static_cast<int>(allegiance));
    if (it == custom_agents_by_allegiance.end()) {
        return fallback;
    }
    for (const CustomAgent* ca : it->second) {
        if (ca->is_default && ca->active && ca->size_active && ca->size >= 0) {
            return ca->size;
        }
    }
    return fallback;
}

float AgentRenderer::GetSize(const GW::Agent* agent, const CustomAgent* ca) const
{
    if (ca && ca->size_active && ca->size >= 0) {
        return ca->size;
    }

    if (agent->agent_id == GW::Agents::GetObservingId()) {
        return size_player;
    }
    if (agent->GetIsGadgetType()) {
        if (IsLockedChest(agent)) {
            return IsOpenedLockedChest(agent) ? size_locked_chest_open : size_locked_chest;
        }
        return size_signpost;
    }
    if (agent->GetIsItemType()) {
        return size_item;
    }
    if (!agent->GetIsLivingType()) {
        return size_item;
    }

    const GW::AgentLiving* living = agent->GetAsAgentLiving();

    if (living->GetHasBossGlow()) {
        return size_boss;
    }

    switch (living->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable: // 盟友
            if (!living->GetIsDead() && living->GetHasQuest()) {
                return size_ally_npc_quest;
            }
            return GetSeededDefaultSize(GW::Constants::Allegiance::Ally_NonAttackable, size_ally);

        case GW::Constants::Allegiance::Neutral: // neutral
            return GetSeededDefaultSize(GW::Constants::Allegiance::Neutral, size_neutral);

        case GW::Constants::Allegiance::Spirit_Pet: // spirit / pet
            return GetSeededDefaultSize(GW::Constants::Allegiance::Spirit_Pet, size_ally_spirit);

        case GW::Constants::Allegiance::Npc_Minipet: // NPC / 小宠物
            if (!living->GetIsDead() && living->GetHasQuest()) {
                return size_ally_npc_quest;
            }
            return GetSeededDefaultSize(GW::Constants::Allegiance::Npc_Minipet, size_ally_npc);

        case GW::Constants::Allegiance::Minion: // minion
            return GetSeededDefaultSize(GW::Constants::Allegiance::Minion, size_minion);

        case GW::Constants::Allegiance::Enemy: // 敌对
            switch (living->player_number) {
                case GW::Constants::ModelID::Rotscale:

                case GW::Constants::ModelID::DoA::StygianLordNecro:
                case GW::Constants::ModelID::DoA::StygianLordMesmer:
                case GW::Constants::ModelID::DoA::StygianLordEle:
                case GW::Constants::ModelID::DoA::StygianLordMonk:
                case GW::Constants::ModelID::DoA::StygianLordDerv:
                case GW::Constants::ModelID::DoA::StygianLordRanger:
                case GW::Constants::ModelID::DoA::BlackBeastOfArgh:
                case GW::Constants::ModelID::DoA::SmotheringTendril:
                case GW::Constants::ModelID::DoA::LordJadoth:

                case GW::Constants::ModelID::UW::KeeperOfSouls:
                case GW::Constants::ModelID::UW::FourHorseman:
                case GW::Constants::ModelID::UW::Slayer:
                case GW::Constants::ModelID::UW::TerrorwebQueen:
                case GW::Constants::ModelID::UW::Dhuum:

                case GW::Constants::ModelID::FoW::ShardWolf:
                case GW::Constants::ModelID::FoW::SeedOfCorruption:
                case GW::Constants::ModelID::FoW::LordKhobay:
                case GW::Constants::ModelID::FoW::DragonLich:

                case GW::Constants::ModelID::Deep::Kanaxai:
                case GW::Constants::ModelID::Deep::KanaxaiAspect:
                case GW::Constants::ModelID::Urgoz::Urgoz:

                case GW::Constants::ModelID::EotnDungeons::DiscOfChaos:
                case GW::Constants::ModelID::EotnDungeons::PlagueOfDestruction:
                case GW::Constants::ModelID::EotnDungeons::ZhimMonns:
                case GW::Constants::ModelID::EotnDungeons::Khabuus:
                case GW::Constants::ModelID::EotnDungeons::DuncanTheBlack:
                case GW::Constants::ModelID::EotnDungeons::JusticiarThommis:
                case GW::Constants::ModelID::EotnDungeons::RandStormweaver:
                case GW::Constants::ModelID::EotnDungeons::Selvetarm:
                case GW::Constants::ModelID::EotnDungeons::Forgewright:
                case GW::Constants::ModelID::EotnDungeons::HavokSoulwail:
                case GW::Constants::ModelID::EotnDungeons::RragarManeater3:
                case GW::Constants::ModelID::EotnDungeons::RragarManeater12:
                case GW::Constants::ModelID::EotnDungeons::Arachni:
                case GW::Constants::ModelID::EotnDungeons::Hidesplitter:
                case GW::Constants::ModelID::EotnDungeons::PrismaticOoze:
                case GW::Constants::ModelID::EotnDungeons::IlsundurLordofFire:
                case GW::Constants::ModelID::EotnDungeons::EldritchEttin:
                case GW::Constants::ModelID::EotnDungeons::TPSRegulartorGolem:
                case GW::Constants::ModelID::EotnDungeons::MalfunctioningEnduringGolem:
                case GW::Constants::ModelID::EotnDungeons::CyndrTheMountainHeart:
                case GW::Constants::ModelID::EotnDungeons::InfernalSiegeWurm:
                case GW::Constants::ModelID::EotnDungeons::Frostmaw:
                case GW::Constants::ModelID::EotnDungeons::RemnantOfAntiquities:
                case GW::Constants::ModelID::EotnDungeons::MurakaiLadyOfTheNight:
                case GW::Constants::ModelID::EotnDungeons::ZoldarkTheUnholy:
                case GW::Constants::ModelID::EotnDungeons::Brigand:
                case GW::Constants::ModelID::EotnDungeons::FendiNin:
                case GW::Constants::ModelID::EotnDungeons::SoulOfFendiNin:
                case GW::Constants::ModelID::EotnDungeons::KeymasterOfMurakai:
                case GW::Constants::ModelID::EotnDungeons::AngrySnowman:

                case GW::Constants::ModelID::BonusMissionPack::WarAshenskull:
                case GW::Constants::ModelID::BonusMissionPack::RoxAshreign:
                case GW::Constants::ModelID::BonusMissionPack::AnrakTindershot:
                case GW::Constants::ModelID::BonusMissionPack::DettMortash:
                case GW::Constants::ModelID::BonusMissionPack::AkinCinderspire:
                case GW::Constants::ModelID::BonusMissionPack::TwangSootpaws:
                case GW::Constants::ModelID::BonusMissionPack::MagisEmberglow:
                case GW::Constants::ModelID::BonusMissionPack::MerciaTheSmug:
                case GW::Constants::ModelID::BonusMissionPack::OptimusCaliph:
                case GW::Constants::ModelID::BonusMissionPack::LazarusTheDire:
                case GW::Constants::ModelID::BonusMissionPack::AdmiralJakman:
                case GW::Constants::ModelID::BonusMissionPack::PalawaJoko:
                case GW::Constants::ModelID::BonusMissionPack::YuriTheHand:
                case GW::Constants::ModelID::BonusMissionPack::MasterRiyo:
                case GW::Constants::ModelID::BonusMissionPack::CaptainSunpu:
                case GW::Constants::ModelID::BonusMissionPack::MinisterWona:
                    return size_boss;

                default:
                    return size_hostile;
            }

        default:
            return size_default;
    }
}

AgentRenderer::Shape_e AgentRenderer::GetShape(const GW::Agent* agent, const CustomAgent* ca) const
{
    if (ca && ca->shape_active) {
        return ca->shape;
    }

    if (agent->GetIsGadgetType()) {
        return Quad;
    }
    if (agent->GetIsItemType()) {
        return Quad;
    }
    if (!agent->GetIsLivingType()) {
        return Quad; // 不太可能发生，但以防万一
    }

    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (living->login_number > 0) {
        if (living->agent_id == GW::Agents::GetControlledCharacterId())
            return shape_player;
        return shape_players; // 玩家
    }

    if (show_quest_npcs_on_minimap && living->GetHasQuest()) {
        return Star;
    }

    const auto* npc = living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
    if (npc) {
        switch (npc->model_file_id) {
            case 0x22A34: // 自然仪式
            case 0x2D0E4: // 防御束缚仪式
            case 0x2963E: // 假人
                return Circle;
            default:
                break;
        }
    }

    return default_shape;
}

void AgentRenderer::Enqueue(const Shape_e shape, const GW::Agent* agent, const float size, const Color color)
{
    const auto alpha = color >> IM_COL32_A_SHIFT & 0xFFu;
    if (!alpha) {
        return;
    }
    const RenderPosition pos = {
        agent->rotation_cos,
        agent->rotation_sin,
        agent->pos
    };
    // 注意：大圆形没有边框
    if (shape != BigCircle) {
        const bool is_target = auto_target_id == agent->agent_id || GW::Agents::GetTargetId() == agent->agent_id;
        if (is_target && target_drawn) {
            return; // 不重复绘制目标
        }
        if (agent_border_thickness != 0.f && agent->GetIsLivingType()) {
            Enqueue(shape, pos, size + agent_border_thickness, Colors::ARGB(static_cast<int>(alpha * 0.8), 0, 0, 0));
        }
        if (is_target) {
            Enqueue(shape, pos, size + target_border_thickness, color_target);
            target_drawn = true;
        }
    }

    return Enqueue(shape, pos, size, color, color_agent_modifier);
}

void AgentRenderer::Enqueue(const Shape_e shape, const GW::MapProp* agent, const float size, const Color color)
{
    const RenderPosition pos = {
        agent->rotation_cos,
        agent->rotation_sin,
        {agent->position.x, agent->position.y}
    };
    return Enqueue(shape, pos, size, color);
}

void AgentRenderer::Enqueue(const Shape_e shape, const RenderPosition& pos, const float size, const Color color, const Color modifier)
{
    if ((color & IM_COL32_A_MASK) == 0) return;
    const auto& shape_verts = shapes[shape].vertices;
    vertices.reserve(vertices.size() + shape_verts.size());

    const DWORD c_dark = Colors::Sub(color, modifier);
    const DWORD c_light = Colors::Add(color, modifier);
    const DWORD c_center = Colors::Sub(color, IM_COL32(0, 0, 0, 50));

    GW::Vec2f calc_pos;
    for (const Shape_Vertex& vert : shape_verts) {
        calc_pos.x = ((vert.x * pos.rotation_cos) - (vert.y * pos.rotation_sin)) * size + pos.position.x;
        calc_pos.y = ((vert.x * pos.rotation_sin) + (vert.y * pos.rotation_cos)) * size + pos.position.y;
        DWORD c;
        switch (vert.modifier) {
            case Dark:
                c = c_dark;
                break;
            case Light:
                c = c_light;
                break;
            case CircleCenter:
                c = c_center;
                break;
            default:
                c = color;
                break;
        }
        vertices.push_back({calc_pos.x, calc_pos.y, c});
    }
}

void AgentRenderer::BuildCustomAgentsMap()
{
    custom_agents_map.clear();
    custom_agents_by_allegiance.clear();
    for (const CustomAgent* ca : custom_agents) {
        if (ca->allegiance >= 0) {
            custom_agents_by_allegiance[ca->allegiance].push_back(ca);
            continue;
        }
        custom_agents_map[ca->modelId].push_back(ca);
    }
}

AgentRenderer::CustomAgent::CustomAgent(const ToolboxIni* ini, const char* section)
    : ui_id(++cur_ui_id)
{
    active = ini->GetBoolValue(section, VAR_NAME(active), active);
    std::snprintf(name, sizeof(name), "%s", ini->GetValue(section, VAR_NAME(name), ""));
    std::snprintf(group, sizeof(group), "%s", ini->GetValue(section, VAR_NAME(group), ""));
    modelId = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(modelId), static_cast<long>(modelId)));
    mapId = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(mapId), static_cast<long>(mapId)));
    combat_state = static_cast<CombatState>(ini->GetLongValue(section, VAR_NAME(combat_state), static_cast<long>(combat_state)));
    weapon_state = static_cast<WeaponState>(ini->GetLongValue(section, VAR_NAME(weapon_state), static_cast<long>(weapon_state)));
    allegiance = static_cast<int>(ini->GetLongValue(section, VAR_NAME(allegiance), allegiance));
    dead_state = static_cast<DeadState>(ini->GetLongValue(section, VAR_NAME(dead_state), static_cast<long>(dead_state)));
    quest_state = static_cast<QuestState>(ini->GetLongValue(section, VAR_NAME(quest_state), static_cast<long>(quest_state)));
    is_default = ini->GetBoolValue(section, VAR_NAME(is_default), is_default);

    color = Colors::Load(ini, section, VAR_NAME(color), color);
    color_text = Colors::Load(ini, section, VAR_NAME(color_text), color_text);
    const int s = ini->GetLongValue(section, VAR_NAME(shape), shape);
    if (s >= 1 && s <= 4) {
        // 这是一个小技巧，因为我们过去使用 shape=0 -> 默认，现在直接转换为 Shape_e。
        // 但文件中的 shape=1 仍然是泪滴（即 Shape_e::Tear == 0）。
        shape = static_cast<Shape_e>(s - 1);
    }
    size = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size), size));

    color_active = ini->GetBoolValue(section, VAR_NAME(color_active), color_active);
    color_text_active = ini->GetBoolValue(section, VAR_NAME(color_text_active), color_text_active);
    shape_active = ini->GetBoolValue(section, VAR_NAME(shape_active), shape_active);
    size_active = ini->GetBoolValue(section, VAR_NAME(size_active), size_active);
}

AgentRenderer::CustomAgent::CustomAgent(const Settings& settings)
    : ui_id(++cur_ui_id)
{
    active = settings.active;
    std::snprintf(name, sizeof(name), "%s", settings.name.c_str());
    std::snprintf(group, sizeof(group), "%s", settings.group.c_str());
    modelId = settings.modelId;
    mapId = settings.mapId;
    combat_state = static_cast<CombatState>(settings.combat_state);
    weapon_state = static_cast<WeaponState>(settings.weapon_state);
    allegiance = settings.allegiance;
    dead_state = static_cast<DeadState>(settings.dead_state);
    quest_state = static_cast<QuestState>(settings.quest_state);
    is_default = settings.is_default;

    color = settings.color;
    color_text = settings.color_text;
    if (settings.shape >= Tear && settings.shape <= BigCircle) {
        shape = static_cast<Shape_e>(settings.shape);
    }
    size = settings.size;

    color_active = settings.color_active;
    color_text_active = settings.color_text_active;
    shape_active = settings.shape_active;
    size_active = settings.size_active;
}

AgentRenderer::CustomAgent::CustomAgent(const DWORD model_id, const Color _color, const char* _name)
    : ui_id(++cur_ui_id)
{
    modelId = model_id;
    color = _color;
    std::snprintf(name, _countof(name), "%s", _name);
    active = true;
}

AgentRenderer::CustomAgent::Settings AgentRenderer::CustomAgent::ToSettings() const
{
    Settings settings;
    settings.active = active;
    settings.name = name;
    settings.group = group;
    settings.modelId = modelId;
    settings.mapId = mapId;
    settings.combat_state = combat_state;
    settings.weapon_state = weapon_state;
    settings.allegiance = allegiance;
    settings.dead_state = dead_state;
    settings.quest_state = quest_state;
    settings.is_default = is_default;

    settings.color = color;
    settings.color_text = color_text;
    settings.shape = shape;
    settings.size = size;

    settings.color_active = color_active;
    settings.color_text_active = color_text_active;
    settings.shape_active = shape_active;
    settings.size_active = size_active;
    return settings;
}

bool AgentRenderer::CustomAgent::DrawHeader()
{
    ImGui::SameLine(0, 18);
    bool changed = ImGui::Checkbox("##visible", &active);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float button_width = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x;
    ImGui::SameLine();
    float cursor_pos = ImGui::GetCursorPosX();
    if (color_active) {
        changed |= ImGui::ColorButtonPicker("##color", &color);
        if (ImGui::IsItemHovered()) {
            const ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
            ImGui::ColorTooltip("小地图颜色##color_tooltip", &col.x, 0);
        }

        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(cursor_pos += button_width);
    if (color_text_active) {
        changed |= ImGui::ColorButtonPicker("##color_text", &color_text);
        if (ImGui::IsItemHovered()) {
            const ImVec4 col = ImGui::ColorConvertU32ToFloat4(color_text);
            ImGui::ColorTooltip("名字标签颜色##color_tooltip", &col.x, 0);
        }
        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(cursor_pos += button_width);
    ImGui::Text(name);
    return changed;
}

bool AgentRenderer::CustomAgent::DrawSettings(Operation& op)
{
    bool changed = false;

    if (ImGui::TreeNodeEx("##params", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap)) {
        ImGui::PushID(static_cast<int>(ui_id));

        changed |= DrawHeader();

        if (ImGui::Checkbox("##visible2", &active)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("此自定义单位是否激活");
        }
        ImGui::SameLine();
        const float x = ImGui::GetCursorPosX();
        if (ImGui::InputText("名称", name, 128)) {
            changed = true;
        }
        ImGui::ShowHelp("帮助你记住此配置的名称。可选。");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputText("组", group, sizeof(group))) {
            changed = true;
        }
        ImGui::ShowHelp("用于筛选此列表的可选标签，例如“刷图”或“首领”。纯粹用于组织。");
        ImGui::SetCursorPosX(x);
        static const char* allegiance_items[] = {"模型 ID（下方）", "盟友", "中立", "敌对", "灵/宠物", "仆从", "NPC/小宠物"};
        int allegiance_combo = allegiance < 0 ? 0 : allegiance;
        if (ImGui::Combo("匹配方式", &allegiance_combo, allegiance_items, 7)) {
            allegiance = allegiance_combo == 0 ? -1 : allegiance_combo;
            changed = true;
        }
        ImGui::ShowHelp("按模型 ID 匹配特定单位，或按阵营匹配每个单位（用于种子默认行）。");
        if (allegiance >= 0) {
            ImGui::SetCursorPosX(x);
            static const char* dead_state_items[] = {"死亡", "存活", "任意"};
            if (ImGui::Combo("死亡状态", (int*)&dead_state, dead_state_items, 3)) {
                changed = true;
            }
            ImGui::SetCursorPosX(x);
            static const char* quest_state_items[] = {"任务给予者", "非任务给予者", "任意"};
            if (ImGui::Combo("任务状态", (int*)&quest_state, quest_state_items, 3)) {
                changed = true;
            }
        }
        ImGui::SetCursorPosX(x);
        ImGui::BeginDisabled(allegiance >= 0);
        if (ImGui::InputInt("模型 ID", (int*)&modelId)) {
            op = Operation::ModelIdChange;
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::ShowHelp("应用此自定义属性的单位。按上方阵营匹配时忽略。");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputInt("地图 ID", (int*)&mapId)) {
            changed = true;
        }
        ImGui::ShowHelp("应用的地图。可选。留 0 表示任意地图");
        ImGui::SetCursorPosX(x);
        static const char* combat_state_items[] = {"战斗中", "非战斗中", "任意"};
        if (ImGui::Combo("战斗状态", (int*)&combat_state, combat_state_items, 3)) {
            changed = true;
        }
        ImGui::ShowHelp("要求单位处于特定的战斗姿态");
        ImGui::SetCursorPosX(x);
        static const char* weapon_state_items[] = {"持有武器", "无武器", "任意"};
        if (ImGui::Combo("武器状态", (int*)&weapon_state, weapon_state_items, 3)) {
            changed = true;
        }
        ImGui::ShowHelp("要求单位持有武器");

        ImGui::Spacing();

        if (ImGui::Checkbox("##color_active", &color_active)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("如果取消勾选，将使用默认颜色");
        }
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("颜色", &color, 0)) {
            changed = true;
        }
        ImGui::ShowHelp("此单位的自定义颜色。");

        if (ImGui::Checkbox("##color_text_active", &color_text_active)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("如果取消勾选，将使用默认颜色");
        }
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("文字颜色", &color_text, 0)) {
            changed = true;
        }
        ImGui::ShowHelp("此单位的自定义文字颜色。");

        if (ImGui::Checkbox("##size_active", &size_active)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("如果取消勾选，将使用默认大小");
        }
        ImGui::SameLine();
        if (ImGui::DragFloat("大小", &size, 1.0f, 0.0f, 200.0f)) {
            changed = true;
        }
        ImGui::ShowHelp("此单位的大小。");

        if (ImGui::Checkbox("##shape_active", &shape_active)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("如果取消勾选，将使用默认形状");
        }
        ImGui::SameLine();
        static const char* items[] = {"泪滴", "圆形", "方形", "大圆形"};
        if (ImGui::Combo("形状", (int*)&shape, items, 4)) {
            changed = true;
        }
        ImGui::ShowHelp("此单位的形状。");

        ImGui::Spacing();

        // === 移动和删除按钮 ===
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float width = (ImGui::CalcItemWidth() - spacing * 2) / 3;
        if (ImGui::Button("上移", ImVec2(width, 0))) {
            op = Operation::MoveUp;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("在列表中上移此颜色");
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("下移", ImVec2(width, 0))) {
            op = Operation::MoveDown;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("在列表中下移此颜色");
        }
        ImGui::SameLine(0, spacing);
        ImGui::BeginDisabled(is_default);
        if (ImGui::Button("删除", ImVec2(width, 0))) {
            ImGui::OpenPopup("删除颜色？");
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(is_default ? "默认行无法删除；取消勾选“激活”以禁用此配置" : "删除此颜色");
        }

        if (ImGui::BeginPopupModal("删除颜色？", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("确定吗？\n此操作不可撤销\n\n");
            if (ImGui::Button("确定", ImVec2(120, 0))) {
                op = Operation::Delete;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("取消", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::TreePop();
        ImGui::PopID();
    }
    else {
        changed |= DrawHeader();
    }
    return changed;
}
