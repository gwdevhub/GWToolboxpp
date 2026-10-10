#include "stdafx.h"

#include <unordered_set>

#include <GWCA/Context/GuildContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Friendslist.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Constants/EncStrings.h>

#include <Modules/Resources.h>
#include <Windows/AgentAppearanceWindow.h>

#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>
#include <Utils/SettingsDoc.h>

constexpr auto AGENTCOLOR_INIFILENAME = L"AgentColors.ini";
constexpr auto AGENTCOLOR_JSONFILENAME = L"AgentColors.json";

namespace {

    constexpr const char* profession_names[] = {"Any", "Warrior", "Ranger", "Monk", "Necromancer", "Mesmer", "Elementalist", "Assassin", "Ritualist", "Paragon", "Dervish"};
    constexpr const char* allegiance_names[] = {"Any", "Ally", "Neutral", "Enemy", "Spirit/Pet", "Minion", "NPC/Minipet"};

    uint32_t StateMaskFromLegacy(const int state)
    {
        return state >= 0 && state < 2 ? 1u << state : 0;
    }

    GW::HookEntry ChatCmd_HookEntry;
    GW::Constants::Profession GetAgentProfession(const GW::AgentLiving* agent)
    {
        if (!agent) {
            return GW::Constants::Profession::None;
        }
        if (agent->primary != GW::Constants::ProfessionByte::None) {
            return static_cast<GW::Constants::Profession>(agent->primary);
        }
        const GW::NPC* npc = GW::Agents::GetNPCByID(agent->player_number);
        if (!npc) {
            return GW::Constants::Profession::None;
        }
        return static_cast<GW::Constants::Profession>(npc->primary);
    }

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

    std::map<uint32_t, MarkedTarget*> marked_targets;
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

    bool hooks_added = false;

    void OnAgentAdded(const uint32_t agent_id)
    {
        AgentAppearanceWindow::InvalidateAppearance(agent_id);
        const auto marked_target = GetMarkedTarget(agent_id);
        if (!marked_target) {
            return;
        }
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        if (!marked_target->Matches(agent)) {
            RemoveMarkedTarget(agent_id);
        }
    }
}

unsigned int AgentAppearanceWindow::CustomAgent::cur_ui_id = 0;
std::array<AgentAppearanceWindow::RuleBucket, 6> AgentAppearanceWindow::rule_buckets;

void AgentAppearanceWindow::Show()
{
    visible = true;
    pending_focus = true;
}

void AgentAppearanceWindow::Draw(IDirect3DDevice9*)
{
    if (visible) {
        ImGui::SetNextWindowSize(ImVec2(650.f, 650.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(650.f, 450.f), ImVec2(FLT_MAX, FLT_MAX));
        if (pending_focus) {
            ImGui::SetNextWindowFocus();
            ImGui::SetNextWindowCollapsed(false);
            pending_focus = false;
        }
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            DrawSettings();
        }
        ImGui::End();
    }
    DrawRuleEditor();
}

bool AgentAppearanceWindow::IsMarked(const uint32_t agent_id)
{
    return GetMarkedTarget(agent_id) != nullptr;
}

void AgentAppearanceWindow::ResetAppearanceCache()
{
    matcher_context_valid = false;
    match_cache.clear();
    pending_names.clear();
    RemoveMarkedTarget();
}

void AgentAppearanceWindow::GetAgentAppearanceRules(std::vector<AppearanceRule*>& out)
{
    out.assign(custom_agents.begin(), custom_agents.end());
}

bool AgentAppearanceWindow::GetAgentAppearance(const GW::Agent* agent, Shape_e* shape_out, Color* color_out,
    Color* border_color_out, float* border_thickness_out, Color* text_color_out, float* size_out,
    std::vector<const AppearanceRule*>* matched_rules_out)
{
    if (matched_rules_out) matched_rules_out->clear();
    if (!agent || (!shape_out && !color_out && !border_color_out && !border_thickness_out && !text_color_out && !size_out && !matched_rules_out)) {
        return false;
    }
    const auto requested_color_out = color_out;
    const AppearanceRule* color_rule = nullptr;
    if (border_thickness_out) {
        const auto is_target = agent->agent_id == GW::Agents::GetTargetId() || agent->agent_id == auto_target_id;
        *border_thickness_out = is_target ? target_border_thickness : agent_border_thickness;
    }

    if (!shape_out && !color_out && !border_color_out && !text_color_out && !size_out && !matched_rules_out) return false;
    const auto* matches = GetAppearanceRules(agent);
    if (matched_rules_out && matches) matched_rules_out->assign(matches->begin(), matches->end());
    if (!shape_out && !color_out && !border_color_out && !text_color_out && !size_out) return false;
    bool overridden = false;
    if (matches) for (const auto* rule : *matches) {
        if (shape_out && rule->shape != Shape_None) {
            *shape_out = rule->shape;
            shape_out = nullptr;
            overridden = true;
        }
        if (color_out && rule->override_color) {
            *color_out = rule->color;
            color_rule = rule;
            color_out = nullptr;
            overridden = true;
        }
        if (border_color_out && rule->override_border_color) {
            *border_color_out = rule->border_color;
            border_color_out = nullptr;
            overridden = true;
        }
        if (text_color_out && rule->override_text_color) {
            *text_color_out = rule->color_text;
            text_color_out = nullptr;
            overridden = true;
        }
        if (size_out && rule->scale > 0.f) {
            *size_out = GetBaseSize() * rule->scale;
            size_out = nullptr;
            overridden = true;
        }
        if (!shape_out && !color_out && !border_color_out && !text_color_out && !size_out) break;
    }
    if (shape_out) *shape_out = default_shape;
    if (color_out) *color_out = color_default;
    if (border_color_out) *border_color_out = color_default;
    if (text_color_out) *text_color_out = color_default;
    if (size_out) *size_out = GetBaseSize();
    if (requested_color_out) {
        const auto* living = agent->GetAsAgentLiving();
        if (color_rule && living && !living->GetIsDead() && living->allegiance == GW::Constants::Allegiance::Enemy
            && living->hp <= 0.9f && color_rule->target_state != Marked) {
            *requested_color_out = Colors::Sub(*requested_color_out, color_agent_damaged_modifier);
        }
        const auto* dead_npc = living && living->GetIsDead() && living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
        if (dead_npc && (dead_npc->model_file_id == 0x22A34 || dead_npc->model_file_id == 0x2D0E4 || dead_npc->model_file_id == 0x2D07E)) {
            *requested_color_out = IM_COL32(0, 0, 0, 0);
        }
    }
    return overridden;
}

AgentAppearanceWindow::Shape_e AgentAppearanceWindow::GetShape(const GW::Agent* agent)
{
    auto shape = Shape_None;
    GetAgentAppearance(agent, &shape);
    return shape;
}

Color AgentAppearanceWindow::GetColor(const GW::Agent* agent)
{
    auto color = Color{0};
    GetAgentAppearance(agent, nullptr, &color);
    return color;
}

float AgentAppearanceWindow::GetSize(const GW::Agent* agent)
{
    auto size = 0.f;
    GetAgentAppearance(agent, nullptr, nullptr, nullptr, nullptr, nullptr, &size);
    return size;
}

void AgentAppearanceWindow::RegisterSettings(ToolboxModule* module)
{
    const std::pair<const char*, Color*> colors[] = {
        {"color_agent_modifier", &color_agent_modifier},
        {"color_agent_damaged_modifier", &color_agent_damaged_modifier},
        {"color_default", &color_default},
    };
    for (const auto& [key, color] : colors) {
        // SettingColor is layout-compatible with Color; the cast lets the registry persist it as a hex string
        SettingsRegistry::RegisterField(module, key, reinterpret_cast<Colors::SettingColor*>(color));
    }
    SettingsRegistry::RegisterField(module, "custom_agent_defaults_seeded", &custom_agent_defaults_seeded);
    SettingsRegistry::RegisterField(module, "appearance_defaults_seeded", &appearance_defaults_seeded);
    SettingsRegistry::RegisterField(module, "spirit_defaults_seeded", &spirit_defaults_seeded);
    SettingsRegistry::RegisterField(module, "size_default", &size_default);
    SettingsRegistry::RegisterField(module, "agent_border_thickness", &agent_border_thickness);
    SettingsRegistry::RegisterField(module, "target_border_thickness", &target_border_thickness);
    SettingsRegistry::RegisterField(module, "default_shape", reinterpret_cast<int*>(&default_shape));
    if (!hooks_added) {
        hooks_added = true;
        GW::UI::RegisterUIMessageCallback(&UIMsg_Entry, GW::UI::UIMessage::kMapLoaded, OnUIMessage);
        // Gw.exe sends kAgentUpdate (wparam = agent_id) from its agent "created" event handler, i.e. when an agent is added
        GW::UI::RegisterUIMessageCallback(&UIMsg_Entry, GW::UI::UIMessage::kAgentUpdate, OnUIMessage);
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"marktarget", CmdMarkTarget);
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"clearmarktarget", CmdClearMarkTarget);
    }
}

void AgentAppearanceWindow::RegisterMinimapSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "show_hidden_npcs", &show_hidden_npcs);
#ifdef _DEBUG
    SettingsRegistry::RegisterField(module, "show_props_on_minimap", &show_props_on_minimap);
#endif
}

void AgentAppearanceWindow::LoadCustomAgents(SettingsDoc& doc, ToolboxIni* legacy)
{
    custom_agents_loaded = false;
    matcher_context_valid = false;
    spirit_defaults_seeded = false;
    match_cache.clear();
    pending_names.clear();
    std::vector<AppearanceRule*> rules;
    GetAgentAppearanceRules(rules);
    for (const auto* ca : rules) {
        delete ca;
    }
    custom_agents.clear();
    size_default = GetBaseSize();
    doc.Get("Game Settings", "spirit_defaults_seeded", spirit_defaults_seeded);

    const auto append_rule = [](CustomAgent* rule, const bool was_seeded = false) {
        const auto append = [](CustomAgent* entry) {
            entry->index = custom_agents.size();
            custom_agents.push_back(entry);
        };
        if (static_cast<int>(rule->agent_type) != 0) {
            append(rule);
            return;
        }
        if (rule->allegiance >= 0) {
            rule->agent_type = NPC;
            if (was_seeded) rule->dead_states = 1u << Alive;
            append(rule);
            return;
        }
        const auto old_rule = rule->ToSettings();
        rule->agent_type = NPC;
        rule->identifier = rule->modelId;
        rule->modelId = 0;
        append(rule);
        auto* gadget_rule = new CustomAgent(old_rule);
        gadget_rule->agent_type = Gadget;
        gadget_rule->identifier = gadget_rule->modelId;
        gadget_rule->modelId = 0;
        append(gadget_rule);
    };

    if (!doc.Has("Game Settings", "custom_agent_defaults_seeded")) {
        if (!doc.Get("Minimap", "custom_agent_defaults_seeded", custom_agent_defaults_seeded) && legacy) {
            custom_agent_defaults_seeded = legacy->GetBoolValue("Minimap", "custom_agent_defaults_seeded", false);
        }
    }
    if (!doc.Has("Game Settings", "appearance_defaults_seeded")) {
        if (!doc.Get("Minimap", "appearance_defaults_seeded", appearance_defaults_seeded) && legacy) {
            appearance_defaults_seeded = legacy->GetBoolValue("Minimap", "appearance_defaults_seeded", false);
        }
    }

    std::vector<CustomAgent::Settings> saved;
    const auto rules_section = doc.Has("Game Settings", "appearance_rules") ? "Game Settings" : "Minimap";
    if (doc.Has(rules_section, "appearance_rules")) {
        if (!doc.Get(rules_section, "appearance_rules", saved)) {
            Log::Error("Failed to parse appearance rules in %s", rules_section);
            return;
        }
        int rules_version = 0;
        doc.Get(rules_section, "appearance_rules_version", rules_version);
        std::vector<CustomAgent::LegacyFlags> flags;
        if (rules_version < 3 && (!doc.Get(rules_section, "appearance_rules", flags) || flags.size() != saved.size())) {
            Log::Error("Failed to migrate appearance rules in %s", rules_section);
            return;
        }
        for (size_t i = 0; i < saved.size(); ++i) {
            auto* rule = new CustomAgent(saved[i]);
            if (rules_version < 2) rule->ApplyLegacyFlags(flags[i]);
            append_rule(rule, rules_version < 3 && flags[i].is_default);
        }
        RebuildRuleMatchers();
        custom_agents_loaded = true;
        if (!appearance_defaults_seeded) {
            SeedAppearanceDefaults(doc, legacy);
            appearance_defaults_seeded = true;
        }
        SeedSpiritDefaults(doc);
        legacy_rule_colors.clear();
        legacy_rule_sizes.clear();
        legacy_rule_scales.clear();
        legacy_rule_shapes.clear();
        return;
    }

    const auto json_path = Resources::GetSettingFile(AGENTCOLOR_JSONFILENAME);
    std::error_code ec;
    if (std::filesystem::exists(json_path, ec)) {
        std::ifstream file(json_path, std::ios::binary);
        const std::string json_buf{std::istreambuf_iterator(file), {}};
        std::vector<CustomAgent::Settings> entries;
        std::vector<CustomAgent::LegacyFlags> flags;
        if (!file || glz::read<glz::opts{.error_on_unknown_keys = false}>(entries, json_buf) ||
            glz::read<glz::opts{.error_on_unknown_keys = false}>(flags, json_buf) || flags.size() != entries.size()) {
            Log::Error("Failed to parse AgentColors.json");
            return;
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            auto* rule = new CustomAgent(entries[i]);
            rule->ApplyLegacyFlags(flags[i]);
            append_rule(rule, flags[i].is_default);
        }
    }
    else {
        ToolboxIni inifile;
        ASSERT(inifile.LoadIfExists(Resources::GetLegacySettingFile(AGENTCOLOR_INIFILENAME)) == SI_OK);

        TNamesDepend entries;
        inifile.GetAllSections(entries);

        for (const auto& entry : entries) {
            append_rule(new CustomAgent(&inifile, entry.pItem), inifile.GetBoolValue(entry.pItem, "is_default", false));
        }
    }
    RebuildRuleMatchers();
    custom_agents_loaded = true;

    if (!custom_agent_defaults_seeded) {
        SeedDefaultCustomAgents();
        custom_agent_defaults_seeded = true;
    }
    if (!appearance_defaults_seeded) {
        SeedAppearanceDefaults(doc, legacy);
        appearance_defaults_seeded = true;
    }
    SeedSpiritDefaults(doc);
    legacy_rule_colors.clear();
    legacy_rule_sizes.clear();
    legacy_rule_scales.clear();
    legacy_rule_shapes.clear();
}

void AgentAppearanceWindow::SeedSpiritDefaults(const SettingsDoc& doc)
{
    if (spirit_defaults_seeded) return;
    struct SpiritDefault {
        uint32_t identifier;
        const char* color_key;
        Color color;
    };
    const SpiritDefault defaults[] = {
        {GW::Constants::ModelID::EoE, "color_eoe", 0x3200FF00},
        {GW::Constants::ModelID::QZ, "color_qz", 0x320000FF},
        {GW::Constants::ModelID::Winnowing, "color_winnowing", 0x3200FFFF},
        {GW::Constants::ModelID::FrozenSoil, "color_frozen_soil", 0x00FEFFFF},
        {GW::Constants::ModelID::Symbiosis, "color_symbiosis", 0x00FF00FF}
    };
    std::vector<AppearanceRule*> spirits;
    for (const auto& entry : defaults) {
        Colors::SettingColor color(LegacyRuleColor(entry.color_key, entry.color));
        doc.Get("Game Settings", entry.color_key, color);
        auto* rule = new AppearanceRule(0, color.value, "");
        rule->active = Colors::IsVisible(color.value);
        rule->override_color = true;
        rule->agent_type = NPC;
        rule->identifier = entry.identifier;
        rule->dead_states = 1u << Alive;
        rule->shape = BigCircle;
        rule->scale = ScaleFromAbsolute(GW::Constants::Range::SpiritExtended);
        rule->stop_processing_rules = true;
        std::snprintf(rule->group, sizeof(rule->group), "Defaults");
        spirits.push_back(rule);
    }
    const auto first_default = std::ranges::find_if(custom_agents, [](const AppearanceRule* rule) { return std::strcmp(rule->group, "Defaults") == 0; });
    custom_agents.insert(first_default, spirits.begin(), spirits.end());
    for (size_t i = 0; i < custom_agents.size(); ++i) custom_agents[i]->index = i;
    spirit_defaults_seeded = true;
    RebuildRuleMatchers();
}

void AgentAppearanceWindow::SeedAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy)
{
    const auto add = [](const AgentType agent_type, const std::optional<Color> color, const float scale, const Shape_e shape, const int allegiance = -1, const DeadState dead = EitherDeadState) {
        auto* rule = new CustomAgent(0, color.value_or(0), "");
        rule->override_color = color.has_value();
        rule->agent_type = agent_type;
        rule->allegiance = allegiance;
        rule->dead_states = StateMaskFromLegacy(dead);
        rule->scale = scale;
        rule->shape = shape;
        std::snprintf(rule->group, sizeof(rule->group), "Defaults");
        rule->index = custom_agents.size();
        custom_agents.push_back(rule);
        return rule;
    };
    auto* target = add(Any, std::nullopt, 0.f, Shape_None);
    target->target_state = Targeted;
    target->border_color = LegacyRuleColor("color_target", 0xFFFFFF00);
    target->override_border_color = true;
    add(Any, LegacyRuleColor("color_marked_target", 0xFFFFFC00), LegacyRuleScale("size_marked_target", 1.f), Shape_None)->target_state = Marked;
    auto* boss = add(NPC, std::nullopt, LegacyRuleScale("size_boss", 1.25f), Shape_None);
    boss->boss_states = 1u;
    add(NPC, LegacyRuleColor("color_hostile_dead", 0xFF320000), LegacyRuleScale("size_hostile", 1.f), Shape_None, static_cast<int>(GW::Constants::Allegiance::Enemy), Dead);
    using GW::Constants::Profession;
    const auto palette = DefaultProfessionColors();
    for (size_t i = 1; i < _countof(profession_names); ++i) {
        const auto profession = static_cast<Profession>(i);
        auto key = std::string("color_profession_") + profession_names[i];
        std::ranges::transform(key, key.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        auto* rule = add(NPC, LegacyRuleColor(key.c_str(), palette[i]), 0.f, Shape_None, static_cast<int>(GW::Constants::Allegiance::Enemy), Alive);
        rule->profession = profession;
        rule->boss_states = legacy_boss_colors_only ? 1u : 0;
        rule->active = legacy_enemy_profession_colors;
    }
    add(NPC, LegacyRuleColor("color_hostile", 0xFFF00000), LegacyRuleScale("size_hostile", 1.f), Shape_None, static_cast<int>(GW::Constants::Allegiance::Enemy), Alive);
    for (const auto allegiance : {GW::Constants::Allegiance::Ally_NonAttackable, GW::Constants::Allegiance::Npc_Minipet, GW::Constants::Allegiance::Spirit_Pet, GW::Constants::Allegiance::Minion}) {
        add(NPC, LegacyRuleColor("color_ally_dead", 0x64006400), LegacyRuleScale("size_ally", 1.f), Shape_None, static_cast<int>(allegiance), Dead);
        add(NPC, LegacyRuleColor("color_ally_npc_quest", 0xFF99FF99), LegacyRuleScale("size_ally_npc_quest", 1.f), Shape_None, static_cast<int>(allegiance), Alive)->quest_states = 1u << QuestGiver;
    }
    add(Item, LegacyRuleColor("color_item", 0xFF0000F0), LegacyRuleScale("size_item", .25f), Quad);
    add(Gadget, LegacyRuleColor("color_locked_chest", 0xFF0000C8), LegacyRuleScale("size_locked_chest", .5f), Quad)->gadget_state = ClosedChest;
    add(Gadget, LegacyRuleColor("color_locked_chest_open", 0xFF0000C8), LegacyRuleScale("size_locked_chest_open", .5f), Quad)->gadget_state = OpenedChest;
    add(Gadget, LegacyRuleColor("color_signpost", 0xFF0000C8), LegacyRuleScale("size_signpost", .5f), Quad)->gadget_state = OtherGadget;
    add(Player, LegacyRuleColor("color_player", 0xFFFF8000), LegacyRuleScale("size_player", 1.f), LegacyRuleShape("shape_player", Shape_None), -1, Alive)->player_relation = Self;
    add(Player, LegacyRuleColor("color_player_dead", 0x64FF8000), LegacyRuleScale("size_player", 1.f), LegacyRuleShape("shape_player", Shape_None), -1, Dead)->player_relation = Self;
    add(Player, LegacyRuleColor("color_ally", 0xFF00B300), LegacyRuleScale("size_ally", 1.f), LegacyRuleShape("shape_players", Shape_None), -1, Alive)->player_relation = Other;
    const auto tag_start = custom_agents.size();
    bool enabled = false;
    if (!doc.Get("Game Settings", "override_name_tag_colors", enabled) && legacy) {
        enabled = legacy->GetBoolValue("Game Settings", "override_name_tag_colors", false);
    }
    if (enabled) {
        const auto add_tag = [&doc, legacy, &add](const char* key, const AgentType agent_type, const int allegiance = -1, const PlayerRelation relation = AnyRelation, const Color fallback = 0) {
            Colors::SettingColor setting(fallback);
            if (!doc.Get("Game Settings", key, setting)) {
                if (legacy && legacy->KeyExists("Game Settings", key)) setting = Colors::Load(legacy, "Game Settings", key, setting.value);
                else if (!fallback) return;
            }
            auto* rule = add(agent_type, std::nullopt, 0.f, Shape_None, allegiance);
            rule->player_relation = relation;
            rule->color_text = setting.value;
            rule->override_text_color = Colors::IsVisible(setting.value);
        };
        add_tag("nametag_color_npc", NPC);
        add_tag("nametag_color_enemy", NPC, static_cast<int>(GW::Constants::Allegiance::Enemy));
        add_tag("nametag_color_gadget", Gadget);
        add_tag("nametag_color_item", Item);
        add_tag("nametag_color_player_other", Player, -1, Other);
        add_tag("nametag_color_player_self", Player, -1, Self);
        add_tag("nametag_color_player_in_my_party", Player, -1, MyParty);
        add_tag("nametag_color_player_in_party", Player, -1, InParty);
        add_tag("nametag_color_friends", Player, -1, Friend, 0xFF60FF60);
        add_tag("nametag_color_guild_members", Player, -1, Guild, 0xFFFFD060);
    }
    enabled = false;
    if (!doc.Get("Friend List", "friend_name_tag_enabled", enabled) && legacy) {
        enabled = legacy->GetBoolValue("Friend List", "friend_name_tag_enabled", false);
    }
    if (enabled) {
        Colors::SettingColor setting(0xff6060ff);
        if (!doc.Get("Friend List", "friend_name_tag_color", setting) && legacy && legacy->KeyExists("Friend List", "friend_name_tag_color")) {
            setting = Colors::Load(legacy, "Friend List", "friend_name_tag_color", setting.value);
        }
        auto* rule = add(Player, std::nullopt, 0.f, Shape_None);
        rule->player_relation = Friend;
        rule->outpost_only = true;
        rule->color_text = setting.value;
        rule->override_text_color = Colors::IsVisible(setting.value);
    }
    const auto specificity = [](const CustomAgent* rule) {
        const int relation_rank[] = {0, 24, 4, 20, 16, 12, 8};
        return (rule->outpost_only ? 32 : 0) +
            (rule->player_relation <= InParty ? relation_rank[rule->player_relation] : 0) +
            (rule->allegiance >= 0 ? 4 : 0);
    };
    std::stable_sort(custom_agents.begin() + tag_start, custom_agents.end(), [&](const CustomAgent* a, const CustomAgent* b) {
        return specificity(a) > specificity(b);
    });
    for (size_t i = tag_start; i < custom_agents.size(); ++i) custom_agents[i]->index = i;
    RebuildRuleMatchers();
}

void AgentAppearanceWindow::SeedDefaultCustomAgents()
{
    struct DefaultRow {
        GW::Constants::Allegiance allegiance;
        QuestState quest_state;
        const char* color_key;
        Color color;
        const char* size_key;
        float scale;
    };
    const DefaultRow rows[] = {
        {GW::Constants::Allegiance::Neutral, EitherQuestState, "color_neutral", 0xFF0000DC, "size_neutral", 1.f},
        {GW::Constants::Allegiance::Ally_NonAttackable, NotQuestGiver, "color_ally", 0xFF00B300, "size_ally", 1.f},
        {GW::Constants::Allegiance::Npc_Minipet, NotQuestGiver, "color_ally_npc", 0xFF99FF99, "size_ally_npc", 1.f},
        {GW::Constants::Allegiance::Spirit_Pet, NotQuestGiver, "color_ally_spirit", 0xFF608000, "size_ally_spirit", 1.f},
        {GW::Constants::Allegiance::Minion, NotQuestGiver, "color_ally_minion", 0xFF008060, "size_minion", .5f},
    };
    for (const auto& row : rows) {
        auto* ca = new CustomAgent(0, LegacyRuleColor(row.color_key, row.color), "");
        ca->override_color = true;
        ca->allegiance = static_cast<int>(row.allegiance);
        ca->agent_type = NPC;
        ca->quest_states = StateMaskFromLegacy(row.quest_state);
        ca->scale = LegacyRuleScale(row.size_key, row.scale);
        ca->dead_states = 1u << Alive;
        ca->shape = Shape_None;
        std::snprintf(ca->group, sizeof(ca->group), "Defaults");
        ca->index = custom_agents.size();
        custom_agents.push_back(ca);
    }
    RebuildRuleMatchers();
}

void AgentAppearanceWindow::SaveCustomAgents(SettingsDoc& doc)
{
    if (custom_agents_loaded) {
        std::vector<AppearanceRule*> rules;
        GetAgentAppearanceRules(rules);
        std::vector<CustomAgent::Settings> entries;
        entries.reserve(rules.size());
        for (const auto* ca : rules) {
            entries.push_back(ca->ToSettings());
        }
        doc.Set("Game Settings", "appearance_rules", entries);
        doc.Set("Game Settings", "appearance_rules_version", 4);
        doc.EraseKey("Game Settings", "fallback_size_scales");
        doc.Set("Game Settings", "custom_agent_defaults_seeded", custom_agent_defaults_seeded);
        doc.Set("Game Settings", "appearance_defaults_seeded", appearance_defaults_seeded);
        doc.Set("Game Settings", "spirit_defaults_seeded", spirit_defaults_seeded);
        for (const auto key : {"color_eoe", "color_qz", "color_winnowing", "color_frozen_soil", "color_symbiosis"}) {
            doc.EraseKey("Game Settings", key);
        }
        doc.EraseKey("Minimap", "appearance_rules");
        doc.EraseKey("Minimap", "appearance_rules_version");
        doc.EraseKey("Minimap", "custom_agent_defaults_seeded");
        doc.EraseKey("Minimap", "appearance_defaults_seeded");
        constexpr const char* migrated_keys[] = {
            "color_agent_modifier", "color_agent_damaged_modifier", "color_eoe", "color_qz", "color_winnowing",
            "color_frozen_soil", "color_symbiosis", "size_default", "default_shape",
            "agent_border_thickness", "target_border_thickness",
            "color_player", "color_player_dead", "color_signpost", "color_locked_chest", "color_locked_chest_open",
            "color_item", "color_hostile", "color_hostile_dead", "color_neutral", "color_ally", "color_ally_npc",
            "color_ally_npc_quest", "color_ally_spirit", "color_ally_minion", "color_ally_dead",
            "size_player", "size_signpost", "size_locked_chest", "size_locked_chest_open", "size_item", "size_minion",
            "size_hostile", "size_neutral", "size_ally", "size_ally_npc", "size_ally_npc_quest", "size_ally_spirit",
            "color_profession_warrior", "color_profession_ranger", "color_profession_monk", "color_profession_necromancer",
            "color_profession_mesmer", "color_profession_elementalist", "color_profession_assassin",
            "color_profession_ritualist", "color_profession_paragon", "color_profession_dervish",
            "enemies_colors_by_profession", "only_color_bosses", "marked_target_inherit_custom_agents",
            "color_marked_target", "size_marked_target", "color_target", "size_boss", "shape_player", "shape_players", "show_quest_npcs_on_minimap"
        };
        for (const auto* key : migrated_keys) {
            doc.EraseKey("Minimap", key);
            const auto control = std::strcmp(key, "color_agent_modifier") == 0 || std::strcmp(key, "color_agent_damaged_modifier") == 0
                || std::strcmp(key, "size_default") == 0 || std::strcmp(key, "default_shape") == 0
                || std::strcmp(key, "agent_border_thickness") == 0 || std::strcmp(key, "target_border_thickness") == 0;
            if (!control) doc.EraseKey("Game Settings", key);
        }
    }
}

void AgentAppearanceWindow::LoadDefaultSizes()
{
    size_default = 100.0f;
    agent_border_thickness = 0.f;
    target_border_thickness = 50.0f;
}

void AgentAppearanceWindow::ResetAppearanceSettings()
{
    LoadDefaultColors();
    LoadDefaultSizes();
    default_shape = Tear;
    legacy_rule_colors.clear();
    legacy_rule_sizes.clear();
    legacy_rule_scales.clear();
    legacy_rule_shapes.clear();
    legacy_enemy_profession_colors = true;
    legacy_boss_colors_only = true;
    custom_agent_defaults_seeded = false;
    appearance_defaults_seeded = false;
    spirit_defaults_seeded = false;
}

void AgentAppearanceWindow::LoadLegacyAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy)
{
    constexpr auto section = "Minimap";
    const char* color_keys[] = {
        "color_eoe", "color_qz", "color_winnowing", "color_frozen_soil", "color_symbiosis", "color_target", "color_player", "color_player_dead",
        "color_signpost", "color_locked_chest", "color_locked_chest_open", "color_item", "color_hostile", "color_hostile_dead", "color_neutral",
        "color_ally", "color_ally_npc", "color_ally_npc_quest", "color_ally_spirit", "color_ally_minion", "color_ally_dead", "color_marked_target",
        "color_profession_warrior", "color_profession_ranger", "color_profession_monk", "color_profession_necromancer", "color_profession_mesmer",
        "color_profession_elementalist", "color_profession_assassin", "color_profession_ritualist", "color_profession_paragon", "color_profession_dervish"
    };
    for (const auto* key : color_keys) {
        Colors::SettingColor value;
        if (doc.Get("Game Settings", key, value) || doc.Get(section, key, value)) legacy_rule_colors[key] = value.value;
        else if (legacy && legacy->KeyExists(section, key)) legacy_rule_colors[key] = Colors::Load(legacy, section, key, 0);
    }
    const std::pair<const char*, Color*> controls[] = {{"color_agent_modifier", &color_agent_modifier}, {"color_agent_damaged_modifier", &color_agent_damaged_modifier}};
    for (const auto& [key, color] : controls) {
        Colors::SettingColor value(*color);
        if (doc.Get(section, key, value)) *color = value.value;
        else if (legacy) *color = Colors::Load(legacy, section, key, *color);
    }
    const std::pair<const char*, float*> controls_sizes[] = {
        {"size_default", &size_default}, {"agent_border_thickness", &agent_border_thickness},
        {"target_border_thickness", &target_border_thickness}
    };
    for (const auto& [key, size] : controls_sizes) {
        if (!doc.Get(section, key, *size) && legacy) *size = static_cast<float>(legacy->GetDoubleValue(section, key, *size));
    }
    const char* size_keys[] = {"size_player", "size_signpost", "size_locked_chest", "size_locked_chest_open", "size_item", "size_boss",
        "size_minion", "size_marked_target", "size_hostile", "size_neutral", "size_ally", "size_ally_npc", "size_ally_npc_quest", "size_ally_spirit"};
    for (const auto* key : size_keys) {
        float value = 0.f;
        if (doc.Get(section, key, value)) legacy_rule_sizes[key] = value;
        else if (legacy && legacy->KeyExists(section, key)) legacy_rule_sizes[key] = static_cast<float>(legacy->GetDoubleValue(section, key, 0.f));
    }
    std::array<float, 14> scales{};
    if (doc.Get("Game Settings", "fallback_size_scales", scales)) {
        for (size_t i = 0; i < scales.size(); ++i) {
            if (std::isfinite(scales[i]) && scales[i] >= 0.f) legacy_rule_scales[size_keys[i]] = scales[i];
        }
    }
    if (!doc.Get(section, "enemies_colors_by_profession", legacy_enemy_profession_colors) && legacy) {
        legacy_enemy_profession_colors = legacy->GetBoolValue(section, "enemies_colors_by_profession", true);
    }
    if (!doc.Get(section, "only_color_bosses", legacy_boss_colors_only) && legacy) {
        legacy_boss_colors_only = legacy->GetBoolValue(section, "only_color_bosses", true);
    }
    for (const auto* key : {"default_shape", "shape_player", "shape_players"}) {
        auto value = static_cast<int>(Shape_None);
        if (!doc.Get(section, key, value) && legacy && legacy->KeyExists(section, key)) value = static_cast<int>(legacy->GetLongValue(section, key, Shape_None));
        if (value >= Tear && value <= Star) {
            if (std::strcmp(key, "default_shape") == 0) default_shape = static_cast<Shape_e>(value);
            else legacy_rule_shapes[key] = static_cast<Shape_e>(value);
        }
    }
}

void AgentAppearanceWindow::LoadDefaultColors()
{
    color_default = 0xFFFFFFFF;
    color_agent_modifier = 0x001E1E1E;
    color_agent_damaged_modifier = 0x00505050;
}

void AgentAppearanceWindow::DrawSettings()
{
    if (ImGui::DragFloat("Default Size", &size_default, 1.f, 1.f, 0.f, "%.0f")) {
        size_default = std::isfinite(size_default) ? std::max(1.f, size_default) : 100.f;
    }
    ImGui::ShowHelp("Base minimap marker size. Each rule's scale multiplies this value.");
    static std::array items = {"Tear", "Circle", "Square", "Big Circle", "Star"};
    ImGui::Combo("Default Shape", reinterpret_cast<int*>(&default_shape), items.data(), items.size());
    ImGui::ShowHelp("The default shape of agents.");
    Colors::DrawSettingHueWheel("Default Color", &color_default);
    ImGui::ShowHelp("Used for marker, border and name tag colours when no matching rule overrides them.");
    ImGui::SliderFloat("Agent Border thickness", &agent_border_thickness, 0.f, 100.f, "%.0f");
    ImGui::SliderFloat("Target Border thickness", &target_border_thickness, 0.f, 100.f, "%.0f");
    Colors::DrawSettingHueWheel("Agent modifier", &color_agent_modifier);
    ImGui::ShowHelp("Controls marker shading: subtracted at the border and added at the centre. Zero gives a solid colour.");
    Colors::DrawSettingHueWheel("Agent damaged modifier", &color_agent_damaged_modifier);
    ImGui::ShowHelp("Subtracted from hostile marker colours at 90% HP or below.");
    ImGui::Separator();
    static char group_filter[64] = "";
    const auto add_width = ImGui::CalcTextSize("Add").x + ImGui::GetStyle().FramePadding.x * 2.f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - add_width - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputTextWithHint("##filter", "Filter by label or group...", group_filter, sizeof(group_filter));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Only affects what's shown here. Rules are evaluated top to bottom, independently for each enabled property. A matching 'Stop after this rule' ends the lookup.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(add_width, 0.f))) {
        auto* rule = new CustomAgent(0, 0, "");
        rule->active = false;
        custom_agents.insert(custom_agents.begin(), rule);
        EditRule(rule);
        for (size_t i = 0; i < custom_agents.size(); ++i) custom_agents[i]->index = i;
        RebuildRuleMatchers();
    }
    const auto matches_filter = [](const CustomAgent* ca) {
        if (!group_filter[0]) return true;
        const auto to_lower = [](std::string s) {
            std::ranges::transform(s, s.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        };
        const auto needle = to_lower(group_filter);
        return to_lower(ca->Label()).find(needle) != std::string::npos || to_lower(ca->group).find(needle) != std::string::npos;
    };

    std::vector<AppearanceRule*> rules;
    GetAgentAppearanceRules(rules);
    const auto* target = GW::Agents::GetTarget();
    if (!target && auto_target_id) target = GW::Agents::GetAgentByID(auto_target_id);
    auto target_shape = Shape_None;
    auto target_color = Color{0};
    auto target_border_color = Color{0};
    auto target_text_color = Color{0};
    auto resolved_border_thickness = 0.f;
    auto target_size = 0.f;
    std::vector<const AppearanceRule*> target_rules;
    std::unordered_set<unsigned int> matching_rule_ids;
    size_t applied_rules = 0;
    uint32_t resolved_properties = 0;
    if (target) {
        GetAgentAppearance(target, &target_shape, &target_color, &target_border_color, &resolved_border_thickness,
            &target_text_color, &target_size, &target_rules);
        for (const auto* rule : target_rules) {
            matching_rule_ids.insert(rule->ui_id);
            const auto properties = (rule->shape != Shape_None ? 1u : 0u) | (rule->override_color ? 2u : 0u)
                | (rule->override_border_color ? 4u : 0u) | (rule->override_text_color ? 8u : 0u) | (rule->scale > 0.f ? 16u : 0u);
            if ((properties & ~resolved_properties) || rule->stop_processing_rules) ++applied_rules;
            resolved_properties |= properties;
        }
    }
    bool changed = false;
    AppearanceRule* move_rule = nullptr;
    int move_offset = 0;
    const auto footer_height = ImGui::GetTextLineHeightWithSpacing() * (target ? 6.f : 2.f) + ImGui::GetStyle().ItemSpacing.y + 1.f;
    ImGui::BeginChild("##custom_agents_scroll", ImVec2(0.f, -footer_height), true);
    if (ImGui::BeginTable("AppearanceRules", 2, ImGuiTableFlags_SizingStretchProp)) {
        const auto button_size = ImGui::GetFrameHeight();
        const auto spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::TableSetupColumn("Rule", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, button_size * 4.f + spacing * 3.f);
        for (unsigned i = 0; i < rules.size(); ++i) {
            auto* custom = rules[i];
            if (!custom || !matches_filter(custom)) continue;
            ImGui::PushID(static_cast<int>(custom->ui_id));
            ImGui::TableNextRow();
            if (matching_rule_ids.contains(custom->ui_id)) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));
            }
            ImGui::TableNextColumn();
            changed |= custom->DrawHeader();
            ImGui::TableNextColumn();
            if (ImGui::ButtonWithHint(ICON_FA_EDIT, "Edit appearance rule", ImVec2(button_size, button_size))) EditRule(custom);
            ImGui::SameLine();
            ImGui::BeginDisabled(i == 0);
            if (ImGui::ButtonWithHint(ICON_FA_ARROW_UP, "Move rule up", ImVec2(button_size, button_size))) {
                move_rule = custom;
                move_offset = -1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i + 1 == rules.size());
            if (ImGui::ButtonWithHint(ICON_FA_ARROW_DOWN, "Move rule down", ImVec2(button_size, button_size))) {
                move_rule = custom;
                move_offset = 1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::ButtonWithHint(ICON_FA_TRASH, "Delete appearance rule", ImVec2(button_size, button_size))) {
                const auto message = std::format("Delete appearance rule '{}'?\nThis cannot be undone.", custom->Label());
                ImGui::ConfirmDialog(message.c_str(), [rule_id = custom->ui_id](const bool confirmed, void*) {
                    if (!confirmed) return;
                    std::vector<AppearanceRule*> rules;
                    GetAgentAppearanceRules(rules);
                    const auto it = std::ranges::find_if(rules, [rule_id](const AppearanceRule* rule) { return rule && rule->ui_id == rule_id; });
                    if (it == rules.end()) return;
                    custom_agents.erase(custom_agents.begin() + (*it)->index);
                    delete *it;
                    rules.erase(it);
                    for (size_t j = 0; j < rules.size(); ++j) rules[j]->index = j;
                    RebuildRuleMatchers();
                });
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    if (move_rule) {
        const auto from = move_rule->index;
        const auto to = move_offset < 0 ? from - 1 : from + 1;
        std::swap(custom_agents[from], custom_agents[to]);
        custom_agents[from]->index = from;
        custom_agents[to]->index = to;
        changed = true;
    }
    if (changed) RebuildRuleMatchers();
    ImGui::SmallConfirmButton("Restore Defaults", "Replace all appearance rules with defaults?\nColours, sizes and border thickness will also be reset.\nThis cannot be undone.", [](const bool confirmed, void*) {
        if (!confirmed) return;
        match_cache.clear();
        pending_names.clear();
        std::vector<AppearanceRule*> rules;
        GetAgentAppearanceRules(rules);
        for (const auto* rule : rules) delete rule;
        custom_agents.clear();
        ResetAppearanceSettings();
        SeedDefaultCustomAgents();
        SeedAppearanceDefaults(SettingsDoc{}, nullptr);
        SeedSpiritDefaults(SettingsDoc{});
        custom_agent_defaults_seeded = true;
        appearance_defaults_seeded = true;
        custom_agents_loaded = true;
        group_filter[0] = '\0';
    });
    ImGui::Separator();
    if (!target) {
        ImGui::TextDisabled("No current target.");
        return;
    }
    ImGui::Text("Target #%u: %zu matching rules, %zu applied", target->agent_id, target_rules.size(), applied_rules);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Highlighted rows match this target. Applied rules supply the first override for a property or stop further matching.");
    }
    static constexpr const char* shape_names[] = {"Tear", "Circle", "Square", "Big Circle", "Star"};
    const auto shape_name = target_shape >= Tear && target_shape <= Star ? shape_names[target_shape] : "None";
    ImGui::Text("Size: %.1f (%.2fx)    Shape: %s", target_size, target_size / GetBaseSize(), shape_name);
    const auto draw_color_readout = [](const char* label, const Color color) {
        ImGui::PushID(label);
        const auto rgba = ImGui::ColorConvertU32ToFloat4(color);
        const auto size = ImGui::GetTextLineHeight();
        ImGui::ColorButton("##preview", rgba, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(size, size));
        ImGui::SameLine();
        ImGui::Text("%s: 0x%08X (%.0f%% alpha)", label, color, rgba.w * 100.f);
        ImGui::PopID();
    };
    draw_color_readout("Marker", target_color);
    if (target_shape == BigCircle || !Colors::IsVisible(target_color)) {
        ImGui::TextDisabled("Border: none (%s)", target_shape == BigCircle ? "Big Circle" : "transparent marker");
    }
    else {
        draw_color_readout("Target border", target_border_color);
        ImGui::SameLine();
        ImGui::Text("Width: %.1f", resolved_border_thickness);
    }
    draw_color_readout("Name tag", target_text_color);
}

void AgentAppearanceWindow::EditRule(CustomAgent* rule)
{
    std::vector<AppearanceRule*> rules;
    GetAgentAppearanceRules(rules);
    for (auto* custom : rules) {
        custom->edit_open = custom->focus_editor = custom == rule;
    }
}

void AgentAppearanceWindow::DrawRuleEditor()
{
    std::vector<AppearanceRule*> rules;
    GetAgentAppearanceRules(rules);
    bool changed = false;
    for (auto* rule : rules) {
        if (rule && rule->edit_open) {
            changed |= rule->DrawSettings();
        }
    }
    if (changed) {
        RebuildRuleMatchers();
    }
}

void AgentAppearanceWindow::ReleaseAppearanceHooks()
{
    GW::UI::RemoveUIMessageCallback(&UIMsg_Entry);
    hooks_added = false;
    ResetAppearanceCache();
    std::vector<AppearanceRule*> rules;
    GetAgentAppearanceRules(rules);
    for (const auto* ca : rules) {
        delete ca;
    }
    custom_agents.clear();
    RebuildRuleMatchers();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    custom_agents_loaded = false;
}

bool AgentAppearanceWindow::AppearanceRulesLoaded() { return custom_agents_loaded; }

Color AgentAppearanceWindow::GetProfessionColor(const GW::Constants::Profession profession)
{
    const auto index = static_cast<size_t>(profession);
    if (!index || index >= profession_rules.size()) return color_default;
    EnsureRuleMatchers();
    return profession_rules[index] ? profession_rules[index]->color : color_default;
}

void AgentAppearanceWindow::OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage msgid, void* wParam, void*)
{
    switch (msgid) {
        case GW::UI::UIMessage::kMapLoaded:
            ResetAppearanceCache();
            break;
        case GW::UI::UIMessage::kAgentUpdate:
            OnAgentAdded(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wParam)));
            break;
    }
}

const std::vector<const AgentAppearanceWindow::AppearanceRule*>* AgentAppearanceWindow::GetAppearanceRules(const GW::Agent* agent)
{
    if (!agent) return nullptr;
    RefreshMatches(agent);
    auto& matches = match_cache.at(agent->agent_id).matches;
    return matches.empty() ? nullptr : &matches;
};

void AgentAppearanceWindow::OnNameDecoded(void* context, const wchar_t* decoded)
{
    const auto token = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context));
    const auto pending = pending_names.find(token);
    if (pending == pending_names.end()) return;
    const auto [id, generation] = pending->second;
    pending_names.erase(pending);
    const auto it = match_cache.find(id);
    if (it == match_cache.end() || it->second.generation != generation || it->second.agent != GW::Agents::GetAgentByID(id)) return;
    it->second.name = TextUtils::StripTags(TextUtils::Replace(decoded ? decoded : L"", L"<brx>", L"\n"));
    it->second.valid = false;
    if (it->second.name.size()) GW::Agents::RefreshAgentNameTag(it->second.agent);
}

void AgentAppearanceWindow::EnsureRuleMatchers()
{
    if (!matcher_context_valid || matcher_map_id != static_cast<uint32_t>(GW::Map::GetMapID())
        || matcher_instance_type != static_cast<uint32_t>(GW::Map::GetInstanceType())) {
        RebuildRuleMatchers();
    }
}

void AgentAppearanceWindow::RefreshMatches(const GW::Agent* agent)
{
    EnsureRuleMatchers();
    const auto living = agent->GetAsAgentLiving();
    const auto item = agent->GetAsAgentItem();
    const auto gadget = agent->GetAsAgentGadget();
    const auto agent_type = item ? Item : gadget ? Gadget : living ? living->IsPlayer() ? Player : NPC : Any;
    const auto& bucket = rule_buckets[agent_type];
    auto& cached = match_cache[agent->agent_id];
    if (cached.agent != agent) {
        cached = {};
        cached.agent = agent;
        cached.generation = ++next_name_token;
    }
    if (bucket.rules.empty()) {
        cached.matches.clear();
        cached.valid = true;
        return;
    }
    const auto item_data = item && (bucket.checks & (CheckIdentifier | CheckName)) ? GW::Items::GetItemById(item->item_id) : nullptr;
    const auto identifier = item_data ? item_data->model_id : gadget ? gadget->gadget_id : living ? living->player_number : 0;
    const auto targeted = GW::Agents::GetTargetId() == agent->agent_id || auto_target_id == agent->agent_id;
    const auto marked = GetMarkedTarget(agent->agent_id) != nullptr;
    const auto allegiance = living ? static_cast<int>(living->allegiance) : -1;
    const auto profession = bucket.checks & CheckProfession ? GetAgentProfession(living) : GW::Constants::Profession::None;
    const auto flags = (living && living->GetIsDead() ? 1u : 0u) | (living && living->GetHasQuest() ? 2u : 0u) |
        (living && living->GetInCombatStance() ? 4u : 0u) | (targeted ? 8u : 0u) | (marked ? 256u : 0u) |
        (living && living->weapon_type != 0 && living->weapon_type != 512 ? 16u : 0u) |
        (gadget && (bucket.checks & CheckGadget) && IsLockedChest(agent) ? IsOpenedLockedChest(agent) ? 64u : 32u : 0u) |
        (living && living->GetHasBossGlow() ? 128u : 0u);
    uint32_t relations = 0;
    if (agent_type == Player) {
        if ((bucket.checks & CheckSelf) && agent->agent_id == GW::Agents::GetControlledCharacterId()) relations |= 1u;
        if (bucket.checks & (CheckFriend | CheckGuild)) {
            const auto decoded = living ? GW::PlayerMgr::GetPlayerName(living->login_number) : nullptr;
            const auto encoded = decoded ? nullptr : GW::Agents::GetAgentEncName(agent->agent_id);
            const auto player_name = decoded ? std::wstring(decoded) : encoded ? TextUtils::GetPlayerNameFromEncodedString(encoded) : std::wstring{};
            if ((bucket.checks & CheckFriend) && !player_name.empty() && GW::FriendListMgr::GetFriend(nullptr, player_name.c_str(), GW::FriendType::Friend)) relations |= 2u;
            if ((bucket.checks & CheckGuild) && !player_name.empty()) {
                if (const auto guild = GW::GetGuildContext()) {
                    for (const auto member : guild->player_roster) {
                        if (member && member->current_name[0] && player_name == member->current_name) {
                            relations |= 4u;
                            break;
                        }
                    }
                }
            }
        }
        if ((bucket.checks & CheckMyParty) && ToolboxUtils::IsAgentInMyParty(agent->agent_id)) relations |= 8u;
        if ((bucket.checks & CheckAnyParty) && ToolboxUtils::IsAgentInParty(agent->agent_id)) relations |= 16u;
    }
    if (cached.valid && cached.identifier == identifier && cached.map_id == matcher_map_id && cached.flags == flags && cached.allegiance == allegiance && cached.type == agent_type && cached.relation_flags == relations && cached.profession == profession) return;
    cached.identifier = identifier;
    cached.map_id = matcher_map_id;
    cached.flags = flags;
    cached.allegiance = allegiance;
    cached.profession = profession;
    cached.type = agent_type;
    cached.relation_flags = relations;
    cached.valid = true;
    cached.matches.clear();
    for (const auto& entry : bucket.rules) {
        const auto* rule = entry.rule;
        if (entry.identifier && entry.identifier != identifier) continue;
        if (rule->allegiance >= 0 && (!living || rule->allegiance != static_cast<int>(living->allegiance))) continue;
        if (rule->profession != GW::Constants::Profession::None && rule->profession != profession) continue;
        if ((flags & entry.required_flags) != entry.required_flags || (flags & entry.rejected_flags)) continue;
        if ((relations & entry.required_relations) != entry.required_relations || (relations & entry.rejected_relations)) continue;
        if (entry.name_pattern) {
            if (cached.name.empty()) {
                if (!cached.name_requested) {
                    const wchar_t* encoded = item_data ? item_data->single_item_name && *item_data->single_item_name ? item_data->single_item_name : item_data->name_enc : GW::Agents::GetAgentEncName(agent->agent_id);
                    if (encoded && *encoded) {
                        cached.name_requested = true;
                        const auto token = ++next_name_token;
                        pending_names.emplace(token, std::pair{agent->agent_id, cached.generation});
                        GW::UI::AsyncDecodeStr(encoded, OnNameDecoded, reinterpret_cast<void*>(static_cast<uintptr_t>(token)));
                    }
                }
                continue;
            }
            if (!entry.name_pattern->Matches(cached.name)) continue;
        }
        cached.matches.push_back(rule);
        if (rule->stop_processing_rules) break;
    }
}

bool AgentAppearanceWindow::ApplyNameTagColor(const GW::Agent* agent, Color& color)
{
    return GetAgentAppearance(agent, nullptr, nullptr, nullptr, nullptr, &color);
}

void AgentAppearanceWindow::InvalidateAppearance(const uint32_t agent_id)
{
    match_cache.erase(agent_id);
}

float AgentAppearanceWindow::GetBaseSize()
{
    return std::isfinite(size_default) && size_default > 0.f ? size_default : 100.f;
}

float AgentAppearanceWindow::ScaleFromAbsolute(const float size)
{
    const auto scale = size / GetBaseSize();
    return std::isfinite(scale) && scale > 0.f ? scale : 0.f;
}

Color AgentAppearanceWindow::LegacyRuleColor(const char* key, const Color preset)
{
    const auto found = legacy_rule_colors.find(key);
    return found == legacy_rule_colors.end() ? preset : found->second;
}

float AgentAppearanceWindow::LegacyRuleScale(const char* key, const float preset)
{
    if (const auto found = legacy_rule_scales.find(key); found != legacy_rule_scales.end()) return found->second;
    if (const auto found = legacy_rule_sizes.find(key); found != legacy_rule_sizes.end()) return ScaleFromAbsolute(found->second);
    return preset;
}

AgentAppearanceWindow::Shape_e AgentAppearanceWindow::LegacyRuleShape(const char* key, const Shape_e preset)
{
    const auto found = legacy_rule_shapes.find(key);
    return found == legacy_rule_shapes.end() ? preset : found->second;
}

void AgentAppearanceWindow::RebuildRuleMatchers()
{
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    const auto instance_type = static_cast<uint32_t>(GW::Map::GetInstanceType());
    if (matcher_context_valid && (matcher_map_id != map_id || matcher_instance_type != instance_type)) {
        match_cache.clear();
        pending_names.clear();
    }
    else {
        for (auto& [id, cached] : match_cache) {
            cached.valid = false;
            cached.matches.clear();
        }
    }
    matcher_map_id = map_id;
    matcher_instance_type = instance_type;
    matcher_context_valid = true;
    for (auto& bucket : rule_buckets) {
        bucket.rules.clear();
        bucket.checks = 0;
    }
    profession_rules.fill(nullptr);
    compiled_name_patterns.clear();
    std::vector<AppearanceRule*> rules;
    GetAgentAppearanceRules(rules);
    for (const auto* ca : rules) {
        if (ca->match_name[0]) compiled_name_patterns.emplace(ca, TextUtils::StringToWString(ca->match_name));
    }
    for (const auto* ca : rules) {
        if (!ca->active || (ca->mapId && ca->mapId != map_id)
            || (ca->outpost_only && instance_type != static_cast<uint32_t>(GW::Constants::InstanceType::Outpost))) continue;

        CompiledRule entry;
        entry.rule = ca;
        entry.identifier = ca->agent_type != Any ? ca->identifier : 0;
        if (ca->match_name[0]) {
            const auto pattern = compiled_name_patterns.find(ca);
            if (pattern == compiled_name_patterns.end() || !pattern->second.IsValid()) continue;
            entry.name_pattern = &pattern->second;
        }
        const auto profession = static_cast<size_t>(ca->profession);
        if (profession && ca->override_color && profession < profession_rules.size() && !profession_rules[profession]) {
            profession_rules[profession] = ca;
        }
        const auto add_state = [&](const uint32_t states, const uint32_t flag) {
            if (states == 1u) entry.required_flags |= flag;
            else if (states == 2u) entry.rejected_flags |= flag;
        };
        add_state(ca->dead_states, 1u);
        add_state(ca->quest_states, 2u);
        add_state(ca->boss_states, 128u);
        if (ca->combat_state == InCombat) entry.required_flags |= 4u;
        else if (ca->combat_state == NotInCombat) entry.rejected_flags |= 4u;
        if (ca->weapon_state == HasWeapon) entry.required_flags |= 16u;
        else if (ca->weapon_state == NoWeapon) entry.rejected_flags |= 16u;
        if (ca->target_state == Targeted) entry.required_flags |= 8u;
        else if (ca->target_state == NotTargeted) entry.rejected_flags |= 8u;
        else if (ca->target_state == Marked) entry.required_flags |= 256u;
        if (ca->gadget_state == ClosedChest) entry.required_flags |= 32u;
        else if (ca->gadget_state == OpenedChest) entry.required_flags |= 64u;
        else if (ca->gadget_state == OtherGadget) entry.rejected_flags |= 32u | 64u;
        switch (ca->player_relation) {
            case Self: entry.required_relations = 1u; break;
            case Other: entry.rejected_relations = 1u; break;
            case Friend: entry.required_relations = 2u; break;
            case Guild: entry.required_relations = 4u; break;
            case MyParty: entry.required_relations = 8u; break;
            case InParty: entry.required_relations = 16u; break;
            default: break;
        }
        const auto requires_living = ca->allegiance >= 0 || ca->dead_states || ca->quest_states || ca->boss_states
            || ca->profession != GW::Constants::Profession::None || ca->combat_state == InCombat || ca->combat_state == NotInCombat
            || ca->weapon_state == HasWeapon || ca->weapon_state == NoWeapon;
        for (auto type = static_cast<int>(Any); type <= Player; ++type) {
            if (ca->agent_type != Any && ca->agent_type != type) continue;
            if (requires_living && type != NPC && type != Player) continue;
            if (ca->gadget_state != AnyGadget && type != Gadget) continue;
            if (ca->player_relation != AnyRelation && type != Player) continue;
            auto& bucket = rule_buckets[type];
            bucket.rules.push_back(entry);
            if (entry.identifier) bucket.checks |= CheckIdentifier;
            if (entry.name_pattern) bucket.checks |= CheckName;
            if (ca->profession != GW::Constants::Profession::None) bucket.checks |= CheckProfession;
            if (ca->gadget_state != AnyGadget) bucket.checks |= CheckGadget;
            switch (ca->player_relation) {
                case Self:
                case Other: bucket.checks |= CheckSelf; break;
                case Friend: bucket.checks |= CheckFriend; break;
                case Guild: bucket.checks |= CheckGuild; break;
                case MyParty: bucket.checks |= CheckMyParty; break;
                case InParty: bucket.checks |= CheckAnyParty; break;
                default: break;
            }
        }
    }
}

AgentAppearanceWindow::CustomAgent::CustomAgent(const ToolboxIni* ini, const char* section)
    : ui_id(++cur_ui_id)
{
    active = ini->GetBoolValue(section, VAR_NAME(active), active);
    stop_processing_rules = ini->GetBoolValue(section, VAR_NAME(stop_processing_rules), stop_processing_rules);
    std::snprintf(name, sizeof(name), "%s", ini->GetValue(section, VAR_NAME(name), ""));
    std::snprintf(group, sizeof(group), "%s", ini->GetValue(section, VAR_NAME(group), ""));
    modelId = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(modelId), static_cast<long>(modelId)));
    mapId = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(mapId), static_cast<long>(mapId)));
    combat_state = static_cast<CombatState>(ini->GetLongValue(section, VAR_NAME(combat_state), static_cast<long>(combat_state)));
    weapon_state = static_cast<WeaponState>(ini->GetLongValue(section, VAR_NAME(weapon_state), static_cast<long>(weapon_state)));
    allegiance = static_cast<int>(ini->GetLongValue(section, VAR_NAME(allegiance), allegiance));
    dead_states = StateMaskFromLegacy(static_cast<int>(ini->GetLongValue(section, "dead_state", EitherDeadState)));
    quest_states = StateMaskFromLegacy(static_cast<int>(ini->GetLongValue(section, "quest_state", EitherQuestState)));
    agent_type = static_cast<AgentType>(ini->GetLongValue(section, VAR_NAME(agent_type), 0));
    identifier = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(identifier), identifier));
    if (!ini->GetBoolValue(section, "identifier_active", identifier != 0)) identifier = 0;
    std::snprintf(match_name, sizeof(match_name), "%s", ini->GetValue(section, VAR_NAME(match_name), ""));
    target_state = static_cast<TargetState>(ini->GetLongValue(section, VAR_NAME(target_state), target_state));
    player_relation = static_cast<PlayerRelation>(ini->GetLongValue(section, VAR_NAME(player_relation), player_relation));
    outpost_only = ini->GetBoolValue(section, VAR_NAME(outpost_only), outpost_only);
    border_color = Colors::Load(ini, section, VAR_NAME(border_color), 0xFFFFFF00);
    gadget_state = static_cast<GadgetState>(ini->GetLongValue(section, VAR_NAME(gadget_state), gadget_state));
    profession = static_cast<GW::Constants::Profession>(ini->GetLongValue(section, VAR_NAME(profession), static_cast<long>(profession)));
    boss_states = StateMaskFromLegacy(static_cast<int>(ini->GetLongValue(section, "boss_state", 0)) - 1);

    color = Colors::Load(ini, section, VAR_NAME(color), 0xFFF00000);
    color_text = Colors::Load(ini, section, VAR_NAME(color_text), 0xFFF00000);
    const int s = ini->GetLongValue(section, VAR_NAME(shape), 0);
    if (s >= 1 && s <= 4) {
        shape = static_cast<Shape_e>(s - 1);
    }
    scale = ini->KeyExists(section, "scale")
        ? static_cast<float>(ini->GetDoubleValue(section, "scale", 0.f))
        : ScaleFromAbsolute(static_cast<float>(ini->GetDoubleValue(section, "size", 0.f)));
    if (!std::isfinite(scale) || scale < 0.f) scale = 0.f;

    LegacyFlags flags;
    flags.color_active = ini->GetBoolValue(section, "color_active", flags.color_active);
    flags.color_text_active = ini->GetBoolValue(section, "color_text_active", flags.color_text_active);
    flags.shape_active = ini->GetBoolValue(section, "shape_active", flags.shape_active);
    flags.size_active = ini->GetBoolValue(section, "size_active", flags.size_active);
    flags.border_color_active = ini->GetBoolValue(section, "border_color_active", flags.border_color_active);
    ApplyLegacyFlags(flags);
}

AgentAppearanceWindow::CustomAgent::CustomAgent(const Settings& settings)
    : ui_id(++cur_ui_id)
{
    active = settings.active;
    stop_processing_rules = settings.stop_processing_rules;
    std::snprintf(name, sizeof(name), "%s", settings.name.c_str());
    std::snprintf(group, sizeof(group), "%s", settings.group.c_str());
    modelId = settings.modelId;
    mapId = settings.mapId;
    combat_state = static_cast<CombatState>(settings.combat_state);
    weapon_state = static_cast<WeaponState>(settings.weapon_state);
    allegiance = settings.allegiance;
    dead_states = settings.dead_states == UINT32_MAX ? StateMaskFromLegacy(settings.dead_state) : settings.dead_states & 3u;
    quest_states = settings.quest_states == UINT32_MAX ? StateMaskFromLegacy(settings.quest_state) : settings.quest_states & 3u;
    agent_type = static_cast<AgentType>(settings.agent_type);
    identifier = settings.identifier;
    std::snprintf(match_name, sizeof(match_name), "%s", settings.match_name.c_str());
    target_state = static_cast<TargetState>(settings.target_state);
    player_relation = static_cast<PlayerRelation>(settings.player_relation);
    outpost_only = settings.outpost_only;
    border_color = settings.border_color;
    gadget_state = static_cast<GadgetState>(settings.gadget_state);
    profession = static_cast<GW::Constants::Profession>(settings.profession);
    boss_states = settings.boss_states == UINT32_MAX ? StateMaskFromLegacy(settings.boss_state - 1) : settings.boss_states & 3u;

    color = settings.color;
    color_text = settings.color_text;
    override_color = settings.override_color.value_or(Colors::IsVisible(color));
    override_text_color = settings.override_text_color.value_or(Colors::IsVisible(color_text));
    override_border_color = settings.override_border_color.value_or(Colors::IsVisible(border_color));
    if (settings.shape >= Shape_None && settings.shape <= Star) {
        shape = static_cast<Shape_e>(settings.shape);
    }
    scale = settings.scale.value_or(ScaleFromAbsolute(settings.size));
    if (!std::isfinite(scale) || scale < 0.f) scale = 0.f;
}

void AgentAppearanceWindow::CustomAgent::ApplyLegacyFlags(const LegacyFlags& flags)
{
    override_color = flags.color_active && Colors::IsVisible(color);
    override_text_color = flags.color_text_active && Colors::IsVisible(color_text);
    override_border_color = flags.border_color_active && Colors::IsVisible(border_color);
    if (!flags.size_active) scale = 0.f;
    if (!flags.shape_active) shape = Shape_None;
    else if (shape == Shape_None) shape = Tear;
}

AgentAppearanceWindow::CustomAgent::CustomAgent(const DWORD model_id, const Color _color, const char* _name)
    : ui_id(++cur_ui_id)
{
    modelId = model_id;
    color = _color;
    override_color = Colors::IsVisible(color);
    std::snprintf(name, _countof(name), "%s", _name);
    active = true;
}

AgentAppearanceWindow::CustomAgent::Settings AgentAppearanceWindow::CustomAgent::ToSettings() const
{
    Settings settings;
    settings.active = active;
    settings.stop_processing_rules = stop_processing_rules;
    settings.name = name;
    settings.group = group;
    settings.modelId = modelId;
    settings.mapId = mapId;
    settings.combat_state = combat_state;
    settings.weapon_state = weapon_state;
    settings.allegiance = allegiance;
    settings.dead_state = dead_states == 1u ? Dead : dead_states == 2u ? Alive : EitherDeadState;
    settings.quest_state = quest_states == 1u ? QuestGiver : quest_states == 2u ? NotQuestGiver : EitherQuestState;
    settings.dead_states = dead_states;
    settings.quest_states = quest_states;
    settings.agent_type = agent_type;
    settings.identifier = identifier;
    settings.match_name = match_name;
    settings.target_state = target_state;
    settings.player_relation = player_relation;
    settings.outpost_only = outpost_only;
    settings.border_color = border_color;
    settings.gadget_state = gadget_state;
    settings.profession = static_cast<int>(profession);
    settings.boss_state = boss_states == 1u ? 1 : boss_states == 2u ? 2 : 0;
    settings.boss_states = boss_states;

    settings.color = color;
    settings.color_text = color_text;
    settings.override_color = override_color;
    settings.override_text_color = override_text_color;
    settings.override_border_color = override_border_color;
    settings.shape = shape;
    settings.scale = scale;
    settings.size = GetBaseSize() * scale;

    return settings;
}

const char* AgentAppearanceWindow::CustomAgent::AgentTypeName() const
{
    for (const auto& option : agent_type_options) {
        if (option.type == agent_type && (agent_type != Player || option.relation == player_relation)
            && (agent_type != Gadget || option.gadget == gadget_state)) {
            return option.label;
        }
    }
    return "Unknown";
}

std::string AgentAppearanceWindow::CustomAgent::DefaultLabel() const
{
    std::string label = AgentTypeName();
    if (agent_type == NPC) {
        const std::pair<uint32_t, const char*> spirits[] = {
            {GW::Constants::ModelID::EoE, "EoE"},
            {GW::Constants::ModelID::QZ, "QZ"},
            {GW::Constants::ModelID::Winnowing, "Winnowing"},
            {GW::Constants::ModelID::FrozenSoil, "Frozen Soil"},
            {GW::Constants::ModelID::Symbiosis, "Symbiosis"}
        };
        const auto spirit = std::ranges::find_if(spirits, [&](const auto& entry) { return entry.first == identifier; });
        if (spirit != std::end(spirits)) label += std::format(" ({})", spirit->second);
    }
    if (agent_type != Any && identifier) {
        label += std::format(" #{}", identifier);
    }
    std::string_view target_suffix;
    switch (target_state) {
        case Targeted:
            target_suffix = " [Target]";
            break;
        case NotTargeted:
            target_suffix = " [Not target]";
            break;
        case Marked:
            target_suffix = " [Marked]";
            break;
        default:
            break;
    }
    const auto append_detail = [&](const std::string_view detail) {
        const auto reserved = target_suffix.size() + (match_name[0] ? 11 : 0);
        if (!detail.empty() && label.size() + detail.size() + 1 + reserved <= 64) {
            label += ' ';
            label += detail;
        }
    };
    if (allegiance > 0 && static_cast<size_t>(allegiance) < _countof(allegiance_names)) {
        append_detail(allegiance_names[allegiance]);
    }
    const auto profession_index = static_cast<size_t>(profession);
    if (profession_index > 0 && profession_index < _countof(profession_names)) {
        append_detail(profession_names[profession_index]);
    }
    if (boss_states == 1u) append_detail("Boss");
    else if (boss_states == 2u) append_detail("Not boss");
    else if (boss_states == 3u) append_detail("Boss/Not boss");
    if (dead_states == 1u) append_detail("Dead");
    else if (dead_states == 2u) append_detail("Alive");
    else if (dead_states == 3u) append_detail("Dead/Alive");
    if (quest_states == 1u) append_detail("Quest");
    else if (quest_states == 2u) append_detail("No quest");
    else if (quest_states == 3u) append_detail("Quest/No quest");
    if (outpost_only) append_detail("Outpost");
    if (override_text_color) append_detail("Name tag");
    const auto fixed_length = label.size() + target_suffix.size() + 3;
    const auto match_limit = fixed_length < 64 ? std::min(size_t{24}, 64 - fixed_length) : 0;
    if (match_name[0] && match_limit >= 3) {
        std::string match = match_name;
        if (match.size() > match_limit) {
            auto end = match_limit - 3;
            while (end > 0 && (static_cast<unsigned char>(match[end]) & 0xc0u) == 0x80u) {
                --end;
            }
            match.resize(end);
            match += "...";
        }
        label += std::format(" \"{}\"", match);
    }
    label += target_suffix;
    return label;
}

std::string AgentAppearanceWindow::CustomAgent::Label() const
{
    return name[0] ? std::string(name) : DefaultLabel();
}

bool AgentAppearanceWindow::CustomAgent::DrawHeader()
{
    const auto changed = ImGui::Checkbox("##visible", &active);
    const auto draw_swatch = [this](const char* id, const Color fill, const Color border, const char* hint) {
        ImGui::SameLine();
        const auto size = ImGui::GetTextLineHeight();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
        if (ImGui::InvisibleButton(id, ImVec2(size, size))) {
            AgentAppearanceWindow::EditRule(this);
        }
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        auto* draw_list = ImGui::GetWindowDrawList();
        ImGui::RenderColorRectWithAlphaCheckerboard(draw_list, min, max, ImGui::GetColorU32(fill),
            std::max(2.f, size * 0.25f), ImVec2(0.f, 0.f));
        const auto thickness = Colors::IsVisible(border) ? std::max(1.f, size * 0.1f) : 1.f;
        const auto inset = thickness * 0.5f;
        const auto outline = Colors::IsVisible(border) ? ImGui::GetColorU32(border) : ImGui::GetColorU32(ImGuiCol_Border);
        draw_list->AddRect(ImVec2(min.x + inset, min.y + inset), ImVec2(max.x - inset, max.y - inset), outline, 0.f, ImDrawFlags_None, thickness);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nClick to edit appearance rule.", hint);
        }
    };
    draw_swatch("##color", override_color ? color : 0, override_border_color ? border_color : 0, "Minimap marker colour and target border colour");
    if (override_text_color) {
        draw_swatch("##color_text", color_text, 0, "Name tag colour");
    }
    ImGui::SameLine();
    const auto label = Label();
    if (name[0]) {
        ImGui::Text("%s [%s]", label.c_str(), AgentTypeName());
    }
    else {
        ImGui::TextUnformatted(label.c_str());
    }
    if (group[0]) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", group);
    }
    return changed;
}

bool AgentAppearanceWindow::CustomAgent::DrawSettings()
{
    bool changed = false;
    const auto title = std::format("Edit Appearance Rule: {}###appearance_rule_editor", Label());
    ImGui::SetNextWindowSizeConstraints(ImVec2(600.f, 0.f), ImVec2(FLT_MAX, FLT_MAX));
    if (focus_editor) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowCollapsed(false);
        focus_editor = false;
    }
    if (ImGui::Begin(title.c_str(), &edit_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushID(static_cast<int>(ui_id));

        if (ImGui::Checkbox("##visible2", &active)) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If this custom agent is active");
        }
        ImGui::SameLine();
        const float x = ImGui::GetCursorPosX();
        const auto default_label = DefaultLabel();
        if (ImGui::InputTextWithHint("Label", default_label.c_str(), name, sizeof(name))) {
            changed = true;
        }
        ImGui::ShowHelp("An optional label for this rule. Leave empty to generate a short label from its type and filters.");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputText("Group", group, sizeof(group))) {
            changed = true;
        }
        ImGui::ShowHelp("An optional tag to filter this list by, e.g. 'Farming' or 'Bosses'. Purely organisational.");
        ImGui::SetCursorPosX(x);
        changed |= ImGui::CheckboxWithHelp("Stop after this rule", &stop_processing_rules,
            "When this rule matches, do not evaluate any later appearance rules.");
        ImGui::SetCursorPosX(x);
        if (ImGui::BeginCombo("Agent type", AgentTypeName())) {
            for (const auto& option : agent_type_options) {
                const auto selected = option.type == agent_type && (agent_type != Player || option.relation == player_relation)
                    && (agent_type != Gadget || option.gadget == gadget_state);
                if (ImGui::Selectable(option.label, selected)) {
                    if (agent_type != option.type) {
                        agent_type = option.type;
                        allegiance = -1;
                        modelId = 0;
                        identifier = 0;
                        if (agent_type != NPC) { profession = GW::Constants::Profession::None; boss_states = 0; }
                        if (agent_type == Item || agent_type == Gadget) {
                            dead_states = 0;
                            quest_states = 0;
                            combat_state = EitherCombat;
                            weapon_state = EitherWeapon;
                        }
                    }
                    player_relation = option.relation;
                    gadget_state = option.gadget;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SetCursorPosX(x);
        ImGui::BeginDisabled(agent_type == Any);
        if (ImGui::InputInt("Identifier", reinterpret_cast<int*>(&identifier))) changed = true;
        ImGui::EndDisabled();
        ImGui::ShowHelp("Item model ID, gadget ID or NPC model ID, according to the chosen type. Leave 0 to match any identifier.");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputText("Match name", match_name, sizeof(match_name))) changed = true;
        ImGui::ShowHelp("Case-insensitive substring, or /pattern/flags for a regular expression, as in Loot Beacons.");
        if (const auto pattern = AgentAppearanceWindow::compiled_name_patterns.find(this);
            pattern != AgentAppearanceWindow::compiled_name_patterns.end() && !pattern->second.IsValid()) {
            ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "Invalid regex");
        }
        ImGui::SetCursorPosX(x);
        static const char* target_states[] = {"Either", "Targeted", "Not targeted", "Marked (/marktarget)"};
        if (ImGui::Combo("Target state", reinterpret_cast<int*>(&target_state), target_states, _countof(target_states))) changed = true;
        ImGui::SetCursorPosX(x);
        ImGui::BeginDisabled(agent_type != NPC);
        auto profession_selection = static_cast<int>(profession);
        if (ImGui::Combo("Profession", &profession_selection, profession_names, _countof(profession_names))) {
            profession = static_cast<GW::Constants::Profession>(profession_selection);
            changed = true;
        }
        static constexpr const char* boss_state_items[] = {"Boss", "Not boss"};
        changed |= ImGui::MultiSelectCombo("Boss state", &boss_states, boss_state_items);
        ImGui::EndDisabled();
        ImGui::ShowHelp("No states selected disables this filter. Otherwise, only selected states match.");
        ImGui::SetCursorPosX(x);
        if (ImGui::Checkbox("Outposts only", &outpost_only)) changed = true;
        ImGui::SetCursorPosX(x);
        int allegiance_combo = allegiance < 0 ? 0 : allegiance;
        if ((agent_type == NPC || agent_type == Player) &&
            ImGui::Combo("Allegiance", &allegiance_combo, allegiance_names, _countof(allegiance_names))) {
            allegiance = allegiance_combo == 0 ? -1 : allegiance_combo;
            changed = true;
        }
        ImGui::ShowHelp("Optional allegiance filter for NPCs and players.");
        if (agent_type == NPC || agent_type == Player || agent_type == Any) {
            ImGui::SetCursorPosX(x);
            static constexpr const char* dead_state_items[] = {"Dead", "Alive"};
            changed |= ImGui::MultiSelectCombo("Dead state", &dead_states, dead_state_items);
            ImGui::ShowHelp("No states selected disables this filter. Otherwise, only selected states match.");
            ImGui::SetCursorPosX(x);
            static constexpr const char* quest_state_items[] = {"Quest giver", "Not quest giver"};
            changed |= ImGui::MultiSelectCombo("Quest state", &quest_states, quest_state_items);
            ImGui::ShowHelp("No states selected disables this filter. Otherwise, only selected states match.");
        }
        ImGui::SetCursorPosX(x);
        auto selected_map = static_cast<uint32_t>(mapId);
        if (ImGui::MapPicker("Map", &selected_map)) {
            mapId = selected_map;
            changed = true;
        }
        ImGui::ShowHelp("The map where it will be applied. Optional. Select Any map to disable this filter.");
        ImGui::SetCursorPosX(x);
        static const char* combat_state_items[] = {"In combat", "Not in combat", "Either"};
        if (ImGui::Combo("Combat", (int*)&combat_state, combat_state_items, 3)) {
            changed = true;
        }
        ImGui::ShowHelp("Require the agent to be in a particular combat stance");
        ImGui::SetCursorPosX(x);
        static const char* weapon_state_items[] = {"Has weapon", "No weapon", "Either"};
        if (ImGui::Combo("Weapon", (int*)&weapon_state, weapon_state_items, 3)) {
            changed = true;
        }
        ImGui::ShowHelp("Require the agent to have a weapon");

        ImGui::Spacing();

        const auto draw_color_override = [&](const char* label, bool& enabled, Color& value, const char* tooltip) {
            ImGui::PushID(label);
            changed |= ImGui::Checkbox("##override", &enabled);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!enabled);
            changed |= Colors::DrawSettingHueWheel(label, &value, ImGuiColorEditFlags_AlphaBar);
            ImGui::EndDisabled();
            ImGui::PopID();
        };
        draw_color_override("Color", override_color, color, "Override marker colour for this rule");
        draw_color_override("Target border color", override_border_color, border_color, "Override border colour for this rule");
        draw_color_override("Text color", override_text_color, color_text, "Override name tag colour for this rule");

        if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.0f, 0.0f, "%.2fx")) {
            if (!std::isfinite(scale) || scale < 0.f) scale = 0.f;
            changed = true;
        }
        ImGui::ShowHelp("Multiplier of Default Size: 1.0 is the default, 0.8 is 80%, and 1.1 is 110%. Zero inherits the next matching size.");

        static const char* items[] = {"Inherit", "Tear", "Circle", "Square", "Big Circle", "Star"};
        auto shape_selection = static_cast<int>(shape) + 1;
        if (ImGui::Combo("Shape", &shape_selection, items, _countof(items))) {
            shape = static_cast<Shape_e>(shape_selection - 1);
            changed = true;
        }
        ImGui::ShowHelp("Inherit uses the next matching shape or the minimap default.");

        ImGui::PopID();
    }
    ImGui::End();
    return changed;
}
