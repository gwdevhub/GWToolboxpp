#include <ActionIO.h>
#include <ActionImpls.h>

#include <imgui.h>

#include <array>

namespace {
    std::shared_ptr<Action> makeAction(ActionType type)
    {
        static_assert((int)ActionType::Count == 17);
        switch (type) {
            case ActionType::MoveTo:
                return std::make_shared<MoveToAction>();
            case ActionType::CastOnSelf:
                return std::make_shared<CastOnSelfAction>();
            case ActionType::Cast:
                return std::make_shared<CastAction>();
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
            default:
                return nullptr;
        }
    }
} // namespace

std::string_view toString(ActionType type)
{
    static_assert((int)ActionType::Count == 17);
    switch (type) {
        case ActionType::MoveTo:
            return "Move to";
        case ActionType::CastOnSelf:
            return "Force-cast on self";
        case ActionType::Cast:
            return "Cast";
        case ActionType::ChangeTarget:
            return "Change target";
        case ActionType::UseItem:
            return "Use item";
        case ActionType::SendDialog:
            return "Send dialog";
        case ActionType::GoToTarget:
            return "Go to current target";
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
        default:
            return "Unknown";
    }
}

std::shared_ptr<Action> readAction(InputStream& stream)
{
    static_assert((int)ActionType::Count == 17);
    int type;

    stream >> type;
    switch (static_cast<ActionType>(type)) {
        case ActionType::MoveTo:
            return std::make_shared<MoveToAction>(stream);
        case ActionType::CastOnSelf:
            return std::make_shared<CastOnSelfAction>(stream);
        case ActionType::Cast:
            return std::make_shared<CastAction>(stream);
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
        default:
            return nullptr;
    }
}

std::shared_ptr<Action> drawActionSelector(float width)
{
    std::shared_ptr<Action> result = nullptr;

    static_assert((int)ActionType::Count == 17);
    constexpr auto actions = std::array{
        ActionType::MoveTo,       
        ActionType::Cast,
        //ActionType::CastOnSelf, // CtoS cast on self despite having a different target. Currently deemed unnecessary
        ActionType::ChangeTarget, 
        ActionType::GoToTarget,
        ActionType::AutoAttackTarget, 
        ActionType::PingTarget,

        ActionType::UseItem,         
        ActionType::EquipItem,
        ActionType::SendDialog, 
        ActionType::SendChat,
        ActionType::DropBuff,
        
        ActionType::RepopMinipet,
        ActionType::PingHardMode,

        ActionType::Cancel,
        ActionType::Wait,
        ActionType::Conditioned,
    };

    if (ImGui::Button("Add action", ImVec2(width, 0))) {
        ImGui::OpenPopup("Add action");
    }
    if (ImGui::BeginPopup("Add action")) {
        for (auto action : actions) {
            if (ImGui::Selectable(toString(action).data())) {
                result = makeAction(action);
            }
        }
        ImGui::EndPopup();
    }

    return result;
}
