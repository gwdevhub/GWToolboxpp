#include <ActionIO.h>
#include <ActionImpls.h>

#include <imgui.h>

namespace {
    ActionPtr makeAction(ActionType type)
    {
        static_assert((int)ActionType::Count == 40);
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
            case ActionType::MoveInchwise:
                return std::make_shared<MoveInchwiseAction>();
            case ActionType::GWKey:
                return std::make_shared<GWKeyAction>();
            case ActionType::EquipItemBySlot:
                return std::make_shared<EquipItemBySlotAction>();
            case ActionType::EnterCriticalSection:
                return std::make_shared<EnterCriticalSectionAction>();
            case ActionType::LeaveCriticalSection:
                return std::make_shared<LeaveCriticalSectionAction>();
            case ActionType::SetVariable:
                return std::make_shared<SetVariableAction>();
            case ActionType::DecrementVariable:
                return std::make_shared<DecrementVariableAction>();
            case ActionType::IncrementVariable:
                return std::make_shared<IncrementVariableAction>();
            case ActionType::AbandonQuest:
                return std::make_shared<AbandonQuestAction>();
            case ActionType::MoveItemToSlot:
                return std::make_shared<MoveItemToSlotAction>();
            case ActionType::RotateCharacter:
                return std::make_shared<RotateCharacterAction>();
            case ActionType::KeyboardMove:
                return std::make_shared<KeyboardMoveAction>();
            default:
                return nullptr;
        }
    }
} // namespace

std::string_view toString(ActionType type)
{
    static_assert((int)ActionType::Count == 40);
    switch (type) {
        case ActionType::MoveTo:
            return "Move to Position";
        case ActionType::MoveToTargetPosition:
            return "Move to distance from target";
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
        case ActionType::MoveInchwise:
            return "Inchwise";
        case ActionType::GWKey:
            return "Guild Wars Key";
        case ActionType::EquipItemBySlot:
            return "Equip item by slot";
        case ActionType::EnterCriticalSection:
            return "Enter critical section";
        case ActionType::LeaveCriticalSection:
            return "Leave critical section";
        case ActionType::SetVariable:
            return "Set";
        case ActionType::DecrementVariable:
            return "Decrement";
        case ActionType::IncrementVariable:
            return "Increment";
        case ActionType::AbandonQuest:
            return "Abandon quest";
        case ActionType::MoveItemToSlot:
            return "Move item to slot";
        case ActionType::RotateCharacter:
            return "Rotate";
        case ActionType::KeyboardMove:
            return "Keyboard move";
        default:
            return "Unknown";
    }
}

ActionPtr readAction(InputStream& stream)
{
    static_assert((int)ActionType::Count == 40);
    int type;

    stream >> type;
    switch (static_cast<ActionType>(type)) {
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
        case ActionType::GWKey:
            return std::make_shared<GWKeyAction>(stream);
        case ActionType::EquipItemBySlot:
            return std::make_shared<EquipItemBySlotAction>(stream);
        case ActionType::EnterCriticalSection:
            return std::make_shared<EnterCriticalSectionAction>(stream);
        case ActionType::LeaveCriticalSection:
            return std::make_shared<LeaveCriticalSectionAction>(stream);
        case ActionType::SetVariable:
            return std::make_shared<SetVariableAction>(stream);
        case ActionType::DecrementVariable:
            return std::make_shared<DecrementVariableAction>(stream);
        case ActionType::IncrementVariable:
            return std::make_shared<IncrementVariableAction>(stream);
        case ActionType::AbandonQuest:
            return std::make_shared<AbandonQuestAction>(stream);
        case ActionType::MoveItemToSlot:
            return std::make_shared<MoveItemToSlotAction>(stream);
        case ActionType::RotateCharacter:
            return std::make_shared<RotateCharacterAction>(stream);
        case ActionType::KeyboardMove:
            return std::make_shared<KeyboardMoveAction>(stream);
        default:
            return nullptr;
    }
}

ActionPtr drawActionSelector(float width)
{
    ActionPtr result = nullptr;

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
        drawSubMenu("Movement", std::array{ActionType::MoveTo, ActionType::MoveToTargetPosition, ActionType::MoveInchwise, ActionType::RotateCharacter, ActionType::KeyboardMove});
        drawSubMenu("Skill", std::array{ActionType::Cast, ActionType::CastBySlot, ActionType::DropBuff, ActionType::UseHeroSkill});
        drawSubMenu("Interaction", std::array{ActionType::SendDialog, ActionType::GoToTarget, ActionType::AutoAttackTarget});
        drawSubMenu("Targeting", std::array{ActionType::ChangeTarget, ActionType::StoreTarget, ActionType::RestoreTarget, ActionType::ClearTarget});
        drawSubMenu("Items", std::array{ActionType::EquipItem, ActionType::EquipItemBySlot, ActionType::MoveItemToSlot, ActionType::ChangeWeaponSet, ActionType::UseItem, ActionType::RepopMinipet, ActionType::UnequipItem});
        drawSubMenu("Chat", std::array{ActionType::SendChat, ActionType::PingTarget, ActionType::PingHardMode});
        drawSubMenu("Control flow", std::array{ActionType::Wait, ActionType::WaitUntil, ActionType::StopScript, ActionType::EnterCriticalSection, ActionType::LeaveCriticalSection});
        drawSubMenu("Variables", std::array{ActionType::SetVariable, ActionType::IncrementVariable, ActionType::DecrementVariable});
        drawSubMenu("Other", std::array{ActionType::Cancel, ActionType::LogOut, ActionType::GWKey, ActionType::AbandonQuest});
        drawActionSelector(ActionType::Conditioned);

        ImGui::EndPopup();
    }

    return result;
}
