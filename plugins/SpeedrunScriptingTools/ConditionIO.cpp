#include <ConditionIO.h>
#include <ConditionImpls.h>

#include <imgui.h>

namespace
{
std::shared_ptr<Condition> makeCondition(ConditionType type)
{
    static_assert((int)ConditionType::Count == 29);
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

        case ConditionType::KeyIsPressed:
            return std::make_shared<KeyIsPressedCondition>();
        case ConditionType::InstanceTime:
            return std::make_shared<InstanceTimeCondition>();
        case ConditionType::NearbyAgent:
            return std::make_shared<NearbyAgentCondition>();
        case ConditionType::CanPopAgent:
            return std::make_shared<CanPopAgentCondition>();

        default:
            return nullptr;
    }
}

std::string_view toString(ConditionType type)
{
    static_assert((int)ConditionType::Count == 29);
    switch (type) {
        case ConditionType::Not:
            return "Not";
        case ConditionType::Or:
            return "Or";
        case ConditionType::And:
            return "And";
        case ConditionType::IsInMap:
            return "Current map ID";
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
        case ConditionType::PlayerHasItemEquipped:
            return "Equipped item";
        case ConditionType::PlayerHasHpBelow:
            return "HP";
        case ConditionType::CanPopAgent:
            return "Can pop agent";

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

        case ConditionType::KeyIsPressed:
            return "Keypress";
        case ConditionType::InstanceTime:
            return "Time";
        case ConditionType::NearbyAgent:
            return "Nearby agent exists";

        default:
            return "Unknown";
    }
}
} // namespace

std::shared_ptr<Condition> readCondition(InputStream& stream)
{
static_assert((int)ConditionType::Count == 29);
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
    case ConditionType::PlayerHasHpBelow:
        return std::make_shared<PlayerHasHpBelowCondition>(stream);

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

    case ConditionType::KeyIsPressed:
        return std::make_shared<KeyIsPressedCondition>(stream);
    case ConditionType::InstanceTime:
        return std::make_shared<InstanceTimeCondition>(stream);
    case ConditionType::NearbyAgent:
        return std::make_shared<NearbyAgentCondition>(stream);
    case ConditionType::CanPopAgent:
        return std::make_shared<CanPopAgentCondition>(stream);

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

    if (ImGui::BeginPopup("Add condition")) 
    {
        drawSubMenu("Player", std::array{ConditionType::PlayerIsNearPosition, ConditionType::PlayerHasBuff, ConditionType::PlayerHasSkill, ConditionType::PlayerHasClass, ConditionType::PlayerHasName, ConditionType::PlayerHasEnergy, ConditionType::PlayerHasHpBelow, ConditionType::PlayerIsIdle, ConditionType::PlayerHasItemEquipped, ConditionType::CanPopAgent});
        drawSubMenu("Current target", std::array{ConditionType::CurrentTargetHasHpBelow, ConditionType::CurrentTargetIsUsingSkill, ConditionType::CurrentTargetHasModel, ConditionType::CurrentTargetAllegiance, ConditionType::CurrentTargetDistance});
        drawSubMenu("Party", std::array{ConditionType::PartyPlayerCount, ConditionType::PartyHasLoadedIn, ConditionType::PartyMemberStatus, ConditionType::HasPartyWindowAllyOfName});
        drawSubMenu("Instance", std::array{ConditionType::IsInMap, ConditionType::QuestHasState, ConditionType::InstanceProgress, ConditionType::InstanceTime});
        drawSubMenu("Logic", std::array{ConditionType::Not, ConditionType::Or, ConditionType::And});
        drawConditionSelector(ConditionType::NearbyAgent);
        drawConditionSelector(ConditionType::KeyIsPressed);
        drawConditionSelector(ConditionType::OnlyTriggerOncePerInstance);

        ImGui::EndPopup();
    }

    return result;
}
