#include <utils.h>

#include <imgui.h>

#include <Keys.h>

namespace 
{
    const std::string endOfStringSignifier{"ENDOFSTRING"};
}

void ShowHelp(const char* help)
{
    ImGui::SameLine();
    ImGui::TextDisabled("%s", "(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", help);
    }
}

std::string readStringWithSpaces(std::istringstream& stream) 
{
    std::string result;
    std::string word;
    while (stream >> word) {
        if (word == endOfStringSignifier) break;
        result += word + " ";
    }
    if (!result.empty()) {
        result.erase(result.size() - 1, 1); // last character is space
    }

    return result;
}

void writeStringWithSpaces(std::ostringstream& stream, const std::string& word) 
{
    stream << word << " " << endOfStringSignifier << " ";
}

std::string_view toString(Status status)
{
    switch (status) {
        case Status::Any:
            return "Any";
        case Status::Dead:
            return "Dead";
        case Status::Alive:
            return "Alive";
    }
    return "";
}

std::string toString(Class c)
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
        default:
            return "Any";
    }
}
std::string_view toString(AgentType type) {
    switch (type) {
        case AgentType::Any:
            return "Any";
        case AgentType::PartyMember:
            return "Party member";
        case AgentType::Friendly:
            return "Friendly";
        case AgentType::Hostile:
            return "Hostile";
        default: 
            return "Any";
    }
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
        default:
            return "";
    }
}
std::string_view toString(HexedStatus status) {
    switch (status) {
        case HexedStatus::Any:
            return "Any";
        case HexedStatus::NotHexed:
            return "Not hexed";
        case HexedStatus::Hexed:
            return "Hexed";
        default:
            return "";
    }
}

std::string makeHotkeyDescription(long keyData, long modifier) 
{
    char newDescription[256];
    ModKeyName(newDescription, _countof(newDescription), modifier, keyData);
    return std::string{newDescription};
}

void drawHotkeySelector(long& keyData, long& modifier, std::string& description, float selectorWidth) 
{
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
        if (ImGui::IsKeyDown(ImGuiKey_ModCtrl)) newmod |= ModKey_Control;
        if (ImGui::IsKeyDown(ImGuiKey_ModShift)) newmod |= ModKey_Shift;
        if (ImGui::IsKeyDown(ImGuiKey_ModAlt)) newmod |= ModKey_Alt;

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
            keyData = newkey;
            modifier = newmod;
            description = makeHotkeyDescription(newkey, newmod);
            newkey = 0;
            ImGui::CloseCurrentPopup();
        }

        ModKeyName(newkey_buf, _countof(newkey_buf), newmod, newkey);
        ImGui::Text("%s", newkey_buf);

        ImGui::EndPopup();
    }
}
