#include <ActionIO.h>
#include <ActionImpls.h>

#include <imgui.h>

namespace {
    std::shared_ptr<Action> makeAction(ActionType type)
    {
        static_assert((int)ActionType::Count == 28);
        switch (type) {
            case ActionType::MoveTo:
                return std::make_shared<MoveToAction>();
            case ActionType::MoveToTargetPosition:
                return std::make_shared<MoveToTargetPositionAction>();
            case ActionType::Cast:
                return std::make_shared<CastAction>();
            case ActionType::CastBySlot:
                return std::make_shared<CastBySlotAction>();
            case ActionType::ChangeTarget:
                return std::make_shared<ChangeTargetAction>();
            case ActionType::UseItem:
                return std::make_shared<UseItemAction>();
            case ActionType::SendDialog:
                return std::make_shared<SendDialogAction>();
            case ActionType::GoToTarget:
                return std::make_shared<GoToTargetAction>();
            case ActionType::Wait:
                return std::make_shared<WaitAction>();
            case ActionType::SendChat:
                return std::make_shared<SendChatAction>();
            case ActionType::Cancel:
                return std::make_shared<CancelAction>();
            case ActionType::DropBuff:
                return std::make_shared<DropBuffAction>();
            case ActionType::EquipItem:
                return std::make_shared<EquipItemAction>();
            case ActionType::Conditioned:
                return std::make_shared<ConditionedAction>();
            case ActionType::RepopMinipet:
                return std::make_shared<RepopMinipetAction>();
            case ActionType::PingHardMode:
                return std::make_shared<PingHardModeAction>();
            case ActionType::PingTarget:
                return std::make_shared<PingTargetAction>();
            case ActionType::AutoAttackTarget:
                return std::make_shared<AutoAttackTargetAction>();
            case ActionType::ChangeWeaponSet:
                return std::make_shared<ChangeWeaponSetAction>();
            case ActionType::StoreTarget:
                return std::make_shared<StoreTargetAction>();
            case ActionType::RestoreTarget:
                return std::make_shared<RestoreTargetAction>();
            case ActionType::StopScript:
                return std::make_shared<StopScriptAction>();
            case ActionType::UseHeroSkill:
                return std::make_shared<UseHeroSkillAction>();
            case ActionType::LogOut:
                return std::make_shared<LogOutAction>();
            case ActionType::UnequipItem:
                return std::make_shared<UnequipItemAction>();
            case ActionType::ClearTarget:
                return std::make_shared<ClearTargetAction>();
            case ActionType::WaitUntil:
                return std::make_shared<WaitUntilAction>();
            default:
                return nullptr;
        }
    }
} // namespace

std::string_view toString(ActionType type)
{
    static_assert((int)ActionType::Count == 28);
    switch (type) {
        case ActionType::MoveTo:
            return "Position";
        case ActionType::MoveToTargetPosition:
            return "Target position";
        case ActionType::Cast:
            return "Use skill by id";
        case ActionType::CastBySlot:
            return "Use skill by slot";
        case ActionType::ChangeTarget:
            return "Change target";
        case ActionType::UseItem:
            return "Use item";
        case ActionType::SendDialog:
            return "Send dialog";
        case ActionType::GoToTarget:
            return "Speak with NPC";
        case ActionType::Wait:
            return "Wait";
        case ActionType::SendChat:
            return "Send chat";
        case ActionType::Cancel:
            return "Cancel current action";
        case ActionType::DropBuff:
            return "Drop buff";
        case ActionType::EquipItem:
            return "Equip Item";
        case ActionType::Conditioned:
            return "Optional";
        case ActionType::RepopMinipet:
            return "Safely repop minipet";
        case ActionType::PingHardMode:
            return "Ping hard mode";
        case ActionType::PingTarget:
            return "Ping current target";
        case ActionType::AutoAttackTarget:
            return "Attack current target";
        case ActionType::ChangeWeaponSet:
            return "Change weapon set";
        case ActionType::StoreTarget:
            return "Store target";
        case ActionType::RestoreTarget:
            return "Restore target";
        case ActionType::StopScript:
            return "Stop script";
        case ActionType::LogOut:
            return "Log out";
        case ActionType::UseHeroSkill:
            return "Use hero skill";
        case ActionType::UnequipItem:
            return "Unequip item";
        case ActionType::ClearTarget:
            return "Clear target";
        case ActionType::WaitUntil:
            return "Wait until";
        default:
            return "Unknown";
    }
}

std::shared_ptr<Action> readAction(InputStream& stream)
{
    static_assert((int)ActionType::Count == 28);
    int type;

    stream >> type;
    switch (static_cast<ActionType>(type)) {
        case ActionType::MoveTo:
            return std::make_shared<MoveToAction>(stream);
        case ActionType::MoveToTargetPosition:
            return std::make_shared<MoveToTargetPositionAction>(stream);
        case ActionType::Cast:
            return std::make_shared<CastAction>(stream);
        case ActionType::CastBySlot:
            return std::make_shared<CastBySlotAction>(stream);
        case ActionType::ChangeTarget:
            return std::make_shared<ChangeTargetAction>(stream);
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
        case ActionType::Conditioned:
            return std::make_shared<ConditionedAction>(stream);
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
        case ActionType::WaitUntil:
            return std::make_shared<WaitUntilAction>(stream);
        default:
            return nullptr;
    }
}

std::shared_ptr<Action> drawActionSelector(float width)
{
    std::shared_ptr<Action> result = nullptr;

    const auto drawActionSelector = [&result](ActionType type) 
    {
        if (ImGui::MenuItem(toString(type).data())) 
        {
            result = makeAction(type);
        }
    };
    const auto drawSubMenu = [&drawActionSelector](std::string_view title, const auto& types) 
        {
        if (ImGui::BeginMenu(title.data())) 
        {
            for (const auto& type : types) 
            {
                drawActionSelector(type);
            }
            ImGui::EndMenu();
        }
    };

    if (ImGui::Button("Add action", ImVec2(width, 0))) 
    {
        ImGui::OpenPopup("Add action");
    }

    if (ImGui::BeginPopup("Add action")) 
    {
        drawSubMenu("Move to", std::array{ActionType::MoveTo, ActionType::MoveToTargetPosition});
        drawSubMenu("Skill", std::array{ActionType::Cast, ActionType::CastBySlot, ActionType::DropBuff, ActionType::UseHeroSkill});
        drawSubMenu("Interaction", std::array{ActionType::SendDialog, ActionType::GoToTarget, ActionType::AutoAttackTarget});
        drawSubMenu("Targeting", std::array{ActionType::ChangeTarget, ActionType::StoreTarget, ActionType::RestoreTarget, ActionType::ClearTarget});
        drawSubMenu("Items", std::array{ActionType::EquipItem, ActionType::ChangeWeaponSet, ActionType::UseItem, ActionType::RepopMinipet, ActionType::UnequipItem});
        drawSubMenu("Chat", std::array{ActionType::SendChat, ActionType::PingTarget, ActionType::PingHardMode});
        drawSubMenu("Other", std::array{ActionType::Wait, ActionType::WaitUntil, ActionType::Cancel, ActionType::LogOut, ActionType::StopScript});
        drawActionSelector(ActionType::Conditioned);

        ImGui::EndPopup();
    }

    return result;
}
