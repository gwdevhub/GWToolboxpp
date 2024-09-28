#include <SerializationIncrement.h>

#include <ActionImpls.h>
#include <ConditionImpls.h>

// The v8 -> v9 version change was necessary because the OR & AND conditions and the conditioned action did not write seperators between the actions & conditions.
// When the actions and conditions had received additional member variables, this meant that the deserialization failed.
// Store the v8 schema of the changed actions & conditions here so they can still be read.
namespace v8 
{
    const std::string missingContentToken = "/";
    const std::string endOfListToken = ">";

    // Disjunction and Conjunction are serialized identically
    struct DisjunctionConjunctionCondition 
    {
        DisjunctionConjunctionCondition(InputStream& stream) 
        { 
            std::string token;

            while (stream >> token) 
            {
                if (token == endOfListToken)
                    return;
                else if (token == missingContentToken)
                    conditions.push_back(nullptr);
                else if (token == "C") {
                    if (auto read = readV8Condition(stream)) conditions.push_back(std::move(read));
                }
                else
                    return;
            }
        }
        std::string serialize()
        {
            OutputStream stream;

            stream << conditions.size();

            for (const auto& condition : conditions) 
            {
                if (condition)
                    condition->serialize(stream);
                else
                    stream << missingContentToken;

                stream.writeSeparator(2);
            }

            return stream.str();
        }
        std::vector<ConditionPtr> conditions;
    };
    struct PlayerHasSkillCondition 
    {
        PlayerHasSkillCondition(InputStream& stream) 
        { 
            stream >> id;
        } 
        std::string serialize()
        {
            OutputStream stream;
            stream << id << HasSkillRequirement::OffCooldown;
            return stream.str();
        }
        GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
    };
    struct NearbyAgentCondition 
    {
        NearbyAgentCondition(InputStream& stream) 
        { 
            stream >> agentType >> primary >> secondary >> alive >> hexed >> skill >> modelId >> minDistance >> maxDistance >> minHp >> maxHp;
            agentName = readStringWithSpaces(stream);
            polygon = readPositions(stream);
        } 
        std::string serialize()
        {
            OutputStream stream;

            stream << agentType << primary << secondary << alive << hexed << skill << modelId << minDistance << maxDistance << minHp << maxHp;
            writeStringWithSpaces(stream, agentName);
            writePositions(stream, polygon);
            stream << 0.f << 180.f << 0 << 0 << 0 << 0 << 0 << 1000 << -10 << 10 << 0; // new members: Min angle, max angle, enchanted, weaponspelled, poisoned, bleeding, minSpeed, maxSpeed, minRegen, maxRegen, weapon;

            return stream.str();
        }
        AgentType agentType = AgentType::Any;
        Class primary = Class::Any;
        Class secondary = Class::Any;
        AnyNoYes alive = AnyNoYes::Yes;
        AnyNoYes hexed = AnyNoYes::Any;
        GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
        int modelId = 0;
        float minDistance = 0.f;
        float maxDistance = 5000.f;
        std::string agentName = "";
        std::vector<GW::Vec2f> polygon;
        float minHp = 0.f;
        float maxHp = 100.f;
    };
    struct KeyPressCondition 
    {
        KeyPressCondition(InputStream& stream)
        {
            stream >> shortcut >> modifier;
        }
        std::string serialize()
        {
            OutputStream stream;

            stream << shortcut << modifier << 0; //New Member: Block key

            return stream.str();
        }
        long shortcut;
        long modifier;
    };

    struct ChangeTargetAction 
    {
        ChangeTargetAction(InputStream& stream)
        {
            stream >> agentType >> primary >> secondary >> alive >> skill >> sorting >> modelId >> minDistance >> maxDistance >> requireSameModelIdAsTarget >> preferNonHexed >> rotateThroughTargets;
            agentName = readStringWithSpaces(stream);
            polygon = readPositions(stream);
        }
        std::string serialize()
        {
            OutputStream stream;

            stream << agentType << primary << secondary << alive << skill << sorting << modelId << minDistance << maxDistance << requireSameModelIdAsTarget << preferNonHexed << rotateThroughTargets;
            writeStringWithSpaces(stream, agentName);
            writePositions(stream, polygon);
            stream << 0.f << 180.f << 0 << 0 << 0 << 0 << 0 << 0 << 1000 << -10 << 10 << 0 << 0 << 100; // new members: Min angle, max angle, enchanted, weaponspelled, poisoned, bleeding, hexed, minSpeed, maxSpeed, minRegen, maxRegen, weapon, minHp, maxHp

            return stream.str();
        }
        AgentType agentType = AgentType::Any;
        Class primary = Class::Any;
        Class secondary = Class::Any;
        AnyNoYes alive = AnyNoYes::Yes;
        GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
        Sorting sorting = Sorting::AgentId;
        int modelId = 0;
        float minDistance = 0.f;
        float maxDistance = 5000.f;
        bool preferNonHexed = false;
        bool requireSameModelIdAsTarget = false;
        std::string agentName = "";
        std::vector<GW::Vec2f> polygon;
        bool rotateThroughTargets = false;
        std::unordered_set<GW::AgentID> recentlyTargetedEnemies;
        float minHp = 0.f;
        float maxHp = 100.f;
    };
    struct GoToTargetAction 
    {
        GoToTargetAction(InputStream&)
        {
            // No member variables
        } 
        std::string serialize()
        {
            OutputStream stream;
            stream << GoToTargetFinishCondition::StoppedMovingNextToTarget;
            return stream.str();
        }
    };
    struct ConditionedAction
    {
        ConditionedAction(InputStream& stream)
        {
            std::string read;
            stream >> read;
            if (read == missingContentToken)
                cond = nullptr;
            else if (read == "C")
                cond = readV8Condition(stream);
            else
                return;

            while (stream >> read) {
                if (read == endOfListToken)
                    break;
                else if (read == missingContentToken)
                    actionsIf.push_back(nullptr);
                else if (read == "A")
                    actionsIf.push_back(readV8Action(stream));
                else
                    return;
            }
            while (stream >> read) {
                if (read == endOfListToken)
                    break;
                else if (read == missingContentToken)
                    actionsElse.push_back(nullptr);
                else if (read == "A")
                    actionsElse.push_back(readV8Action(stream));
                else
                    return;
            }
        }
        std::string serialize()
        {
            OutputStream stream;
            
            const auto writeCondition = [&](const auto& condition) {
                if (condition)
                    condition->serialize(stream);
                else
                    stream << missingContentToken;
                stream.writeSeparator(2);
            };
            const auto writeActionSequence = [&](const auto& sequence) {
                stream << sequence.size();
                for (const auto& action : sequence) {
                    if (action)
                        action->serialize(stream);
                    else
                        stream << missingContentToken;
                    stream.writeSeparator(2);
                }
            };

            writeCondition(cond);
            writeActionSequence(actionsIf);
            writeActionSequence(actionsElse);

            stream << 0; //actionsElseIfSize

            return stream.str();
        }

        ConditionPtr cond = nullptr;
        std::vector<ActionPtr> actionsIf = {};
        std::vector<ActionPtr> actionsElse = {};
    };
    struct EquipItemAction {
        EquipItemAction(InputStream& stream)
        { 
            stream >> id;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << id << 0 << 0; // New Members: modstruct, hasModStruct
            return stream.str();
        }
        int id;
    };
}

ConditionPtr readV8Condition(InputStream& stream) 
{
    int type;
    stream >> type;
    switch (static_cast<ConditionType>(type)) {
        case ConditionType::Or : 
        {
            const auto converted = v8::DisjunctionConjunctionCondition(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<DisjunctionCondition>(convertedStream);
        }
        case ConditionType::And: 
        {
            const auto converted = v8::DisjunctionConjunctionCondition(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<ConjunctionCondition>(convertedStream);
        }
        case ConditionType::NearbyAgent: 
        {
            const auto converted = v8::NearbyAgentCondition(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<NearbyAgentCondition>(convertedStream);
        }
        case ConditionType::PlayerHasSkill: 
        {
            const auto converted = v8::PlayerHasSkillCondition(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasSkillCondition>(convertedStream);
        }
        case ConditionType::KeyIsPressed: 
        {
            const auto converted = v8::KeyPressCondition(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<KeyIsPressedCondition>(convertedStream);
        }
        // No changes, read with current deserializer:
        case ConditionType::Not:
            return std::make_shared<NegatedCondition>(stream);
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
        case ConditionType::InstanceTime:
            return std::make_shared<InstanceTimeCondition>(stream);
        case ConditionType::CanPopAgent:
            return std::make_shared<CanPopAgentCondition>(stream);

        default:
            return nullptr;
    }
}

ActionPtr readV8Action(InputStream& stream)
{
    int type;
    stream >> type;
    switch (static_cast<ActionType>(type)) {
        case ActionType::ChangeTarget: 
        {
            const auto converted = v8::ChangeTargetAction(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<ChangeTargetAction>(convertedStream);
        }
        case ActionType::Conditioned: 
        {
            const auto converted = v8::ConditionedAction(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<ConditionedAction>(convertedStream);
        }
        case ActionType::GoToTarget: 
        {
            const auto converted = v8::GoToTargetAction(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<GoToTargetAction>(convertedStream);
        }
        case ActionType::EquipItem: 
        {
            const auto converted = v8::EquipItemAction(stream).serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<EquipItemAction>(convertedStream);
        }

        // No changes, read with current deserializer:
        case ActionType::MoveTo:
            return std::make_shared<MoveToAction>(stream);
        case ActionType::Cast:
            return std::make_shared<CastAction>(stream);
        case ActionType::CastBySlot:
            return std::make_shared<CastBySlotAction>(stream);
        case ActionType::UseItem:
            return std::make_shared<UseItemAction>(stream);
        case ActionType::SendDialog:
            return std::make_shared<SendDialogAction>(stream);
        case ActionType::Wait:
            return std::make_shared<WaitAction>(stream);
        case ActionType::SendChat:
            return std::make_shared<SendChatAction>(stream);
        case ActionType::Cancel:
            return std::make_shared<CancelAction>(stream);
        case ActionType::DropBuff:
            return std::make_shared<DropBuffAction>(stream);
        case ActionType::RepopMinipet:
            return std::make_shared<RepopMinipetAction>(stream);
        case ActionType::PingHardMode:
            return std::make_shared<PingHardModeAction>(stream);
        case ActionType::PingTarget:
            return std::make_shared<PingTargetAction>(stream);
        case ActionType::AutoAttackTarget:
            return std::make_shared<AutoAttackTargetAction>(stream);
        case ActionType::ChangeWeaponSet:
            return std::make_shared<ChangeWeaponSetAction>(stream);
        case ActionType::StoreTarget:
            return std::make_shared<StoreTargetAction>(stream);
        case ActionType::RestoreTarget:
            return std::make_shared<RestoreTargetAction>(stream);
        case ActionType::StopScript:
            return std::make_shared<StopScriptAction>(stream);
        default:
            return nullptr;
    }
}
