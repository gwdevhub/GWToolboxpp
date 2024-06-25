#include <ConditionIO.h>
#include <ConditionImpls.h>

#include <imgui.h>

namespace
{
std::shared_ptr<Condition> makeCondition(ConditionType type)
{
    static_assert((int)ConditionType::Count == 45);
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

        case ConditionType::PlayerIsNearPosition:
            return std::make_shared<PlayerIsNearPositionCondition>();
        case ConditionType::PlayerHasBuff:
            return std::make_shared<PlayerHasBuffCondition>();
        case ConditionType::PlayerHasSkill:
            return std::make_shared<PlayerHasSkillCondition>();
        case ConditionType::PlayerHasClass:
            return std::make_shared<PlayerHasClassCondition>();
        case ConditionType::PlayerHasName:
            return std::make_shared<PlayerHasNameCondition>();
        case ConditionType::PlayerHasEnergy:
            return std::make_shared<PlayerHasEnergyCondition>();
        case ConditionType::PlayerIsIdle:
            return std::make_shared<PlayerIsIdleCondition>();
        case ConditionType::PlayerHasItemEquipped:
            return std::make_shared<PlayerHasItemEquippedCondition>();
        case ConditionType::PlayerHasHpBelow:
            return std::make_shared<PlayerHasHpBelowCondition>();
        case ConditionType::PlayerStatus:
            return std::make_shared<PlayerStatusCondition>();
        case ConditionType::PlayerInPolygon:
            return std::make_shared<PlayerInPolygonCondition>();
        case ConditionType::PlayerMorale:
            return std::make_shared<MoraleCondition>();
        case ConditionType::ItemInInventory:
            return std::make_shared<ItemInInventoryCondition>();
        case ConditionType::RemainingCooldown:
            return std::make_shared<RemainingCooldownCondition>();

        case ConditionType::CurrentTargetHasHpBelow:
            return std::make_shared<CurrentTargetHasHpBelowCondition>();
        case ConditionType::CurrentTargetIsUsingSkill:
            return std::make_shared<CurrentTargetIsCastingSkillCondition>();
        case ConditionType::CurrentTargetHasModel:
            return std::make_shared<CurrentTargetModelCondition>();
        case ConditionType::CurrentTargetAllegiance:
            return std::make_shared<CurrentTargetAllegianceCondition>();
        case ConditionType::CurrentTargetDistance:
            return std::make_shared<CurrentTargetDistanceCondition>();
        case ConditionType::CurrentTargetStatus:
            return std::make_shared<CurrentTargetStatusCondition>();
        case ConditionType::CurrentTargetName:
            return std::make_shared<CurrentTargetNameCondition>();

        case ConditionType::KeyIsPressed:
            return std::make_shared<KeyIsPressedCondition>();
        case ConditionType::InstanceTime:
            return std::make_shared<InstanceTimeCondition>();
        case ConditionType::FoeCount:
            return std::make_shared<FoeCountCondition>();
        case ConditionType::NearbyAgent:
            return std::make_shared<NearbyAgentCondition>();
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

        default:
            return nullptr;
    }
}

std::string_view toString(ConditionType type)
{
    static_assert((int)ConditionType::Count == 45);
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

        case ConditionType::PlayerIsNearPosition:
            return "Position";
        case ConditionType::PlayerHasBuff:
            return "Buff";
        case ConditionType::PlayerHasSkill:
            return "Has skill";
        case ConditionType::PlayerHasClass:
            return "Class";
        case ConditionType::PlayerHasName:
            return "Name";
        case ConditionType::PlayerHasEnergy:
            return "Energy";
        case ConditionType::PlayerIsIdle:
            return "Is idle";
        case ConditionType::PlayerStatus:
            return "Status";
        case ConditionType::PlayerMorale:
            return "Morale";
        case ConditionType::PlayerHasItemEquipped:
            return "Equipped item";
        case ConditionType::PlayerInPolygon:
            return "Inside polygon";
        case ConditionType::ItemInInventory:
            return "Item in inventory";
        case ConditionType::PlayerHasHpBelow:
            return "HP";
        case ConditionType::CanPopAgent:
            return "Can pop agent";
        case ConditionType::RemainingCooldown:
            return "Skill cooldown";

        case ConditionType::CurrentTargetHasHpBelow:
            return "HP";
        case ConditionType::CurrentTargetIsUsingSkill:
            return "Skill";
        case ConditionType::CurrentTargetHasModel:
            return "Model";
        case ConditionType::CurrentTargetAllegiance:
            return "Allegiance";
        case ConditionType::CurrentTargetDistance:
            return "Distance";
        case ConditionType::CurrentTargetStatus:
            return "Status";
        case ConditionType::CurrentTargetName:
            return "Name";

        case ConditionType::KeyIsPressed:
            return "Keypress";
        case ConditionType::InstanceTime:
            return "Time";
        case ConditionType::NearbyAgent:
            return "Nearby agent exists";
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

        default:
            return "Unknown";
    }
}
} // namespace

std::shared_ptr<Condition> readCondition(InputStream& stream)
{
static_assert((int)ConditionType::Count == 45);
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

    case ConditionType::PlayerIsNearPosition:
        return std::make_shared<PlayerIsNearPositionCondition>(stream);
    case ConditionType::PlayerHasBuff:
        return std::make_shared<PlayerHasBuffCondition>(stream);
    case ConditionType::PlayerHasSkill:
        return std::make_shared<PlayerHasSkillCondition>(stream);
    case ConditionType::PlayerHasClass:
        return std::make_shared<PlayerHasClassCondition>(stream);
    case ConditionType::PlayerHasName:
        return std::make_shared<PlayerHasNameCondition>(stream);
    case ConditionType::PlayerHasEnergy:
        return std::make_shared<PlayerHasEnergyCondition>(stream);
    case ConditionType::PlayerIsIdle:
        return std::make_shared<PlayerIsIdleCondition>(stream);
    case ConditionType::PlayerHasItemEquipped:
        return std::make_shared<PlayerHasItemEquippedCondition>(stream);
    case ConditionType::ItemInInventory:
        return std::make_shared<ItemInInventoryCondition>(stream);
    case ConditionType::PlayerHasHpBelow:
        return std::make_shared<PlayerHasHpBelowCondition>(stream);
    case ConditionType::PlayerStatus:
        return std::make_shared<PlayerStatusCondition>(stream);
    case ConditionType::PlayerInPolygon:
        return std::make_shared<PlayerInPolygonCondition>(stream);
    case ConditionType::PlayerMorale:
        return std::make_shared<MoraleCondition>(stream);
    case ConditionType::RemainingCooldown:
        return std::make_shared<RemainingCooldownCondition>(stream);

    case ConditionType::CurrentTargetHasHpBelow:
        return std::make_shared<CurrentTargetHasHpBelowCondition>(stream);
    case ConditionType::CurrentTargetIsUsingSkill:
        return std::make_shared<CurrentTargetIsCastingSkillCondition>(stream);
    case ConditionType::CurrentTargetHasModel:
        return std::make_shared<CurrentTargetModelCondition>(stream);
    case ConditionType::CurrentTargetAllegiance:
        return std::make_shared<CurrentTargetAllegianceCondition>(stream);
    case ConditionType::CurrentTargetDistance:
        return std::make_shared<CurrentTargetDistanceCondition>(stream);
    case ConditionType::CurrentTargetStatus:
        return std::make_shared<CurrentTargetStatusCondition>(stream);
    case ConditionType::CurrentTargetName:
        return std::make_shared<CurrentTargetNameCondition>(stream);

    case ConditionType::KeyIsPressed:
        return std::make_shared<KeyIsPressedCondition>(stream);
    case ConditionType::InstanceTime:
        return std::make_shared<InstanceTimeCondition>(stream);
    case ConditionType::NearbyAgent:
        return std::make_shared<NearbyAgentCondition>(stream);
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

    default:
        return nullptr;
}
}

std::shared_ptr<Condition> drawConditionSelector(float width)
{
    std::shared_ptr<Condition> result = nullptr;

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

    constexpr auto playerConditions = std::array{ConditionType::PlayerIsNearPosition,  ConditionType::PlayerInPolygon, ConditionType::PlayerHasBuff,    ConditionType::PlayerHasSkill, ConditionType::RemainingCooldown, ConditionType::PlayerHasClass,
                                                 ConditionType::PlayerHasName,         ConditionType::PlayerHasEnergy, ConditionType::PlayerHasHpBelow, ConditionType::PlayerStatus,   ConditionType::PlayerIsIdle,
                                                 ConditionType::PlayerHasItemEquipped, ConditionType::ItemInInventory, ConditionType::PlayerMorale,     ConditionType::CanPopAgent};
    constexpr auto targetConditions = std::array{ConditionType::CurrentTargetHasHpBelow, ConditionType::CurrentTargetStatus, ConditionType::CurrentTargetIsUsingSkill, ConditionType::CurrentTargetHasModel,
                                                 ConditionType::CurrentTargetAllegiance, ConditionType::CurrentTargetDistance, ConditionType::CurrentTargetName};
    constexpr auto partyCondtions = std::array{ConditionType::PartyPlayerCount, ConditionType::PartyHasLoadedIn, ConditionType::PartyMemberStatus, ConditionType::HasPartyWindowAllyOfName};
    constexpr auto instanceConditions = std::array{ConditionType::IsInMap, ConditionType::InstanceType, ConditionType::QuestHasState, ConditionType::InstanceProgress, ConditionType::InstanceTime, ConditionType::FoeCount};
    constexpr auto logicConditions = std::array{ConditionType::Not, ConditionType::Or, ConditionType::And, ConditionType::True, ConditionType::False};
    constexpr auto controlFlowCondition = std::array{ConditionType::OnlyTriggerOncePerInstance, ConditionType::Once, ConditionType::Until, ConditionType::After, ConditionType::Toggle, ConditionType::Throttle};

    if (ImGui::BeginPopup("Add condition")) 
    {
        drawSubMenu("Player", playerConditions);
        drawSubMenu("Current target", targetConditions);
        drawSubMenu("Party", partyCondtions);
        drawSubMenu("Instance", instanceConditions);
        drawSubMenu("Logic", logicConditions);
        drawSubMenu("Control flow", controlFlowCondition);
        drawConditionSelector(ConditionType::NearbyAgent);
        drawConditionSelector(ConditionType::KeyIsPressed);

        ImGui::EndPopup();
    }

    return result;
}
