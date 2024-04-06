#include <ActionIO.h>
#include <ActionImpls.h>

#include <imgui.h>

namespace {
    std::shared_ptr<Action> makeAction(ActionType type)
    {
        static_assert((int)ActionType::Count == 9);
        switch (type) {
            case ActionType::MoveTo:
                return std::make_shared<MoveToAction>();
            case ActionType::CastOnSelf:
                return std::make_shared<CastOnSelfAction>();
            case ActionType::CastOnTarget:
                return std::make_shared<CastOnTargetAction>();
            case ActionType::UseItem:
                return std::make_shared<UseItemAction>();
            case ActionType::SendDialog:
                return std::make_shared<SendDialogAction>();
            case ActionType::GoToNpc:
                return std::make_shared<GoToNpcAction>();
            case ActionType::Wait:
                return std::make_shared<WaitAction>();
            case ActionType::SendChat:
                return std::make_shared<SendChatAction>();
            case ActionType::Cancel:
                return std::make_shared<CancelAction>();
            default:
                return nullptr;
        }
    }

    std::string_view toString(ActionType type)
    {
        static_assert((int)ActionType::Count == 9);
        switch (type) {
            case ActionType::MoveTo:
                return "Move to";
            case ActionType::CastOnSelf:
                return "Cast on self";
            case ActionType::CastOnTarget:
                return "Cast on current target";
            case ActionType::UseItem:
                return "Use item";
            case ActionType::SendDialog:
                return "Send dialog";
            case ActionType::GoToNpc:
                return "Go to NPC";
            case ActionType::Wait:
                return "Wait";
            case ActionType::SendChat:
                return "Send chat";
            case ActionType::Cancel:
                return "Cancel current action";
            default:
                return "Unknown";
        }
    }
} // namespace

std::shared_ptr<Action> readAction(std::istringstream& stream)
{
    static_assert((int)ActionType::Count == 9);
    int type;

    stream >> type;
    switch (static_cast<ActionType>(type)) {
        case ActionType::MoveTo:
            return std::make_shared<MoveToAction>(stream);
        case ActionType::CastOnSelf:
            return std::make_shared<CastOnSelfAction>(stream);
        case ActionType::CastOnTarget:
            return std::make_shared<CastOnTargetAction>(stream);
        case ActionType::UseItem:
            return std::make_shared<UseItemAction>(stream);
        case ActionType::SendDialog:
            return std::make_shared<SendDialogAction>(stream);
        case ActionType::GoToNpc:
            return std::make_shared<GoToNpcAction>(stream);
        case ActionType::Wait:
            return std::make_shared<WaitAction>(stream);
        case ActionType::SendChat:
            return std::make_shared<SendChatAction>(stream);
        case ActionType::Cancel:
            return std::make_shared<CancelAction>(stream);
        default:
            return nullptr;
    }
}

std::shared_ptr<Action> drawActionSelector(float width)
{
    std::shared_ptr<Action> result = nullptr;
    
    if (ImGui::Button("Add action", ImVec2(width, 0))) {
        ImGui::OpenPopup("Add action");
    }
    if (ImGui::BeginPopup("Add action")) {
        for (auto i = 0; i < (int)ActionType::Count; ++i) {
            if (ImGui::Selectable(toString((ActionType)i).data())) {
                result = makeAction(ActionType(i));
            }
        }
        ImGui::EndPopup();
    }

    return result;
}
