#include <ConditionIO.h>
#include <ConditionImpls.h>

#include <CharacteristicIO.h>

#include <imgui.h>

namespace
{
ConditionPtr makeCondition(ConditionType type)
{
    static_assert((int)ConditionType::Count == 50);
    switch (type) {
        case ConditionType::Not:
            return std::make_shared<NegatedCondition>();
        case ConditionType::Or:
            return std::make_shared<DisjunctionCondition>();
        case ConditionType::And:
            return std::make_shared<ConjunctionCondition>();
        case ConditionType::IsInMap:
            return std::make_shared<IsInMapCondition>();
        case ConditionType::QuestHasState:
            return std::make_shared<QuestHasStateCondition>();
        case ConditionType::PartyPlayerCount:
            return std::make_shared<PartyPlayerCountCondition>();
        case ConditionType::PartyHasLoadedIn:
            return std::make_shared<PartyHasLoadedInCondition>();
        case ConditionType::InstanceProgress:
            return std::make_shared<InstanceProgressCondition>();
        case ConditionType::HasPartyWindowAllyOfName:
            return std::make_shared<HasPartyWindowAllyOfNameCondition>();
        case ConditionType::PartyMemberStatus:
            return std::make_shared<PartyMemberStatusCondition>();
        case ConditionType::OnlyTriggerOncePerInstance:
            return std::make_shared<OnlyTriggerOnceCondition>();
        case ConditionType::InstanceType:
            return std::make_shared<InstanceTypeCondition>();

        case ConditionType::PlayerHasBuff:
            return std::make_shared<PlayerHasBuffCondition>();
        case ConditionType::PlayerHasSkill:
            return std::make_shared<PlayerHasSkillCondition>();
        case ConditionType::PlayerHasEnergy:
            return std::make_shared<PlayerHasEnergyCondition>();
        case ConditionType::PlayerHasItemEquipped:
            return std::make_shared<PlayerHasItemEquippedCondition>();
        case ConditionType::PlayerMorale:
            return std::make_shared<MoraleCondition>();
        case ConditionType::ItemInInventory:
            return std::make_shared<ItemInInventoryCondition>();
        case ConditionType::RemainingCooldown:
            return std::make_shared<RemainingCooldownCondition>();

        case ConditionType::KeyIsPressed:
            return std::make_shared<KeyIsPressedCondition>();
        case ConditionType::InstanceTime:
            return std::make_shared<InstanceTimeCondition>();
        case ConditionType::FoeCount:
            return std::make_shared<FoeCountCondition>();
        case ConditionType::CanPopAgent:
            return std::make_shared<CanPopAgentCondition>();
        case ConditionType::Throttle:
            return std::make_shared<ThrottleCondition>();

        case ConditionType::True:
            return std::make_shared<TrueCondition>();
        case ConditionType::False:
            return std::make_shared<FalseCondition>();
        case ConditionType::Once:
            return std::make_shared<OnceCondition>();
        case ConditionType::Until:
            return std::make_shared<UntilCondition>();
        case ConditionType::Toggle:
            return std::make_shared<ToggleCondition>();
        case ConditionType::After:
            return std::make_shared<AfterCondition>();

        // Handled differently, we should never run into these here.
        /* case ConditionType::PlayerHasCharacteristics:
            return std::make_shared<PlayerHasCharacteristicsCondition>();
        case ConditionType::TargetHasCharacteristics:
            return std::make_shared<TargetHasCharacteristicsCondition>();*/
        case ConditionType::AgentWithCharacteristicsCount:
            return std::make_shared<AgentWithCharacteristicsCountCondition>();

        case ConditionType::ScriptVariable:
            return std::make_shared<ScriptVariableCondition>();

        default:
            return nullptr;
    }
}

std::string_view toString(ConditionType type)
{
    static_assert((int)ConditionType::Count == 50);
    switch (type) {
        case ConditionType::Not:
            return "Not";
        case ConditionType::Or:
            return "Or";
        case ConditionType::And:
            return "And";
        case ConditionType::IsInMap:
            return "Map ID";
        case ConditionType::QuestHasState:
            return "Quest state";
        case ConditionType::PartyPlayerCount:
            return "Size";
        case ConditionType::InstanceProgress:
            return "Progress";
        case ConditionType::HasPartyWindowAllyOfName:
            return "Party window contains ally";
        case ConditionType::OnlyTriggerOncePerInstance:
            return "Only trigger once";
        case ConditionType::PartyMemberStatus:
            return "Member status";
        case ConditionType::PartyHasLoadedIn:
            return "Finshed loading";
        case ConditionType::InstanceType:
            return "Type";

        case ConditionType::PlayerHasBuff:
            return "Buff";
        case ConditionType::PlayerHasSkill:
            return "Has skill";
        case ConditionType::PlayerHasEnergy:
            return "Energy";
        case ConditionType::PlayerMorale:
            return "Morale";
        case ConditionType::PlayerHasItemEquipped:
            return "Equipped item";
        case ConditionType::ItemInInventory:
            return "Item in inventory";
        case ConditionType::CanPopAgent:
            return "Can pop agent";
        case ConditionType::RemainingCooldown:
            return "Skill cooldown";

        case ConditionType::KeyIsPressed:
            return "Keypress";
        case ConditionType::InstanceTime:
            return "Time";
        case ConditionType::FoeCount:
            return "Number of enemies";
        case ConditionType::Throttle:
            return "Throttle";

        case ConditionType::True:
            return "True";
        case ConditionType::False:
            return "False";
        case ConditionType::Once:
            return "Once";
        case ConditionType::Until:
            return "Until";
        case ConditionType::Toggle:
            return "Toggle";
        case ConditionType::After:
            return "After";

        case ConditionType::PlayerHasCharacteristics:
            return "Player characteristics";
        case ConditionType::TargetHasCharacteristics:
            return "Target characteristics";
        case ConditionType::AgentWithCharacteristicsCount:
            return "Agent characteristics";

        case ConditionType::ScriptVariable:
            return "Variable";

        default:
            return "Unknown";
    }
}
} // namespace

ConditionPtr readCondition(InputStream& stream)
{
static_assert((int)ConditionType::Count == 50);
int type;
stream >> type;
switch (static_cast<ConditionType>(type))
{
    case ConditionType::Not:
        return std::make_shared<NegatedCondition>(stream);
    case ConditionType::Or:
        return std::make_shared<DisjunctionCondition>(stream);
    case ConditionType::And:
        return std::make_shared<ConjunctionCondition>(stream);
    case ConditionType::IsInMap:
        return std::make_shared<IsInMapCondition>(stream);
    case ConditionType::QuestHasState:
        return std::make_shared<QuestHasStateCondition>(stream);
    case ConditionType::PartyPlayerCount:
        return std::make_shared<PartyPlayerCountCondition>(stream);
    case ConditionType::PartyHasLoadedIn:
        return std::make_shared<PartyHasLoadedInCondition>(stream);
    case ConditionType::InstanceProgress:
        return std::make_shared<InstanceProgressCondition>(stream);
    case ConditionType::HasPartyWindowAllyOfName:
        return std::make_shared<HasPartyWindowAllyOfNameCondition>(stream);
    case ConditionType::OnlyTriggerOncePerInstance:
        return std::make_shared<OnlyTriggerOnceCondition>(stream);
    case ConditionType::PartyMemberStatus:
        return std::make_shared<PartyMemberStatusCondition>(stream);
    case ConditionType::InstanceType:
        return std::make_shared<InstanceTypeCondition>(stream);

    case ConditionType::PlayerHasBuff:
        return std::make_shared<PlayerHasBuffCondition>(stream);
    case ConditionType::PlayerHasSkill:
        return std::make_shared<PlayerHasSkillCondition>(stream);
    case ConditionType::PlayerHasEnergy:
        return std::make_shared<PlayerHasEnergyCondition>(stream);
    case ConditionType::PlayerHasItemEquipped:
        return std::make_shared<PlayerHasItemEquippedCondition>(stream);
    case ConditionType::ItemInInventory:
        return std::make_shared<ItemInInventoryCondition>(stream);
    case ConditionType::PlayerMorale:
        return std::make_shared<MoraleCondition>(stream);
    case ConditionType::RemainingCooldown:
        return std::make_shared<RemainingCooldownCondition>(stream);

    case ConditionType::KeyIsPressed:
        return std::make_shared<KeyIsPressedCondition>(stream);
    case ConditionType::InstanceTime:
        return std::make_shared<InstanceTimeCondition>(stream);
    case ConditionType::CanPopAgent:
        return std::make_shared<CanPopAgentCondition>(stream);
    case ConditionType::FoeCount:
        return std::make_shared<FoeCountCondition>(stream);
    case ConditionType::Throttle:
        return std::make_shared<ThrottleCondition>(stream);

    case ConditionType::True:
        return std::make_shared<TrueCondition>(stream);
    case ConditionType::False:
        return std::make_shared<FalseCondition>(stream);
    case ConditionType::Once:
        return std::make_shared<OnceCondition>(stream);
    case ConditionType::Until:
        return std::make_shared<UntilCondition>(stream);
    case ConditionType::Toggle:
        return std::make_shared<ToggleCondition>(stream);
    case ConditionType::After:
        return std::make_shared<AfterCondition>(stream);

    case ConditionType::PlayerHasCharacteristics:
        return std::make_shared<PlayerHasCharacteristicsCondition>(stream);
    case ConditionType::TargetHasCharacteristics:
        return std::make_shared<TargetHasCharacteristicsCondition>(stream);
    case ConditionType::AgentWithCharacteristicsCount:
        return std::make_shared<AgentWithCharacteristicsCountCondition>(stream);

        
    case ConditionType::ScriptVariable:
        return std::make_shared<ScriptVariableCondition>(stream);

    default:
        return nullptr;
}
}

ConditionPtr drawConditionSelector(float width)
{
    ConditionPtr result = nullptr;

    const auto drawConditionSelector = [&result](ConditionType type)
    {
        if (ImGui::MenuItem(toString(type).data())) 
        {
            result = makeCondition(type);
        }
    };
    const auto drawSubMenu = [&drawConditionSelector](std::string_view title, const auto& types) 
    {
        if (ImGui::BeginMenu(title.data())) 
        {
            for (const auto& type : types) 
            {
                drawConditionSelector(type);
            }
            ImGui::EndMenu();
        }
    };

    if (ImGui::Button("Add condition", ImVec2(width, 0))) 
    {
        ImGui::OpenPopup("Add condition");
    }

    constexpr auto skillConditions = std::array{ConditionType::PlayerHasBuff, ConditionType::PlayerHasSkill, ConditionType::RemainingCooldown, ConditionType::PlayerHasEnergy, ConditionType::PlayerMorale};
    constexpr auto itemConditions = std::array{ConditionType::PlayerHasItemEquipped, ConditionType::ItemInInventory, ConditionType::CanPopAgent};
    constexpr auto partyConditions = std::array{ConditionType::PartyPlayerCount, ConditionType::PartyHasLoadedIn, ConditionType::PartyMemberStatus, ConditionType::HasPartyWindowAllyOfName};
    constexpr auto instanceConditions = std::array{ConditionType::IsInMap, ConditionType::InstanceType, ConditionType::QuestHasState, ConditionType::InstanceProgress, ConditionType::InstanceTime, ConditionType::FoeCount};
    constexpr auto logicConditions = std::array{ConditionType::Not, ConditionType::Or, ConditionType::And, ConditionType::True, ConditionType::False};
    constexpr auto controlFlowCondition = std::array{ConditionType::OnlyTriggerOncePerInstance, ConditionType::Throttle, ConditionType::Once, ConditionType::Until, ConditionType::After, ConditionType::Toggle, ConditionType::ScriptVariable};

    if (ImGui::BeginPopup("Add condition")) 
    {
        if (ImGui::BeginMenu("Player")) 
        {
            drawSubMenu("Skillbar info", skillConditions);
            if (const auto type = drawCharacteristicSubMenu()) result = std::make_shared<PlayerHasCharacteristicsCondition>(*type);

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Target")) {
            if (const auto type = drawCharacteristicSubMenu()) result = std::make_shared<TargetHasCharacteristicsCondition>(*type);
            ImGui::EndMenu();
        }
        drawConditionSelector(ConditionType::AgentWithCharacteristicsCount);
        
        drawSubMenu("Item", itemConditions);
        drawSubMenu("Party", partyConditions);
        drawSubMenu("Instance", instanceConditions);
        drawSubMenu("Logic", logicConditions);
        drawSubMenu("Control flow", controlFlowCondition);
        drawConditionSelector(ConditionType::KeyIsPressed);

        ImGui::EndPopup();
    }

    return result;
}
