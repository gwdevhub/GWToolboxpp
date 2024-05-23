#include <TriggerPacket.h>

namespace{
    std::string_view toString(Trigger type)
    {
        static_assert((int)Trigger::Count == 4);
        switch (type) {
            case Trigger::None:
                return "No packet trigger";
            case Trigger::InstanceLoad:
                return "Trigger on instance load";
            case Trigger::HardModePing:
                return "Trigger on hard mode ping";
            case Trigger::Hotkey:
                return "Trigger on keypress";
            default:
                return "Unknown";
        }
    }
} // namespace

void drawTriggerSelector(Trigger& trigger, float width, long& hotkeyData, long& hotkeyMod)
{
    if (trigger == Trigger::None) 
    {
        if (ImGui::Button("Add trigger", ImVec2(width, 0))) {
            ImGui::OpenPopup("Add trigger");
        }
        if (ImGui::BeginPopup("Add trigger")) {
            for (auto i = 1; i < (int)Trigger::Count; ++i) {
                if (ImGui::Selectable(toString((Trigger)i).data())) {
                    trigger = (Trigger)i;
                }
            }
            ImGui::EndPopup();
        }
    }
    else if (trigger == Trigger::Hotkey) 
    {
        auto description = hotkeyData ? makeHotkeyDescription(hotkeyData, hotkeyMod) : "Click to change key";
        drawHotkeySelector(hotkeyData, hotkeyMod, description, width - 20.f);
        ImGui::SameLine();
        ImGui::Text("Hotkey");
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
            hotkeyData = 0;
            hotkeyMod = 0;
        }
    }
    else 
    {
        ImGui::Text(toString(trigger).data());
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = Trigger::None;
        }
    }
}

InputStream& operator>>(InputStream& in, Trigger& packet)
{
    std::string read;
    in >> read;
    packet = (Trigger)std::stoi(read);
    return in;
}
