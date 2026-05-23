#include <enumUtils.h>

#include <commonIncludes.h>
#include <bitset>
#include <Keys.h>
#include <ImGuiCppWrapper.h>

#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <ModelNames.h>

std::string getSkillName(GW::Constants::SkillID id, bool zeroIsAny)
{
    if (id == GW::Constants::SkillID::No_Skill)
        return zeroIsAny ? "Any" : "No skill";
    static std::unordered_map<GW::Constants::SkillID, std::wstring> decodedNames;
    if (const auto it = decodedNames.find(id); it != decodedNames.end()) {
        return WStringToString(it->second);
    }

    const auto skillData = GW::SkillbarMgr::GetSkillConstantData(id);
    if (!skillData || (uint32_t)id >= (uint32_t)GW::Constants::SkillID::Count) return "";

    wchar_t out[8] = {0};
    if (GW::UI::UInt32ToEncStr(skillData->name, out, _countof(out))) {
        GW::UI::AsyncDecodeStr(
            out,
            [](void* param, const wchar_t* s) {
                GWCA_ASSERT(param && s);
                std::wstring* str = (std::wstring*)param;
                *str = s;
            },
            &decodedNames[id]
        );
    }
    return "";
}

std::string WStringToString(const std::wstring_view str)
{
    if (str.empty()) {
        return "";
    }
    const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), str_to.data(), size_needed, NULL, NULL);
    return str_to;
}
std::wstring StringToWString(const std::string_view str)
{
    if (str.empty()) {
        return L"";
    }
    const auto size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstrTo.data(), size_needed);
    return wstrTo;
}

std::string_view toString(Class c)
{
    switch (c) {
        case Class::Warrior:
            return "Warrior";
        case Class::Ranger:
            return "Ranger";
        case Class::Monk:
            return "Monk";
        case Class::Necro:
            return "Necro";
        case Class::Mesmer:
            return "Mesmer";
        case Class::Elementalist:
            return "Elementalist";
        case Class::Assassin:
            return "Assassin";
        case Class::Ritualist:
            return "Ritualist";
        case Class::Paragon:
            return "Paragon";
        case Class::Dervish:
            return "Dervish";
        case Class::Any:
            return "Any";
    }
    return "";
}
std::string_view toString(AgentType type) {
    switch (type) {
        case AgentType::Any:
            return "Any";
        case AgentType::Self:
            return "Self";
        case AgentType::PartyMember:
            return "Party member";
        case AgentType::Friendly:
            return "Friendly";
        case AgentType::Hostile:
            return "Hostile";
    }
    return "";
}
std::string_view toString(Sorting sorting)
{
    switch (sorting) {
        case Sorting::AgentId:
            return "Any";
        case Sorting::ClosestToPlayer:
            return "Closest to player";
        case Sorting::FurthestFromPlayer:
            return "Furthest from player";
        case Sorting::ClosestToTarget:
            return "Closest to target";
        case Sorting::FurthestFromTarget:
            return "Furthest from target";
        case Sorting::LowestHp:
            return "Lowest HP";
        case Sorting::HighestHp:
            return "Highest HP";
        case Sorting::ModelID:
            return "Model ID";
    }
    return "";
}
std::string_view toString(AnyNoYes anyNoYes) {
    switch (anyNoYes) {
        case AnyNoYes::Any:
            return "Any";
        case AnyNoYes::Yes:
            return "Yes";
        case AnyNoYes::No:
            return "No";
    }
    return "";
}
std::string_view toString(Channel channel)
{
    switch (channel) {
        case Channel::All:
            return "All";
        case Channel::Guild:
            return "Guild";
        case Channel::Team:
            return "Team";
        case Channel::Trade:
            return "Trade";
        case Channel::Alliance:
            return "Alliance";
        case Channel::Whisper:
            return "Whisper";
        case Channel::Emote:
            return "/ Commands";
        case Channel::Log:
            return "Log";
    }
    return "";
}
std::string_view toString(QuestStatus status)
{
    switch (status) {
        case QuestStatus::NotStarted:
            return "Not started";
        case QuestStatus::Running:
            return "Running";
        case QuestStatus::Completed:
            return "Completed";
        case QuestStatus::Failed:
            return "Failed";
    }
    return "";
}

std::string_view toString(GoToTargetFinishCondition cond)
{
    switch (cond) {
        case GoToTargetFinishCondition::None:
            return "Immediately";
        case GoToTargetFinishCondition::StoppedMovingNextToTarget:
            return "When next to target and not moving";
        case GoToTargetFinishCondition::DialogOpen:
            return "When dialog has opened";
    }
    return "";
}

std::string_view toString(HasSkillRequirement req)
{
    switch (req) {
        case HasSkillRequirement::OnBar:
            return "On the skillbar";
        case HasSkillRequirement::OffCooldown:
            return "Off cooldown";
        case HasSkillRequirement::ReadyToUse:
            return "Ready to use";
    }
    return "";
}

std::string_view toString(PlayerConnectednessRequirement req)
{
    switch (req) {
        case PlayerConnectednessRequirement::All:
            return "Everyone";
        case PlayerConnectednessRequirement::Individual:
            return "Single player";
    }
    return "";
}

std::string_view toString(Status status)
{
    switch (status) {
        case Status::Enchanted:
            return "Enchanted";
        case Status::WeaponSpelled:
            return "Weapon spelled";
        case Status::Alive:
            return "Alive";
        case Status::Bleeding:
            return "Bleeding";
        case Status::Crippled:
            return "Crippled";
        case Status::DeepWounded:
            return "Deep wounded";
        case Status::Poisoned:
            return "Poisoned";
        case Status::Hexed:
            return "Hexed";
        case Status::Idle:
            return "Idle";
        case Status::KnockedDown:
            return "Knocked down";
        case Status::Moving:
            return "Moving";
        case Status::Attacking:
            return "Attacking";
        case Status::Casting:
            return "Casting";
    }
    return "";
}

std::string_view toString(EquippedItemSlot slot)
{
    switch (slot) {
        case EquippedItemSlot::Mainhand:
            return "Main hand";
        case EquippedItemSlot::Offhand:
            return "Off hand";
        case EquippedItemSlot::Head:
            return "Head";
        case EquippedItemSlot::Chest:
            return "Chest";
        case EquippedItemSlot::Hands:
            return "Hands";
        case EquippedItemSlot::Legs:
            return "Legs";
        case EquippedItemSlot::Feet:
            return "Feet";
    }
    return "";
}

std::string_view toString(TrueFalse val)
{
    switch (val) {
        case TrueFalse::True:
            return "True";
        case TrueFalse::False:
            return "False";
    }
    return "";
}

std::string_view toString(MoveToBehaviour behaviour)
{
    switch (behaviour) {
        case MoveToBehaviour::SendOnce:
            return "Finish when at target or not moving";
        case MoveToBehaviour::RepeatIfIdle:
            return "Finish when at target, repeat move if not moving";
        case MoveToBehaviour::ImmediateFinish:
            return "Immediately finish";
    }
    return "";
}

std::string_view toString(ReferenceFrame refFrame)
{
    switch (refFrame) {
        case ReferenceFrame::Player:
            return "Player viewing direction";
        case ReferenceFrame::Camera:
            return "Camera viewing direction";
    }
    return "";
}

std::string_view toString(Trigger type)
{
    switch (type) {
        case Trigger::None:
            return "No packet trigger";
        case Trigger::InstanceLoad:
            return "Trigger on instance load";
        case Trigger::HardModePing:
            return "Trigger on hard mode ping";
        case Trigger::Hotkey:
            return "Trigger on keypress";
        case Trigger::ChatMessage:
            return "Trigger on chat message";
        case Trigger::BeginSkillCast:
            return "Trigger on begin skill cast";
        case Trigger::SkillCastInterrupt:
            return "Trigger on interrupt";
        case Trigger::BeginCooldown:
            return "Trigger on end skill cast";
        case Trigger::DungeonReward:
            return "Trigger on dungeon reward";
        case Trigger::DoaZoneComplete:
            return "Trigger on DoA zone complete";
        case Trigger::DisplayDialog:
            return "Trigger on displaying dialog";
    }
    return "";
}

std::string_view toString(GW::Constants::InstanceType type)
{
    switch (type) {
        case GW::Constants::InstanceType::Explorable:
            return "Explorable";
        case GW::Constants::InstanceType::Loading:
            return "Loading";
        case GW::Constants::InstanceType::Outpost:
            return "Outpost";
    }
    return "";
}
std::string_view toString(WeaponType type)
{
    switch (type) {
        case WeaponType::Any:
            return "Any";
        case WeaponType::None:
            return "None";
        case WeaponType::Bow:
            return "Bow";
        case WeaponType::Axe:
            return "Axe";
        case WeaponType::Hammer:
            return "Hammer";
        case WeaponType::Daggers:
            return "Daggers";
        case WeaponType::Scythe:
            return "Scythe";
        case WeaponType::Spear:
            return "Spear";
        case WeaponType::Sword:
            return "Sword";
        case WeaponType::Wand:
            return "Wand";
        case WeaponType::Staff:
            return "Staff";
    }
    return "";
}

std::string_view toString(ComparisonOperator comp)
{
    switch (comp)
    {
        case ComparisonOperator::Equals:
            return "==";
        case ComparisonOperator::Less:
            return "<";
        case ComparisonOperator::Greater:
            return ">";
        case ComparisonOperator::LessOrEqual:
            return "<=";
        case ComparisonOperator::GreaterOrEqual:
            return ">=";
        case ComparisonOperator::NotEquals:
            return "!=";
    }
    return "";
}

std::string_view toString(IsIsNot comp)
{
    switch (comp)
    {
        case IsIsNot::Is:
            return "is";
        case IsIsNot::IsNot:
            return "is not";
    }
    return "";
}

std::string_view toString(DoorStatus status)
{
    switch (status)
    {
        case DoorStatus::Closed:
            return "Closed";
        case DoorStatus::Open:
            return "Open";
    }
    return "";
}

std::string_view toString(DoaZone status)
{
    switch (status)
    {
        case DoaZone::Foundry:
            return "Foundry";
        case DoaZone::City:
            return "City";
        case DoaZone::Gloom:
            return "Gloom";
        case DoaZone::Veil:
            return "Veil";
    }
    return "";
}

std::string_view toString(MovementDirection direction)
{
    switch (direction) {
        case MovementDirection::Forwards:
            return "Forwards";
        case MovementDirection::Left:
            return "Sideways (Left)";
        case MovementDirection::Right:
            return "Sideways (Right)";
        case MovementDirection::Backwards:
            return "Backwards";
    }
    return "";
}

std::string_view toString(IdRestriction restriction)
{
    switch (restriction) {
        case IdRestriction::Any:
            return "Any ID";
        case IdRestriction::SpecificId:
            return "Specific ID";
    }
    return "";
}

std::string_view toString(ObjectiveType restriction)
{
    switch (restriction) {
        case ObjectiveType::Mission:
            return "Current Mission";
        case ObjectiveType::Quest:
            return "Quest";
    }
    return "";
}

std::string_view toString(SkillType type)
{
    switch (type) {
        case SkillType::Any:
            return "Anything";
        case SkillType::Spell:
            return "Spell";
        case SkillType::Signet:
            return "Signet";
        case SkillType::Well:
            return "Well";
        case SkillType::Skill:
            return "Skill";
        case SkillType::Ward:
            return "Ward";
        case SkillType::Glyph:
            return "Glpyh";
        case SkillType::Attack:
            return "Attack";
        case SkillType::Preparation:
            return "Preperation";
        case SkillType::Trap:
            return "Trap";
        case SkillType::Ritual:
            return "Ritual";
        case SkillType::WeaponSpell:
            return "WeaponSpell";
        case SkillType::Chant:
            return "Chant";
        case SkillType::Hex:
            return "Hex";
        case SkillType::Enchantment:
            return "Enchantment";
    }
    return "";
}

std::string_view toString(Bag bag)
{
    switch (bag)
    {
        case Bag::Backpack:
            return "Backpack";
        case Bag::BeltPouch:
            return "Belt Pouch";
        case Bag::Bag1:
            return "Bag 1";
        case Bag::Bag2:
            return "Bag 2";
        case Bag::EquipmentPack:
            return "Equipment pack";
    }
    return "";
}

std::string_view toString(DoorID id, Area area)
{
    switch (area)
    {
        case Area::Urgoz:
            switch (id)
            {
                case DoorID::Urgoz_zone_2:
                    return "Zone 2 entrace (Life Drain)";
                case DoorID::Urgoz_zone_3:
                    return "Zone 3 entrace (Levers)";
                case DoorID::Urgoz_zone_4:
                    return "Zone 4 entrace (Bridge Wolves)";
                case DoorID::Urgoz_zone_5:
                    return "Zone 5 entrace (More Wolves)";
                case DoorID::Urgoz_zone_6:
                    return "Zone 6 entrace (Energy Drain)";
                case DoorID::Urgoz_zone_7:
                    return "Zone 7 entrace (Exhaustion)";
                case DoorID::Urgoz_zone_8:
                    return "Zone 8 entrace (Pillars)";
                case DoorID::Urgoz_zone_9:
                    return "Zone 9 entrace (Blood Drinkers)";
                case DoorID::Urgoz_zone_10:
                    return "Zone 10 entrace (Bridge)";
                case DoorID::Urgoz_zone_11:
                    return "Zone 11 entrace (Urgoz)";
            }
            return "";
        case Area::Deep:
            switch (id) {
                case DoorID::Deep_room_1_first:
                    return "Room 1 (first)";
                case DoorID::Deep_room_1_second:
                    return "Room 1 (second)";
                case DoorID::Deep_room_2_first:
                    return "Room 2 (first)";
                case DoorID::Deep_room_2_second:
                    return "Room 2 (second)";
                case DoorID::Deep_room_3_first:
                    return "Room 3 (first)";
                case DoorID::Deep_room_3_second:
                    return "Room 3 (second)";
                case DoorID::Deep_room_4_first:
                    return "Room 4 (first)";
                case DoorID::Deep_room_4_second:
                    return "Room 4 (second)";
                case DoorID::Deep_room_5:
                    return "Room 5 entrance";
                case DoorID::Deep_room_6:
                    return "Room 6 entrance";
                case DoorID::Deep_room_7:
                    return "Room 7 entrance";
                case DoorID::Deep_room_9:
                    return "Room 9 entrance";
                case DoorID::Deep_room_11:
                    return "Room 11 entrance";
            }
            return "";
        case Area::Doa:
            switch (id) {
                case DoorID::DoA_foundry_entrance_r1:
                    return "Room 1 entrance";
                case DoorID::DoA_foundry_r1_r2:
                    return "Room 1 to room 2";
                case DoorID::DoA_foundry_r2_r3:
                    return "Room 2 to room 3";
                case DoorID::DoA_foundry_r3_r4:
                    return "Room 3 to room 4";
                case DoorID::DoA_foundry_r4_r5:
                    return "Room 4 to room 5";
                case DoorID::DoA_foundry_r5_bb:
                    return "Room 5 to black beast";
                case DoorID::DoA_foundry_behind_bb:
                    return "Black beast to city";
                case DoorID::DoA_city_entrance:
                    return "City entrance";
                case DoorID::DoA_city_wall:
                    return "Wall";
                case DoorID::DoA_city_jadoth:
                    return "Veil entrance (Jadoth)";
                case DoorID::DoA_veil_360_left:
                    return "360 left";
                case DoorID::DoA_veil_360_middle:
                    return "360 middle";
                case DoorID::DoA_veil_360_right:
                    return "360 right";
                case DoorID::DoA_veil_derv:
                    return "360 derv";
                case DoorID::DoA_veil_ranger:
                    return "360 ranger";
                case DoorID::DoA_veil_trench_necro:
                    return "Necro trench";
                case DoorID::DoA_veil_trench_mes:
                    return "Mesmer trench";
                case DoorID::DoA_veil_trench_ele:
                    return "Ele trench";
                case DoorID::DoA_veil_trench_monk:
                    return "Monk trench";
                case DoorID::DoA_veil_trench_gloom:
                    return "Gloom trench";
                case DoorID::DoA_veil_to_gloom:
                    return "Gloom entrance";
                case DoorID::DoA_gloom_to_foundry:
                    return "Necro trench";
                case DoorID::DoA_gloom_rift:
                    return "Rift closure";
            }
    }
    return "";
}

std::string_view toString(GW::Constants::HeroID hero)
{
    switch (hero)
    {
        using GW::Constants::HeroID;
        case HeroID::NoHero:
            return "Any";
        case HeroID::Norgu:
            return "Norgu";
        case HeroID::Goren:
            return "Goren";
        case HeroID::Tahlkora:
            return "Tahlkora";
        case HeroID::MasterOfWhispers:
            return "Master of Whispers";
        case HeroID::AcolyteJin:
            return "Acolyte Jin";
        case HeroID::Koss:
            return "Koss";
        case HeroID::Dunkoro:
            return "Dunkoro";
        case HeroID::AcolyteSousuke:
            return "Acolyte Sousuke";
        case HeroID::Melonni:
            return "Melonni";
        case HeroID::ZhedShadowhoof:
            return "Zhed Shadowhoof";
        case HeroID::GeneralMorgahn:
            return "General Morgahn";
        case HeroID::MargridTheSly:
            return "Margrid the Sly";
        case HeroID::Zenmai:
            return "Zenmai";
        case HeroID::Olias:
            return "Olias";
        case HeroID::Razah:
            return "Razah";
        case HeroID::MOX:
            return "M.O.X.";
        case HeroID::KeiranThackeray:
            return "Keiran Thackeray";
        case HeroID::Jora:
            return "Jora";
        case HeroID::PyreFierceshot:
            return "Pyre Fierceshot";
        case HeroID::Anton:
            return "Anton";
        case HeroID::Livia:
            return "Livia";
        case HeroID::Hayda:
            return "Hayda";
        case HeroID::Kahmu:
            return "Kahmu";
        case HeroID::Gwen:
            return "Gwen";
        case HeroID::Xandra:
            return "Xandra";
        case HeroID::Vekk:
            return "Vekk";
        case HeroID::Ogden:
            return "Ogden";
        case HeroID::Merc1:
            return "Mercenary 1";
        case HeroID::Merc2:
            return "Mercenary 2";
        case HeroID::Merc3:
            return "Mercenary 3";
        case HeroID::Merc4:
            return "Mercenary 4";
        case HeroID::Merc5:
            return "Mercenary 5";
        case HeroID::Merc6:
            return "Mercenary 6";
        case HeroID::Merc7:
            return "Mercenary 7";
        case HeroID::Merc8:
            return "Mercenary 8";
        case HeroID::Miku:
            return "Miku";
        case HeroID::ZeiRi:
            return "Zei Ri";
        case HeroID::Devona:
            return "Devona";
    }
    return "";
}

std::string_view toString(GW::UI::ControlAction action)
{
    constexpr auto names = std::array{
        "Action: Attack/Interact (Do It)",
        "Inventory: Activate Weapon Set 1",
        "Inventory: Activate Weapon Set 2",
        "Inventory: Activate Weapon Set 3",
        "Inventory: Activate Weapon Set 4",
        "Panel: Close All Panels",
        "Inventory: Cycle Equipment",
        "Miscellaneous: Log Out",
        "Chat: Open Chat",
        "Targeting: Show Others",
        "Panel: Open Hero",
        "Inventory: Open Inventory",
        "Panel: Open World Map",
        "Panel: Open Options",
        "Panel: Open Quest",
        "Panel: Open Skills and Attributes",
        "Camera: Reverse Camera",
        "Movement: Strafe Left",
        "Movement: Strafe Right",
        "Targeting: Foe - Nearest",
        "Targeting: Show Targets",
        "Targeting: Foe - Next",
        "Targeting: Party Member - 1",
        "Targeting: Party Member - 2",
        "Targeting: Party Member - 3",
        "Targeting: Party Member - 4",
        "Targeting: Party Member - 5",
        "Targeting: Party Member - 6",
        "Targeting: Party Member - 7",
        "Targeting: Party Member - 8",
        "Targeting: Foe - Previous",
        "Targeting: Priority Target",
        "Targeting: Self",
        "Chat: Toggle Chat",
        "Movement: Turn Left",
        "Movement: Turn Right",
        "Action: Use Skill 1",
        "Action: Use Skill 2",
        "Action: Use Skill 3",
        "Action: Use Skill 4",
        "Action: Use Skill 5",
        "Action: Use Skill 6",
        "Action: Use Skill 7",
        "Action: Use Skill 8",
        "Movement: Move Backward",
        "Movement: Move Forward",
        "Miscellaneous: Screenshot",
        "Action: Cancel Action",
        "Camera: Free Camera",
        "Movement: Reverse Direction",
        "Inventory: Open Backpack",
        "Inventory: Open Belt Pouch",
        "Inventory: Open Bag 1",
        "Inventory: Open Bag 2",
        "Panel: Open Mission Map",
        "Movement: Automatic Run",
        "Inventory: Toggle All Bags",
        "Panel: Open Friends",
        "Panel: Open Guild",
        "Miscellaneous: Language Quick Toggle",
        "Targeting: Ally - Nearest",
        "Panel: Open Score Chart",
        "Chat: Reply",
        "Panel: Open Party",
        "Chat: Start Chat Command",
        "Panel: Open Customize Layout",
        "Panel: Open Observe",
        "Targeting: Item - Nearest",
        "Targeting: Item - Next",
        "Targeting: Item - Previous",
        "Targeting: Party Member - 9",
        "Targeting: Party Member - 10",
        "Targeting: Party Member - 11",
        "Targeting: Party Member - 12",
        "Targeting: Party Member - Next",
        "Targeting: Party Member - Previous",
        "Action: Follow",
        "Action: Drop Item",
        "Camera: Zoom In",
        "Camera: Zoom Out",
        "Action: Suppress Action",
        "Panel: Open Load from Equipment Template",
        "Panel: Open Load from Skills Template",
        "Panel: Open Templates Manager",
        "Panel: Open Save to Equipment Template",
        "Panel: Open Save to Skills Template",
        "Action: Command Party",
        "Action: Command Hero 1",
        "Action: Command Hero 2",
        "Action: Command Hero 3",
        "Panel: Open PvP Equipment",
        "Action: Clear Party Commands",
        "Panel: Open Hero Commander 1",
        "Panel: Open Hero Commander 2",
        "Panel: Open Hero Commander 3",
        "Panel: Open Pet Commander",
        "Panel: Open Hero 1 Pet Commander",
        "Panel: Open Hero 2 Pet Commander",
        "Panel: Open Hero 3 Pet Commander",
        "Targeting: Clear Target",
        "Panel: Open Help",
        "Action: Order Hero 1 to use Skill 1",
        "Action: Order Hero 1 to use Skill 2",
        "Action: Order Hero 1 to use Skill 3",
        "Action: Order Hero 1 to use Skill 4",
        "Action: Order Hero 1 to use Skill 5",
        "Action: Order Hero 1 to use Skill 6",
        "Action: Order Hero 1 to use Skill 7",
        "Action: Order Hero 1 to use Skill 8",
        "Action: Order Hero 2 to use Skill 1",
        "Action: Order Hero 2 to use Skill 2",
        "Action: Order Hero 2 to use Skill 3",
        "Action: Order Hero 2 to use Skill 4",
        "Action: Order Hero 2 to use Skill 5",
        "Action: Order Hero 2 to use Skill 6",
        "Action: Order Hero 2 to use Skill 7",
        "Action: Order Hero 2 to use Skill 8",
        "Action: Order Hero 3 to use Skill 1",
        "Action: Order Hero 3 to use Skill 2",
        "Action: Order Hero 3 to use Skill 3",
        "Action: Order Hero 3 to use Skill 4",
        "Action: Order Hero 3 to use Skill 5",
        "Action: Order Hero 3 to use Skill 6",
        "Action: Order Hero 3 to use Skill 7",
        "Action: Order Hero 3 to use Skill 8",
        "Panel: Open Minion List",
        "Panel: Open Hero 4 Pet Commander",
        "Panel: Open Hero 5 Pet Commander",
        "Panel: Open Hero 6 Pet Commander",
        "Panel: Open Hero 7 Pet Commander",
        "Action: Command Hero 4",
        "Action: Command Hero 5",
        "Action: Command Hero 6",
        "Action: Command Hero 7",
        "Action: Order Hero 4 to use Skill 1",
        "Action: Order Hero 4 to use Skill 2",
        "Action: Order Hero 4 to use Skill 3",
        "Action: Order Hero 4 to use Skill 4",
        "Action: Order Hero 4 to use Skill 5",
        "Action: Order Hero 4 to use Skill 6",
        "Action: Order Hero 4 to use Skill 7",
        "Action: Order Hero 4 to use Skill 8",
        "Action: Order Hero 5 to use Skill 1",
        "Action: Order Hero 5 to use Skill 2",
        "Action: Order Hero 5 to use Skill 3",
        "Action: Order Hero 5 to use Skill 4",
        "Action: Order Hero 5 to use Skill 5",
        "Action: Order Hero 5 to use Skill 6",
        "Action: Order Hero 5 to use Skill 7",
        "Action: Order Hero 5 to use Skill 8",
        "Action: Order Hero 6 to use Skill 1",
        "Action: Order Hero 6 to use Skill 2",
        "Action: Order Hero 6 to use Skill 3",
        "Action: Order Hero 6 to use Skill 4",
        "Action: Order Hero 6 to use Skill 5",
        "Action: Order Hero 6 to use Skill 6",
        "Action: Order Hero 6 to use Skill 7",
        "Action: Order Hero 6 to use Skill 8",
        "Action: Order Hero 7 to use Skill 1",
        "Action: Order Hero 7 to use Skill 2",
        "Action: Order Hero 7 to use Skill 3",
        "Action: Order Hero 7 to use Skill 4",
        "Action: Order Hero 7 to use Skill 5",
        "Action: Order Hero 7 to use Skill 6",
        "Action: Order Hero 7 to use Skill 7",
        "Action: Order Hero 7 to use Skill 8",
        "Panel: Open Hero Commander 4",
        "Panel: Open Hero Commander 5",
        "Panel: Open Hero Commander 6",
        "Panel: Open Hero Commander 7"
    };
    return names[static_cast<int>(action - 0x80)];
}

std::string_view toString(GW::Constants::MapID map)
{
    // This array needs testing lol.
    constexpr std::array NAME_FROM_ID = {
        "",
        "Gladiators Arena",
        "DEV Test Arena (1v1)",
        "Test Map",
        "Warriors Isle",
        "Hunters Isle",
        "Wizards Isle",
        "Warriors Isle",
        "Hunters Isle",
        "Wizards Isle",
        "Bloodstone Fen",
        "The Wilds",
        "Aurora Glade",
        "Diessa Lowlands",
        "Gates of Kryta",
        "DAlessio Seaboard",
        "Divinity Coast",
        "Talmark Wilderness",
        "The Black Curtain",
        "Sanctum Cay",
        "Droknars Forge",
        "The Frost Gate",
        "Ice Caves of Sorrow",
        "Thunderhead Keep",
        "Iron Mines of Moladune",
        "Borlis Pass",
        "Talus Chute",
        "Griffons Mouth",
        "The Great Northern Wall",
        "Fort Ranik",
        "Ruins of Surmia",
        "Xaquang Skyway",
        "Nolani Academy",
        "Old Ascalon",
        "The Fissure of Woe",
        "Ember Light Camp",
        "Grendich Courthouse",
        "Glints Challenge",
        "Augury Rock",
        "Sardelac Sanitarium",
        "Piken Square",
        "Sage Lands",
        "Mamnoon Lagoon",
        "Silverwood",
        "Ettins Back",
        "Reed Bog",
        "The Falls",
        "Dry Top",
        "Tangle Root",
        "Henge of Denravi",
        "Test Map",
        "Senjis Corner",
        "Burning Isle",
        "Tears of the Fallen",
        "Scoundrels Rise",
        "Lions Arch",
        "Cursed Lands",
        "Bergen Hot Springs",
        "North Kryta Province",
        "Nebo Terrace",
        "Majestys Rest",
        "Twin Serpent Lakes",
        "Watchtower Coast",
        "Stingray Strand",
        "Kessex Peak",
        "DAlessio Arena",
        "All Call Click Point 1",
        "Burning Isle",
        "Frozen Isle",
        "Nomads Isle",
        "Druids Isle",
        "Isle of the Dead",
        "The Underworld",
        "Riverside Province",
        "Tournament 6",
        "The Hall of Heroes",
        "Broken Tower",
        "House zu Heltzer",
        "The Courtyard",
        "Unholy Temples",
        "Burial Mounds",
        "Ascalon City",
        "Tomb of the Primeval Kings",
        "The Vault",
        "The Underworld",
        "Ascalon Arena",
        "Sacred Temples",
        "Icedome",
        "Iron Horse Mine",
        "Anvil Rock",
        "Lornars Pass",
        "Snake Dance",
        "Tascas Demise",
        "Spearhead Peak",
        "Ice Floe",
        "Witmans Folly",
        "Mineral Springs",
        "Dreadnoughts Drift",
        "Frozen Forest",
        "Travelers Vale",
        "Deldrimor Bowl",
        "Regent Valley",
        "The Breach",
        "Ascalon Foothills",
        "Pockmark Flats",
        "Dragons Gullet",
        "Flame Temple Corridor",
        "Eastern Frontier",
        "The Scar",
        "The Amnoon Oasis",
        "Diviners Ascent",
        "Vulture Drifts",
        "The Arid Sea",
        "Prophets Path",
        "Salt Flats",
        "Skyward Reach",
        "Dunes of Despair",
        "Thirsty River",
        "Elona Reach",
        "Augury Rock",
        "The Dragons Lair",
        "Perdition Rock",
        "Ring of Fire",
        "Abaddons Mouth",
        "Hells Precipice",
        "Titans Tears",
        "Golden Gates",
        "Scarred Earth",
        "The Eternal Grove",
        "Lutgardis Conservatory",
        "Vasburg Armory",
        "Serenity Temple",
        "Ice Tooth Cave",
        "Beacons Perch",
        "Yaks Bend",
        "Frontier Gate",
        "Beetletun",
        "Fishermens Haven",
        "Temple of the Ages",
        "Ventaris Refuge",
        "Druids Overlook",
        "Maguuma Stade",
        "Quarrel Falls",
        "Ascalon Academy",
        "Gyala Hatchery",
        "The Catacombs",
        "Lakeside County",
        "The Northlands",
        "Ascalon City",
        "Ascalon Academy",
        "Ascalon Academy",
        "Ascalon Academy",
        "Heroes Audience",
        "Seekers Passage",
        "Destinys Gorge",
        "Camp Rankor",
        "The Granite Citadel",
        "Marhans Grotto",
        "Port Sledge",
        "Copperhammer Mines",
        "Green Hills County",
        "Wizards Folly",
        "Regent Valley",
        "The Barradin Estate",
        "Ashford Abbey",
        "Foibles Fair",
        "Fort Ranik",
        "Burning Isle",
        "Druids Isle",
        "[null]",
        "Frozen Isle",
        "Warriors Isle",
        "Hunters Isle",
        "Wizards Isle",
        "Nomads Isle",
        "Isle of the Dead",
        "Frozen Isle",
        "Nomads Isle",
        "Druids Isle",
        "Isle of the Dead",
        "Fort Koga",
        "Shiverpeak Arena",
        "Amnoon Arena",
        "Deldrimor Arena",
        "The Crag",
        "The Underworld",
        "The Underworld",
        "The Underworld",
        "Random Arenas",
        "Team Arenas",
        "Sorrows Furnace",
        "Grenths Footprint",
        "All Call Click Point 2",
        "Cavalon",
        "Kaineng Center",
        "Drazach Thicket",
        "Jaya Bluffs",
        "Shenzun Tunnels",
        "Archipelagos",
        "Maishang Hills",
        "Mount Qinkai",
        "Melandrus Hope",
        "Rheas Crater",
        "Silent Surf",
        "Unwaking Waters",
        "Morostav Trail",
        "Deldrimor War Camp",
        "Dragons Thieves",
        "Heroes Crypt",
        "Mourning Veil Falls",
        "Ferndale",
        "Pongmei Valley",
        "Monastery Overlook",
        "Zen Daijun",
        "Minister Chos Estate",
        "Vizunah Square",
        "Nahpui Quarter",
        "Tahnnakai Temple",
        "Arborstone",
        "Boreas Seabed",
        "Sunjiang District",
        "Fort Aspenwood",
        "The Eternal Grove",
        "The Jade Quarry",
        "Gyala Hatchery",
        "Raisu Palace",
        "Imperial Sanctum",
        "Unwaking Waters",
        "Grenz Frontier",
        "The Ancestral Lands",
        "Amatz Basin",
        "Kaanai Canyon",
        "Shadows Passage",
        "Raisu Palace",
        "The Aurios Mines",
        "Panjiang Peninsula",
        "Kinya Province",
        "Haiju Lagoon",
        "Sunqua Vale",
        "Wajjun Bazaar",
        "Bukdek Byway",
        "The Undercity",
        "Shing Jea Monastery",
        "Shing Jea Arena",
        "Arborstone",
        "Minister Chos Estate",
        "Zen Daijun",
        "Boreas Seabed",
        "Great Temple of Balthazar",
        "Tsumei Village",
        "Seitung Harbor",
        "Ran Musu Gardens",
        "Linnok Courtyard",
        "Dwayna Vs Grenth",
        "Dwaynas Camp",
        "Grenths Camp",
        "Sunjiang District",
        "Minister Chos Estate",
        "Zen Daijun",
        "The Jade Quarry (Kurzick)",
        "Nahpui Quarter",
        "Tahnnakai Temple",
        "Arborstone",
        "Boreas Seabed",
        "Sunjiang District",
        "Nahpui Quarter",
        "Urgozs Warren",
        "The Eternal Grove",
        "Gyala Hatchery",
        "Tahnnakai Temple",
        "Raisu Palace",
        "Imperial Sanctum",
        "Altrumm Ruins",
        "Zos Shivros Channel",
        "Dragons Throat",
        "Isle of Weeping Stone",
        "Isle of Jade",
        "Harvest Temple",
        "Breaker Hollow",
        "Leviathan Pits",
        "Isle of the Nameless",
        "Zaishen Challenge",
        "Zaishen Elite",
        "Maatu Keep",
        "Zin Ku Corridor",
        "Monastery Overlook",
        "Brauer Academy",
        "Durheim Archives",
        "Bai Paasu Reach",
        "Seafarers Rest",
        "Bejunkan Pier",
        "Vizunah Square (Local Quarter)",
        "Vizunah Square (Foreign Quarter)",
        "Fort Aspenwood (Luxon)",
        "Fort Aspenwood (Kurzick)",
        "The Jade Quarry (Luxon)",
        "The Jade Quarry (Kurzick)",
        "Unwaking Waters (Luxon)",
        "Unwaking Waters (Kurzick)",
        "Saltspray Beach",
        "Etnaran Keys",
        "Raisu Pavilion",
        "Kaineng Docks",
        "The Marketplace",
        "Vizunah Square (Local Quarter)",
        "Vizunah Square (Foreign Quarter)",
        "The Jade Quarry (Luxon)",
        "The Deep",
        "Ascalon Arena",
        "Annihilation Training",
        "Kill Count Training",
        "Priest Annihilation Training",
        "Obelisk Annihilation Training",
        "Saoshang Trail",
        "Shiverpeak Arena",
        "Travel",
        "Travel",
        "Travel",
        "DAlessio Arena",
        "Amnoon Arena",
        "Fort Koga",
        "Heroes Crypt",
        "Shiverpeak Arena",
        "Fort Aspenwood (Kurzick)",
        "Fort Aspenwood (Luxon)",
        "The Harvest Ceremony",
        "The Harvest Ceremony",
        "Imperial Sanctum",
        "Saltspray Beach (Luxon)",
        "Saltspray Beach (Kurzick)",
        "Heroes Ascent",
        "Grenz Frontier (Luxon)",
        "Grenz Frontier (Kurzick)",
        "The Ancestral Lands (Luxon)",
        "The Ancestral Lands (Kurzick)",
        "Etnaran Keys (Luxon)",
        "Etnaran Keys (Kurzick)",
        "Kaanai Canyon (Luxon)",
        "Kaanai Canyon (Kurzick)",
        "DAlessio Arena",
        "Amnoon Arena",
        "Fort Koga",
        "Heroes Crypt",
        "Shiverpeak Arena",
        "The Hall of Heroes",
        "The Courtyard",
        "Scarred Earth",
        "The Underworld",
        "Tanglewood Copse",
        "Saint Anjekas Shrine",
        "Eredon Terrace",
        "Divine Path",
        "Brawlers Pit",
        "Petrified Arena",
        "Seabed Arena",
        "Isle of Weeping Stone",
        "Isle of Jade",
        "Imperial Isle",
        "Isle of Meditation",
        "Imperial Isle",
        "Isle of Meditation",
        "Isle of Weeping Stone",
        "Isle of Jade",
        "Imperial Isle",
        "Isle of Meditation",
        "Random Arenas Test",
        "Shing Jea Arena",
        "All Skills",
        "Dragon Arena",
        "Jahai Bluffs",
        "Kamadan",
        "Marga Coast",
        "Fahranur",
        "Sunward Marches",
        "oleplaying Character //Buuug? ",
        "Vortex",
        "Camp Hojanu",
        "Bahdok Caverns",
        "Wehhan Terraces",
        "Dejarin Estate",
        "Arkjok Ward",
        "Yohlon Haven",
        "Gandara",
        "Vortex",
        "The Floodplain of Mahnkelon",
        "Lions Arch",
        "Turais Procession",
        "Sunspear Sanctuary",
        "Aspenwood Gate (Kurzick)",
        "Aspenwood Gate (Luxon)",
        "Jade Flats (Kurzick)",
        "Jade Flats (Luxon)",
        "Yatendi Canyons",
        "Chantry of Secrets",
        "Garden of Seborhin",
        "Holdings of Chokhin",
        "Mihanu Township",
        "Vehjin Mines",
        "Basalt Grotto",
        "Forum Highlands",
        "Kaineng Center",
        "Sebelkeh Basilica",
        "Resplendent Makuun",
        "Honur Hill",
        "Wilderness of Bahdza",
        "Sun Docks",
        "Vehtendi Valley",
        "Yahnur Market",
        "Here Be Dragons",
        "Here Be Dragons",
        "Here Be Dragons",
        "Here Be Dragons",
        "Here Be Dragons",
        "The Hidden City of Ahdashim",
        "The Kodash Bazaar",
        "Lions Gate",
        "Monastery Overlook",
        "Bejunkan Pier",
        "Lions Gate",
        "The Mirror of Lyss",
        "Secure the Refuge",
        "Venta Cemetery",
        "Kamadan",
        "The Tribunal",
        "Kodonur Crossroads",
        "Rilohn Refuge",
        "Pogahn Passage",
        "Moddok Crevice",
        "Tihark Orchard",
        "Consulate",
        "Plains of Jarin",
        "Sunspear Great Hall",
        "Cliffs of Dohjok",
        "Dzagonur Bastion",
        "Dasha Vestibule",
        "Grand Court of Sebelkeh",
        "Command Post",
        "Jokos Domain",
        "Bone Palace",
        "The Ruptured Heart",
        "The Mouth of Torment",
        "The Shattered Ravines",
        "Lair of the Forgotten",
        "Poisoned Outcrops",
        "The Sulfurous Wastes",
        "The Ebony Citadel of Mallyx",
        "The Alkali Pan",
        "Cliffs of Dohjok",
        "Crystal Overlook",
        "Kamadan",
        "Gate of Torment",
        "Gate of Anguish",
        "Secure the Refuge",
        "Evacuation",
        "Test Map",
        "Nightfallen Garden",
        "Churrhir Fields",
        "Beknur Harbor",
        "Kodonur Crossroads",
        "Rilohn Refuge",
        "Pogahn Passage",
        "The Underworld",
        "Heart of Abaddon",
        "The Underworld",
        "Nightfallen Coast",
        "Nightfallen Jahai",
        "Depths of Madness",
        "Rollerbeetle Racing",
        "Domain of Fear",
        "Gate of Fear",
        "Domain of Pain",
        "Bloodstone Fen",
        "Domain of Secrets",
        "Gate of Secrets",
        "Gate of Anguish",
        "Jelly Den",
        "Jennurs Horde",
        "Nundu Bay",
        "Gate of Desolation",
        "Champions Dawn",
        "Ruins of Morah",
        "Fahranur",
        "Bjora Marches",
        "Zehlon Reach",
        "Lahtenda Bog",
        "Arbor Bay",
        "Issnur Isles",
        "Beknur Harbor",
        "Mehtani Keys",
        "Kodlonu Hamlet",
        "Island of Shehkah",
        "Jokanur Diggings",
        "Blacktide Den",
        "Consulate Docks",
        "Gate of Pain",
        "Gate of Madness",
        "Abaddons Gate",
        "Sunspear Arena",
        "Travel",
        "Ice Cliff Chasms",
        "Bokka Amphitheatre",
        "Riven Earth",
        "The Astralarium",
        "Throne Of Secrets",
        "Churranu Island Arena",
        "Shing Jea Monastery",
        "Haiju Lagoon",
        "Jaya Bluffs",
        "Seitung Harbor",
        "Tsumei Village",
        "Seitung Harbor",
        "Tsumei Village",
        "Minister Chos Estate",
        "Drakkar Lake",
        "Island of Shehkah",
        "Jokanur Diggings",
        "Blacktide Den",
        "Consulate Docks",
        "Tihark Orchard",
        "Dzagonur Bastion",
        "Hidden City of Ahdashim",
        "Grand Court of Sebelkeh",
        "Jennurs Horde",
        "Nundu Bay",
        "Gates of Desolation",
        "Ruins of Morah",
        "Domain of Pain",
        "Gate of Madness",
        "Abaddons Gate",
        "Uncharted Isle",
        "Isle of Wurms",
        "Uncharted Isle",
        "Isle of Wurms",
        "Uncharted Isle",
        "Isle of Wurms",
        "Ahmtur Arena",
        "Sunspear Arena",
        "Corrupted Isle",
        "Isle of Solitude",
        "Corrupted Isle",
        "Isle of Solitude",
        "Corrupted Isle",
        "Isle of Solitude",
        "Sun Docks",
        "Chahbek Village",
        "Remains of Sahlahja",
        "Jaga Moraine",
        "Bombardment",
        "Norrhart Domains",
        "Hero Battles",
        "Hero Battles",
        "The Crossing",
        "Desert Sands",
        "Varajar Fells",
        "Dajkah Inlet",
        "The Shadow Nexus",
        "Chahbek Village",
        "Throne Of Secrets",
        "Sparkfly Swamp",
        "Gate of the Nightfallen Lands",
        "Cathedral of Flames",
        "Gate of Torment",
        "Fortress of Jahai",
        "Halls of Chokhin",
        "Citadel of Dzagon",
        "Dynastic Tombs",
        "Verdant Cascades",
        "Cathedral of Flames: Level 2",
        "Cathedral of Flames: Level 3",
        "Magus Stones",
        "Catacombs of Kathandrax",
        "Catacombs of Kathandrax: Level 2",
        "Alcazia Tangle",
        "Rragars Menagerie",
        "Rragars Menagerie: Level 2",
        "Rragars Menagerie: Level 3",
        "Jelly Den: Level 2",
        "Slavers Exile",
        "Oolas Lab",
        "Oolas Lab: Level 2",
        "Oolas Lab: Level 3",
        "Shards of Orr",
        "Shards of Orr: Level 2",
        "Shards of Orr: Level 3",
        "Arachnis Haunt",
        "Arachnis Haunt: Level 2",
        "Burning Embers",
        "Burning Furnace",
        "Burning Temple",
        "Catacombs Lush",
        "Catacombs Stone",
        "Catacombs Tomb",
        "5 team test",
        "Fetid River",
        "Overlook",
        "Cemetery",
        "Forgotten Shrines",
        "Track",
        "The Antechamber",
        "Collision",
        "The Hall of Heroes",
        "Frozen Crevasse",
        "Frozen Depleted",
        "Frozen Fort",
        "Vloxen Excavations",
        "Vloxen Excavations: Level 2",
        "Vloxen Excavations: Level 3",
        "Heart of the Shiverpeaks",
        "Heart of the Shiverpeaks: Level 2",
        "Heart of the Shiverpeaks: Level 3",
        "The Journey to Nornheim",
        "The Journey to Nornheim",
        "Bloodstone Caves",
        "Bloodstone Caves: Level 2",
        "Bloodstone Caves: Level 3",
        "Bogroot Growths",
        "Bogroot Growths: Level 2",
        "Ravens Point",
        "Ravens Point: Level 2",
        "Ravens Point: Level 3",
        "Slavers Exile",
        "Slavers Exile",
        "Slavers Exile",
        "Slavers Exile",
        "Vloxs Falls",
        "Battledepths",
        "Battledepths: Level 2",
        "Battledepths: Level 3",
        "Sepulchre of Dragrimmar",
        "Sepulchre of Dragrimmar: Level 2",
        "Frostmaws Burrows",
        "Frostmaws Burrows: Level 2",
        "Frostmaws Burrows: Level 3",
        "Frostmaws Burrows: Level 4",
        "Frostmaws Burrows: Level 5",
        "Darkrime Delves",
        "Darkrime Delves: Level 2",
        "Darkrime Delves: Level 3",
        "Gadds Encampment",
        "Umbral Grotto",
        "Rata Sum",
        "Tarnished Haven",
        "Eye of the North",
        "Sifhalla",
        "Gunnars Hold",
        "Olafstead",
        "Hall of Monuments",
        "Dalada Uplands",
        "Doomlore Shrine",
        "Grothmar Wardowns",
        "Longeyes Ledge",
        "Sacnoth Valley",
        "Central Transfer Chamber",
        "Revealing the Curse",
        "My Side of the Mountain",
        "Mountain Owl",
        "Mountain Owl",
        "Mountain Owl",
        "Oolas Laboratory",
        "Oolas Laboratory",
        "Oolas Laboratory",
        "Completing Gadds Research",
        "Completing Gadds Research",
        "Completing Gadds Research",
        "Genius Operated Living Enchanted Manifestation",
        "Assaulting the Charr Camp",
        "Freeing Pyres Warband",
        "Freeing Pyres Warband",
        "Freeing Pyres Warband",
        "Freeing the Vanguard",
        "Source of Destruction",
        "Source of Destruction",
        "Source of Destruction",
        "A Time for Heroes",
        "Steppe Practice",
        "Boreal Station",
        "Catacombs of Kathandrax: Level 3",
        "Hall of Primordus",
        "Mountain Holdout",
        "Cinematic Cave Norn Cursed",
        "Cinematic Steppe Interrogation",
        "Cinematic Interior Research",
        "Cinematic Eye Vision A",
        "Cinematic Eye Vision B",
        "Cinematic Eye Vision C",
        "Cinematic Eye Vision D",
        "Polymock Coliseum",
        "Polymock Glacier",
        "Polymock Crossing",
        "Cinematic Mountain Resolution",
        "<Mountain Polar OP>",
        "Beneath Lions Arch",
        "Tunnels Below Cantha",
        "Caverns Below Kamadan",
        "Cinematic Mountain Dwarfs",
        "The Eye of the North",
        "The Eye of the North",
        "The Eye of the North",
        "Hero Tutorial",
        "Prototype Map",
        "The Norn Fighting Tournament",
        "Hundars Resplendent Treasure Vault",
        "Norn Brawling Championship",
        "Kilroys Punchout Training",
        "Fronis Irontoe's Lair",
        "INTERIOR_WATCH_OP1_FINAL_BATTLE",
        "Designer Test Map",
        "The Great Norn Alemoot",
        "(Mountain Traverse)",
        "Battle for Nornheim",
        "Destroyer Ending",
        "Alcazia Tangle",
        "Mission Pack Test",
        "North Kryta Province",
        "Bukdek Byway",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "Plains of Jarin",
        "(crash)"
    };

    if ((uint32_t)map >= NAME_FROM_ID.size()) return "";
    return NAME_FROM_ID[(uint32_t)map];
}

bool checkWeaponType(WeaponType targetType, uint16_t gameType)
{
    switch (targetType) {
        case WeaponType::Any:
            return true;
        case WeaponType::None:
            return gameType == 0;
        case WeaponType::Bow:
            return gameType == 1;
        case WeaponType::Axe:
            return gameType == 2;
        case WeaponType::Hammer:
            return gameType == 3;
        case WeaponType::Daggers:
            return gameType == 4;
        case WeaponType::Scythe:
            return gameType == 5;
        case WeaponType::Spear:
            return gameType == 6;
        case WeaponType::Sword:
            return gameType == 7;
        case WeaponType::Wand:
            return gameType == 10;
        case WeaponType::Staff:
            return gameType == 8 || gameType == 9 || gameType == 12 || gameType == 14;
    }
    return false;
}

std::string makeHotkeyDescription(Hotkey hotkey)
{
    char newDescription[256];
    ModKeyName(newDescription, _countof(newDescription), hotkey.modifier, hotkey.keyData);
    return std::string{newDescription};
}

bool drawHotkeySelector(Hotkey& hotkey, std::string& description, float selectorWidth)
{
    bool keyChanged = false;

    ImGui::PushItemWidth(selectorWidth);
    if (ImGui::Button(description.c_str())) {
        ImGui::OpenPopup("Select Hotkey");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change hotkey");
    if (ImGui::BeginPopup("Select Hotkey")) {
        static long newkey = 0;
        char newkey_buf[256]{};
        long newmod = 0;

        ImGui::Text("Press key");
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) newmod |= ModKey_Control;
        if (ImGui::IsKeyDown(ImGuiMod_Shift)) newmod |= ModKey_Shift;
        if (ImGui::IsKeyDown(ImGuiMod_Alt)) newmod |= ModKey_Alt;

        if (newkey == 0) { // we are looking for the key
            for (WORD i = 0; i < 512; i++) {
                switch (i) {
                    case VK_CONTROL:
                    case VK_LCONTROL:
                    case VK_RCONTROL:
                    case VK_SHIFT:
                    case VK_LSHIFT:
                    case VK_RSHIFT:
                    case VK_MENU:
                    case VK_LMENU:
                    case VK_RMENU:
                    case VK_LBUTTON:
                    case VK_RBUTTON:
                        continue;
                    default: {
                        if (::GetKeyState(i) & 0x8000) newkey = i;
                    }
                }
            }
        }
        else if (!(::GetKeyState(newkey) & 0x8000)) { // We have a key, check if it was released
            keyChanged = true;
            hotkey.keyData = newkey;
            hotkey.modifier = newmod;
            description = makeHotkeyDescription(hotkey);
            newkey = 0;
            ImGui::CloseCurrentPopup();
        }

        ModKeyName(newkey_buf, _countof(newkey_buf), newmod, newkey);
        ImGui::Text("%s", newkey_buf);

        ImGui::EndPopup();
    }
    ImGui::PopItemWidth();

    return keyChanged;
}

void drawTriggerSelector(Trigger& trigger, TriggerData& triggerData, [[maybe_unused]] float width)
{
    if (trigger == Trigger::None)
    {
        #ifdef LiveSplitMode
        drawEnumButton(trigger, {.first = Trigger::DisplayDialog, .last = Trigger::DoaZoneComplete, .buttonTextOverWrite = "Add trigger"});
        #else
        drawEnumButton(trigger, {.first = Trigger::InstanceLoad, .last = Trigger::DoaZoneComplete, .buttonTextOverWrite = "Add trigger"});
        #endif
    }
    else if (trigger == Trigger::Hotkey)
    {
        auto description = triggerData.hotkey.keyData ? makeHotkeyDescription(triggerData.hotkey) : "Click to change key";
        drawHotkeySelector(triggerData.hotkey, description, width - 20.f);
        ImGui::SameLine();
        ImGui::Text("Hotkey");
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            triggerData.hotkey.keyData = 0;
            triggerData.hotkey.modifier = 0;
        }
    }
    else if (trigger == Trigger::ChatMessage)
    {
        ImGui::PushItemWidth(200);
        ImGui::InputText("Trigger chat message", &triggerData.message);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            triggerData.message = "";
        }
    }
    else if (trigger == Trigger::BeginSkillCast)
    {
        ImGui::Text("Trigger on begin skill");
        ImGui::SameLine();
        drawSkillIDSelector(triggerData.skillId, true);
        ImGui::SameLine();
        drawEnumButton(triggerData.hsr, {.last = AnyNoYes::Yes, .width = 50.f});
        ImGui::SameLine();
        ImGui::Text("HSR");
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            triggerData.skillId = GW::Constants::SkillID::No_Skill;
            triggerData.hsr = AnyNoYes::Any;
        }
    }
    else if (trigger == Trigger::SkillCastInterrupt)
    {
        ImGui::Text("Trigger on interrupt of");
        ImGui::SameLine();
        drawSkillIDSelector(triggerData.skillId, true);
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            triggerData.skillId = GW::Constants::SkillID::No_Skill;
        }
    }
    else if (trigger == Trigger::BeginCooldown)
    {
        ImGui::Text("Trigger on end skill");
        ImGui::SameLine();
        drawSkillIDSelector(triggerData.skillId, true);
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            triggerData.skillId = GW::Constants::SkillID::No_Skill;
        }
    }
    else if (trigger == Trigger::DisplayDialog)
    {
        ImGui::PushItemWidth(200);
        ImGui::InputText("Trigger display dialog", &triggerData.message);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            triggerData.message = "";
        }
    }
    else if (trigger == Trigger::DoaZoneComplete)
    {
        ImGui::Text("Trigger DoA zone complete");
        ImGui::SameLine();
        drawEnumButton(triggerData.doaZone, {.first = DoaZone::Foundry, .last = DoaZone::City});
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            triggerData.doaZone = DoaZone::Foundry;
        }
    }
    else {
        ImGui::Text(toString(trigger).data());
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
        }
    }
}

void drawPolygonSelector(std::vector<GW::Vec2f>& polygon)
{
    ImGui::PushItemWidth(200);
    if (ImGui::Button("Add Polygon Point")) {
        if (const auto player = GW::Agents::GetControlledCharacter()) {
            polygon.emplace_back(player->pos.x, player->pos.y);
        }
    }

    ImGui::Indent();

    std::optional<int> remove_point;
    for (auto j = 0u; j < polygon.size(); j++) {
        ImGui::PushID(j);
        ImGui::Bullet();
        ImGui::InputFloat2("", reinterpret_cast<float*>(&polygon.at(j)));
        ImGui::SameLine();
        if (ImGui::Button("x")) remove_point = j;
        ImGui::PopID();
    }
    if (remove_point) {
        polygon.erase(polygon.begin() + remove_point.value());
    }

    ImGui::Unindent();
    ImGui::PopItemWidth();
}
bool pointIsInsidePolygon(const GW::GamePos pos, const std::vector<GW::Vec2f>& points)
{
    bool b = false;
    for (auto i = 0u, j = points.size() - 1; i < points.size(); j = i++) {
        if (points[i].y >= pos.y != points[j].y >= pos.y && pos.x <= (points[j].x - points[i].x) * (pos.y - points[i].y) / (points[j].y - points[i].y) + points[i].x) {
            b = !b;
        }
    }
    return b;
}

void drawSkillIDSelector(GW::Constants::SkillID& id, bool zeroIsAny)
{
    ImGui::PushItemWidth(50.f);
    ImGui::Text("%s", getSkillName(id, zeroIsAny).c_str());
    ImGui::SameLine();
    ImGui::SameLine();
    ImGui::InputInt("Skill ID", reinterpret_cast<int*>(&id), 0);
    ImGui::PopItemWidth();
}

void drawMapIDSelector(GW::Constants::MapID& id)
{
    ImGui::PushItemWidth(50.f);
    auto map_name = toString(id);
    if (map_name != "") {
        ImGui::Text("%s", map_name);
        ImGui::SameLine();
    }

    ImGui::InputInt("Map ID", reinterpret_cast<int*>(&id), 0);
    ImGui::PopItemWidth();
}
void drawModelIDSelector(uint16_t& id, std::optional<std::string_view> label)
{
    ImGui::PushItemWidth(50.f);
    const auto& modelNames = getModelNames();
    const auto& modelNameIt = modelNames.find(id);
    if (modelNameIt != modelNames.end()) {
        ImGui::Text("%s", modelNameIt->second.data());
        ImGui::SameLine();
    }
    int editValue = id;

    if (ImGui::InputInt(label ? label->data() : "Model ID", &editValue, 0)) {
        if (editValue >= 0 && editValue <= 0xFFFF)
            id = uint16_t(editValue);
        else
            id = 0;
    }
    ImGui::PopItemWidth();
}

void drawDoorSelector(DoorID& id, Area& area)
{
    const auto drawSubMenu = [&](std::string_view title, const auto& candidates, Area candidateArea)
    {
        if (ImGui::BeginMenu(title.data())) {
            for (const auto& candidate : candidates) {
                if (ImGui::MenuItem(toString(candidate, candidateArea).data())) {
                    id = candidate;
                    area = candidateArea;
                }
            }
            ImGui::EndMenu();
        }
    };

    constexpr auto foundry = std::array{DoorID::DoA_foundry_entrance_r1, DoorID::DoA_foundry_r1_r2, DoorID::DoA_foundry_r2_r3, DoorID::DoA_foundry_r3_r4, DoorID::DoA_foundry_r4_r5, DoorID::DoA_foundry_r5_bb, DoorID::DoA_foundry_behind_bb};
    constexpr auto city = std::array{DoorID::DoA_city_entrance, DoorID::DoA_city_wall};
    constexpr auto veil = std::array{DoorID::DoA_city_jadoth,       DoorID::DoA_veil_360_left,   DoorID::DoA_veil_360_middle, DoorID::DoA_veil_360_right,   DoorID::DoA_veil_derv, DoorID::DoA_veil_ranger,
                                     DoorID::DoA_veil_trench_necro, DoorID::DoA_veil_trench_mes, DoorID::DoA_veil_trench_ele, DoorID::DoA_veil_trench_monk, DoorID::DoA_veil_trench_gloom};
    constexpr auto gloom = std::array{DoorID::DoA_veil_to_gloom, DoorID::DoA_gloom_to_foundry};
    constexpr auto urgoz = std::array{DoorID::Urgoz_zone_2, DoorID::Urgoz_zone_3, DoorID::Urgoz_zone_4, DoorID::Urgoz_zone_5, DoorID::Urgoz_zone_6, DoorID::Urgoz_zone_7, DoorID::Urgoz_zone_8, DoorID::Urgoz_zone_9, DoorID::Urgoz_zone_10, DoorID::Urgoz_zone_11};
    constexpr auto deep = std::array{DoorID::Deep_room_1_first, DoorID::Deep_room_1_second, DoorID::Deep_room_2_first, DoorID::Deep_room_2_second, DoorID::Deep_room_3_first, DoorID::Deep_room_3_second,
                                     DoorID::Deep_room_4_first, DoorID::Deep_room_4_second, DoorID::Deep_room_5,       DoorID::Deep_room_6,        DoorID::Deep_room_7,       DoorID::Deep_room_11};

    if (ImGui::Button(toString(id, area).data()))
        ImGui::OpenPopup("DoorSelector");

    if (ImGui::BeginPopup("DoorSelector")) {
        drawSubMenu("Foundry", foundry, Area::Doa);
        drawSubMenu("City", city, Area::Doa);
        drawSubMenu("Veil", veil, Area::Doa);
        drawSubMenu("Gloom", gloom, Area::Doa);
        drawSubMenu("Urgoz", urgoz, Area::Urgoz);
        drawSubMenu("Deep", deep, Area::Deep);

        ImGui::EndPopup();
    }
}
