#include "stdafx.h"

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

#include <Windows/AgentAppearanceWindow.h>

#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>
#include <Utils/SettingsDoc.h>

namespace {

    using CustomAgent = AgentAppearanceWindow::CustomAgent;
    using RuleSettings = AgentAppearanceWindow::RuleSettings;
    using AgentAppearance = AgentAppearanceWindow::AgentAppearance;
    using MarkerAppearance = AgentAppearanceWindow::MarkerAppearance;
    using Shape_e = AgentAppearanceWindow::Shape_e;
    using enum AgentAppearanceWindow::Shape_e;
    using AgentType = AgentAppearanceWindow::AgentType;
    using DeadState = AgentAppearanceWindow::DeadState;
    using enum AgentAppearanceWindow::CombatState;
    using enum AgentAppearanceWindow::WeaponState;
    using enum AgentAppearanceWindow::DeadState;
    using enum AgentAppearanceWindow::QuestState;
    using enum AgentAppearanceWindow::AgentType;
    using enum AgentAppearanceWindow::TargetState;
    using enum AgentAppearanceWindow::PlayerRelation;
    using enum AgentAppearanceWindow::GadgetState;
    constexpr auto LayerCount = AgentAppearanceWindow::LayerCount;

    constexpr const char* profession_names[] = {"Any", "Warrior", "Ranger", "Monk", "Necromancer", "Mesmer", "Elementalist", "Assassin", "Ritualist", "Paragon", "Dervish"};
    constexpr const char* allegiance_names[] = {"Any", "Ally", "Neutral", "Enemy", "Spirit/Pet", "Minion", "NPC/Minipet"};
    constexpr const char* shape_names[] = {"Tear", "Circle", "Square", "Big Circle", "Star"};
    constexpr const char* layer_names[] = {"1 (Primary)", "2", "3", "4", "5 (Back)"};
    constexpr Color default_profession_colors[] = {0xFF666666, 0xFFEEAA33, 0xFF55AA00, 0xFF4444BB, 0xFF00AA55, 0xFF8800AA,
                                                   0xFFBB3333, 0xFFAA0088, 0xFF00AAAA, 0xFF996600, 0xFF7777CC};

    struct SpiritDefault {
        uint32_t identifier;
        const char* label;
        Color color;
    };
    constexpr SpiritDefault spirit_defaults[] = {
        {GW::Constants::ModelID::EoE, "EoE", 0x3200FF00},
        {GW::Constants::ModelID::QZ, "QZ", 0x320000FF},
        {GW::Constants::ModelID::Winnowing, "Winnowing", 0x3200FFFF},
        {GW::Constants::ModelID::FrozenSoil, "Frozen Soil", 0x00FEFFFF},
        {GW::Constants::ModelID::Symbiosis, "Symbiosis", 0x00FF00FF}
    };

    enum AgentFlag : uint32_t {
        FlagDead = 1u,
        FlagQuest = 2u,
        FlagCombat = 4u,
        FlagTargeted = 8u,
        FlagWeapon = 16u,
        FlagClosedChest = 32u,
        FlagOpenedChest = 64u,
        FlagBoss = 128u,
        FlagMarked = 256u
    };
    enum RelationFlag : uint32_t {
        RelationSelf = 1u,
        RelationFriend = 2u,
        RelationGuild = 4u,
        RelationMyParty = 8u,
        RelationInParty = 16u
    };
    enum RuleCheck : uint32_t {
        CheckIdentifier = 1u,
        CheckProfession = 2u,
        CheckGadget = 4u,
        CheckSelf = 8u,
        CheckFriend = 16u,
        CheckGuild = 32u,
        CheckMyParty = 64u,
        CheckAnyParty = 128u,
        CheckName = 256u
    };
    enum RuleProperty : uint32_t {
        PropShape = 1u,
        PropColor = 2u,
        PropBorderColor = 4u,
        PropScale = 8u,
        PropBorderThickness = 16u,
        AllProperties = 31u
    };
    constexpr uint32_t relation_flags[] = {0u, RelationSelf, RelationSelf, RelationFriend, RelationGuild, RelationMyParty, RelationInParty};
    constexpr uint32_t relation_checks[] = {0u, CheckSelf, CheckSelf, CheckFriend, CheckGuild, CheckMyParty, CheckAnyParty};

    struct CompiledRule {
        const AppearanceRule* rule = nullptr;
        uint32_t required_flags = 0;
        uint32_t rejected_flags = 0;
        uint32_t required_relations = 0;
        uint32_t rejected_relations = 0;
        uint32_t identifier = 0;
        const TextUtils::SearchPattern<wchar_t>* name_pattern = nullptr;
    };
    struct RuleBucket {
        std::vector<CompiledRule> rules;
        uint32_t checks = 0;
    };
    struct MatchKey {
        uint32_t identifier = 0;
        uint32_t map_id = 0;
        uint32_t flags = 0;
        int allegiance = -1;
        GW::Constants::Profession profession = GW::Constants::Profession::None;
        AgentType type = Any;
        uint32_t relations = 0;
        bool operator==(const MatchKey&) const = default;
    };
    struct MatchCache {
        const GW::Agent* agent = nullptr;
        uint32_t generation = 0;
        MatchKey key;
        bool valid = false;
        bool name_requested = false;
        std::wstring name;
        std::vector<const CustomAgent*> matches;
    };

    std::vector<CustomAgent*> custom_agents;
    std::unordered_map<const CustomAgent*, TextUtils::SearchPattern<wchar_t>> compiled_name_patterns;
    bool custom_agents_loaded = false;
    float size_default = 100.f;
    Shape_e default_shape = Tear;
    float agent_border_thickness = 0.f;
    float target_border_thickness = 50.f;
    Color color_agent_modifier = 0x001E1E1E;
    Color color_agent_damaged_modifier = 0x00505050;
    Color color_default = 0xFFFFFFFF;
    std::array<const AppearanceRule*, 11> profession_rules{};
    uint32_t matcher_map_id = UINT32_MAX;
    uint32_t matcher_instance_type = UINT32_MAX;
    bool matcher_context_valid = false;
    std::unordered_map<uint32_t, MatchCache> match_cache;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> pending_names;
    uint32_t next_name_token = 0;
    std::array<RuleBucket, 6> rule_buckets;
    unsigned int cur_ui_id = 0;
    bool pending_focus = false;
    bool hooks_added = false;
    GW::HookEntry UIMsg_Entry;
    GW::HookEntry ChatCmd_HookEntry;

    uint32_t StateMask(const int state)
    {
        return state >= 0 && state < 2 ? 1u << state : 0;
    }

    bool InputString(const char* label, std::string& value, const char* hint = "")
    {
        constexpr auto resize = [](ImGuiInputTextCallbackData* data) {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                auto* text = static_cast<std::string*>(data->UserData);
                text->resize(static_cast<size_t>(data->BufTextLen));
                data->Buf = text->data();
            }
            return 0;
        };
        return ImGui::InputTextWithHint(label, hint, value.data(), value.capacity() + 1, ImGuiInputTextFlags_CallbackResize, resize, &value);
    }

    GW::Constants::Profession GetAgentProfession(const GW::AgentLiving* agent)
    {
        if (!agent) return GW::Constants::Profession::None;
        if (agent->primary != GW::Constants::ProfessionByte::None) return static_cast<GW::Constants::Profession>(agent->primary);
        const auto* npc = GW::Agents::GetNPCByID(agent->player_number);
        return npc ? static_cast<GW::Constants::Profession>(npc->primary) : GW::Constants::Profession::None;
    }

    bool IsLockedChest(const GW::Agent* agent)
    {
        return agent && agent->GetIsGadgetType() && wcseq(GW::Agents::GetAgentEncName(agent->agent_id), GW::EncStrings::LockedChest);
    }

    bool IsOpenedLockedChest(const GW::Agent* agent)
    {
        return IsLockedChest(agent) && !GW::Agents::GetAgentMatchesFlags(agent, GW::TargetFilter::Gadgets);
    }

    uint32_t MarkIdentifier(const GW::Agent* agent)
    {
        switch (agent->type) {
            case 0xdb:
                return agent->GetAsAgentLiving()->player_number;
            case 0x200:
                return agent->GetAsAgentGadget()->extra_type;
            case 0x400:
                return static_cast<uint32_t>(agent->GetAsAgentItem()->x);
            default:
                return 0;
        }
    }

    std::wstring EncodedName(const GW::Agent* agent)
    {
        const auto* name = GW::Agents::GetAgentEncName(agent);
        return name ? name : L"";
    }

    struct MarkedTarget {
        uint32_t type;
        uint32_t identifier;
        std::wstring name;

        explicit MarkedTarget(const GW::Agent* agent)
            : type(agent->type), identifier(MarkIdentifier(agent)), name(EncodedName(agent)) {}

        [[nodiscard]] bool Matches(const GW::Agent* agent) const
        {
            return agent && agent->type == type && MarkIdentifier(agent) == identifier && EncodedName(agent) == name;
        }
    };

    std::map<uint32_t, MarkedTarget> marked_targets;

    void CHAT_CMD_FUNC(CmdMarkTarget)
    {
        if (argc > 1 && wcscmp(argv[1], L"clearall") == 0) {
            marked_targets.clear();
            return;
        }
        const auto agent = GW::Agents::GetTarget();
        if (!agent) return;
        if (argc > 1 && (wcscmp(argv[1], L"clear") == 0 || wcscmp(argv[1], L"remove") == 0)) {
            marked_targets.erase(agent->agent_id);
            return;
        }
        marked_targets.try_emplace(agent->agent_id, agent);
    }

    void CHAT_CMD_FUNC(CmdClearMarkTarget)
    {
        marked_targets.clear();
    }

    void OnAgentAdded(const uint32_t agent_id)
    {
        AgentAppearanceWindow::InvalidateAppearance(agent_id);
        const auto found = marked_targets.find(agent_id);
        if (found != marked_targets.end() && !found->second.Matches(GW::Agents::GetAgentByID(agent_id))) marked_targets.erase(found);
    }

    void EditRule(const CustomAgent* rule)
    {
        for (auto* custom : custom_agents) custom->edit_open = custom->focus_editor = custom == rule;
    }

    void DeleteRules()
    {
        for (const auto* rule : custom_agents) delete rule;
        custom_agents.clear();
        match_cache.clear();
        pending_names.clear();
    }

    float BaseMarkerSize()
    {
        return std::isfinite(size_default) && size_default > 0.f ? size_default : 100.f;
    }

    bool MatchesTypeOption(const CustomAgent& rule, const CustomAgent::AgentTypeOption& option)
    {
        return option.type == rule.agent_type && (rule.agent_type != Player || option.relation == rule.player_relation)
            && (rule.agent_type != Gadget || option.gadget == rule.gadget_state);
    }

    void EnsureRuleMatchers()
    {
        if (!matcher_context_valid || matcher_map_id != static_cast<uint32_t>(GW::Map::GetMapID())
            || matcher_instance_type != static_cast<uint32_t>(GW::Map::GetInstanceType())) {
            AgentAppearanceWindow::RebuildRuleMatchers();
        }
    }

    void OnNameDecoded(void* context, const wchar_t* decoded)
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

    void RefreshMatches(const GW::Agent* agent)
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
        const auto checks = bucket.checks;
        const auto item_data = item && (checks & (CheckIdentifier | CheckName)) ? GW::Items::GetItemById(item->item_id) : nullptr;
        const auto targeted = GW::Agents::GetTargetId() == agent->agent_id || AgentAppearanceWindow::auto_target_id == agent->agent_id;
        MatchKey key;
        key.identifier = item_data ? item_data->model_id : gadget ? gadget->gadget_id : living ? living->player_number : 0;
        key.map_id = matcher_map_id;
        key.allegiance = living ? static_cast<int>(living->allegiance) : -1;
        key.profession = checks & CheckProfession ? GetAgentProfession(living) : GW::Constants::Profession::None;
        key.type = agent_type;
        key.flags = (living && living->GetIsDead() ? FlagDead : 0u) | (living && living->GetHasQuest() ? FlagQuest : 0u)
            | (living && living->GetInCombatStance() ? FlagCombat : 0u) | (targeted ? FlagTargeted : 0u)
            | (marked_targets.contains(agent->agent_id) ? FlagMarked : 0u)
            | (living && living->weapon_type != 0 && living->weapon_type != 512 ? FlagWeapon : 0u)
            | (gadget && (checks & CheckGadget) && IsLockedChest(agent) ? IsOpenedLockedChest(agent) ? FlagOpenedChest : FlagClosedChest : 0u)
            | (living && living->GetHasBossGlow() ? FlagBoss : 0u);
        if (agent_type == Player) {
            if ((checks & CheckSelf) && agent->agent_id == GW::Agents::GetControlledCharacterId()) key.relations |= RelationSelf;
            if (checks & (CheckFriend | CheckGuild)) {
                const auto decoded = living ? GW::PlayerMgr::GetPlayerName(living->login_number) : nullptr;
                const auto encoded = decoded ? nullptr : GW::Agents::GetAgentEncName(agent->agent_id);
                const auto player_name = decoded ? std::wstring(decoded) : encoded ? TextUtils::GetPlayerNameFromEncodedString(encoded) : std::wstring{};
                if ((checks & CheckFriend) && !player_name.empty() && GW::FriendListMgr::GetFriend(nullptr, player_name.c_str(), GW::FriendType::Friend)) key.relations |= RelationFriend;
                const auto guild = (checks & CheckGuild) && !player_name.empty() ? GW::GetGuildContext() : nullptr;
                if (guild) {
                    for (const auto member : guild->player_roster) {
                        if (member && member->current_name[0] && player_name == member->current_name) {
                            key.relations |= RelationGuild;
                            break;
                        }
                    }
                }
            }
            if ((checks & CheckMyParty) && ToolboxUtils::IsAgentInMyParty(agent->agent_id)) key.relations |= RelationMyParty;
            if ((checks & CheckAnyParty) && ToolboxUtils::IsAgentInParty(agent->agent_id)) key.relations |= RelationInParty;
        }
        if (cached.valid && cached.key == key) return;
        cached.key = key;
        cached.valid = true;
        cached.matches.clear();
        for (const auto& entry : bucket.rules) {
            const auto* rule = entry.rule;
            if (entry.identifier && entry.identifier != key.identifier) continue;
            if (rule->allegiance >= 0 && rule->allegiance != key.allegiance) continue;
            if (rule->profession != GW::Constants::Profession::None && rule->profession != key.profession) continue;
            if ((key.flags & entry.required_flags) != entry.required_flags || (key.flags & entry.rejected_flags)) continue;
            if ((key.relations & entry.required_relations) != entry.required_relations || (key.relations & entry.rejected_relations)) continue;
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
        }
    }

    void CopyProperties(MarkerAppearance& marker, const MarkerAppearance& source, const uint32_t properties)
    {
        if (properties & PropShape) marker.shape = source.shape;
        if (properties & PropColor) marker.color = source.color;
        if (properties & PropBorderColor) marker.border_color = source.border_color;
        if (properties & PropScale) marker.size = source.size;
        if (properties & PropBorderThickness) marker.border_thickness = source.border_thickness;
    }

    size_t ResolveAppearance(const GW::Agent* agent, AgentAppearance& appearance)
    {
        appearance = {};
        if (!agent) return 0;
        const auto is_target = agent->agent_id == GW::Agents::GetTargetId() || agent->agent_id == AgentAppearanceWindow::auto_target_id;
        appearance.layers[0] = {default_shape, color_default, color_default, is_target ? target_border_thickness : agent_border_thickness, BaseMarkerSize()};
        appearance.active_layers = 1u;
        std::array<uint32_t, LayerCount> resolved{};
        std::array<const AppearanceRule*, LayerCount> color_rules{};
        uint8_t stopped_layers = 0;
        size_t applied_rules = 0;
        const auto* matches = AgentAppearanceWindow::GetAppearanceRules(agent);
        if (matches) for (const auto* rule : *matches) {
            auto applied = !appearance.name_tag_color && rule->override_text_color;
            if (applied) appearance.name_tag_color = rule->color_text.value;
            const auto layer = static_cast<size_t>(rule->layer);
            const auto bit = static_cast<uint8_t>(1u << layer);
            if (!(stopped_layers & bit)) {
                const auto set_properties = (rule->shape != Shape_None ? PropShape : 0u) | (rule->override_color ? PropColor : 0u)
                    | (rule->override_border_color ? PropBorderColor : 0u) | (rule->scale > 0.f ? PropScale : 0u)
                    | (rule->border_thickness > 0.f ? PropBorderThickness : 0u);
                const auto properties = set_properties & ~resolved[layer];
                CopyProperties(appearance.layers[layer], {rule->shape, rule->color, rule->border_color, rule->border_thickness, BaseMarkerSize() * rule->scale}, properties);
                if (properties & PropColor) color_rules[layer] = rule;
                resolved[layer] |= properties;
                if (resolved[layer]) appearance.active_layers |= bit;
                if (rule->stop_processing_rules) stopped_layers |= bit;
                applied |= properties || rule->stop_processing_rules;
            }
            if (applied) ++applied_rules;
        }
        for (size_t layer = 1; layer < LayerCount; ++layer) {
            const auto inherited = AllProperties & ~resolved[layer];
            CopyProperties(appearance.layers[layer], appearance.layers[0], inherited);
            if (inherited & PropColor) color_rules[layer] = color_rules[0];
        }
        const auto* living = agent->GetAsAgentLiving();
        const auto* dead_npc = living && living->GetIsDead() && living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
        const auto hidden_dead_spirit = dead_npc && (dead_npc->model_file_id == 0x22A34 || dead_npc->model_file_id == 0x2D0E4 || dead_npc->model_file_id == 0x2D07E);
        const auto damaged_enemy = living && !living->GetIsDead() && living->allegiance == GW::Constants::Allegiance::Enemy && living->hp <= 0.9f;
        for (size_t layer = 0; layer < LayerCount; ++layer) {
            auto& marker = appearance.layers[layer];
            if (damaged_enemy && color_rules[layer] && color_rules[layer]->target_state != Marked) marker.color = Colors::Sub(marker.color, color_agent_damaged_modifier);
            if (hidden_dead_spirit) marker.color = 0;
        }
        return applied_rules;
    }

    void OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage msgid, void* wParam, void*)
    {
        switch (msgid) {
            case GW::UI::UIMessage::kMapLoaded:
                AgentAppearanceWindow::ResetAppearanceCache();
                break;
            case GW::UI::UIMessage::kAgentUpdate:
                OnAgentAdded(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wParam)));
                break;
        }
    }

    CustomAgent* AddDefaultRule(const AgentType agent_type, const std::optional<Color> color = std::nullopt, const float scale = 0.f,
                                const Shape_e shape = Shape_None, const int allegiance = -1, const DeadState dead = EitherDeadState)
    {
        auto* rule = new CustomAgent();
        rule->color = color.value_or(0);
        rule->override_color = color.has_value();
        rule->agent_type = agent_type;
        rule->allegiance = allegiance;
        rule->dead_states = StateMask(dead);
        rule->scale = scale;
        rule->shape = shape;
        rule->group = "Defaults";
        custom_agents.push_back(rule);
        return rule;
    }

    void SeedDefaults()
    {
        using GW::Constants::Allegiance;
        constexpr auto enemy = static_cast<int>(Allegiance::Enemy);
        for (const auto& spirit : spirit_defaults) {
            auto* rule = AddDefaultRule(NPC, spirit.color, 0.f, BigCircle, -1, Alive);
            rule->active = Colors::IsVisible(spirit.color);
            rule->identifier = spirit.identifier;
            rule->stop_processing_rules = true;
        }
        struct NpcDefault {
            Allegiance allegiance;
            uint32_t quest_states;
            Color color;
            float scale;
        };
        constexpr NpcDefault npc_defaults[] = {
            {Allegiance::Neutral, 0u, 0xFF0000DC, 1.f},
            {Allegiance::Ally_NonAttackable, 1u << NotQuestGiver, 0xFF00B300, 1.f},
            {Allegiance::Npc_Minipet, 1u << NotQuestGiver, 0xFF99FF99, 1.f},
            {Allegiance::Spirit_Pet, 1u << NotQuestGiver, 0xFF608000, 1.f},
            {Allegiance::Minion, 1u << NotQuestGiver, 0xFF008060, .5f},
        };
        for (const auto& npc : npc_defaults) {
            AddDefaultRule(NPC, npc.color, npc.scale, Shape_None, static_cast<int>(npc.allegiance), Alive)->quest_states = npc.quest_states;
        }
        auto* target = AddDefaultRule(Any);
        target->target_state = Targeted;
        target->border_color = 0xFFFFFF00;
        target->override_border_color = true;
        AddDefaultRule(Any, 0xFFFFFC00, 1.f)->target_state = Marked;
        AddDefaultRule(NPC, std::nullopt, 1.25f)->boss_states = 1u;
        AddDefaultRule(NPC, 0xFF320000, 1.f, Shape_None, enemy, Dead);
        for (size_t i = 1; i < std::size(profession_names); ++i) {
            auto* rule = AddDefaultRule(NPC, default_profession_colors[i], 0.f, Shape_None, enemy, Alive);
            rule->profession = static_cast<GW::Constants::Profession>(i);
            rule->boss_states = 1u;
        }
        AddDefaultRule(NPC, 0xFFF00000, 1.f, Shape_None, enemy, Alive);
        for (const auto allegiance : {Allegiance::Ally_NonAttackable, Allegiance::Npc_Minipet, Allegiance::Spirit_Pet, Allegiance::Minion}) {
            AddDefaultRule(NPC, 0x64006400, 1.f, Shape_None, static_cast<int>(allegiance), Dead);
            AddDefaultRule(NPC, 0xFF99FF99, 1.f, Shape_None, static_cast<int>(allegiance), Alive)->quest_states = 1u << QuestGiver;
        }
        AddDefaultRule(Item, 0xFF0000F0, .25f, Quad);
        AddDefaultRule(Gadget, 0xFF0000C8, .5f, Quad)->gadget_state = ClosedChest;
        AddDefaultRule(Gadget, 0xFF0000C8, .5f, Quad)->gadget_state = OpenedChest;
        AddDefaultRule(Gadget, 0xFF0000C8, .5f, Quad)->gadget_state = OtherGadget;
        AddDefaultRule(Player, 0xFFFF8000, 1.f, Shape_None, -1, Alive)->player_relation = Self;
        AddDefaultRule(Player, 0x64FF8000, 1.f, Shape_None, -1, Dead)->player_relation = Self;
        AddDefaultRule(Player, 0xFF00B300, 1.f, Shape_None, -1, Alive)->player_relation = Other;
        AddDefaultRule(Any);
    }
}

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
    return marked_targets.contains(agent_id);
}

void AgentAppearanceWindow::ResetAppearanceCache()
{
    matcher_context_valid = false;
    match_cache.clear();
    pending_names.clear();
    marked_targets.clear();
}

void AgentAppearanceWindow::GetAgentAppearance(const GW::Agent* agent, AgentAppearance& appearance)
{
    ResolveAppearance(agent, appearance);
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
    SettingsRegistry::RegisterField(module, "size_default", &size_default);
    SettingsRegistry::RegisterField(module, "agent_border_thickness", &agent_border_thickness);
    SettingsRegistry::RegisterField(module, "target_border_thickness", &target_border_thickness);
    SettingsRegistry::RegisterField(module, "default_shape", reinterpret_cast<int*>(&default_shape));
    if (!hooks_added) {
        hooks_added = true;
        GW::UI::RegisterUIMessageCallback(&UIMsg_Entry, GW::UI::UIMessage::kMapLoaded, OnUIMessage);
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

void AgentAppearanceWindow::LoadCustomAgents(const SettingsDoc& doc)
{
    custom_agents_loaded = false;
    matcher_context_valid = false;
    DeleteRules();
    std::vector<RuleSettings> saved;
    if (!doc.Has("Game Settings", "appearance_rules")) {
        SeedDefaults();
    }
    else if (!doc.Get("Game Settings", "appearance_rules", saved)) {
        Log::Error("Failed to parse appearance rules");
        return;
    }
    for (const auto& settings : saved) custom_agents.push_back(new CustomAgent(settings));
    custom_agents_loaded = true;
    RebuildRuleMatchers();
}

void AgentAppearanceWindow::SaveCustomAgents(SettingsDoc& doc)
{
    if (!custom_agents_loaded) return;
    std::vector<RuleSettings> entries;
    entries.reserve(custom_agents.size());
    for (const auto* rule : custom_agents) entries.push_back(*rule);
    doc.Set("Game Settings", "appearance_rules", entries);
}

void AgentAppearanceWindow::ResetAppearanceSettings()
{
    color_default = 0xFFFFFFFF;
    color_agent_modifier = 0x001E1E1E;
    color_agent_damaged_modifier = 0x00505050;
    size_default = 100.0f;
    agent_border_thickness = 0.f;
    target_border_thickness = 50.0f;
    default_shape = Tear;
}

void AgentAppearanceWindow::DrawSettings()
{
    if (ImGui::DragFloat("Default Size", &size_default, 1.f, 1.f, 0.f, "%.0f")) {
        size_default = std::isfinite(size_default) ? std::max(1.f, size_default) : 100.f;
    }
    ImGui::ShowHelp("Base minimap marker size. Each rule's scale multiplies this value.");
    ImGui::Combo("Default Shape", reinterpret_cast<int*>(&default_shape), shape_names, _countof(shape_names));
    ImGui::ShowHelp("The default shape of agents.");
    Colors::DrawSettingHueWheel("Default Color", &color_default);
    ImGui::ShowHelp("Used for marker and border colours when no matching rule overrides them.");
    ImGui::SliderFloat("Agent Border thickness", &agent_border_thickness, 0.f, 100.f, "%.0f");
    ImGui::ShowHelp("Border width for agents when no matching rule sets a Border thickness.");
    ImGui::SliderFloat("Target Border thickness", &target_border_thickness, 0.f, 100.f, "%.0f");
    ImGui::ShowHelp("Border width for the target when no matching rule sets a Border thickness.");
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
        ImGui::SetTooltip("Only affects what's shown here. Rules are evaluated top to bottom, independently for each property in each layer. Name tag colour is shared across layers.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(add_width, 0.f))) {
        auto* rule = new CustomAgent();
        rule->active = false;
        custom_agents.insert(custom_agents.begin(), rule);
        EditRule(rule);
        RebuildRuleMatchers();
    }
    const auto filter = TextUtils::ToLower(group_filter);
    const auto matches_filter = [&filter](const CustomAgent* rule) {
        return filter.empty() || TextUtils::ToLower(rule->Label()).contains(filter) || TextUtils::ToLower(rule->group).contains(filter);
    };

    const auto* target = GW::Agents::GetTarget();
    if (!target && auto_target_id) target = GW::Agents::GetAgentByID(auto_target_id);
    AgentAppearance target_appearance;
    std::vector<const AppearanceRule*> target_rules;
    size_t applied_rules = 0;
    if (target) {
        applied_rules = ResolveAppearance(target, target_appearance);
        if (const auto* matches = GetAppearanceRules(target)) target_rules = *matches;
    }
    bool changed = false;
    auto move_from = custom_agents.size();
    auto move_to = move_from;
    const auto footer_height = ImGui::GetTextLineHeightWithSpacing() * (target ? 8.f : 2.f) + ImGui::GetStyle().ItemSpacing.y + 1.f;
    ImGui::BeginChild("##custom_agents_scroll", ImVec2(0.f, -footer_height), true);
    if (ImGui::BeginTable("AppearanceRules", 2, ImGuiTableFlags_SizingStretchProp)) {
        const auto button_size = ImGui::GetFrameHeight();
        const ImVec2 button(button_size, button_size);
        ImGui::TableSetupColumn("Rule", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, button_size * 4.f + ImGui::GetStyle().ItemSpacing.x * 3.f);
        for (size_t i = 0; i < custom_agents.size(); ++i) {
            auto* custom = custom_agents[i];
            if (!matches_filter(custom)) continue;
            ImGui::PushID(static_cast<int>(custom->ui_id));
            ImGui::TableNextRow();
            if (std::ranges::contains(target_rules, custom)) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));
            }
            ImGui::TableNextColumn();
            changed |= custom->DrawHeader();
            ImGui::TableNextColumn();
            if (ImGui::ButtonWithHint(ICON_FA_EDIT, "Edit appearance rule", button)) EditRule(custom);
            ImGui::SameLine();
            ImGui::BeginDisabled(i == 0);
            if (ImGui::ButtonWithHint(ICON_FA_ARROW_UP, "Move rule up", button)) {
                move_from = i;
                move_to = i - 1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i + 1 == custom_agents.size());
            if (ImGui::ButtonWithHint(ICON_FA_ARROW_DOWN, "Move rule down", button)) {
                move_from = i;
                move_to = i + 1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::ButtonWithHint(ICON_FA_TRASH, "Delete appearance rule", button)) {
                const auto message = std::format("Delete appearance rule '{}'?\nThis cannot be undone.", custom->Label());
                ImGui::ConfirmDialog(message.c_str(), [rule_id = custom->ui_id](const bool confirmed, void*) {
                    if (!confirmed) return;
                    const auto it = std::ranges::find(custom_agents, rule_id, &CustomAgent::ui_id);
                    if (it == custom_agents.end()) return;
                    delete *it;
                    custom_agents.erase(it);
                    RebuildRuleMatchers();
                });
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    if (move_from < custom_agents.size()) {
        std::swap(custom_agents[move_from], custom_agents[move_to]);
        changed = true;
    }
    if (changed) RebuildRuleMatchers();
    ImGui::SmallConfirmButton("Restore Defaults", "Replace all appearance rules with defaults?\nColours, sizes and border thickness will also be reset.\nThis cannot be undone.", [](const bool confirmed, void*) {
        if (!confirmed) return;
        DeleteRules();
        ResetAppearanceSettings();
        SeedDefaults();
        RebuildRuleMatchers();
        group_filter[0] = '\0';
    });
    ImGui::Separator();
    if (!target) {
        ImGui::TextDisabled("No current target.");
        return;
    }
    ImGui::Text("Target #%u: %zu matching rules, %zu applied", target->agent_id, target_rules.size(), applied_rules);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Highlighted rows match this target. Applied rules supply the first override for a property or stop their layer.");
    }
    static int preview_layer = 0;
    ImGui::Combo("Preview layer", &preview_layer, layer_names, static_cast<int>(LayerCount));
    const auto& marker = target_appearance.layers[preview_layer];
    if (!(target_appearance.active_layers & (1u << preview_layer))) ImGui::TextDisabled("No marker in this layer.");
    const auto shape_name = marker.shape >= Tear && marker.shape <= Star ? shape_names[marker.shape] : "None";
    ImGui::Text("Size: %.1f (%.2fx)    Shape: %s", marker.size, marker.size / BaseMarkerSize(), shape_name);
    const auto draw_color_readout = [](const char* label, const Color color) {
        ImGui::PushID(label);
        const auto rgba = ImGui::ColorConvertU32ToFloat4(color);
        const auto size = ImGui::GetTextLineHeight();
        ImGui::ColorButton("##preview", rgba, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(size, size));
        ImGui::SameLine();
        ImGui::Text("%s: 0x%08X (%.0f%% alpha)", label, color, rgba.w * 100.f);
        ImGui::PopID();
    };
    draw_color_readout("Marker", marker.color);
    if (marker.shape == BigCircle || !Colors::IsVisible(marker.color)) {
        ImGui::TextDisabled("Border: none (%s)", marker.shape == BigCircle ? "Big Circle" : "transparent marker");
    }
    else {
        draw_color_readout("Border", marker.border_color);
        ImGui::SameLine();
        ImGui::Text("Width: %.1f", marker.border_thickness);
    }
    if (target_appearance.name_tag_color) draw_color_readout("Name tag", *target_appearance.name_tag_color);
    else ImGui::TextDisabled("Name tag: game colour");
}

void AgentAppearanceWindow::DrawRuleEditor()
{
    bool changed = false;
    for (auto* rule : custom_agents) {
        if (rule->edit_open) changed |= rule->DrawSettings();
    }
    if (changed) RebuildRuleMatchers();
}

void AgentAppearanceWindow::ReleaseAppearanceHooks()
{
    GW::UI::RemoveUIMessageCallback(&UIMsg_Entry);
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    hooks_added = false;
    ResetAppearanceCache();
    DeleteRules();
    RebuildRuleMatchers();
    custom_agents_loaded = false;
}

bool AgentAppearanceWindow::AppearanceRulesLoaded() { return custom_agents_loaded; }

Color AgentAppearanceWindow::GetProfessionColor(const GW::Constants::Profession profession)
{
    const auto index = static_cast<size_t>(profession);
    if (!index || index >= profession_rules.size()) return color_default;
    EnsureRuleMatchers();
    return profession_rules[index] ? profession_rules[index]->color.value : color_default;
}

const std::vector<const AgentAppearanceWindow::AppearanceRule*>* AgentAppearanceWindow::GetAppearanceRules(const GW::Agent* agent)
{
    if (!agent) return nullptr;
    RefreshMatches(agent);
    auto& matches = match_cache.at(agent->agent_id).matches;
    return matches.empty() ? nullptr : &matches;
}

bool AgentAppearanceWindow::ApplyNameTagColor(const GW::Agent* agent, Color& color)
{
    const auto* rules = GetAppearanceRules(agent);
    if (!rules) return false;
    const auto rule = std::ranges::find_if(*rules, &CustomAgent::override_text_color);
    if (rule == rules->end()) return false;
    color = (*rule)->color_text;
    return true;
}

void AgentAppearanceWindow::InvalidateAppearance(const uint32_t agent_id)
{
    match_cache.erase(agent_id);
}

float AgentAppearanceWindow::GetBaseSize()
{
    return BaseMarkerSize();
}

AgentAppearanceWindow::Shape_e AgentAppearanceWindow::GetDefaultShape()
{
    return default_shape;
}

Color AgentAppearanceWindow::GetDefaultColor()
{
    return color_default;
}

Color AgentAppearanceWindow::GetAgentModifierColor()
{
    return color_agent_modifier;
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
    rule_buckets = {};
    profession_rules.fill(nullptr);
    compiled_name_patterns.clear();
    const auto in_outpost = instance_type == static_cast<uint32_t>(GW::Constants::InstanceType::Outpost);
    for (size_t i = 0; i < custom_agents.size(); ++i) {
        auto* rule = custom_agents[i];
        rule->index = i;
        const TextUtils::SearchPattern<wchar_t>* pattern = nullptr;
        if (!rule->match_name.empty()) pattern = &compiled_name_patterns.emplace(rule, TextUtils::StringToWString(rule->match_name)).first->second;
        if (!rule->active || (!rule->map_ids.empty() && !std::ranges::contains(rule->map_ids, map_id))
            || (rule->outpost_only && !in_outpost) || (pattern && !pattern->IsValid())) continue;

        CompiledRule entry{.rule = rule, .identifier = rule->agent_type != Any ? rule->identifier : 0, .name_pattern = pattern};
        const auto profession = static_cast<size_t>(rule->profession);
        if (profession && profession < profession_rules.size() && rule->layer == 0 && rule->override_color && !profession_rules[profession]) {
            profession_rules[profession] = rule;
        }
        const auto add_states = [&entry](const uint32_t states, const uint32_t flag) {
            if (states == 1u) entry.required_flags |= flag;
            else if (states == 2u) entry.rejected_flags |= flag;
        };
        add_states(rule->dead_states, FlagDead);
        add_states(rule->quest_states, FlagQuest);
        add_states(rule->boss_states, FlagBoss);
        add_states(StateMask(rule->combat_state), FlagCombat);
        add_states(StateMask(rule->weapon_state), FlagWeapon);
        if (rule->target_state == Targeted) entry.required_flags |= FlagTargeted;
        else if (rule->target_state == NotTargeted) entry.rejected_flags |= FlagTargeted;
        else if (rule->target_state == Marked) entry.required_flags |= FlagMarked;
        if (rule->gadget_state == ClosedChest) entry.required_flags |= FlagClosedChest;
        else if (rule->gadget_state == OpenedChest) entry.required_flags |= FlagOpenedChest;
        else if (rule->gadget_state == OtherGadget) entry.rejected_flags |= FlagClosedChest | FlagOpenedChest;
        const auto relation = static_cast<size_t>(rule->player_relation) < std::size(relation_flags) ? static_cast<size_t>(rule->player_relation) : 0;
        if (rule->player_relation == Other) entry.rejected_relations = relation_flags[relation];
        else entry.required_relations = relation_flags[relation];

        const auto checks = (entry.identifier ? CheckIdentifier : 0u) | (entry.name_pattern ? CheckName : 0u)
            | (rule->profession != GW::Constants::Profession::None ? CheckProfession : 0u)
            | (rule->gadget_state != AnyGadget ? CheckGadget : 0u) | relation_checks[relation];
        const auto requires_living = rule->allegiance >= 0 || rule->dead_states || rule->quest_states || rule->boss_states
            || rule->profession != GW::Constants::Profession::None || rule->combat_state != EitherCombat || rule->weapon_state != EitherWeapon;
        for (auto type = static_cast<int>(Any); type <= Player; ++type) {
            if ((rule->agent_type != Any && rule->agent_type != type) || (requires_living && type != NPC && type != Player)
                || (rule->gadget_state != AnyGadget && type != Gadget) || (rule->player_relation != AnyRelation && type != Player)) continue;
            rule_buckets[type].rules.push_back(entry);
            rule_buckets[type].checks |= checks;
        }
    }
}

AgentAppearanceWindow::CustomAgent::CustomAgent()
    : CustomAgent(RuleSettings{}) {}

AgentAppearanceWindow::CustomAgent::CustomAgent(const RuleSettings& settings)
    : RuleSettings(settings), ui_id(++cur_ui_id)
{
    layer = std::clamp(layer, 0, static_cast<int>(LayerCount) - 1);
    if (!std::isfinite(scale) || scale < 0.f) scale = 0.f;
    if (!std::isfinite(border_thickness) || border_thickness < 0.f) border_thickness = 0.f;
}

const char* AgentAppearanceWindow::CustomAgent::AgentTypeName() const
{
    const auto option = std::ranges::find_if(agent_type_options, [this](const auto& candidate) { return MatchesTypeOption(*this, candidate); });
    return option != std::end(agent_type_options) ? option->label : "Unknown";
}

std::string AgentAppearanceWindow::CustomAgent::DefaultLabel() const
{
    std::string label = AgentTypeName();
    if (agent_type == NPC) {
        const auto spirit = std::ranges::find(spirit_defaults, identifier, &SpiritDefault::identifier);
        if (spirit != std::end(spirit_defaults)) label += std::format(" ({})", spirit->label);
    }
    if (agent_type != Any && identifier) label += std::format(" #{}", identifier);
    constexpr std::string_view target_suffixes[] = {"", " [Target]", " [Not target]", " [Marked]"};
    const auto target_suffix = static_cast<size_t>(target_state) < std::size(target_suffixes) ? target_suffixes[target_state] : std::string_view{};
    const auto append_detail = [&](const std::string_view detail) {
        const auto reserved = target_suffix.size() + (match_name.empty() ? 0 : 11);
        if (!detail.empty() && label.size() + detail.size() + 1 + reserved <= 64) {
            label += ' ';
            label += detail;
        }
    };
    const auto append_states = [&](const uint32_t states, const std::string_view first, const std::string_view second) {
        if (states == 1u) append_detail(first);
        else if (states == 2u) append_detail(second);
        else if (states == 3u) append_detail(std::format("{}/{}", first, second));
    };
    if (allegiance > 0 && static_cast<size_t>(allegiance) < std::size(allegiance_names)) append_detail(allegiance_names[allegiance]);
    if (const auto index = static_cast<size_t>(profession); index > 0 && index < std::size(profession_names)) append_detail(profession_names[index]);
    append_states(boss_states, "Boss", "Not boss");
    append_states(dead_states, "Dead", "Alive");
    append_states(quest_states, "Quest", "No quest");
    if (outpost_only) append_detail("Outpost");
    if (override_text_color) append_detail("Name tag");
    const auto fixed_length = label.size() + target_suffix.size() + 3;
    const auto match_limit = fixed_length < 64 ? std::min(size_t{24}, 64 - fixed_length) : 0;
    if (!match_name.empty() && match_limit >= 3) {
        auto match = match_name;
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
    return name.empty() ? DefaultLabel() : name;
}

bool AgentAppearanceWindow::CustomAgent::DrawHeader()
{
    const auto changed = ImGui::Checkbox("##visible", &active);
    const auto draw_swatch = [this](const char* id, const Color fill, const Color border, const char* hint) {
        ImGui::SameLine();
        const auto size = ImGui::GetTextLineHeight();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
        if (ImGui::InvisibleButton(id, ImVec2(size, size))) {
            EditRule(this);
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
    draw_swatch("##color", override_color ? color.value : 0u, override_border_color ? border_color.value : 0u, "Minimap marker colour and border colour");
    if (override_text_color) {
        draw_swatch("##color_text", color_text, 0, "Name tag colour");
    }
    ImGui::SameLine();
    const auto label = Label();
    if (!name.empty()) {
        ImGui::Text("%s [%s]", label.c_str(), AgentTypeName());
    }
    else {
        ImGui::TextUnformatted(label.c_str());
    }
    if (!group.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", group.c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("[Layer %d]", layer + 1);
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
        changed |= ImGui::Checkbox("##visible2", &active);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If this custom agent is active");
        }
        ImGui::SameLine();
        const float x = ImGui::GetCursorPosX();
        changed |= InputString("Label", name, DefaultLabel().c_str());
        ImGui::ShowHelp("An optional label for this rule. Leave empty to generate a short label from its type and filters.");
        ImGui::SetCursorPosX(x);
        changed |= InputString("Group", group);
        ImGui::ShowHelp("An optional tag to filter this list by, e.g. 'Farming' or 'Bosses'. Purely organisational.");
        ImGui::SetCursorPosX(x);
        changed |= ImGui::Combo("Layer", &layer, layer_names, static_cast<int>(LayerCount));
        ImGui::ShowHelp("Markers draw from layer 5 to layer 1. Extra layers inherit unset properties from the primary marker. Name tag colour is shared across all layers.");
        ImGui::SetCursorPosX(x);
        changed |= ImGui::CheckboxWithHelp("Stop processing this layer", &stop_processing_rules,
            "When this rule matches, later rules cannot change this marker layer. Other layers and name tag colour continue to resolve.");
        ImGui::SetCursorPosX(x);
        if (ImGui::BeginCombo("Agent type", AgentTypeName())) {
            for (const auto& option : agent_type_options) {
                const auto selected = MatchesTypeOption(*this, option);
                if (ImGui::Selectable(option.label, selected)) {
                    if (agent_type != option.type) {
                        agent_type = option.type;
                        allegiance = -1;
                        identifier = 0;
                        if (agent_type != NPC) {
                            profession = GW::Constants::Profession::None;
                            boss_states = 0;
                        }
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
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SetCursorPosX(x);
        ImGui::BeginDisabled(agent_type == Any);
        changed |= ImGui::InputInt("Identifier", reinterpret_cast<int*>(&identifier));
        ImGui::EndDisabled();
        ImGui::ShowHelp("Item model ID, gadget ID or NPC model ID, according to the chosen type. Leave 0 to match any identifier.");
        ImGui::SetCursorPosX(x);
        changed |= InputString("Match name", match_name);
        ImGui::ShowHelp("Case-insensitive substring, or /pattern/flags for a regular expression, as in Loot Beacons.");
        if (const auto pattern = compiled_name_patterns.find(this);
            pattern != compiled_name_patterns.end() && !pattern->second.IsValid()) {
            ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "Invalid regex");
        }
        ImGui::SetCursorPosX(x);
        static constexpr const char* target_states[] = {"Either", "Targeted", "Not targeted", "Marked (/marktarget)"};
        changed |= ImGui::Combo("Target state", reinterpret_cast<int*>(&target_state), target_states, _countof(target_states));
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
        changed |= ImGui::Checkbox("Outposts only", &outpost_only);
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
        changed |= ImGui::MapPicker("Maps", &map_ids);
        ImGui::ShowHelp("Apply this rule in any of the selected maps. An empty selection applies it in every map.");
        ImGui::SetCursorPosX(x);
        static constexpr const char* combat_state_items[] = {"In combat", "Not in combat", "Either"};
        changed |= ImGui::Combo("Combat", reinterpret_cast<int*>(&combat_state), combat_state_items, _countof(combat_state_items));
        ImGui::ShowHelp("Require the agent to be in a particular combat stance");
        ImGui::SetCursorPosX(x);
        static constexpr const char* weapon_state_items[] = {"Has weapon", "No weapon", "Either"};
        changed |= ImGui::Combo("Weapon", reinterpret_cast<int*>(&weapon_state), weapon_state_items, _countof(weapon_state_items));
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
        draw_color_override("Color", override_color, color.value, "Override marker colour for this rule");
        draw_color_override("Border color", override_border_color, border_color.value, "Override border colour for this rule");
        if (ImGui::SliderFloat("Border thickness", &border_thickness, 0.f, 100.f, "%.0f")) {
            if (!std::isfinite(border_thickness) || border_thickness < 0.f) border_thickness = 0.f;
            changed = true;
        }
        ImGui::ShowHelp("Border width for this rule. Zero inherits the next matching thickness in this layer, then the Agent or Target Border thickness default.");
        draw_color_override("Text color", override_text_color, color_text.value, "Override name tag colour for this rule");

        if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.0f, 0.0f, "%.2fx")) {
            if (!std::isfinite(scale) || scale < 0.f) scale = 0.f;
            changed = true;
        }
        ImGui::ShowHelp("Multiplier of Default Size: 1.0 is the default, 0.8 is 80%, and 1.1 is 110%. Zero inherits the next matching size in this layer, then the primary marker size.");

        static constexpr const char* shape_items[] = {"Inherit", "Tear", "Circle", "Square", "Big Circle", "Star"};
        auto shape_selection = static_cast<int>(shape) + 1;
        if (ImGui::Combo("Shape", &shape_selection, shape_items, _countof(shape_items))) {
            shape = static_cast<Shape_e>(shape_selection - 1);
            changed = true;
        }
        ImGui::ShowHelp("Inherit uses the next matching shape in this layer, then the primary marker shape or minimap default.");

        ImGui::PopID();
    }
    ImGui::End();
    return changed;
}
