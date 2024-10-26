#include <CharacteristicIO.h>
#include <CharacteristicImpls.h>

#include <imgui.h>

namespace {
    std::string_view toString(CharacteristicType type)
    {
        static_assert((int)CharacteristicType::Count == 19);
        switch (type) {
            case CharacteristicType::Position:
                return "Position";
            case CharacteristicType::PositionPolygon:
                return "Position in polygon";
            case CharacteristicType::DistanceToPlayer:
                return "Distance to player";
            case CharacteristicType::DistanceToTarget:
                return "Distance to target";
            case CharacteristicType::DistanceToModelId:
                return "Distance to model ID";
            case CharacteristicType::Class:
                return "Class";
            case CharacteristicType::Name:
                return "Name";
            case CharacteristicType::HP:
                return "HP";
            case CharacteristicType::Speed:
                return "Speed";
            case CharacteristicType::HPRegen:
                return "Regeneration";
            case CharacteristicType::WeaponType:
                return "Weapon type";
            case CharacteristicType::Model:
                return "Model ID";
            case CharacteristicType::Allegiance:
                return "Allegiance";
            case CharacteristicType::Status:
                return "Status";
            case CharacteristicType::Skill:
                return "Uses skill";
            case CharacteristicType::Bond:
                return "Affected by Bond";
            case CharacteristicType::AngleToPlayerForward:
                return "Angle to player forward";
            case CharacteristicType::AngleToCameraForward:
                return "Angle to camera forward";
            case CharacteristicType::IsStoredTarget:
                return "Is stored target";
            default:
                return "";
        }
    }
} // namespace

CharacteristicPtr makeCharacteristic(CharacteristicType type)
{
    static_assert((int)CharacteristicType::Count == 19);
    switch (type) {
        case CharacteristicType::Position:
            return std::make_unique<PositionCharacteristic>();
        case CharacteristicType::PositionPolygon:
            return std::make_unique<PositionPolygonCharacteristic>();
        case CharacteristicType::DistanceToPlayer:
            return std::make_unique<DistanceToPlayerCharacteristic>();
        case CharacteristicType::DistanceToTarget:
            return std::make_unique<DistanceToTargetCharacteristic>();
        case CharacteristicType::DistanceToModelId:
            return std::make_unique<DistanceToModelIdCharacteristic>();
        case CharacteristicType::Class:
            return std::make_unique<ClassCharacteristic>();
        case CharacteristicType::Name:
            return std::make_unique<NameCharacteristic>();
        case CharacteristicType::HP:
            return std::make_unique<HPCharacteristic>();
        case CharacteristicType::Speed:
            return std::make_unique<SpeedCharacteristic>();
        case CharacteristicType::HPRegen:
            return std::make_unique<HPRegenCharacteristic>();
        case CharacteristicType::WeaponType:
            return std::make_unique<WeaponTypeCharacteristic>();
        case CharacteristicType::Model:
            return std::make_unique<ModelCharacteristic>();
        case CharacteristicType::Allegiance:
            return std::make_unique<AllegianceCharacteristic>();
        case CharacteristicType::Status:
            return std::make_unique<StatusCharacteristic>();
        case CharacteristicType::Skill:
            return std::make_unique<SkillCharacteristic>();
        case CharacteristicType::Bond:
            return std::make_unique<BondCharacteristic>();
        case CharacteristicType::AngleToPlayerForward:
            return std::make_unique<AngleToPlayerForwardCharacteristic>();
        case CharacteristicType::AngleToCameraForward:
            return std::make_unique<AngleToCameraForwardCharacteristic>();
        case CharacteristicType::IsStoredTarget:
            return std::make_unique<IsStoredTargetCharacteristic>();
        default:
            return nullptr;
    }
}

CharacteristicPtr readCharacteristic(InputStream& stream)
{
    static_assert((int)CharacteristicType::Count == 19);
    int type;
    stream >> type;
    switch (static_cast<CharacteristicType>(type)) {
        case CharacteristicType::Position:
            return std::make_unique<PositionCharacteristic>(stream);
        case CharacteristicType::PositionPolygon:
            return std::make_unique<PositionPolygonCharacteristic>(stream);
        case CharacteristicType::DistanceToPlayer:
            return std::make_unique<DistanceToPlayerCharacteristic>(stream);
        case CharacteristicType::DistanceToTarget:
            return std::make_unique<DistanceToTargetCharacteristic>(stream);
        case CharacteristicType::DistanceToModelId:
            return std::make_unique<DistanceToModelIdCharacteristic>(stream);
        case CharacteristicType::Class:
            return std::make_unique<ClassCharacteristic>(stream);
        case CharacteristicType::Name:
            return std::make_unique<NameCharacteristic>(stream);
        case CharacteristicType::HP:
            return std::make_unique<HPCharacteristic>(stream);
        case CharacteristicType::Speed:
            return std::make_unique<SpeedCharacteristic>(stream);
        case CharacteristicType::HPRegen:
            return std::make_unique<HPRegenCharacteristic>(stream);
        case CharacteristicType::WeaponType:
            return std::make_unique<WeaponTypeCharacteristic>(stream);
        case CharacteristicType::Model:
            return std::make_unique<ModelCharacteristic>(stream);
        case CharacteristicType::Allegiance:
            return std::make_unique<AllegianceCharacteristic>(stream);
        case CharacteristicType::Status:
            return std::make_unique<StatusCharacteristic>(stream);
        case CharacteristicType::Skill:
            return std::make_unique<SkillCharacteristic>(stream);
        case CharacteristicType::Bond:
            return std::make_unique<BondCharacteristic>(stream);
        case CharacteristicType::AngleToPlayerForward:
            return std::make_unique<AngleToPlayerForwardCharacteristic>(stream);
        case CharacteristicType::AngleToCameraForward:
            return std::make_unique<AngleToCameraForwardCharacteristic>(stream);
        case CharacteristicType::IsStoredTarget:
            return std::make_unique<IsStoredTargetCharacteristic>(stream);
        default:
            return nullptr;
    }
}

std::optional<CharacteristicType> drawCharacteristicSubMenu(std::unordered_set<CharacteristicType> toSkip)
{
    std::optional<CharacteristicType> result;
    const auto drawCharacteristicSelector = [&](CharacteristicType type) {
        if (toSkip.contains(type)) return;
        if (ImGui::MenuItem(toString(type).data())) {
            result = type;
        }
    };
    const auto drawSubMenu = [&](std::string_view title, const auto& types) {
        if (std::ranges::all_of(types, [&](const auto& type){ return toSkip.contains(type); })) return;
        if (ImGui::BeginMenu(title.data())) {
            for (const auto& type : types)
                drawCharacteristicSelector(type);
            ImGui::EndMenu();
        }
    };

    constexpr auto positionCharacteristics = std::array{CharacteristicType::Position, CharacteristicType::PositionPolygon, CharacteristicType::DistanceToPlayer, CharacteristicType::DistanceToTarget, CharacteristicType::DistanceToModelId};
    constexpr auto angleCharacteristics = std::array{CharacteristicType::AngleToPlayerForward, CharacteristicType::AngleToCameraForward};
    constexpr auto hpCharacteristics = std::array{CharacteristicType::HP, CharacteristicType::HPRegen};
    constexpr auto skillCharacteristics = std::array{CharacteristicType::Skill, CharacteristicType::Bond};
    constexpr auto propertyCharacteristics = std::array{CharacteristicType::Model, CharacteristicType::Class, CharacteristicType::Name, CharacteristicType::Speed, CharacteristicType::WeaponType, CharacteristicType::IsStoredTarget};

    drawSubMenu("Position", positionCharacteristics);
    drawSubMenu("Angle", angleCharacteristics);
    drawSubMenu("Health", hpCharacteristics);
    drawSubMenu("Skill", skillCharacteristics);
    drawSubMenu("Properties", propertyCharacteristics);
    drawCharacteristicSelector(CharacteristicType::Status);
    drawCharacteristicSelector(CharacteristicType::Allegiance);

    return result;
}

CharacteristicPtr drawCharacteristicSelector(float width)
{
    CharacteristicPtr result = nullptr;

    if (ImGui::Button("Add Characteristic", ImVec2(width, 0))) 
    {
        ImGui::OpenPopup("Add Characteristic");
    }

    if (ImGui::BeginPopup("Add Characteristic")) 
    {
        if (const auto type = drawCharacteristicSubMenu())
            result = makeCharacteristic(*type);
        ImGui::EndPopup();
    }

    return result;
}
