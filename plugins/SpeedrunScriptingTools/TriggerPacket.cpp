#include <TriggerPacket.h>
#include <imgui.h>

#include <string_view>

namespace{
    std::string_view toString(TriggerPacket type)
    {
        static_assert((int)TriggerPacket::Count == 3);
        switch (type) {
            case TriggerPacket::None:
                return "No packet trigger";
            case TriggerPacket::InstanceLoad:
                return "Trigger on instance load";
            case TriggerPacket::HardModePing:
                return "Trigger on hard mode ping";
            default:
                return "Unknown";
        }
    }
} // namespace

void drawTriggerPacketSelector(TriggerPacket& trigger, float width)
{
    if (trigger == TriggerPacket::None) 
    {
        if (ImGui::Button("Add trigger", ImVec2(width, 0))) {
            ImGui::OpenPopup("Add trigger");
        }
        if (ImGui::BeginPopup("Add trigger")) {
            for (auto i = 1; i < (int)TriggerPacket::Count; ++i) {
                if (ImGui::Selectable(toString((TriggerPacket)i).data())) {
                    trigger = (TriggerPacket)i;
                }
            }
            ImGui::EndPopup();
        }
    }
    else 
    {
        ImGui::Text(toString(trigger).data());
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) {
            trigger = TriggerPacket::None;
        }
    }
}

std::istringstream& operator>>(std::istringstream& in, TriggerPacket& packet)
{
    std::string read;
    in >> read;
    packet = (TriggerPacket)std::stoi(read);
    return in;
}
