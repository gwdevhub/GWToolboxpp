#include "Slowload.h"

#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GWCA.h>

#include <Keys.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static Slowload instance;
    return &instance;
}

void logMessage(std::string_view message)
{
    const auto wMessage = std::wstring{message.begin(), message.end()};
    const size_t len = 42 + wcslen(wMessage.c_str());
    auto to_send = new wchar_t[len];
    swprintf(to_send, len - 1, L"<a=1>%s</a><c=#%6X>: %s</c>", L"Slowload plugin", 0xFFFFFF, wMessage.c_str());
    GW::GameThread::Enqueue([to_send] {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA2, to_send, nullptr);
        delete[] to_send;
    });
}

std::string makeHotkeyDescription(long keyData, long modifier)
{
    char newDescription[256];
    ModKeyName(newDescription, _countof(newDescription), modifier, keyData);
    return std::string{newDescription};
}

void drawHotkeySelector(long& keyData, long& modifier, std::string& description)
{
    ImGui::PushItemWidth(80);
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
    ImGui::PopItemWidth();
}

void Slowload::stopSlowLoad()
{
    if (status != Status::WaitingInLoadScreen) return;
    status = Status::StoppingSlowload;
    GW::GameThread::Enqueue([packet = this->packet, this]() mutable {
        GW::StoC::EmulatePacket(reinterpret_cast<GW::Packet::StoC::PacketBase*>(&packet));
        status = Status::Inactive;
    });
}

bool Slowload::WndProc(const UINT Message, const WPARAM wParam, LPARAM lparam)
{
    if (GW::Chat::GetIsTyping() || GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow()) {
        return false;
    }
    long keyData = 0;
    switch (Message) {
        case WM_KEYDOWN:
            if (const auto isRepeated = (int)lparam & (1 << 30)) break;
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            keyData = static_cast<int>(wParam);
            break;
        case WM_XBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
            if (LOWORD(wParam) & MK_MBUTTON) {
                keyData = VK_MBUTTON;
            }
            if (LOWORD(wParam) & MK_XBUTTON1) {
                keyData = VK_XBUTTON1;
            }
            if (LOWORD(wParam) & MK_XBUTTON2) {
                keyData = VK_XBUTTON2;
            }
            break;
        case WM_XBUTTONUP:
        case WM_MBUTTONUP:
            // leave keydata to none, need to handle special case below
            break;
        case WM_MBUTTONDBLCLK:
            keyData = VK_MBUTTON;
            break;
        default:
            break;
    }

    if (!keyData) return false;

    switch (Message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: {
            long modifier = 0;
            if (GetKeyState(VK_CONTROL) < 0) {
                modifier |= ModKey_Control;
            }
            if (GetKeyState(VK_SHIFT) < 0) {
                modifier |= ModKey_Shift;
            }
            if (GetKeyState(VK_MENU) < 0) {
                modifier |= ModKey_Alt;
            }

            const auto triggered = shortcutKey && modifier == shortcutMod && keyData == shortcutKey;
            if (triggered)
            {
                switch (status) {
                    case Status::Inactive:
                        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return false;
                        status = Status::WaitingForLoadScreen;
                        logMessage("Activated");
                        break;
                    case Status::WaitingForLoadScreen:
                        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return false;
                        status = Status::Inactive;
                        logMessage("Deactivated");
                        break;
                    case Status::WaitingInLoadScreen:
                        stopSlowLoad();
                        break;
                    case Status::StoppingSlowload:
                        break;
                }
            }
            return triggered;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:

        case WM_XBUTTONUP:
        case WM_MBUTTONUP:
        default:
            return false;
    }
}

namespace {
    // Forget you ever saw this
    std::string to_string(const std::wstring& wstring)
    {
        return std::filesystem::path(wstring).string();
    }
    std::wstring to_wstring(const std::string& string)
    {
        return std::filesystem::path(string).wstring();
    }
} // namespace

void Slowload::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);

    const auto ini = LoadIni(folder);

    shortcutKey = ini.GetLongValue(Name(), VAR_NAME(shortcutKey), shortcutKey);
    shortcutMod = ini.GetLongValue(Name(), VAR_NAME(shortcutMod), shortcutMod);

    ModKeyName(hotkeyDescription, _countof(hotkeyDescription), shortcutMod, shortcutKey);
}

void Slowload::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);

    auto ini = LoadIni(folder);

    ini.SetLongValue(Name(), VAR_NAME(shortcutKey), shortcutKey);
    ini.SetLongValue(Name(), VAR_NAME(shortcutMod), shortcutMod);

    PLUGIN_ASSERT(ini.SaveFile(ini.location_on_disk) == SI_OK);
}

void Slowload::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

    ImGui::Text("Hotkey: ");
    ImGui::SameLine();
    
    auto description = shortcutKey ? makeHotkeyDescription(shortcutKey, shortcutMod) : "Set hotkey";
    drawHotkeySelector(shortcutKey, shortcutMod, description);
    
    if (shortcutKey)
    {
        ImGui::SameLine();
        if (ImGui::Button("Clear###0")) {
            shortcutKey = 0;
            shortcutMod = 0;
        }
    }

    ImGui::Text("Version 1.0.4");
}

void Slowload::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&instanceLoadEntry, [this](GW::HookStatus* hookStatus, GW::Packet::StoC::InstanceLoadFile* packet) {
        if (shortcutKey && status == Status::WaitingForLoadScreen)
        {
            status = Status::WaitingInLoadScreen;
            this->packet = *packet;
            hookStatus->blocked = true;
        }
        else 
        {
            status = Status::Inactive;
        }
    });
}

void Slowload::SignalTerminate()
{
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadFile>(&instanceLoadEntry);
    ToolboxPlugin::SignalTerminate();
}
