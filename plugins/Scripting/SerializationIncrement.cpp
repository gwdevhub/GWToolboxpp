#include <SerializationIncrement.h>

#include <ActionImpls.h>
#include <ConditionImpls.h>
#include <Characteristic.h>

// v10 was deprecated when most conditions were moved to use characteristics. Save the schema to stll be able to read them.
namespace v10 {
    const std::string missingContentToken = "/";
    const std::string endOfListToken = ">";

    struct PlayerIsNearPositionCondition
    {
        PlayerIsNearPositionCondition(InputStream& stream) 
        { 
            stream >> pos.x >> pos.y >> accuracy;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Position << pos.x << pos.y << accuracy << ComparisonOperator::LessOrEqual;
            stream.writeSeparator(3);
            return stream.str();
        }
        GW::GamePos pos = {};
        float accuracy = GW::Constants::Range::Adjacent;
    };

    struct PlayerHasClassCondition 
    {
        PlayerHasClassCondition(InputStream& stream) 
        { 
            stream >> primary >> secondary;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Class << primary << secondary << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }

        Class primary = Class::Any;
        Class secondary = Class::Any;
    };

    struct PlayerOrTargetHasHpBelowCondition 
    {
        PlayerOrTargetHasHpBelowCondition(InputStream& stream) 
        { 
            stream >> hp;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::HP << hp << ComparisonOperator::LessOrEqual;
            stream.writeSeparator(3);
            return stream.str();
        }

        float hp = 50.f;
    };

    struct PlayerOrTargetHasNameCondition 
    {
        PlayerOrTargetHasNameCondition(InputStream& stream) 
        { 
            name = readStringWithSpaces(stream); 
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Name;
            writeStringWithSpaces(stream, name);
            stream << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }

        std::string name;
    };

    struct PlayerOrTargetStatusCondition 
    {
        PlayerOrTargetStatusCondition(InputStream& stream) 
        { 
            stream >> status;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Status << status << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }

        Status status = Status::Enchanted;
    };

    struct PlayerIsIdleCondition 
    {
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Status << Status::Idle << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }
    };

    struct PlayerInPolygonCondition 
    {
        PlayerInPolygonCondition(InputStream& stream) 
        { 
            polygon = readPositions(stream);
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::PositionPolygon;
            writePositions(stream, polygon);
            stream << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }
        std::vector<GW::Vec2f> polygon;
    };

    struct PlayerOrTargetIsCastingSkillCondition 
    {
        PlayerOrTargetIsCastingSkillCondition(InputStream& stream) 
        { 
            stream >> id;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Skill << id << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }
        GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
    };

    struct TargetHasModelCondition 
    {
        TargetHasModelCondition(InputStream& stream) 
        { 
            stream >> modelId;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Model << modelId << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }
        uint16_t modelId = 0;
    };

    struct TargetHasAllegianceCondition 
    {
        TargetHasAllegianceCondition(InputStream& stream) 
        { 
            stream >> agentType;
        }
        std::string serialize()
        {
            OutputStream stream;
            stream << "X" << CharacteristicType::Allegiance << agentType << IsIsNot::Is;
            stream.writeSeparator(3);
            return stream.str();
        }
        AgentType agentType = AgentType::Hostile;
    };

    struct TargetDistanceCondition 
    {
        TargetDistanceCondition(InputStream& stream) 
        { 
            stream >> minDistance >> maxDistance;
        }
        std::string serialize()
        {
            OutputStream stream;
            if (minDistance > 0.f) 
            {
                stream << "X" << CharacteristicType::DistanceToPlayer << minDistance << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            else if (maxDistance < 5000.f) 
            {
                stream << "X" << CharacteristicType::DistanceToPlayer << maxDistance << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }

            return stream.str();
        }
        float minDistance = 0.f;
        float maxDistance = 5000.f;
    };

    struct NearbyAgentCondition 
    {
        NearbyAgentCondition(InputStream& stream) 
        {
            stream >> agentType >> primary >> secondary >> alive >> hexed >> skill >> modelId >> minDistance >> maxDistance >> minHp >> maxHp;
            agentName = readStringWithSpaces(stream);
            polygon = readPositions(stream);
            stream >> minAngle >> maxAngle >> enchanted >> weaponspelled >> poisoned >> bleeding >> minSpeed >> maxSpeed >> minRegen >> maxRegen >> weapon;
        }
        std::string serialize()
        {
            OutputStream stream;
            
            stream << ComparisonOperator::GreaterOrEqual << 1;

            const auto maybeWriteStatus = [&stream](AnyNoYes any, Status status) 
            {
                if (any != AnyNoYes::Any) {
                    stream << "X" << CharacteristicType::Status << status << (any == AnyNoYes::No ? IsIsNot::IsNot : IsIsNot::Is);
                    stream.writeSeparator(3);
                }
            };
            
            if (agentType != AgentType::Any) 
            {
                stream << "X" << CharacteristicType::Allegiance << agentType << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (primary != Class::Any || secondary != Class::Any) 
            {
                stream << "X" << CharacteristicType::Class << primary << secondary << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            maybeWriteStatus(alive, Status::Alive);
            maybeWriteStatus(hexed, Status::Hexed);
            maybeWriteStatus(bleeding, Status::Bleeding);
            maybeWriteStatus(poisoned, Status::Poisoned);
            maybeWriteStatus(weaponspelled, Status::WeaponSpelled);
            maybeWriteStatus(enchanted, Status::Enchanted);

            if (skill != GW::Constants::SkillID::No_Skill) 
            {
                stream << "X" << CharacteristicType::Skill << skill << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (modelId != 0) 
            {
                stream << "X" << CharacteristicType::Model << modelId << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (minDistance > 0.f) 
            {
                stream << "X" << CharacteristicType::DistanceToPlayer << minDistance << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxDistance < 5000.f) 
            {
                stream << "X" << CharacteristicType::DistanceToPlayer << maxDistance << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (!agentName.empty()) 
            {
                stream << "X" << CharacteristicType::Name;
                writeStringWithSpaces(stream, agentName);
                stream << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (minHp > 0.f) 
            {
                stream << "X" << CharacteristicType::HP << minHp << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxHp < 100.f) 
            {
                stream << "X" << CharacteristicType::HP << maxHp << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (minAngle > 0.f) 
            {
                stream << "X" << CharacteristicType::AngleToPlayerForward << minAngle << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxAngle < 180.f) 
            {
                stream << "X" << CharacteristicType::AngleToPlayerForward << maxAngle << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (minSpeed > 0.f) 
            {
                stream << "X" << CharacteristicType::Speed << minSpeed << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (std::abs(maxSpeed - 1000.f) > 0.1f) 
            {
                stream << "X" << CharacteristicType::Speed << maxSpeed << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (minRegen > -10) {
                stream << "X" << CharacteristicType::HPRegen << minRegen << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxRegen < 10) {
                stream << "X" << CharacteristicType::HPRegen << maxRegen << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (weapon != WeaponType::Any) 
            {
                stream << "X" << CharacteristicType::WeaponType << weapon << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (!polygon.empty()) {
                stream << "X" << CharacteristicType::PositionPolygon;
                writePositions(stream, polygon);
                stream << IsIsNot::Is;
                stream.writeSeparator(3);
            }

            return stream.str();
        }
        AgentType agentType = AgentType::Any;
        Class primary = Class::Any;
        Class secondary = Class::Any;
        AnyNoYes alive = AnyNoYes::Yes;
        AnyNoYes hexed = AnyNoYes::Any;
        AnyNoYes bleeding = AnyNoYes::Any;
        AnyNoYes poisoned = AnyNoYes::Any;
        AnyNoYes weaponspelled = AnyNoYes::Any;
        AnyNoYes enchanted = AnyNoYes::Any;
        GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
        uint16_t modelId = 0;
        float minDistance = 0.f;
        float maxDistance = 5000.f;
        std::string agentName = "";
        std::vector<GW::Vec2f> polygon;
        float minHp = 0.f;
        float maxHp = 100.f;
        float minAngle = 0.f;
        float maxAngle = 180.f;
        float minSpeed = 0.f;
        float maxSpeed = 1000.f;
        int minRegen = -10;
        int maxRegen = 10;
        WeaponType weapon = WeaponType::Any;
    };

    struct ChangeTargetAction {
        ChangeTargetAction(InputStream& stream)
        {
            stream >> agentType >> primary >> secondary >> alive >> skill >> sorting >> modelId >> minDistance >> maxDistance >> requireSameModelIdAsTarget >> preferNonHexed >> rotateThroughTargets;
            agentName = readStringWithSpaces(stream);
            polygon = readPositions(stream);
            stream >> minAngle >> maxAngle >> enchanted >> weaponspelled >> poisoned >> bleeding >> hexed >> minSpeed >> maxSpeed >> minRegen >> maxRegen >> weapon >> minHp >> maxHp;
        }
        std::string serialize()
        {
            OutputStream stream;

            stream << sorting << preferNonHexed << requireSameModelIdAsTarget << rotateThroughTargets;

            const auto maybeWriteStatus = [&stream](AnyNoYes any, Status status) {
                if (any != AnyNoYes::Any) {
                    stream << "X" << CharacteristicType::Status << status << (any == AnyNoYes::No ? IsIsNot::IsNot : IsIsNot::Is);
                    stream.writeSeparator(3);
                }
            };

            if (agentType != AgentType::Any) {
                stream << "X" << CharacteristicType::Allegiance << agentType << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (primary != Class::Any || secondary != Class::Any) {
                stream << "X" << CharacteristicType::Class << primary << secondary << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            maybeWriteStatus(alive, Status::Alive);
            maybeWriteStatus(hexed, Status::Hexed);
            maybeWriteStatus(bleeding, Status::Bleeding);
            maybeWriteStatus(poisoned, Status::Poisoned);
            maybeWriteStatus(weaponspelled, Status::WeaponSpelled);
            maybeWriteStatus(enchanted, Status::Enchanted);

            if (skill != GW::Constants::SkillID::No_Skill) {
                stream << "X" << CharacteristicType::Skill << skill << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (modelId != 0) {
                stream << "X" << CharacteristicType::Model << modelId << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (minDistance > 0.f) {
                stream << "X" << CharacteristicType::DistanceToPlayer << minDistance << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxDistance < 5000.f) {
                stream << "X" << CharacteristicType::DistanceToPlayer << maxDistance << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (!agentName.empty()) {
                stream << "X" << CharacteristicType::Name;
                writeStringWithSpaces(stream, agentName);
                stream << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (minHp > 0.f) {
                stream << "X" << CharacteristicType::HP << minHp << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxHp < 100.f) {
                stream << "X" << CharacteristicType::HP << maxHp << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (minAngle > 0.f) {
                stream << "X" << CharacteristicType::AngleToPlayerForward << minAngle << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxAngle < 180.f) {
                stream << "X" << CharacteristicType::AngleToPlayerForward << maxAngle << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (minSpeed > 0.f) {
                stream << "X" << CharacteristicType::Speed << minSpeed << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (std::abs(maxSpeed - 1000.f) > 0.1f) {
                stream << "X" << CharacteristicType::Speed << maxSpeed << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (minRegen > -10) {
                stream << "X" << CharacteristicType::HPRegen << minRegen << ComparisonOperator::GreaterOrEqual;
                stream.writeSeparator(3);
            }
            if (maxRegen < 10) {
                stream << "X" << CharacteristicType::HPRegen << maxRegen << ComparisonOperator::LessOrEqual;
                stream.writeSeparator(3);
            }
            if (weapon != WeaponType::Any) {
                stream << "X" << CharacteristicType::WeaponType << weapon << IsIsNot::Is;
                stream.writeSeparator(3);
            }
            if (!polygon.empty()) {
                stream << "X" << CharacteristicType::PositionPolygon;
                writePositions(stream, polygon);
                stream << IsIsNot::Is;
                stream.writeSeparator(3);
            }

            return stream.str();
        }
        AgentType agentType = AgentType::Any;
        Class primary = Class::Any;
        Class secondary = Class::Any;
        AnyNoYes alive = AnyNoYes::Yes;
        AnyNoYes bleeding = AnyNoYes::Any;
        AnyNoYes poisoned = AnyNoYes::Any;
        AnyNoYes weaponspelled = AnyNoYes::Any;
        AnyNoYes enchanted = AnyNoYes::Any;
        AnyNoYes hexed = AnyNoYes::Any;
        GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
        Sorting sorting = Sorting::AgentId;
        uint16_t modelId = 0;
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
        float minAngle = 0.f;
        float maxAngle = 180.f;
        float minSpeed = 0.f;
        float maxSpeed = 1000.f;
        int minRegen = -10;
        int maxRegen = 10;
        WeaponType weapon = WeaponType::Any;
    };

    struct NegatedCondition {
        NegatedCondition(InputStream& stream)
        {
            std::string read;
            stream >> read;
            if (read == missingContentToken)
                cond = nullptr;
            else if (read == "C")
                cond = readV10Condition(stream);
            else
                return;
        }
        std::string serialize() const
        {
            OutputStream stream;
            if (cond)
                cond->serialize(stream);
            else
                stream << missingContentToken;
            return stream.str();
        }
        ConditionPtr cond;
    };
    struct DisjunctionConjunctionCondition {
        DisjunctionConjunctionCondition(InputStream& stream)
        {
            int count;
            std::string token;
            stream >> count;

            for (int i = 0; i < count; ++i) {
                if (stream.isAtSeparator() || !(stream >> token)) continue;

                if (token == missingContentToken)
                    conditions.push_back(nullptr);
                else if (token == "C") {
                    if (auto read = readV10Condition(stream)) conditions.push_back(std::move(read));
                }
                else {
                    return;
                }

                stream.proceedPastSeparator(2);
            }
        }
        std::string serialize() const
        {
            OutputStream stream;
            stream << conditions.size();

            for (const auto& condition : conditions) {
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

    struct OnceCondition {
        OnceCondition(InputStream& stream)
        {
            std::string read;
            stream >> read;
            if (read == missingContentToken)
                cond = nullptr;
            else if (read == "C")
                cond = readV10Condition(stream);
            else
                return;
        }
        std::string serialize() const
        {
            OutputStream stream;
            if (cond)
                cond->serialize(stream);
            else
                stream << missingContentToken;
            return stream.str();
        }
        ConditionPtr cond = nullptr;
    };
    struct UntilCondition {
        UntilCondition(InputStream& stream)
        {
            std::string read;
            stream >> read;
            if (read == missingContentToken)
                cond = nullptr;
            else if (read == "C")
                cond = readV10Condition(stream);
            else
                return;
        }
        std::string serialize() const
        {
            OutputStream stream;
            if (cond)
                cond->serialize(stream);
            else
                stream << missingContentToken;
            return stream.str();
        }
        ConditionPtr cond = nullptr;
    };
    struct AfterCondition 
    {
        AfterCondition(InputStream& stream)
        {
            std::string read;
            stream >> read;
            if (read == missingContentToken)
                cond = nullptr;
            else if (read == "C")
                cond = readV10Condition(stream);
            else
                return;
        }
        std::string serialize() const
        {
            OutputStream stream;
            if (cond)
                cond->serialize(stream);
            else
                stream << missingContentToken;
            return stream.str();
        }

        ConditionPtr cond = nullptr;
    };
    struct ToggleCondition {
        ToggleCondition(InputStream& stream)
        {
            stream >> defaultState;

            std::string read;

            stream >> read;
            if (read == missingContentToken)
                toggleOnCond = nullptr;
            else if (read == "C") {
                toggleOnCond = readV10Condition(stream);
                stream.proceedPastSeparator(2);
            }
            else
                return;

            stream >> read;
            if (read == missingContentToken)
                toggleOffCond = nullptr;
            else if (read == "C") {
                toggleOffCond = readV10Condition(stream);
                stream.proceedPastSeparator(2);
            }
        }
        std::string serialize() const
        {
            OutputStream stream;
            stream << defaultState;

            if (toggleOnCond) {
                toggleOnCond->serialize(stream);
                stream.writeSeparator(2);
            }
            else
                stream << missingContentToken;

            if (toggleOffCond) {
                toggleOffCond->serialize(stream);
                stream.writeSeparator(2);
            }
            else
                stream << missingContentToken;
            return stream.str();
        }

        TrueFalse defaultState = TrueFalse::False;
        ConditionPtr toggleOnCond = nullptr;
        ConditionPtr toggleOffCond = nullptr;
    };
    struct ConditionedAction {
        ConditionedAction(InputStream& stream)
        {
            std::string read;
            const auto readCond = [&](auto& condition) {
                stream >> read;
                if (read == missingContentToken)
                    condition = nullptr;
                else if (read == "C")
                    condition = readV10Condition(stream);
                else
                    throw std::runtime_error("Invalid string");

                stream.proceedPastSeparator(2);
            };
            const auto readActionSequence = [&](auto& sequence) {
                size_t size;
                stream >> size;
                for (size_t i = 0; i < size; ++i) {
                    if (stream.isAtSeparator() || !(stream >> read)) continue;

                    if (read == missingContentToken)
                        sequence.push_back(nullptr);
                    else if (read == "A")
                        sequence.push_back(readV10Action(stream));
                    else
                        throw std::runtime_error("Invalid string");

                    stream.proceedPastSeparator(2);
                }
            };

            try {
                readCond(cond);
                readActionSequence(actionsIf);
                readActionSequence(actionsElse);

                size_t actionsElseIfSize;
                stream >> actionsElseIfSize;

                for (size_t i = 0; i < actionsElseIfSize; ++i) {
                    actionsElseIf.push_back({});
                    readCond(actionsElseIf.back().first);
                    readActionSequence(actionsElseIf.back().second);
                }
            } catch (...) {}
        }
        std::string serialize() const
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

            stream << actionsElseIf.size();
            for (const auto& [condEI, actionsEI] : actionsElseIf) {
                writeCondition(condEI);
                writeActionSequence(actionsEI);
            }
            return stream.str();
        }
        ConditionPtr cond = nullptr;
        std::vector<ActionPtr> actionsIf = {};
        std::vector<ActionPtr> actionsElse = {};
        std::vector<std::pair<ConditionPtr, std::vector<ActionPtr>>> actionsElseIf = {};
    };
    struct WaitUntilAction {
        WaitUntilAction(InputStream& stream)
        {
            std::string read;
            stream >> read;
            if (read == missingContentToken)
                condition = nullptr;
            else if (read == "C")
                condition = readV10Condition(stream);
        }
        std::string serialize() const
        {
            OutputStream stream;
            if (condition)
                condition->serialize(stream);
            else
                stream << missingContentToken;
            return stream.str();
        }
        ConditionPtr condition;
    };

    enum class ConditionType : int {
        Not,
        Or,
        And,
        IsInMap,
        QuestHasState,
        PartyPlayerCount,
        PartyMemberStatus,
        HasPartyWindowAllyOfName,
        InstanceProgress,
        InstanceTime,
        OnlyTriggerOncePerInstance,
        CanPopAgent,
        PlayerIsNearPosition,
        PlayerHasBuff,
        PlayerHasSkill,
        PlayerHasClass,
        PlayerHasName,
        PlayerHasEnergy,
        PlayerIsIdle,
        PlayerHasItemEquipped,
        KeyIsPressed,
        CurrentTargetHasHpBelow,
        CurrentTargetIsUsingSkill,
        CurrentTargetHasModel,
        CurrentTargetAllegiance,
        NearbyAgent,
        CurrentTargetDistance,
        PlayerHasHpBelow,
        PartyHasLoadedIn,
        ItemInInventory,
        PlayerStatus,
        CurrentTargetStatus,
        PlayerInPolygon,
        InstanceType,
        RemainingCooldown,
        FoeCount,
        PlayerMorale,
        False,
        True,
        Until,
        Once,
        Toggle,
        After,
        CurrentTargetName,
        Throttle,
        PlayerIsCastingSkill
    };
}

ConditionPtr readV10Condition(InputStream& stream) 
{
    int type;
    stream >> type;
    switch (static_cast<v10::ConditionType>(type)) {
        case v10::ConditionType::PlayerIsNearPosition:
        {
            const auto converted = v10::PlayerIsNearPositionCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::PlayerHasClass: 
        {
            const auto converted = v10::PlayerHasClassCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::PlayerHasName: 
        {
            const auto converted = v10::PlayerOrTargetHasNameCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::PlayerHasHpBelow: 
        {
            const auto converted = v10::PlayerOrTargetHasHpBelowCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::PlayerStatus: 
        {
            const auto converted = v10::PlayerOrTargetStatusCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::PlayerIsIdle: 
        {
            const auto converted = v10::PlayerIsIdleCondition{}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::PlayerInPolygon: 
        {
            const auto converted = v10::PlayerInPolygonCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::PlayerIsCastingSkill: 
        {
            const auto converted = v10::PlayerOrTargetIsCastingSkillCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<PlayerHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::CurrentTargetHasHpBelow: 
        {
            const auto converted = v10::PlayerOrTargetHasHpBelowCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<TargetHasCharacteristicsCondition>(convertedStream); 
        }
        case v10::ConditionType::CurrentTargetIsUsingSkill: 
        {
            const auto converted = v10::PlayerOrTargetIsCastingSkillCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<TargetHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::CurrentTargetStatus: 
        {
            const auto converted = v10::PlayerOrTargetStatusCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<TargetHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::CurrentTargetName: 
        {
            const auto converted = v10::PlayerOrTargetHasNameCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<TargetHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::CurrentTargetHasModel: 
        {
            const auto converted = v10::TargetHasModelCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<TargetHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::CurrentTargetAllegiance: 
        {
            const auto converted = v10::TargetHasAllegianceCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<TargetHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::CurrentTargetDistance: 
        {
            const auto converted = v10::TargetDistanceCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<TargetHasCharacteristicsCondition>(convertedStream);
        }
        case v10::ConditionType::NearbyAgent:
        {
            const auto converted = v10::NearbyAgentCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<AgentWithCharacteristicsCountCondition>(convertedStream);
        }

        // Unchanged, but read/write Conditions
        case v10::ConditionType::Not: 
        {
            const auto converted = v10::NegatedCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<NegatedCondition>(convertedStream);
        }
        case v10::ConditionType::Or:
        {
            const auto converted = v10::DisjunctionConjunctionCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<DisjunctionCondition>(convertedStream);
        }
        case v10::ConditionType::And:
        {
            const auto converted = v10::DisjunctionConjunctionCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<ConjunctionCondition>(convertedStream);
        }
        case v10::ConditionType::Once:
        {
            const auto converted = v10::OnceCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<OnceCondition>(convertedStream);
        }
        case v10::ConditionType::Until:
        {
            const auto converted = v10::UntilCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<UntilCondition>(convertedStream);
        }
        case v10::ConditionType::Toggle:
        {
            const auto converted = v10::ToggleCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<ToggleCondition>(convertedStream);
        }
        case v10::ConditionType::After:
        {
            const auto converted = v10::AfterCondition{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<AfterCondition>(convertedStream);
        }

        // Unchanged:
        case v10::ConditionType::IsInMap:
            return std::make_shared<IsInMapCondition>(stream);
        case v10::ConditionType::QuestHasState:
            return std::make_shared<QuestHasStateCondition>(stream);
        case v10::ConditionType::PartyPlayerCount:
            return std::make_shared<PartyPlayerCountCondition>(stream);
        case v10::ConditionType::PartyHasLoadedIn:
            return std::make_shared<PartyHasLoadedInCondition>(stream);
        case v10::ConditionType::InstanceProgress:
            return std::make_shared<InstanceProgressCondition>(stream);
        case v10::ConditionType::HasPartyWindowAllyOfName:
            return std::make_shared<HasPartyWindowAllyOfNameCondition>(stream);
        case v10::ConditionType::OnlyTriggerOncePerInstance:
            return std::make_shared<OnlyTriggerOnceCondition>(stream);
        case v10::ConditionType::PartyMemberStatus:
            return std::make_shared<PartyMemberStatusCondition>(stream);
        case v10::ConditionType::InstanceType:
            return std::make_shared<InstanceTypeCondition>(stream);

        case v10::ConditionType::PlayerHasBuff:
            return std::make_shared<PlayerHasBuffCondition>(stream);
        case v10::ConditionType::PlayerHasSkill:
            return std::make_shared<PlayerHasSkillCondition>(stream);
        case v10::ConditionType::PlayerHasEnergy:
            return std::make_shared<PlayerHasEnergyCondition>(stream);
        case v10::ConditionType::PlayerHasItemEquipped:
            return std::make_shared<PlayerHasItemEquippedCondition>(stream);
        case v10::ConditionType::ItemInInventory:
            return std::make_shared<ItemInInventoryCondition>(stream);
        case v10::ConditionType::PlayerMorale:
            return std::make_shared<MoraleCondition>(stream);
        case v10::ConditionType::RemainingCooldown:
            return std::make_shared<RemainingCooldownCondition>(stream);
        case v10::ConditionType::KeyIsPressed:
            return std::make_shared<KeyIsPressedCondition>(stream);
        case v10::ConditionType::InstanceTime:
            return std::make_shared<InstanceTimeCondition>(stream);

        case v10::ConditionType::CanPopAgent:
            return std::make_shared<CanPopAgentCondition>(stream);
        case v10::ConditionType::FoeCount:
            return std::make_shared<FoeCountCondition>(stream);
        case v10::ConditionType::Throttle:
            return std::make_shared<ThrottleCondition>(stream);

        case v10::ConditionType::True:
            return std::make_shared<TrueCondition>(stream);
        case v10::ConditionType::False:
            return std::make_shared<FalseCondition>(stream);

        default:
            return nullptr;
    }
}

ActionPtr readV10Action(InputStream& stream)
{
    int type;
    stream >> type;
    switch (static_cast<ActionType>(type)) {
        case ActionType::ChangeTarget: 
        {
            const auto converted = v10::ChangeTargetAction{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<ChangeTargetAction>(convertedStream);
        }

        // Unchanged, but read/write Conditions
        case ActionType::Conditioned: 
        {
            const auto converted = v10::ConditionedAction{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<ConditionedAction>(convertedStream);
        }
        case ActionType::WaitUntil: 
        {
            const auto converted = v10::WaitUntilAction{stream}.serialize();
            auto convertedStream = InputStream{converted};
            return std::make_shared<WaitUntilAction>(convertedStream);
        }
        
        // Unchanged:
        case ActionType::MoveTo:
            return std::make_shared<MoveToAction>(stream);
        case ActionType::MoveToTargetPosition:
            return std::make_shared<MoveToTargetPositionAction>(stream);
        case ActionType::MoveInchwise:
            return std::make_shared<MoveInchwiseAction>(stream);
        case ActionType::Cast:
            return std::make_shared<CastAction>(stream);
        case ActionType::CastBySlot:
            return std::make_shared<CastBySlotAction>(stream);
        case ActionType::UseItem:
            return std::make_shared<UseItemAction>(stream);
        case ActionType::SendDialog:
            return std::make_shared<SendDialogAction>(stream);
        case ActionType::GoToTarget:
            return std::make_shared<GoToTargetAction>(stream);
        case ActionType::Wait:
            return std::make_shared<WaitAction>(stream);
        case ActionType::SendChat:
            return std::make_shared<SendChatAction>(stream);
        case ActionType::Cancel:
            return std::make_shared<CancelAction>(stream);
        case ActionType::DropBuff:
            return std::make_shared<DropBuffAction>(stream);
        case ActionType::EquipItem:
            return std::make_shared<EquipItemAction>(stream);
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
        case ActionType::LogOut:
            return std::make_shared<LogOutAction>(stream);
        case ActionType::UseHeroSkill:
            return std::make_shared<UseHeroSkillAction>(stream);
        case ActionType::UnequipItem:
            return std::make_shared<UnequipItemAction>(stream);
        case ActionType::ClearTarget:
            return std::make_shared<ClearTargetAction>(stream);
        case ActionType::GWKey:
            return std::make_shared<GWKeyAction>(stream);
        default:
            return nullptr;
    }
}
