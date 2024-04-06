#include <ConditionIO.h>
#include <ConditionImpls.h>

#include <imgui.h>

namespace
{
std::shared_ptr<Condition> makeCondition(ConditionType type)
{
    static_assert((int)ConditionType::Count == 17);
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
            return nullptr;

        case ConditionType::PlayerIsNearPosition:
            return std::make_shared<PlayerIsNearPositionCondition>();
        case ConditionType::PlayerHasBuff:
            return std::make_shared<PlayerHasBuffCondition>();
        case ConditionType::PlayerHasSkill:
            return std::make_shared<PlayerHasSkillCondition>();

        case ConditionType::CurrentTargetHasHpPercentBelow:
            return nullptr;
        case ConditionType::CurrentTargetIsUsingSkill:
            return std::make_shared<CurrentTargetIsCastingSkillCondition>();

        case ConditionType::NearbyAllyOfModelIdExists:
            return nullptr;
        case ConditionType::NearbyEnemyWithModelIdCastingSpellExists:
            return nullptr;
        case ConditionType::EnemyWithModelIdAndEffectExists:
            return nullptr;
        case ConditionType::EnemyInPolygonWithModelIdExists:
            return nullptr;
        case ConditionType::EnemyInCircleSegmentWithModelIdExísts:
            return nullptr;
        default:
            return nullptr;
    }
}

std::string_view toString(ConditionType type)
{
    static_assert((int)ConditionType::Count == 17);
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

        case ConditionType::PlayerIsNearPosition:
            return "Player position";
        case ConditionType::PlayerHasBuff:
            return "Player buff";
        case ConditionType::PlayerHasSkill:
            return "Player has skill";

        case ConditionType::CurrentTargetHasHpPercentBelow:
            return "Current target HP";
        case ConditionType::CurrentTargetIsUsingSkill:
            return "Current target skill";

        case ConditionType::NearbyAllyOfModelIdExists:
            return "Has nearby ally with model id";
        case ConditionType::NearbyEnemyWithModelIdCastingSpellExists:
            return "Nearby enemy casts skill";
        case ConditionType::EnemyWithModelIdAndEffectExists:
            return "Enemy has effect";
        case ConditionType::EnemyInPolygonWithModelIdExists:
            return "Enemies in polygon";
        case ConditionType::EnemyInCircleSegmentWithModelIdExísts:
            return "Enemies in circle segment";
        default:
            return "Unknown";
    }
}
} // namespace

std::shared_ptr<Condition> readCondition(std::istringstream& stream)
{
static_assert((int)ConditionType::Count == 17);
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
        assert(false);
        return nullptr;

    case ConditionType::PlayerIsNearPosition:
        return std::make_shared<PlayerIsNearPositionCondition>(stream);
    case ConditionType::PlayerHasBuff:
        return std::make_shared<PlayerHasBuffCondition>(stream);
    case ConditionType::PlayerHasSkill:
        return std::make_shared<PlayerHasSkillCondition>(stream);

    case ConditionType::CurrentTargetHasHpPercentBelow:
        assert(false);
        return nullptr;
    case ConditionType::CurrentTargetIsUsingSkill:
        return std::make_shared<CurrentTargetIsCastingSkillCondition>(stream);

    case ConditionType::NearbyAllyOfModelIdExists:
        assert(false);
        return nullptr;
    case ConditionType::NearbyEnemyWithModelIdCastingSpellExists:
        assert(false);
        return nullptr;
    case ConditionType::EnemyWithModelIdAndEffectExists:
        assert(false);
        return nullptr;
    case ConditionType::EnemyInPolygonWithModelIdExists:
        assert(false);
        return nullptr;
    case ConditionType::EnemyInCircleSegmentWithModelIdExísts:
        assert(false);
        return nullptr;
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
            if (ImGui::Selectable(toString((ConditionType)i).data())) {
                result = makeCondition(ConditionType(i));
            }
        }
        ImGui::EndPopup();
    }

    return result;
}
