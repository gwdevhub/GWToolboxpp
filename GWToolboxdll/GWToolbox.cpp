#include "stdafx.h"

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/MapContext.h>

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Modules/ChatCommands.h>
#include <Modules/ToolboxTheme.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/CrashHandler.h>
#include <Modules/DialogModule.h>
#include "Modules/AprilFools.h"
#include "Modules/ChatSettings.h"
#include "Modules/GameSettings.h"
#include "Modules/GwDatTextureModule.h"
#include "Modules/HallOfMonumentsModule.h"
#include "Modules/InventoryManager.h"
#include "Modules/LoginModule.h"
#include "Modules/Updater.h"
#include "Windows/SettingsWindow.h"

#include <Windows/MainWindow.h>
#include <Widgets/Minimap/Minimap.h>
#include <hidusage.h>




// declare method here as recommended by imgui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    HMODULE dllmodule = nullptr;
    WNDPROC OldWndProc = nullptr;
    bool defer_close = false;
    HWND gw_window_handle = nullptr;

    utf8::string imgui_inifile;
    bool imgui_inifile_changed = false;
    bool settings_folder_changed = false;

    bool must_self_destruct = false; // is true when toolbox should quit
    GW::HookEntry Update_Entry;

    bool event_handler_attached = false;

    bool IsPvP() {
        const auto m = GW::Map::GetMapInfo();
        return (m && /*GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost &&*/ m->GetIsPvP());
    }

    bool AttachWndProcHandler()
    {
        if (event_handler_attached) {
            return true;
        }
        Log::Log("installing event handler\n");
        gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
        OldWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(gw_window_handle, GWL_WNDPROC, reinterpret_cast<LONG>(SafeWndProc)));
        Log::Log("Installed input event handler, oldwndproc = 0x%X\n", OldWndProc);

        // RegisterRawInputDevices to be able to receive WM_INPUT via WndProc
        static RAWINPUTDEVICE rid;
        rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid.usUsage = HID_USAGE_GENERIC_MOUSE;
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = gw_window_handle;
        ASSERT(RegisterRawInputDevices(&rid, 1, sizeof(rid)));

        event_handler_attached = true;
        return true;
    }

    bool DetachWndProcHandler()
    {
        if (!event_handler_attached) {
            return true;
        }
        Log::Log("Restoring input hook\n");
        SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, reinterpret_cast<LONG>(OldWndProc));
        event_handler_attached = false;
        return true;
    }



    bool render_callback_attached = false;
    bool AttachRenderCallback() {
        if (!render_callback_attached) {
            GW::Render::SetRenderCallback(GWToolbox::Draw);
            render_callback_attached = true;
        }
        return render_callback_attached;
    }
    bool DetachRenderCallback() {
        if (render_callback_attached) {
            GW::Render::SetRenderCallback(nullptr);
            render_callback_attached = false;
        }
        return !render_callback_attached;
    }

    bool game_loop_callback_attached = false;
    GW::HookEntry game_loop_callback_entry;
    bool AttachGameLoopCallback() {
        if (!game_loop_callback_attached) {
            GW::GameThread::RegisterGameThreadCallback(&game_loop_callback_entry,GWToolbox::Update);
            game_loop_callback_attached = true;
        }
        return game_loop_callback_attached;
    }
    bool DetachGameLoopCallback() {
        if (game_loop_callback_attached) {
            GW::GameThread::RemoveGameThreadCallback(&game_loop_callback_entry);
            game_loop_callback_attached = false;
        }
        return !game_loop_callback_attached;
    }

    bool imgui_initialized = false;
    bool AttachImgui(IDirect3DDevice9* device)
    {
        if (imgui_initialized) {
            return true;
        }
        ImGui::CreateContext();
        //ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), device);
        ImGui_ImplDX9_Init(device);
        ImGui_ImplWin32_Init(GW::MemoryMgr::GetGWWindowHandle());

        GW::Render::SetResetCallback([](IDirect3DDevice9*) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
        });

        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = false;
        io.IniFilename = imgui_inifile.bytes;

        imgui_initialized = true;

        Resources::EnsureFileExists(
            Resources::GetPath(L"Font.ttf"),
            "https://raw.githubusercontent.com/gwdevhub/GWToolboxpp/master/resources/Font.ttf",
            [](const bool success, const std::wstring& error) {
                if (success) {
                    GuiUtils::LoadFonts();
                }
                else {
                    Log::ErrorW(L"Cannot download font, please download it manually!\n%s", error.c_str());
                    GWToolbox::SignalTerminate();
                }
            });

        return true;
    }

    bool DetachImgui()
    {
        if (!imgui_initialized) {
            return true;
        }
        GW::Render::SetResetCallback(nullptr);
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imgui_initialized = false;
        return true;
    }

    std::vector<ToolboxModule*> modules_enabled{};
    std::vector<ToolboxWidget*> widgets_enabled{};
    std::vector<ToolboxWindow*> windows_enabled{};

    std::vector<ToolboxModule*> all_modules_enabled{};
    std::vector<ToolboxUIElement*> ui_elements_enabled{};

    std::vector<ToolboxModule*> modules_terminating{};

    enum class GWToolboxState {
        Initialising,
        UpdateInitialising,
        DrawInitialising,
        Initialised,
        DrawTerminating,
        Terminating,
        Terminated,
        Disabled
    };
    GWToolboxState gwtoolbox_state = GWToolboxState::Terminated;
    bool gwtoolbox_disabled = false;

    bool pending_detach_dll = false;

    bool greeted = false;

    void ReorderModules(std::vector<ToolboxModule*>& modules)
    {
        std::ranges::sort(modules, [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
            return std::string(lhs->SettingsName()).compare(rhs->SettingsName()) < 0;
        });
    }

    ToolboxIni* OpenSettingsFile()
    {
        static ToolboxIni* inifile = nullptr;
        const auto full_path = Resources::GetSettingFile(GWTOOLBOX_INI_FILENAME);
        if (!GWToolbox::SettingsFolderChanged() && inifile) {
            return inifile;
        }
        auto tmp = new ToolboxIni(false, false, false);
        ASSERT(tmp->LoadIfExists(full_path) == SI_OK);
        tmp->location_on_disk = full_path;
        inifile = tmp;
        return inifile;
    }

    bool ShouldDisableToolbox() {
        const auto m = GW::Map::GetMapInfo();
        return m && m->GetIsPvP();
    }

    bool CanRenderToolbox() {
        return !gwtoolbox_disabled
            && GW::UI::GetIsUIDrawn()
            && !GW::GetPreGameContext()
            && !GW::Map::GetIsInCinematic()
            && !IsIconic(GW::MemoryMgr::GetGWWindowHandle())
            && GuiUtils::FontsLoaded();
    }

    bool ToggleTBModule(ToolboxModule& m, std::vector<ToolboxModule*>& vec, const bool enable)
    {
        const auto found = std::ranges::find(vec, &m);
        if (found != vec.end()) {
            // Module found
            if (enable) {
                return true;
            }
            m.SaveSettings(OpenSettingsFile());
            modules_terminating.push_back(&m);
            m.SignalTerminate();
            vec.erase(found);
            ReorderModules(vec);
            return false;
        }
        // Module not found
        if (!enable) {
            return false;
        }
        const auto is_terminating = std::ranges::find(modules_terminating, &m);
        if (is_terminating != modules_terminating.end()) {
            return false; // Not finished terminating
        }
        vec.push_back(&m);
        m.Initialize();
        m.LoadSettings(OpenSettingsFile());
        ReorderModules(vec);
        return true; // Added successfully
    }
}

const std::vector<ToolboxModule*>& GWToolbox::GetAllModules()
{
    return all_modules_enabled;
}

const std::vector<ToolboxUIElement*>& GWToolbox::GetUIElements()
{
    return ui_elements_enabled;
}

const std::vector<ToolboxModule*>& GWToolbox::GetModules()
{
    return modules_enabled;
}

const std::vector<ToolboxWindow*>& GWToolbox::GetWindows()
{
    return windows_enabled;
}

const std::vector<ToolboxWidget*>& GWToolbox::GetWidgets()
{
    return widgets_enabled;
}

void UpdateEnabledWidgetVectors(ToolboxModule* m, bool added)
{
    const auto update_vec = [added](std::vector<void*>& vec, void* m) {
        const auto found = std::ranges::find(vec, m);
        if (added) {
            if (found == vec.end()) {
                vec.push_back(m);
            }
        }
        else {
            if (found != vec.end()) {
                vec.erase(found);
            }
        }
    };
    update_vec(reinterpret_cast<std::vector<void*>&>(all_modules_enabled), m);
    if (m->IsUIElement()) {
        update_vec(reinterpret_cast<std::vector<void*>&>(ui_elements_enabled), m);
        if (m->IsWidget()) {
            update_vec(reinterpret_cast<std::vector<void*>&>(widgets_enabled), m);
        }
        if (m->IsWindow()) {
            update_vec(reinterpret_cast<std::vector<void*>&>(windows_enabled), m);
        }
    }
    else {
        update_vec(reinterpret_cast<std::vector<void*>&>(modules_enabled), m);
    }
}

bool GWToolbox::IsInitialized() { return gwtoolbox_state == GWToolboxState::Initialised; }

bool GWToolbox::ToggleModule(ToolboxWidget& m, const bool enable)
{
    const bool added = ToggleTBModule(m, reinterpret_cast<std::vector<ToolboxModule*>&>(widgets_enabled), enable);
    UpdateEnabledWidgetVectors(&m, added);
    return added;
}

bool GWToolbox::ToggleModule(ToolboxWindow& m, const bool enable)
{
    const bool added = ToggleTBModule(m, reinterpret_cast<std::vector<ToolboxModule*>&>(windows_enabled), enable);
    UpdateEnabledWidgetVectors(&m, added);
    return added;
}

bool GWToolbox::ToggleModule(ToolboxModule& m, const bool enable)
{
    const bool added = ToggleTBModule(m, modules_enabled, enable);
    UpdateEnabledWidgetVectors(&m, added);
    return added;
}

HMODULE GWToolbox::GetDLLModule()
{
    return dllmodule;
}

DWORD __stdcall SafeThreadEntry(const LPVOID module) noexcept
{
    dllmodule = static_cast<HMODULE>(module);
    __try {
        ThreadEntry(nullptr);
    } __except (EXCEPT_EXPRESSION_ENTRY) {
        Log::Log("SafeThreadEntry __except body\n");
    }
    return EXIT_SUCCESS;
}

DWORD __stdcall ThreadEntry(LPVOID)
{
    Log::Log("Initializing API\n");

    GW::HookBase::Initialize();
    if (!GW::Initialize()) {
        if (MessageBoxA(nullptr, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0) == IDOK) { }
        return 0;
    }

    Log::Log("Installing dx hooks\n");

    // Some modules rely on the gwdx_ptr being present for stuff like getting viewport coords.
    // Because this ptr isn't set until the Render loop runs at least once, let it run and then reassign SetRenderCallback.
    GWToolbox::Initialize();

    Log::Log("Installed dx hooks\n");

    Log::InitializeChat();

    Log::Log("Installed chat hooks\n");

    while (gwtoolbox_state != GWToolboxState::Terminated) {
        // wait until destruction
        Sleep(100);

        // Feel free to uncomment to get this behavior for testing, but don't commit.
        //#ifdef _DEBUG
        //        if (GetAsyncKeyState(VK_END) & 1) {
        //            GWToolbox::Instance().StartSelfDestruct();
        //        }
        //#endif
    }

    // @Remark:
    // Hooks are disable from Guild Wars thread (safely), so we just make sure we exit the last hooks
    while (GW::HookBase::GetInHookCount()) {
        Sleep(16);
    }

    // @Remark:
    // We can't guarantee that the code in Guild Wars thread isn't still in the trampoline, but
    // practically a short sleep is fine.
    Sleep(16);

    Log::Log("Destroying API\n");
    GW::Terminate();

    Log::Log("Closing log/console, bye!\n");
    Log::Terminate();
    return 0;
}

LRESULT CALLBACK SafeWndProc(const HWND hWnd, const UINT Message, const WPARAM wParam, const LPARAM lParam) noexcept
{
    __try {
        return WndProc(hWnd, Message, wParam, lParam);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
    }
}

LRESULT CALLBACK WndProc(const HWND hWnd, const UINT Message, const WPARAM wParam, const LPARAM lParam)
{
    static bool right_mouse_down = false;

    if (Message == WM_CLOSE || (Message == WM_SYSCOMMAND && wParam == SC_CLOSE)) {
        // This is naughty, but we need to defer the closing signal until toolbox has terminated properly.
        // we can't sleep here, because toolbox modules will probably be using the render loop to close off things
        // like hooks
        defer_close = true;
        GWToolbox::SignalTerminate();

        return 0;
    }

    if (!(!GW::GetPreGameContext() && GWToolbox::IsInitialized())) {
        return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
    }

    if (Message == WM_RBUTTONUP) {
        right_mouse_down = false;
    }
    if (Message == WM_RBUTTONDOWN) {
        right_mouse_down = true;
    }
    if (Message == WM_RBUTTONDBLCLK) {
        right_mouse_down = true;
    }

    GWToolbox::Instance().right_mouse_down = right_mouse_down;

    // === Send events to ImGui ===
    const auto& io = ImGui::GetIO();
    const bool skip_mouse_capture = right_mouse_down || GW::UI::GetIsWorldMapShowing() || GW::Map::GetIsInCinematic();
    if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam) && !skip_mouse_capture) {
        return TRUE;
    }

    // === Send events to toolbox ===
    auto& tb = GWToolbox::Instance();
    switch (Message) {
        // Send button up mouse events to everything, to avoid being stuck on mouse-down
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_INPUT:
            for (const auto m : tb.GetAllModules()) {
                m->WndProc(Message, wParam, lParam);
            }
            break;

        // Other mouse events:
        // - If right mouse down, leave it to gw
        // - ImGui first (above), if WantCaptureMouse that's it
        // - Toolbox module second (e.g.: minimap), if captured, that's it
        // - otherwise pass to gw
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL: {
            if (io.WantCaptureMouse && !skip_mouse_capture) {
                return true;
            }
            bool captured = false;
            for (const auto m : tb.GetAllModules()) {
                if (m->WndProc(Message, wParam, lParam)) {
                    captured = true;
                }
            }
            if (captured) {
                return true;
            }
        }
        //if (!skip_mouse_capture) {

        //}
        break;

        // keyboard messages
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (io.WantTextInput) {
                break; // if imgui wants them, send to imgui (above) and to gw
            }
        // else fallthrough
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_IME_CHAR:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONUP:
            if (io.WantTextInput) {
                return true; // if imgui wants them, send just to imgui (above)
            }

        // send input to chat commands for camera movement
            if (ChatCommands::Instance().WndProc(Message, wParam, lParam)) {
                return true;
            }
        case WM_ACTIVATE:
            // send to toolbox modules and plugins
        {
            bool captured = false;
            for (const auto m : tb.GetAllModules()) {
                if (m->WndProc(Message, wParam, lParam)) {
                    captured = true;
                }
            }
            if (captured) {
                return true;
            }
        }
        // note: capturing those events would prevent typing if you have a hotkey assigned to normal letters.
        // We may want to not send events to toolbox if the player is typing in-game
        // Otherwise, we may want to capture events.
        // For that, we may want to only capture *successfull* hotkey activations.
        break;

        case WM_SIZE:
            // ImGui doesn't need this, it reads the viewport size directly
            break;
        default:
            // Custom messages registered via RegisterWindowMessage
            if (Message >= 0xC000 && Message <= 0xFFFF) {
                for (const auto m : tb.GetAllModules()) {
                    m->WndProc(Message, wParam, lParam);
                }
            }
            break;
    }

    return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
}

void GWToolbox::Initialize()
{
    switch (gwtoolbox_state) {
    case GWToolboxState::Terminated:
        gwtoolbox_state = GWToolboxState::Initialising;
        AttachRenderCallback();
        //AttachGameLoopCallback();
        GW::EnableHooks();
        UpdateInitialising(.0f);
        AttachGameLoopCallback();
        pending_detach_dll = false;
    }
}

std::filesystem::path GWToolbox::LoadSettings()
{
    const auto ini = OpenSettingsFile();
    if (!ini->location_on_disk.empty()) {
        for (const auto m : modules_enabled) {
            m->LoadSettings(ini);
        }
        for (const auto m : widgets_enabled) {
            m->LoadSettings(ini);
        }
        for (const auto m : windows_enabled) {
            m->LoadSettings(ini);
        }
    }
    return ini->location_on_disk;
}

bool GWToolbox::SetSettingsFolder(const std::filesystem::path& path)
{
    static auto last_path = std::filesystem::path{};
    if (last_path != path) {
        if (Resources::SetSettingsFolder(path)) {
            imgui_inifile = Unicode16ToUtf8(Resources::GetSettingFile(L"interface.ini").c_str());
            settings_folder_changed = true;
            imgui_inifile_changed = true;
            last_path = path;
            return true;
        }
        return false;
    }
    return true;
}

bool GWToolbox::SettingsFolderChanged()
{
    return settings_folder_changed;
}

std::filesystem::path GWToolbox::SaveSettings()
{
    const auto ini = OpenSettingsFile();
    for (const auto m : modules_enabled) {
        m->SaveSettings(ini);
    }
    for (const auto m : widgets_enabled) {
        m->SaveSettings(ini);
    }
    for (const auto m : windows_enabled) {
        m->SaveSettings(ini);
    }
    ASSERT(Resources::SaveIniToFile(ini->location_on_disk, ini) == 0);
    const auto dir = ini->location_on_disk.parent_path();
    const auto dirstr = dir.wstring();
    const std::wstring printable = std::regex_replace(dirstr, std::wregex(L"\\\\"), L"/");
    Log::LogW(L"Toolbox settings saved to %s", printable.c_str());
    settings_folder_changed = false;
    return ini->location_on_disk;
}

void GWToolbox::SignalTerminate(bool detach_dll)
{
    switch (gwtoolbox_state) {
    case GWToolboxState::Disabled:
    case GWToolboxState::Terminating:
    case GWToolboxState::Initialised:
    case GWToolboxState::Initialising:
        gwtoolbox_state = GWToolboxState::DrawTerminating;
        AttachGameLoopCallback();
        AttachRenderCallback();
        pending_detach_dll = detach_dll;
    }
}

void GWToolbox::Enable()
{
    if (!gwtoolbox_disabled)
        return;
    GW::EnableHooks();
    gwtoolbox_disabled = false;
}
void GWToolbox::Disable()
{
    if (gwtoolbox_disabled)
        return;
    GW::DisableHooks();
    GW::RenderModule.enable_hooks();
    AttachRenderCallback();
    gwtoolbox_disabled = true;
}

bool GWToolbox::CanTerminate()
{
    return modules_terminating.empty() 
        && GuiUtils::FontsLoaded() 
        && all_modules_enabled.empty()
        && !imgui_initialized
        && !event_handler_attached;
}

void GWToolbox::Update(GW::HookStatus*) {
    static DWORD last_tick_count;
    if (last_tick_count == 0) {
        last_tick_count = GetTickCount();
    }

    // @Enhancement:
    // Improve precision with QueryPerformanceCounter
    const auto tick = GetTickCount();
    const auto delta = tick - last_tick_count;
    const auto delta_f = static_cast<float>(delta) / 1000.f;

    switch (gwtoolbox_state) {
    case GWToolboxState::Terminating:
        return UpdateTerminating(delta_f);
    case GWToolboxState::Initialising:
        return UpdateInitialising(delta_f);
    case GWToolboxState::Initialised:
        break;
    default:
        return;
    }

    // Update loop
    for (const auto m : all_modules_enabled) {
        m->Update(delta_f);
    }
    last_tick_count = tick;
}

void GWToolbox::Draw(IDirect3DDevice9* device)
{

    switch (gwtoolbox_state) {
    case GWToolboxState::DrawTerminating:
        return DrawTerminating(device);
    case GWToolboxState::DrawInitialising:
        return DrawInitialising(device);
    case GWToolboxState::Initialised:
        break;
    default:
        return;
    }

    if (imgui_inifile_changed) {
        auto& io = ImGui::GetIO();
        io.IniFilename = imgui_inifile.bytes;
        imgui_inifile_changed = false;
    }
    if (gwtoolbox_disabled) {
        if (!ShouldDisableToolbox()) {
            Enable();
        }
        return;
    }
    else if (ShouldDisableToolbox()) {
        Disable();
        return;
    }
    if (!CanRenderToolbox())
        return;

    // Draw loop
    Resources::DxUpdate(device);

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();

    const bool world_map_showing = GW::UI::GetIsWorldMapShowing();

    if (!world_map_showing) {
        Minimap::Render(device);
    }

    ImGui::NewFrame();

    // Key up/down events don't get passed to gw window when out of focus, but we need the following to be correct,
    // or things like alt-tab make imgui think that alt is still down.
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_ModCtrl, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiKey_ModShift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiKey_ModAlt, (GetKeyState(VK_MENU) & 0x8000) != 0);

    for (const auto uielement : ui_elements_enabled) {
        if (world_map_showing && !uielement->ShowOnWorldMap()) {
            continue;
        }
        uielement->Draw(device);
    }

#ifdef _DEBUG
    // Feel free to uncomment to play with ImGui's features
    //ImGui::ShowDemoWindow();
    //ImGui::ShowStyleEditor(); // Warning, this WILL change your theme. Back up theme.ini first!
#endif

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void GWToolbox::DrawInitialising(IDirect3DDevice9* device) {
    ASSERT(gwtoolbox_state == GWToolboxState::DrawInitialising);

    if(!imgui_inifile.bytes)
        imgui_inifile = Unicode16ToUtf8(Resources::GetSettingFile(L"interface.ini").c_str());

    // Attach WndProc in the render loop to make sure the window is loaded and ready
    ASSERT(AttachWndProcHandler());
    // Attach imgui if not already done so
    ASSERT(AttachImgui(device));

    if (!GuiUtils::FontsLoaded())
        return;

    gwtoolbox_state = GWToolboxState::Initialised;

}
void GWToolbox::UpdateInitialising(float) {
    ASSERT(gwtoolbox_state == GWToolboxState::Initialising);

    Log::Log("Creating Toolbox\n");

    Resources::EnsureFolderExists(Resources::GetComputerFolderPath());
    Resources::EnsureFolderExists(Resources::GetPath(L"img"));
    Resources::EnsureFolderExists(Resources::GetPath(L"location logs"));
    Resources::EnsureFolderExists(Resources::GetPath(L"configs"));

    // if the file does not exist we'll load module settings once downloaded, but we need the file open
    // in order to read defaults
    const auto ini = OpenSettingsFile();

    Log::Log("Creating Modules\n");
    ToggleModule(CrashHandler::Instance());
    ToggleModule(Resources::Instance());
    ToggleModule(ToolboxTheme::Instance());
    ToggleModule(ToolboxSettings::Instance());
    ToggleModule(MainWindow::Instance());
    ToggleModule(DialogModule::Instance());

    ToggleModule(GwDatTextureModule::Instance());
    ToggleModule(Updater::Instance());
    ToggleModule(ChatCommands::Instance());
    ToggleModule(GameSettings::Instance());
    ToggleModule(ChatSettings::Instance());
    ToggleModule(InventoryManager::Instance());
    ToggleModule(HallOfMonumentsModule::Instance());
    ToggleModule(LoginModule::Instance());
    ToggleModule(AprilFools::Instance());
    ToggleModule(SettingsWindow::Instance());

    ToolboxSettings::LoadModules(ini); // initialize all other modules as specified by the user

    if (!greeted && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        const auto* c = GW::GetCharContext();
        if (c && c->player_name) {
            Log::InfoW(L"Hello!");
            greeted = true;
        }
    }

    gwtoolbox_state = GWToolboxState::DrawInitialising;
}

void GWToolbox::UpdateTerminating(float delta_f) {
    ASSERT(gwtoolbox_state == GWToolboxState::Terminating);

    if (all_modules_enabled.size()) {
        SaveSettings();
        while (modules_enabled.size()) {
            ASSERT(ToggleModule(*modules_enabled[0], false) == false);
        }
        while (widgets_enabled.size()) {
            ASSERT(ToggleModule(*widgets_enabled[0], false) == false);
        }
        while (windows_enabled.size()) {
            ASSERT(ToggleModule(*windows_enabled[0], false) == false);
        }
    }
    ASSERT(all_modules_enabled.empty());
    terminate_modules:
    for (const auto m : modules_terminating) {
        if (m->CanTerminate()) {
            m->Terminate();
            const auto found = std::ranges::find(modules_terminating, m);
            ASSERT(found != modules_terminating.end());
            modules_terminating.erase(found);
            goto terminate_modules;
        }
        m->Update(delta_f);
    }
    if (!modules_terminating.empty())
        return;

    ASSERT(DetachWndProcHandler());

    if (!CanTerminate())
        return;

    GW::DisableHooks();

    gwtoolbox_state = GWToolboxState::Terminated;

    if (defer_close) {
        // Toolbox was closed by a user closing GW - close it here for the by sending the `WM_CLOSE` message again.
        SendMessageW(gw_window_handle, WM_CLOSE, NULL, NULL);
    }
}

void GWToolbox::DrawTerminating(IDirect3DDevice9*) {
    ASSERT(gwtoolbox_state == GWToolboxState::DrawTerminating);
    ASSERT(DetachImgui());
    gwtoolbox_state = GWToolboxState::Terminating;
}

