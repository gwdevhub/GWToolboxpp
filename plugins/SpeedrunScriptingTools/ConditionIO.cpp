#include <ConditionIO.h>
#include <ConditionImpls.h>

#include <imgui.h>

namespace
{
std::shared_ptr<Condition> makeCondition(ConditionType type)
{
    static_assert((int)ConditionType::Count == 21);
    switch (type) {
        case ConditionType::Not:
            return std::make_shared<NegatedCondition>();
        case ConditionType::Or:
            return std::make_shared<DisjunctionCondition>();
        case ConditionType::IsInMap:
            return std::make_shared<IsInMapCondition>();
        case ConditionType::QuestHasState:
            return std::make_shared<QuestHasStateCondition>();
        case ConditionType::PartyPlayerCount:
            return std::make_shared<PartyPlayerCountCondition>();
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

        case ConditionType::CurrentTargetHasHpBelow:
            return std::make_shared<CurrentTargetHasHpBelowCondition>();
        case ConditionType::CurrentTargetIsUsingSkill:
            return std::make_shared<CurrentTargetIsCastingSkillCondition>();

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
    static_assert((int)ConditionType::Count == 21);
    switch (type) {
        case ConditionType::Not:
            return "Negation";
        case ConditionType::Or:
            return "Disjunction";
        case ConditionType::IsInMap:
            return "Current map ID";
        case ConditionType::QuestHasState:
            return "Queststate";
        case ConditionType::PartyPlayerCount:
            return "Party player count";
        case ConditionType::InstanceProgress:
            return "Instance progress";
        case ConditionType::HasPartyWindowAllyOfName:
            return "Party window contains ally";
        case ConditionType::OnlyTriggerOncePerInstance:
            return "Only trigger once";
        case ConditionType::PartyMemberStatus:
            return "Party member has status";

        case ConditionType::PlayerIsNearPosition:
            return "Player position";
        case ConditionType::PlayerHasBuff:
            return "Player buff";
        case ConditionType::PlayerHasSkill:
            return "Player has skill";
        case ConditionType::PlayerHasClass:
            return "Player has class";
        case ConditionType::PlayerHasName:
            return "Player has name";
        case ConditionType::PlayerHasEnergy:
            return "Player has energy";
        case ConditionType::CanPopAgent:
            return "Can pop agent";

        case ConditionType::CurrentTargetHasHpBelow:
            return "Current target HP";
        case ConditionType::CurrentTargetIsUsingSkill:
            return "Current target skill";

        case ConditionType::KeyIsPressed:
            return "Keypress";
        case ConditionType::InstanceTime:
            return "Instance time";
        case ConditionType::NearbyAgent:
            return "Nearby agent exists";

        default:
            return "Unknown";
    }
}
} // namespace

std::shared_ptr<Condition> readCondition(std::istringstream& stream)
{
static_assert((int)ConditionType::Count == 21);
int type;
stream >> type;
switch (static_cast<ConditionType>(type))
{
    case ConditionType::Not:
        return std::make_shared<NegatedCondition>(stream);
    case ConditionType::Or:
        return std::make_shared<DisjunctionCondition>(stream);
    case ConditionType::IsInMap:
        return std::make_shared<IsInMapCondition>(stream);
    case ConditionType::QuestHasState:
        return std::make_shared<QuestHasStateCondition>(stream);
    case ConditionType::PartyPlayerCount:
        return std::make_shared<PartyPlayerCountCondition>(stream);
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

    case ConditionType::CurrentTargetHasHpBelow:
        return std::make_shared<CurrentTargetHasHpBelowCondition>(stream);
    case ConditionType::CurrentTargetIsUsingSkill:
        return std::make_shared<CurrentTargetIsCastingSkillCondition>(stream);

    case ConditionType::KeyIsPressed:
        return std::make_shared<KeyIsPressedCondition>(stream);
    case ConditionType::InstanceTime:
        return std::make_shared<InstanceTimeCondition>(stream);
    case ConditionType::NearbyAgent:
        return std::make_shared<NearbyAgentCondition>(stream);
    case ConditionType::CanPopAgent:
        return std::make_shared<CanPopAgentCondition>(stream);

    default:
        assert(false);
        return nullptr;
}
}

std::shared_ptr<Condition> drawConditionSelector(float width)
{
    std::shared_ptr<Condition> result = nullptr;
    
    if (ImGui::Button("Add condition", ImVec2(width, 0))) {
        ImGui::OpenPopup("Add condition");
    }
    if (ImGui::BeginPopup("Add condition")) {
        for (auto i = 0; i < (int)ConditionType::Count; ++i) {
            if (ConditionType(i) == ConditionType::PartyMemberStatus) continue; //Does not work properly
            if (ImGui::Selectable(toString((ConditionType)i).data())) {
                result = makeCondition(ConditionType(i));
            }
        }
        ImGui::EndPopup();
    }

    return result;
}
