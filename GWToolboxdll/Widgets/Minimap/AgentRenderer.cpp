#include "stdafx.h"

#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/GuildContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Constants/EncStrings.h>

#include <Modules/Resources.h>
#include <Widgets/Minimap/AgentRenderer.h>
#include <Widgets/Minimap/Minimap.h>

#include "GWToolbox.h"
#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>
#include <Utils/SettingsDoc.h>

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

    std::map<uint32_t, MarkedTarget*> marked_targets; // {agent_id, MarkedTarget}
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
        AgentRenderer::Instance().InvalidateAppearance(agent_id);
        const auto marked_target = GetMarkedTarget(agent_id);
        if (!marked_target) {
            return;
        }
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        if (!marked_target->Matches(agent)) {
            // agent_id has been recycled
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
    };
    for (const auto& [key, color] : colors) {
        // SettingColor is layout-compatible with Color; the cast lets the registry persist it as a hex string
        SettingsRegistry::RegisterField(module, key, reinterpret_cast<Colors::SettingColor*>(color));
    }
    SettingsRegistry::RegisterField(module, "custom_agent_defaults_seeded", &custom_agent_defaults_seeded);
    SettingsRegistry::RegisterField(module, "appearance_defaults_seeded", &appearance_defaults_seeded);
    SettingsRegistry::RegisterField(module, "size_default", &size_default);
    SettingsRegistry::RegisterField(module, "agent_border_thickness", &agent_border_thickness);
    SettingsRegistry::RegisterField(module, "target_border_thickness", &target_border_thickness);
    SettingsRegistry::RegisterField(module, "default_shape", reinterpret_cast<int*>(&default_shape));
    if (!hooks_added) {
        hooks_added = true;
        RegisterUIMessageCallback(&UIMsg_Entry, GW::UI::UIMessage::kMapLoaded, OnUIMessage);
        GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdded_HookEntry, OnAgentAdded);
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"marktarget", CmdMarkTarget);
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"clearmarktarget", CmdClearMarkTarget);
    }
}

void AgentRenderer::RegisterMinimapSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "show_quest_npcs_on_minimap", &show_quest_npcs_on_minimap);
    SettingsRegistry::RegisterField(module, "show_hidden_npcs", &show_hidden_npcs);
#ifdef _DEBUG
    SettingsRegistry::RegisterField(module, "show_props_on_minimap", &show_props_on_minimap);
#endif
}

void AgentRenderer::LoadCustomAgents(SettingsDoc& doc, ToolboxIni* legacy)
{
    custom_agents_loaded = false;
    match_cache.clear();
    pending_names.clear();
    for (const CustomAgent* ca : custom_agents) {
        delete ca;
    }
    custom_agents.clear();

    const auto append_rule = [this](CustomAgent* rule) {
        const auto append = [this](CustomAgent* entry) {
            entry->index = custom_agents.size();
            custom_agents.push_back(entry);
        };
        if (static_cast<int>(rule->agent_type) != 0) {
            append(rule);
            return;
        }
        if (rule->allegiance >= 0) {
            rule->agent_type = NPC;
            if (rule->is_default) rule->dead_state = Alive;
            append(rule);
            return;
        }
        const auto old_rule = rule->ToSettings();
        rule->agent_type = NPC;
        rule->identifier = rule->modelId;
        rule->identifier_active = true;
        rule->modelId = 0;
        append(rule);
        auto* gadget_rule = new CustomAgent(old_rule);
        gadget_rule->agent_type = Gadget;
        gadget_rule->identifier = gadget_rule->modelId;
        gadget_rule->identifier_active = true;
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
        if (rules_version < 2 && (!doc.Get(rules_section, "appearance_rules", flags) || flags.size() != saved.size())) {
            Log::Error("Failed to migrate appearance rules in %s", rules_section);
            return;
        }
        for (size_t i = 0; i < saved.size(); ++i) {
            auto* rule = new CustomAgent(saved[i]);
            if (rules_version < 2) rule->ApplyLegacyFlags(flags[i]);
            append_rule(rule);
        }
        BuildCustomAgentsMap();
        custom_agents_loaded = true;
        if (!appearance_defaults_seeded) {
            SeedAppearanceDefaults(doc, legacy);
            appearance_defaults_seeded = true;
        }
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
            append_rule(rule);
        }
    }
    else {
        ToolboxIni inifile;
        ASSERT(inifile.LoadIfExists(Resources::GetLegacySettingFile(AGENTCOLOR_INIFILENAME)) == SI_OK);

        TNamesDepend entries;
        inifile.GetAllSections(entries);

        for (const auto& entry : entries) {
            append_rule(new CustomAgent(&inifile, entry.pItem));
        }
    }
    BuildCustomAgentsMap();
    custom_agents_loaded = true;

    if (!custom_agent_defaults_seeded) {
        SeedDefaultCustomAgents();
        custom_agent_defaults_seeded = true;
    }
    if (!appearance_defaults_seeded) {
        SeedAppearanceDefaults(doc, legacy);
        appearance_defaults_seeded = true;
    }
}

void AgentRenderer::SeedAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy)
{
    const auto add = [this](const char* label, const AgentType type, const Color color, const float size, const Shape_e shape, const int allegiance = -1, const DeadState dead = EitherDeadState) {
        auto* rule = new CustomAgent(0, color, label);
        rule->agent_type = type;
        rule->allegiance = allegiance;
        rule->dead_state = dead;
        rule->size = size;
        rule->shape = shape;
        rule->is_default = true;
        std::snprintf(rule->group, sizeof(rule->group), "Defaults");
        rule->index = custom_agents.size();
        custom_agents.push_back(rule);
        return rule;
    };
    auto* target = add("Target", Any, 0, 0.f, Shape_None);
    target->target_state = Targeted;
    target->border_color = color_target;
    add("Marked Target", Any, color_marked_target, size_marked_target, default_shape)->target_state = Marked;
    auto* boss = add("Boss", NPC, 0, size_boss, Shape_None);
    boss->boss_state = 1;
    add("Hostile (dead)", NPC, color_hostile_dead, size_hostile, Shape_None, static_cast<int>(GW::Constants::Allegiance::Enemy), Dead);
    constexpr const char* professions[] = {"", "Warrior", "Ranger", "Monk", "Necromancer", "Mesmer", "Elementalist", "Assassin", "Ritualist", "Paragon", "Dervish"};
    for (int profession = 1; profession <= 10; ++profession) {
        auto* rule = add(professions[profession], NPC, profession_colors[profession], 0.f, Shape_None, static_cast<int>(GW::Constants::Allegiance::Enemy), Alive);
        rule->profession = profession;
        rule->boss_state = only_color_bosses ? 1 : 0;
        rule->active = enemies_colors_by_profession;
    }
    add("Hostile", NPC, color_hostile, size_hostile, Shape_None, static_cast<int>(GW::Constants::Allegiance::Enemy), Alive);
    for (const auto allegiance : {GW::Constants::Allegiance::Ally_NonAttackable, GW::Constants::Allegiance::Npc_Minipet, GW::Constants::Allegiance::Spirit_Pet, GW::Constants::Allegiance::Minion}) {
        add("Ally (dead)", NPC, color_ally_dead, size_ally, Shape_None, static_cast<int>(allegiance), Dead);
        add("Ally (quest giver)", NPC, color_ally_npc_quest, size_ally_npc_quest, Shape_None, static_cast<int>(allegiance), Alive)->quest_state = QuestGiver;
    }
    add("Item", Item, color_item, size_item, Quad);
    add("Locked chest (closed)", Gadget, color_locked_chest, size_locked_chest, Quad)->gadget_state = ClosedChest;
    add("Locked chest (opened)", Gadget, color_locked_chest_open, size_locked_chest_open, Quad)->gadget_state = OpenedChest;
    add("Gadget", Gadget, color_signpost, size_signpost, Quad)->gadget_state = OtherGadget;
    add("Player", Player, color_player, size_player, shape_player, -1, Alive)->player_relation = Self;
    add("Player (dead)", Player, color_player_dead, size_player, shape_player, -1, Dead)->player_relation = Self;
    add("Other player", Player, color_ally, size_ally, shape_players, -1, Alive)->player_relation = Other;
    const auto tag_start = custom_agents.size();
    bool enabled = false;
    if (!doc.Get("Game Settings", "override_name_tag_colors", enabled) && legacy) {
        enabled = legacy->GetBoolValue("Game Settings", "override_name_tag_colors", false);
    }
    if (enabled) {
        const auto add_tag = [&doc, legacy, &add](const char* key, const char* label, const AgentType type, const int allegiance = -1, const PlayerRelation relation = AnyRelation, const Color fallback = 0) {
            Colors::SettingColor setting(fallback);
            if (!doc.Get("Game Settings", key, setting)) {
                if (legacy && legacy->KeyExists("Game Settings", key)) setting = Colors::Load(legacy, "Game Settings", key, setting.value);
                else if (!fallback) return;
            }
            auto* rule = add(label, type, 0, 0.f, Shape_None, allegiance);
            rule->player_relation = relation;
            rule->color_text = setting.value;
        };
        add_tag("nametag_color_npc", "NPC name tag", NPC);
        add_tag("nametag_color_enemy", "Enemy name tag", NPC, static_cast<int>(GW::Constants::Allegiance::Enemy));
        add_tag("nametag_color_gadget", "Gadget name tag", Gadget);
        add_tag("nametag_color_item", "Item name tag", Item);
        add_tag("nametag_color_player_other", "Other player name tag", Player, -1, Other);
        add_tag("nametag_color_player_self", "My name tag", Player, -1, Self);
        add_tag("nametag_color_player_in_my_party", "Party name tag", Player, -1, MyParty);
        add_tag("nametag_color_player_in_party", "Player in party name tag", Player, -1, InParty);
        add_tag("nametag_color_friends", "Friend name tag", Player, -1, Friend, 0xFF60FF60);
        add_tag("nametag_color_guild_members", "Guild name tag", Player, -1, Guild, 0xFFFFD060);
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
        auto* rule = add("Friend (outpost) name tag", Player, 0, 0.f, Shape_None);
        rule->player_relation = Friend;
        rule->outpost_only = true;
        rule->color_text = setting.value;
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
    BuildCustomAgentsMap();
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
        {"Neutral", GW::Constants::Allegiance::Neutral, EitherQuestState, &color_neutral, &size_neutral},
        {"Ally", GW::Constants::Allegiance::Ally_NonAttackable, NotQuestGiver, &color_ally, &size_ally},
        {"Ally (NPC)", GW::Constants::Allegiance::Npc_Minipet, NotQuestGiver, &color_ally_npc, &size_ally_npc},
        {"Ally (Spirit/Pet)", GW::Constants::Allegiance::Spirit_Pet, NotQuestGiver, &color_ally_spirit, &size_ally_spirit},
        {"Ally (Minion)", GW::Constants::Allegiance::Minion, NotQuestGiver, &color_ally_minion, &size_minion},
    };
    for (const auto& row : rows) {
        auto* ca = new CustomAgent(0, *row.color, row.label);
        ca->allegiance = static_cast<int>(row.allegiance);
        ca->agent_type = NPC;
        ca->quest_state = row.quest_state;
        ca->size = *row.size;
        ca->is_default = true;
        ca->dead_state = Alive;
        ca->shape = Shape_None;
        std::snprintf(ca->group, sizeof(ca->group), "Defaults");
        ca->index = custom_agents.size();
        custom_agents.push_back(ca);
    }
    BuildCustomAgentsMap();
}

void AgentRenderer::SyncSeededDefaultsFromLegacyFields()
{
    const std::pair<GW::Constants::Allegiance, std::pair<Color, float>> legacy_values[] = {
        {GW::Constants::Allegiance::Neutral, {color_neutral, size_neutral}},
        {GW::Constants::Allegiance::Ally_NonAttackable, {color_ally, size_ally}},
        {GW::Constants::Allegiance::Npc_Minipet, {color_ally_npc, size_ally_npc}},
        {GW::Constants::Allegiance::Spirit_Pet, {color_ally_spirit, size_ally_spirit}},
        {GW::Constants::Allegiance::Minion, {color_ally_minion, size_minion}},
    };
    for (const auto& [allegiance, values] : legacy_values) {
        for (CustomAgent* ca : custom_agents) {
            if (!ca->is_default || ca->agent_type != NPC || ca->allegiance != static_cast<int>(allegiance) ||
                ca->dead_state != Alive || ca->quest_state == QuestGiver || ca->profession || ca->boss_state) {
                continue;
            }
            ca->color = values.first;
            ca->size = values.second;
        }
    }
    for (auto* rule : custom_agents) {
        if (!rule->is_default || Colors::IsVisible(rule->color_text)) continue;
        const std::string_view name = rule->name;
        if (name == "Hostile") { rule->color = color_hostile; rule->size = size_hostile; }
        else if (name == "Hostile (dead)") { rule->color = color_hostile_dead; rule->size = size_hostile; }
        else if (name == "Ally (dead)") { rule->color = color_ally_dead; rule->size = size_ally; }
        else if (name == "Ally (quest giver)") { rule->color = color_ally_npc_quest; rule->size = size_ally_npc_quest; }
        else if (name == "Item") { rule->color = color_item; rule->size = size_item; }
        else if (name == "Locked chest (closed)") { rule->color = color_locked_chest; rule->size = size_locked_chest; }
        else if (name == "Locked chest (opened)") { rule->color = color_locked_chest_open; rule->size = size_locked_chest_open; }
        else if (name == "Gadget") { rule->color = color_signpost; rule->size = size_signpost; }
        else if (name == "Player") { rule->color = color_player; rule->size = size_player; rule->shape = shape_player; }
        else if (name == "Player (dead)") { rule->color = color_player_dead; rule->size = size_player; rule->shape = shape_player; }
        else if (name == "Other player") { rule->color = color_ally; rule->size = size_ally; rule->shape = shape_players; }
        else if (name == "Marked Target") { rule->color = color_marked_target; rule->size = size_marked_target; }
        else if (name == "Boss") { rule->size = size_boss; }
        else if (name == "Target") { rule->border_color = color_target; }
    }
    rules_changed = true;
}

void AgentRenderer::SaveCustomAgents(SettingsDoc& doc) const
{
    if (custom_agents_loaded) {
        std::vector<CustomAgent::Settings> entries;
        entries.reserve(custom_agents.size());
        for (const CustomAgent* ca : custom_agents) {
            entries.push_back(ca->ToSettings());
        }
        doc.Set("Game Settings", "appearance_rules", entries);
        doc.Set("Game Settings", "appearance_rules_version", 2);
        doc.Set("Game Settings", "custom_agent_defaults_seeded", custom_agent_defaults_seeded);
        doc.Set("Game Settings", "appearance_defaults_seeded", appearance_defaults_seeded);
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
            "color_marked_target", "size_marked_target", "color_target", "size_boss", "shape_player", "shape_players"
        };
        for (const auto key : migrated_keys) doc.EraseKey("Minimap", key);
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

void AgentRenderer::ResetAppearanceSettings()
{
    LoadDefaultColors();
    LoadDefaultSizes();
    default_shape = shape_player = shape_players = Tear;
    profession_colors = DefaultProfessionColors();
    custom_agent_defaults_seeded = false;
    appearance_defaults_seeded = false;
}

void AgentRenderer::LoadLegacyAppearanceDefaults(const SettingsDoc& doc, const ToolboxIni* legacy)
{
    constexpr auto section = "Minimap";
    const std::pair<const char*, Color*> colors[] = {
        {"color_agent_modifier", &color_agent_modifier}, {"color_agent_damaged_modifier", &color_agent_damaged_modifier},
        {"color_eoe", &color_eoe}, {"color_qz", &color_qz}, {"color_winnowing", &color_winnowing},
        {"color_frozen_soil", &color_frozen_soil}, {"color_symbiosis", &color_symbiosis},
        {"color_target", &color_target}, {"color_player", &color_player}, {"color_player_dead", &color_player_dead},
        {"color_signpost", &color_signpost}, {"color_locked_chest", &color_locked_chest},
        {"color_locked_chest_open", &color_locked_chest_open}, {"color_item", &color_item},
        {"color_hostile", &color_hostile}, {"color_hostile_dead", &color_hostile_dead},
        {"color_neutral", &color_neutral}, {"color_ally", &color_ally}, {"color_ally_npc", &color_ally_npc},
        {"color_ally_npc_quest", &color_ally_npc_quest}, {"color_ally_spirit", &color_ally_spirit},
        {"color_ally_minion", &color_ally_minion}, {"color_ally_dead", &color_ally_dead},
        {"color_marked_target", &color_marked_target},
        {"color_profession_warrior", &profession_colors[1]}, {"color_profession_ranger", &profession_colors[2]},
        {"color_profession_monk", &profession_colors[3]}, {"color_profession_necromancer", &profession_colors[4]},
        {"color_profession_mesmer", &profession_colors[5]}, {"color_profession_elementalist", &profession_colors[6]},
        {"color_profession_assassin", &profession_colors[7]}, {"color_profession_ritualist", &profession_colors[8]},
        {"color_profession_paragon", &profession_colors[9]}, {"color_profession_dervish", &profession_colors[10]}
    };
    for (const auto& [key, color] : colors) {
        Colors::SettingColor staged(*color);
        if (doc.Get(section, key, staged)) *color = staged.value;
        else if (legacy) *color = Colors::Load(legacy, section, key, *color);
    }
    const std::pair<const char*, float*> sizes[] = {
        {"size_default", &size_default}, {"agent_border_thickness", &agent_border_thickness},
        {"target_border_thickness", &target_border_thickness},
        {"size_player", &size_player}, {"size_signpost", &size_signpost}, {"size_locked_chest", &size_locked_chest},
        {"size_locked_chest_open", &size_locked_chest_open}, {"size_item", &size_item}, {"size_boss", &size_boss},
        {"size_minion", &size_minion}, {"size_marked_target", &size_marked_target}, {"size_hostile", &size_hostile},
        {"size_neutral", &size_neutral}, {"size_ally", &size_ally}, {"size_ally_npc", &size_ally_npc},
        {"size_ally_npc_quest", &size_ally_npc_quest}, {"size_ally_spirit", &size_ally_spirit}
    };
    for (const auto& [key, size] : sizes) {
        if (!doc.Get(section, key, *size) && legacy) *size = static_cast<float>(legacy->GetDoubleValue(section, key, *size));
    }
    if (!doc.Get(section, "enemies_colors_by_profession", enemies_colors_by_profession) && legacy) {
        enemies_colors_by_profession = legacy->GetBoolValue(section, "enemies_colors_by_profession", enemies_colors_by_profession);
    }
    if (!doc.Get(section, "only_color_bosses", only_color_bosses) && legacy) {
        only_color_bosses = legacy->GetBoolValue(section, "only_color_bosses", only_color_bosses);
    }
    const std::pair<const char*, Shape_e*> shapes[] = {{"default_shape", &default_shape}, {"shape_player", &shape_player}, {"shape_players", &shape_players}};
    for (const auto& [key, shape] : shapes) {
        auto value = static_cast<int>(*shape);
        if (!doc.Get(section, key, value) && legacy) value = static_cast<int>(legacy->GetLongValue(section, key, value));
        if (value >= Tear && value <= BigCircle) *shape = static_cast<Shape_e>(value);
    }
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
    ImGui::Checkbox("Show props on minimap", &show_props_on_minimap);
#endif
    if (ImGui::TreeNodeEx("Agent Colors", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::SmallConfirmButton("Restore Defaults", "Reset default appearance rows, effect colours and fallback sizes?\nThis cannot be undone.", [&](bool result, void*) {
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
            {"EoE", &color_eoe, nullptr, nullptr, "This is the color at the edge, the color in the middle is the same, with alpha-50"},
            {"QZ", &color_qz, nullptr, nullptr, "This is the color at the edge, the color in the middle is the same, with alpha-50"},
            {"Winnowing", &color_winnowing, nullptr, nullptr, "This is the color at the edge, the color in the middle is the same, with alpha-50"},
            {"Frozen Soil", &color_frozen_soil, nullptr, nullptr, "This is the color at the edge, the color in the middle is the same, with alpha-50"},
            {"Symbiosis", &color_symbiosis, nullptr, nullptr, "This is the color at the edge, the color in the middle is the same, with alpha-50"},
            {"Agent modifier", &color_agent_modifier, nullptr, nullptr, "Each agent has this value removed on the border and added at the center\nZero makes agents have solid color, while a high number makes them appear more shaded."},
            {"Agent damaged modifier", &color_agent_damaged_modifier, nullptr, nullptr, "Each hostile agent has this value subtracted from it when under 90% HP."},
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
                ImGui::DragFloat("Size##size", row.size, 1.0f, 1.0f, 0.0f, "%.0f");
                ImGui::PopID();
                if (row.size_tooltip && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", row.size_tooltip);
                }
            }
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Agent Sizes", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::SmallConfirmButton("Restore Defaults", "Are you sure?\nThis will reset all agent sizes to the default values.\nThis operation cannot be undone.\n\n",
            [&](const bool result, void*) {
                if (result) {
                    LoadDefaultSizes();
                    SyncSeededDefaultsFromLegacyFields();
                }
            });
        {
            struct SizeEntry { const char* label; float* size; const char* help; };
            const SizeEntry entries[] = {
                {"Default Size",       &size_default,       nullptr},
            };
            for (const auto& [label, sz, help] : entries) {
                ImGui::DragFloat(label, sz, 1.0f, 1.0f, 0.0f, "%.0f");
                if (help) {
                    ImGui::ShowHelp(help);
                }
            }
        }
        static std::array items = {"Tear", "Circle", "Square", "Big Circle"};
        ImGui::Combo("Default Shape", reinterpret_cast<int*>(&default_shape), items.data(), items.size());
        ImGui::ShowHelp("The default shape of agents.");

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Agent Appearance", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        static char group_filter[64] = "";
        ImGui::InputTextWithHint("Filter", "Filter by name or group...", group_filter, sizeof(group_filter));
        ImGui::ShowHelp("Only affects what's shown here. Rules are evaluated top to bottom, independently for each enabled colour, size and shape.");

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
                        // render the moved one and increase i
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
            BuildCustomAgentsMap();
        }
        if (ImGui::Button("Add Appearance Rule")) {
            custom_agents.push_back(new CustomAgent(0, 0, "<name>"));
            custom_agents.back()->index = custom_agents.size() - 1;
            custom_agents.back()->active = false;
            rules_changed = true;
        }
        ImGui::TreePop();
    }
}

void AgentRenderer::Terminate()
{
    D3DVertexBuffer::Terminate();
    match_cache.clear();
    pending_names.clear();
    RemoveMarkedTarget();
}

void AgentRenderer::ReleaseAppearanceHooks()
{
    GW::UI::RemoveUIMessageCallback(&UIMsg_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdded_HookEntry);
    hooks_added = false;
    match_cache.clear();
    pending_names.clear();
    for (const CustomAgent* ca : custom_agents) {
        delete ca;
    }
    custom_agents.clear();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    custom_agents_loaded = false;
}

AgentRenderer& AgentRenderer::Instance() { return *instance; }

bool AgentRenderer::AppearanceRulesLoaded() { return instance && instance->custom_agents_loaded; }

Color AgentRenderer::GetProfessionColor(const uint32_t profession) const
{
    if (profession >= profession_colors.size()) return 0;
    for (const auto* rule : custom_agents) {
        if (rule->profession == static_cast<int>(profession) && Colors::IsVisible(rule->color)) return rule->color;
    }
    return profession_colors[profession];
}

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
            Instance().match_cache.clear();
            Instance().pending_names.clear();
            break;
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
}

std::vector<const AgentRenderer::CustomAgent*>* AgentRenderer::GetCustomAgentsToDraw(const GW::Agent* agent)
{
    if (!agent) return nullptr;
    RefreshMatches(agent);
    auto& matches = match_cache.at(agent->agent_id).matches;
    return matches.empty() ? nullptr : &matches;
};

void AgentRenderer::OnNameDecoded(void* context, const wchar_t* decoded)
{
    const auto token = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context));
    auto& self = Instance();
    const auto pending = self.pending_names.find(token);
    if (pending == self.pending_names.end()) return;
    const auto [id, generation] = pending->second;
    self.pending_names.erase(pending);
    const auto it = self.match_cache.find(id);
    if (it == self.match_cache.end() || it->second.generation != generation || it->second.agent != GW::Agents::GetAgentByID(id)) return;
    it->second.name = TextUtils::StripTags(TextUtils::Replace(decoded ? decoded : L"", L"<brx>", L"\n"));
    it->second.valid = false;
    if (it->second.name.size()) GW::Agents::RefreshAgentNameTag(it->second.agent);
}

void AgentRenderer::RefreshMatches(const GW::Agent* agent)
{
    if (rules_changed) {
        match_cache.clear();
        rules_changed = false;
    }
    const auto living = agent->GetAsAgentLiving();
    const auto item = agent->GetAsAgentItem();
    const auto gadget = agent->GetAsAgentGadget();
    const auto item_data = item ? GW::Items::GetItemById(item->item_id) : nullptr;
    const auto type = item ? Item : gadget ? Gadget : living ? living->IsPlayer() ? Player : NPC : Any;
    const auto identifier = item_data ? item_data->model_id : gadget ? gadget->gadget_id : living ? living->player_number : 0;
    const auto targeted = GW::Agents::GetTargetId() == agent->agent_id || auto_target_id == agent->agent_id;
    const auto marked = GetMarkedTarget(agent->agent_id) != nullptr;
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    const auto allegiance = living ? static_cast<int>(living->allegiance) : -1;
    const auto profession = living ? GetAgentProfession(living) : 0;
    const auto flags = (living && living->GetIsDead() ? 1u : 0u) | (living && living->GetHasQuest() ? 2u : 0u) |
        (living && living->GetInCombatStance() ? 4u : 0u) | (targeted ? 8u : 0u) | (marked ? 256u : 0u) |
        (living && living->weapon_type != 0 && living->weapon_type != 512 ? 16u : 0u) |
        (gadget && IsLockedChest(agent) ? IsOpenedLockedChest(agent) ? 64u : 32u : 0u) |
        (living && living->GetHasBossGlow() ? 128u : 0u);
    uint32_t relations = 0;
    if (type == Player) {
        if (agent->agent_id == GW::Agents::GetControlledCharacterId()) relations |= 1u;
        if (check_friends || check_guild) {
            const auto decoded = living ? GW::PlayerMgr::GetPlayerName(living->login_number) : nullptr;
            const auto encoded = decoded ? nullptr : GW::Agents::GetAgentEncName(agent->agent_id);
            const auto player_name = decoded ? std::wstring(decoded) : encoded ? TextUtils::GetPlayerNameFromEncodedString(encoded) : std::wstring{};
            if (check_friends && !player_name.empty() && GW::FriendListMgr::GetFriend(nullptr, player_name.c_str(), GW::FriendType::Friend)) relations |= 2u;
            if (check_guild && !player_name.empty()) {
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
        if (check_party) {
            if (ToolboxUtils::IsAgentInMyParty(agent->agent_id)) relations |= 8u;
            if (ToolboxUtils::IsAgentInParty(agent->agent_id)) relations |= 16u;
        }
    }
    auto& cached = match_cache[agent->agent_id];
    if (cached.agent != agent) {
        cached = {};
        cached.agent = agent;
        cached.generation = ++next_name_token;
    }
    if (cached.valid && cached.identifier == identifier && cached.map_id == map_id && cached.flags == flags && cached.allegiance == allegiance && cached.type == type && cached.relation_flags == relations && cached.profession == profession) return;
    cached.identifier = identifier;
    cached.map_id = map_id;
    cached.flags = flags;
    cached.allegiance = allegiance;
    cached.profession = profession;
    cached.type = type;
    cached.relation_flags = relations;
    cached.valid = true;
    cached.matches.clear();
    for (const auto* rule : custom_agents) {
        if (!rule->active || rule->mapId && rule->mapId != static_cast<DWORD>(GW::Map::GetMapID())) continue;
        if (rule->outpost_only && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) continue;
        if (rule->agent_type != Any && rule->agent_type != type) continue;
        if (rule->agent_type != Any && rule->identifier_active && rule->identifier != identifier) continue;
        if (rule->allegiance >= 0 && (!living || rule->allegiance != static_cast<int>(living->allegiance))) continue;
        if (rule->target_state == Targeted && !targeted || rule->target_state == NotTargeted && targeted || rule->target_state == Marked && !marked) continue;
        if (rule->profession && rule->profession != static_cast<int>(profession)) continue;
        if (rule->boss_state == 1 && !(flags & 128u) || rule->boss_state == 2 && (flags & 128u)) continue;
        if (rule->gadget_state != AnyGadget && (type != Gadget ||
            rule->gadget_state == ClosedChest && !(flags & 32u) || rule->gadget_state == OpenedChest && !(flags & 64u) ||
            rule->gadget_state == OtherGadget && (flags & (32u | 64u)))) continue;
        if (rule->player_relation != AnyRelation && (type != Player ||
            rule->player_relation == Self && !(relations & 1u) || rule->player_relation == Other && (relations & 1u) ||
            rule->player_relation == Friend && !(relations & 2u) || rule->player_relation == Guild && !(relations & 4u) ||
            rule->player_relation == MyParty && !(relations & 8u) || rule->player_relation == InParty && !(relations & 16u))) continue;
        if (rule->dead_state == Dead && (!living || !living->GetIsDead()) || rule->dead_state == Alive && (!living || living->GetIsDead())) continue;
        if (rule->quest_state == QuestGiver && (!living || !living->GetHasQuest()) || rule->quest_state == NotQuestGiver && (!living || living->GetHasQuest())) continue;
        if (rule->combat_state == InCombat && (!living || !living->GetInCombatStance()) || rule->combat_state == NotInCombat && (!living || living->GetInCombatStance())) continue;
        if (rule->weapon_state == HasWeapon && (!living || living->weapon_type == 0 || living->weapon_type == 512) ||
            rule->weapon_state == NoWeapon && (!living || living->weapon_type != 0 && living->weapon_type != 512)) continue;
        if (rule->match_name[0]) {
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
            const auto pattern = compiled_name_patterns.find(rule);
            if (pattern == compiled_name_patterns.end() || !pattern->second.IsValid() || !pattern->second.Matches(cached.name)) continue;
        }
        cached.matches.push_back(rule);
    }
}

bool AgentRenderer::ApplyNameTagColor(const GW::Agent* agent, Color& color)
{
    const auto matches = GetCustomAgentsToDraw(agent);
    if (!matches) return false;
    for (const auto* rule : *matches) {
        if (!Colors::IsVisible(rule->color_text)) continue;
        color = rule->color_text;
        return true;
    }
    return false;
}

void AgentRenderer::InvalidateAppearance(const uint32_t agent_id)
{
    match_cache.erase(agent_id);
}

void AgentRenderer::Render(IDirect3DDevice9* device)
{
    const auto now = TIMER_INIT();
    // Only update every 30 frames, reduce CPU load
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

        // 1. eoes
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
        // 2. non-player agents
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
            custom_agents_to_draw.push_back({agent, custom_agents_for_this_agent->front()});
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
                continue; //  7. player
            }
            if (agent == target) {
                continue; // 4. target if it's a non-player, 6. target if it's a player
            }
            if (agent->GetIsGadgetType()) {
                const auto gadget = agent->GetAsAgentGadget();
                if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && gadget->extra_type == 7602) {
                    continue;
                }
                if (add_custom_agents_to_draw(gadget)) continue;
            }
            else if (agent->GetIsLivingType()) {
                const auto living = agent->GetAsAgentLiving();
                if (!show_hidden_npcs && !GW::Agents::GetAgentMatchesFlags(living)) {
                    continue;
                }
                if (add_marked_target(living)) {
                    continue; // 8. marked targets
                }
                if (add_other_players_to_draw(living)) {
                    continue; // 5. players
                }
                if (add_dead_agent_to_draw(living)) {
                    continue;
                }
                if (add_custom_agents_to_draw(living)) {
                    continue; // 3. custom colored models
                }
            }
            else if (agent->GetIsItemType()) {
                if (add_custom_agents_to_draw(agent)) continue;
            }
            other_agents_to_draw.push_back(agent);
        }

        // Dead agents
        for (const auto agent : dead_agents_to_draw) {
            const auto matches = GetCustomAgentsToDraw(agent);
            Enqueue(agent, matches ? matches->front() : nullptr);
        }

        // 2. Generic agents
        for (const auto agent : other_agents_to_draw) {
            Enqueue(agent);
        }

        // 3. custom colored models
        sort_custom_agents_to_draw();
        for (const auto& [fst, snd] : custom_agents_to_draw) {
            Enqueue(fst, snd);
        }

        // 8. marked
        for (const auto agent : marked_targets_to_draw) {
            if (!agent->GetIsAlive()) {
                continue;
            }
            const auto* cas = GetCustomAgentsToDraw(agent);
            const auto* ca = cas && !cas->empty() ? cas->front() : nullptr;
            if (ca) Enqueue(agent, ca);
            else Enqueue(default_shape, agent, size_marked_target, color_marked_target);
        }

        // 4. target if it's a non-player
        if (target && (!target->GetAsAgentLiving() || !target->GetAsAgentLiving()->IsPlayer())) {
            const auto marked = GetMarkedTarget(target->agent_id);
            const auto custom_agents_for_this_agent = GetCustomAgentsToDraw(target);
            if (custom_agents_for_this_agent) Enqueue(target, custom_agents_for_this_agent->front());
            else if (marked) Enqueue(default_shape, target, size_marked_target, color_marked_target);
            else Enqueue(target);
        }

        // 5. players
        for (const auto agent : players_to_draw) {
            const auto matches = GetCustomAgentsToDraw(agent);
            Enqueue(agent, matches ? matches->front() : nullptr);
        }

        // 6. target if it's a player
        if (target && target != player && target->GetAsAgentLiving() && target->GetAsAgentLiving()->IsPlayer()) {
            const auto matches = GetCustomAgentsToDraw(target);
            Enqueue(target, matches ? matches->front() : nullptr);
        }

        // 7. player
        if (player) {
            const auto matches = GetCustomAgentsToDraw(player);
            Enqueue(player, matches ? matches->front() : nullptr);
        }
    }

    D3DVertexBuffer::Render(device);
}

void AgentRenderer::Enqueue(const GW::Agent* agent, const CustomAgent* ca)
{
    auto color = GetColor(agent);
    auto size = GetSize(agent);
    auto shape = GetShape(agent);
    if (ca) {
        const auto matches = GetCustomAgentsToDraw(agent);
        bool found_color = false, found_size = false, found_shape = false;
        if (matches) for (const auto* rule : *matches) {
            if (!found_color && Colors::IsVisible(rule->color)) {
                color = GetColor(agent, rule);
                found_color = true;
            }
            if (!found_size && rule->size > 0.f) {
                size = rule->size;
                found_size = true;
            }
            if (!found_shape && rule->shape != Shape_None) {
                shape = rule->shape;
                found_shape = true;
            }
            if (found_color && found_size && found_shape) break;
        }
    }
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
    const auto* dead_npc = is_dead && living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
    if (dead_npc && (dead_npc->model_file_id == 0x22A34 || dead_npc->model_file_id == 0x2D0E4 || dead_npc->model_file_id == 0x2D07E)) {
        return IM_COL32(0, 0, 0, 0);
    }
    if (ca && Colors::IsVisible(ca->color)) {
        if (living && !is_dead && ca->target_state != Marked && living->allegiance == GW::Constants::Allegiance::Enemy && living->hp <= 0.9f) {
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

    if (living->allegiance == GW::Constants::Allegiance::Enemy) {
        if (living->GetIsDead()) {
            return color_hostile_dead;
        }
        const Color* c = &color_hostile;
        constexpr auto relevance_range_squared = 2500.f * 2500.f;
        const auto is_inside = [](const GW::GamePos pos, const std::vector<GW::GamePos>& points) -> bool {
            bool b = false;
            //TODO: This might need adjust to take into account zlevels
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

    // friendly
    if (living->GetIsDead()) {
        return color_ally_dead;
    }
    if (living->GetHasQuest()) {
        return color_ally_npc_quest;
    }
    switch (living->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
            return color_ally; // ally
        case GW::Constants::Allegiance::Npc_Minipet:
            return color_ally_npc; // npc / minipet
        case GW::Constants::Allegiance::Spirit_Pet:
            return color_ally_spirit; // spirit / pet
        case GW::Constants::Allegiance::Minion:
            return color_ally_minion; // minion
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
        if (ca->is_default && ca->active && ca->size > 0.f) {
            return ca->size;
        }
    }
    return fallback;
}

float AgentRenderer::GetSize(const GW::Agent* agent, const CustomAgent* ca) const
{
    if (ca && ca->size > 0.f) {
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
        case GW::Constants::Allegiance::Ally_NonAttackable: // ally
            if (!living->GetIsDead() && living->GetHasQuest()) {
                return size_ally_npc_quest;
            }
            return GetSeededDefaultSize(GW::Constants::Allegiance::Ally_NonAttackable, size_ally);

        case GW::Constants::Allegiance::Neutral: // neutral
            return GetSeededDefaultSize(GW::Constants::Allegiance::Neutral, size_neutral);

        case GW::Constants::Allegiance::Spirit_Pet: // spirit / pet
            return GetSeededDefaultSize(GW::Constants::Allegiance::Spirit_Pet, size_ally_spirit);

        case GW::Constants::Allegiance::Npc_Minipet: // npc / minipet
            if (!living->GetIsDead() && living->GetHasQuest()) {
                return size_ally_npc_quest;
            }
            return GetSeededDefaultSize(GW::Constants::Allegiance::Npc_Minipet, size_ally_npc);

        case GW::Constants::Allegiance::Minion: // minion
            return GetSeededDefaultSize(GW::Constants::Allegiance::Minion, size_minion);

        case GW::Constants::Allegiance::Enemy: // hostile
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
    if (ca && ca->shape != Shape_None) {
        return ca->shape;
    }

    if (agent->GetIsGadgetType()) {
        return Quad;
    }
    if (agent->GetIsItemType()) {
        return Quad;
    }
    if (!agent->GetIsLivingType()) {
        return Quad; // shouldn't happen but just in case
    }

    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (living->login_number > 0) {
        if (living->agent_id == GW::Agents::GetControlledCharacterId())
            return shape_player;
        return shape_players; // players
    }

    if (show_quest_npcs_on_minimap && living->GetHasQuest()) {
        return Star;
    }

    const auto* npc = living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
    if (npc) {
        switch (npc->model_file_id) {
            case 0x22A34: // nature rituals
            case 0x2D0E4: // defensive binding rituals
            case 0x2963E: // dummies
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
    // NB: No border if BigCircle
    if (shape != BigCircle) {
        const bool is_target = auto_target_id == agent->agent_id || GW::Agents::GetTargetId() == agent->agent_id;
        if (is_target && target_drawn) {
            return; // Don't draw target twice
        }
        if (agent_border_thickness != 0.f && agent->GetIsLivingType()) {
            Enqueue(shape, pos, size + agent_border_thickness, Colors::ARGB(static_cast<int>(alpha * 0.8), 0, 0, 0));
        }
        if (is_target) {
            auto border_color = color_target;
            if (const auto matches = GetCustomAgentsToDraw(agent)) {
                for (const auto* rule : *matches) {
                    if (!Colors::IsVisible(rule->border_color)) continue;
                    border_color = rule->border_color;
                    break;
                }
            }
            Enqueue(shape, pos, size + target_border_thickness, border_color);
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
    if (shape == Shape_None || (color & IM_COL32_A_MASK) == 0) return;
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
    rules_changed = true;
    custom_agents_by_allegiance.clear();
    compiled_name_patterns.clear();
    check_friends = check_guild = check_party = false;
    for (const CustomAgent* ca : custom_agents) {
        if (ca->allegiance >= 0) {
            custom_agents_by_allegiance[ca->allegiance].push_back(ca);
        }
        if (ca->match_name[0]) compiled_name_patterns.emplace(ca, TextUtils::StringToWString(ca->match_name));
        if (ca->active) {
            check_friends |= ca->player_relation == Friend;
            check_guild |= ca->player_relation == Guild;
            check_party |= ca->player_relation == MyParty || ca->player_relation == InParty;
        }
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
    agent_type = static_cast<AgentType>(ini->GetLongValue(section, VAR_NAME(agent_type), 0));
    identifier = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(identifier), identifier));
    identifier_active = ini->GetBoolValue(section, VAR_NAME(identifier_active), identifier != 0);
    std::snprintf(match_name, sizeof(match_name), "%s", ini->GetValue(section, VAR_NAME(match_name), ""));
    target_state = static_cast<TargetState>(ini->GetLongValue(section, VAR_NAME(target_state), target_state));
    player_relation = static_cast<PlayerRelation>(ini->GetLongValue(section, VAR_NAME(player_relation), player_relation));
    outpost_only = ini->GetBoolValue(section, VAR_NAME(outpost_only), outpost_only);
    border_color = Colors::Load(ini, section, VAR_NAME(border_color), 0xFFFFFF00);
    gadget_state = static_cast<GadgetState>(ini->GetLongValue(section, VAR_NAME(gadget_state), gadget_state));
    profession = static_cast<int>(ini->GetLongValue(section, VAR_NAME(profession), profession));
    boss_state = static_cast<int>(ini->GetLongValue(section, VAR_NAME(boss_state), boss_state));

    color = Colors::Load(ini, section, VAR_NAME(color), 0xFFF00000);
    color_text = Colors::Load(ini, section, VAR_NAME(color_text), 0xFFF00000);
    const int s = ini->GetLongValue(section, VAR_NAME(shape), 0);
    if (s >= 1 && s <= 4) {
        shape = static_cast<Shape_e>(s - 1);
    }
    size = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size), size));

    LegacyFlags flags;
    flags.color_active = ini->GetBoolValue(section, "color_active", flags.color_active);
    flags.color_text_active = ini->GetBoolValue(section, "color_text_active", flags.color_text_active);
    flags.shape_active = ini->GetBoolValue(section, "shape_active", flags.shape_active);
    flags.size_active = ini->GetBoolValue(section, "size_active", flags.size_active);
    flags.border_color_active = ini->GetBoolValue(section, "border_color_active", flags.border_color_active);
    ApplyLegacyFlags(flags);
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
    agent_type = static_cast<AgentType>(settings.agent_type);
    identifier = settings.identifier;
    identifier_active = settings.identifier_active || identifier != 0;
    std::snprintf(match_name, sizeof(match_name), "%s", settings.match_name.c_str());
    target_state = static_cast<TargetState>(settings.target_state);
    player_relation = static_cast<PlayerRelation>(settings.player_relation);
    outpost_only = settings.outpost_only;
    border_color = settings.border_color;
    gadget_state = static_cast<GadgetState>(settings.gadget_state);
    profession = settings.profession;
    boss_state = settings.boss_state;

    color = settings.color;
    color_text = settings.color_text;
    if (settings.shape >= Shape_None && settings.shape <= BigCircle) {
        shape = static_cast<Shape_e>(settings.shape);
    }
    size = settings.size;
}

void AgentRenderer::CustomAgent::ApplyLegacyFlags(const LegacyFlags& flags)
{
    if (!flags.color_active) color &= ~IM_COL32_A_MASK;
    if (!flags.color_text_active) color_text &= ~IM_COL32_A_MASK;
    if (!flags.border_color_active) border_color &= ~IM_COL32_A_MASK;
    if (!flags.size_active) size = 0.f;
    if (!flags.shape_active) shape = Shape_None;
    else if (shape == Shape_None) shape = Tear;
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
    settings.agent_type = agent_type;
    settings.identifier = identifier_active ? identifier : 0;
    settings.identifier_active = identifier_active;
    settings.match_name = match_name;
    settings.target_state = target_state;
    settings.player_relation = player_relation;
    settings.outpost_only = outpost_only;
    settings.border_color = border_color;
    settings.gadget_state = gadget_state;
    settings.profession = profession;
    settings.boss_state = boss_state;

    settings.color = color;
    settings.color_text = color_text;
    settings.shape = shape;
    settings.size = size;

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
    if (Colors::IsVisible(color)) {
        changed |= ImGui::ColorButtonPicker("##color", &color);
        if (ImGui::IsItemHovered()) {
            const ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
            ImGui::ColorTooltip("Minimap Color##color_tooltip", &col.x, 0);
        }

        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(cursor_pos += button_width);
    if (Colors::IsVisible(color_text)) {
        changed |= ImGui::ColorButtonPicker("##color_text", &color_text);
        if (ImGui::IsItemHovered()) {
            const ImVec4 col = ImGui::ColorConvertU32ToFloat4(color_text);
            ImGui::ColorTooltip("Name Tag Color##color_tooltip", &col.x, 0);
        }
        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(cursor_pos += button_width);
    static const char* types[] = {"Any", "Item", "Gadget", "NPC", "Player"};
    const auto type_name = agent_type >= Any && agent_type <= Player ? types[agent_type - Any] : "Unknown";
    ImGui::Text("%s [%s]", name, type_name);
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
            ImGui::SetTooltip("If this custom agent is active");
        }
        ImGui::SameLine();
        const float x = ImGui::GetCursorPosX();
        ImGui::BeginDisabled(is_default);
        if (ImGui::InputText("Name", name, 128)) {
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::ShowHelp("A name to help you remember what this is. Optional.");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputText("Group", group, sizeof(group))) {
            changed = true;
        }
        ImGui::ShowHelp("An optional tag to filter this list by, e.g. 'Farming' or 'Bosses'. Purely organisational.");
        ImGui::SetCursorPosX(x);
        static const char* agent_types[] = {"Any", "Item", "Gadget", "NPC", "Player"};
        auto type_selection = static_cast<int>(agent_type) - Any;
        if (ImGui::Combo("Agent type", &type_selection, agent_types, _countof(agent_types))) {
            agent_type = static_cast<AgentType>(type_selection + Any);
            changed = true;
            allegiance = -1;
            modelId = 0;
            identifier = 0;
            identifier_active = false;
            if (agent_type != NPC) { profession = 0; boss_state = 0; }
            if (agent_type != Gadget) gadget_state = AnyGadget;
            if (agent_type != Player) player_relation = AnyRelation;
            if (agent_type == Item || agent_type == Gadget) {
                dead_state = EitherDeadState;
                quest_state = EitherQuestState;
                combat_state = EitherCombat;
                weapon_state = EitherWeapon;
            }
        }
        ImGui::SetCursorPosX(x);
        ImGui::BeginDisabled(agent_type == Any);
        if (ImGui::Checkbox("Match identifier", &identifier_active)) changed = true;
        ImGui::EndDisabled();
        ImGui::SetCursorPosX(x);
        ImGui::BeginDisabled(agent_type == Any || !identifier_active);
        if (ImGui::InputInt("Identifier", reinterpret_cast<int*>(&identifier))) changed = true;
        ImGui::EndDisabled();
        ImGui::ShowHelp("Item model ID, gadget ID or NPC model ID, according to the chosen type. If unchecked, matches any identifier; checked also allows an exact ID of 0.");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputText("Match name", match_name, sizeof(match_name))) changed = true;
        ImGui::ShowHelp("Case-insensitive substring, or /pattern/flags for a regular expression, as in Loot Beacons.");
        if (const auto pattern = AgentRenderer::Instance().compiled_name_patterns.find(this);
            pattern != AgentRenderer::Instance().compiled_name_patterns.end() && !pattern->second.IsValid()) {
            ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "Invalid regex");
        }
        ImGui::SetCursorPosX(x);
        static const char* target_states[] = {"Either", "Targeted", "Not targeted", "Marked (/marktarget)"};
        if (ImGui::Combo("Target state", reinterpret_cast<int*>(&target_state), target_states, _countof(target_states))) changed = true;
        ImGui::SetCursorPosX(x);
        static const char* player_relations[] = {"Any player", "Self", "Other player", "Friend", "Guild member", "My party", "In a party"};
        ImGui::BeginDisabled(agent_type != Player);
        if (ImGui::Combo("Player relation", reinterpret_cast<int*>(&player_relation), player_relations, _countof(player_relations))) changed = true;
        ImGui::EndDisabled();
        ImGui::SetCursorPosX(x);
        static const char* gadget_states[] = {"Any gadget", "Locked chest (closed)", "Locked chest (opened)", "Other gadget"};
        ImGui::BeginDisabled(agent_type != Gadget);
        if (ImGui::Combo("Gadget state", reinterpret_cast<int*>(&gadget_state), gadget_states, _countof(gadget_states))) changed = true;
        ImGui::EndDisabled();
        ImGui::SetCursorPosX(x);
        static const char* professions[] = {"Any", "Warrior", "Ranger", "Monk", "Necromancer", "Mesmer", "Elementalist", "Assassin", "Ritualist", "Paragon", "Dervish"};
        ImGui::BeginDisabled(agent_type != NPC);
        if (ImGui::Combo("Profession", &profession, professions, _countof(professions))) changed = true;
        static const char* boss_states[] = {"Either", "Boss", "Not boss"};
        if (ImGui::Combo("Boss state", &boss_state, boss_states, _countof(boss_states))) changed = true;
        ImGui::EndDisabled();
        ImGui::SetCursorPosX(x);
        if (ImGui::Checkbox("Outposts only", &outpost_only)) changed = true;
        ImGui::SetCursorPosX(x);
        static const char* typed_allegiances[] = {"Any", "Ally", "Neutral", "Enemy", "Spirit/Pet", "Minion", "NPC/Minipet"};
        int allegiance_combo = allegiance < 0 ? 0 : allegiance;
        if ((agent_type == NPC || agent_type == Player) &&
            ImGui::Combo("Allegiance", &allegiance_combo, typed_allegiances, 7)) {
            allegiance = allegiance_combo == 0 ? -1 : allegiance_combo;
            changed = true;
        }
        ImGui::ShowHelp("Optional allegiance filter for NPCs and players.");
        if (agent_type == NPC || agent_type == Player || agent_type == Any) {
            ImGui::SetCursorPosX(x);
            static const char* dead_state_items[] = {"Dead", "Alive", "Either"};
            if (ImGui::Combo("Dead state", (int*)&dead_state, dead_state_items, 3)) {
                changed = true;
            }
            ImGui::SetCursorPosX(x);
            static const char* quest_state_items[] = {"Quest giver", "Not quest giver", "Either"};
            if (ImGui::Combo("Quest state", (int*)&quest_state, quest_state_items, 3)) {
                changed = true;
            }
        }
        ImGui::SetCursorPosX(x);
        if (ImGui::InputInt("Map ID", (int*)&mapId)) {
            changed = true;
        }
        ImGui::ShowHelp("The map where it will be applied. Optional. Leave 0 for any map");
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

        if (Colors::DrawSettingHueWheel("Color", &color, ImGuiColorEditFlags_AlphaBar)) {
            changed = true;
        }
        ImGui::ShowHelp("Minimap marker color. Alpha 0 inherits the next matching color.");
        if (Colors::DrawSettingHueWheel("Target border color", &border_color, ImGuiColorEditFlags_AlphaBar)) changed = true;
        ImGui::ShowHelp("Alpha 0 inherits the next matching target border color.");

        if (Colors::DrawSettingHueWheel("Text color", &color_text, ImGuiColorEditFlags_AlphaBar)) {
            changed = true;
        }
        ImGui::ShowHelp("In-game name tag color. Alpha 0 inherits the next matching color or the game's default.");

        if (ImGui::DragFloat("Size", &size, 1.0f, 0.0f, 200.0f)) {
            changed = true;
        }
        ImGui::ShowHelp("Minimap marker size. Zero inherits the next matching size.");

        static const char* items[] = {"Inherit", "Tear", "Circle", "Square", "Big Circle"};
        auto shape_selection = static_cast<int>(shape) + 1;
        if (ImGui::Combo("Shape", &shape_selection, items, _countof(items))) {
            shape = static_cast<Shape_e>(shape_selection - 1);
            changed = true;
        }
        ImGui::ShowHelp("Inherit uses the next matching shape or the minimap default.");

        ImGui::Spacing();

        // === Move and delete buttons ===
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float width = (ImGui::CalcItemWidth() - spacing * 2) / 3;
        if (ImGui::Button("Move Up", ImVec2(width, 0))) {
            op = Operation::MoveUp;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Move the color up in the list");
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Move Down", ImVec2(width, 0))) {
            op = Operation::MoveDown;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Move the color down in the list");
        }
        ImGui::SameLine(0, spacing);
        ImGui::BeginDisabled(is_default);
        if (ImGui::Button("Delete", ImVec2(width, 0))) {
            ImGui::OpenPopup("Delete Color?");
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(is_default ? "Default rows can't be deleted; uncheck \"active\" to disable this one instead" : "Delete the color");
        }

        if (ImGui::BeginPopupModal("Delete Color?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure?\nThis operation cannot be undone\n\n");
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                op = Operation::Delete;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
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
