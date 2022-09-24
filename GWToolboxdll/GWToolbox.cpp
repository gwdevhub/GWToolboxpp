#include "stdafx.h"

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <CursorFix.h>
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

#include <Windows/MainWindow.h>
#include <Widgets/Minimap/Minimap.h>
#include <GWCA/Context/GameContext.h>

// declare method here as recommended by imgui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    HMODULE dllmodule = nullptr;
    WNDPROC OldWndProc = nullptr;
    bool tb_destroyed = false;
    bool defer_close = false;
    HWND gw_window_handle = nullptr;

    utf8::string imgui_inifile;
    CSimpleIni* inifile = nullptr;
    bool SaveIniToFile(const CSimpleIni* ini, const std::filesystem::path& location) {
        auto tmp_file = std::filesystem::path(location);
        tmp_file += ".tmp";
        const SI_Error res = ini->SaveFile(tmp_file.c_str());
        if (res < 0) {
            return false;
        }
        std::filesystem::rename(tmp_file, location);
        return true;
    }

    bool event_handler_attached = false;
    bool AttachWndProcHandler() {
        if (event_handler_attached)
            return true;
        Log::Log("installing event handler\n");
        gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
        OldWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(gw_window_handle, GWL_WNDPROC, reinterpret_cast<LONG>(SafeWndProc)));
        Log::Log("Installed input event handler, oldwndproc = 0x%X\n", OldWndProc);
        event_handler_attached = true;
        return true;
    }
    bool DetachWndProcHandler() {
        if (!event_handler_attached)
            return true;
        Log::Log("Restoring input hook\n");
        SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, reinterpret_cast<LONG>(OldWndProc));
        event_handler_attached = false;
        return true;
    }

    bool imgui_initialized = false;
    bool AttachImgui(IDirect3DDevice9* device) {
        if (imgui_initialized)
            return true;
        ImGui::CreateContext();
        //ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), device);
        ImGui_ImplDX9_Init(device);
        ImGui_ImplWin32_Init(GW::MemoryMgr::GetGWWindowHandle());

        GW::Render::SetResetCallback([](IDirect3DDevice9* device) {
            UNREFERENCED_PARAMETER(device);
            ImGui_ImplDX9_InvalidateDeviceObjects();
            });

        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = false;
        io.IniFilename = imgui_inifile.bytes;

        Resources::EnsureFileExists(Resources::GetPath(L"Font.ttf"),
            "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/Font.ttf",
            [](bool success, const std::wstring& error) {
            if (success) {
                GuiUtils::LoadFonts();
            }
            else {
                Log::ErrorW(L"Cannot download font, please download it manually!\n%s", error.c_str());
            }
        });
        imgui_initialized = true;
        return true;
    }
    bool DetachImgui() {
        if (!imgui_initialized)
            return true;
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imgui_initialized = false;
        return true;
    }
}

HMODULE GWToolbox::GetDLLModule() {
    return dllmodule;
}

DWORD __stdcall SafeThreadEntry(LPVOID module) noexcept {
    dllmodule = static_cast<HMODULE>(module);
    __try {
        ThreadEntry(nullptr);
    } __except ( EXCEPT_EXPRESSION_ENTRY ) {
        Log::Log("SafeThreadEntry __except body\n");
    }
    return EXIT_SUCCESS;
}

DWORD __stdcall ThreadEntry(LPVOID) {
    Log::Log("Initializing API\n");

    GW::HookBase::Initialize();
    if (!GW::Initialize()){
        if (MessageBoxA(nullptr, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0) == IDOK) {

        }
        goto leave;
    }

    Log::Log("Installing Cursor Fix\n");

    InstallCursorFix();

    Log::Log("Installing dx hooks\n");

    // Some modules rely on the gwdx_ptr being present for stuff like getting viewport coords.
    // Becuase this ptr isn't set until the Render loop runs at least once, let it run and then reassign SetRenderCallback.
    GWToolbox::Instance().Initialize();

    Log::Log("Installed dx hooks\n");

    Log::InitializeChat();

    Log::Log("Installed chat hooks\n");

    GW::HookBase::EnableHooks();

    Log::Log("Hooks Enabled!\n");



    while (!tb_destroyed) { // wait until destruction
        Sleep(100);

        // Feel free to uncomment to get this behavior for testing, but don't commit.
//#ifdef _DEBUG
//        if (GetAsyncKeyState(VK_END) & 1) {
//            GWToolbox::Instance().StartSelfDestruct();
//        }
//#endif
    }

    Log::Log("Removing Cursor Fix\n");
    UninstallCursorFix();

    // @Remark:
    // Hooks are disable from Guild Wars thread (safely), so we just make sure we exit the last hooks
    while (GW::HookBase::GetInHookCount())
        Sleep(16);

    // @Remark:
    // We can't guarantee that the code in Guild Wars thread isn't still in the trampoline, but
    // practically a short sleep is fine.
    Sleep(16);
leave:
    Log::Log("Destroying API\n");
    GW::Terminate();

    Log::Log("Closing log/console, bye!\n");
    Log::Terminate();

    FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
}

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) noexcept {
    __try {
        return WndProc(hWnd, Message, wParam, lParam);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    static bool right_mouse_down = false;

    if (Message == WM_CLOSE || Message == WM_SYSCOMMAND && wParam == SC_CLOSE) {
        // This is naughty, but we need to defer the closing signal until toolbox has terminated properly.
        // we can't sleep here, because toolbox modules will probably be using the render loop to close off things
        // like hooks
        GWToolbox::Instance().StartSelfDestruct();
        defer_close = true;
        return 0;
    }

    if (!(!GW::PreGameContext::instance() && imgui_initialized && GWToolbox::Instance().IsInitialized() && !tb_destroyed)) {
        return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
    }

    if (Message == WM_RBUTTONUP) right_mouse_down = false;
    if (Message == WM_RBUTTONDOWN) right_mouse_down = true;
    if (Message == WM_RBUTTONDBLCLK) right_mouse_down = true;

    GWToolbox::Instance().right_mouse_down = right_mouse_down;

    // === Send events to ImGui ===
    const ImGuiIO& io = ImGui::GetIO();
    const bool skip_mouse_capture = right_mouse_down || GW::UI::GetIsWorldMapShowing();
    if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam) && !skip_mouse_capture) {
        return TRUE;
    }


    // === Send events to toolbox ===
    const GWToolbox& tb = GWToolbox::Instance();
    switch (Message) {
    // Send button up mouse events to everything, to avoid being stuck on mouse-down
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_INPUT:
        for (ToolboxModule* m : tb.GetModules()) {
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
        if (io.WantCaptureMouse && !skip_mouse_capture)
            return true;
        bool captured = false;
        for (ToolboxModule* m : tb.GetModules()) {
            if (m->WndProc(Message, wParam, lParam)) captured = true;
        }
        if (captured)
            return true;
    }
        //if (!skip_mouse_capture) {

        //}
        break;

    // keyboard messages
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (io.WantTextInput) break; // if imgui wants them, send to imgui (above) and to gw
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
        if (io.WantTextInput) return true; // if imgui wants them, send just to imgui (above)

        // send input to chat commands for camera movement
        if (ChatCommands::Instance().WndProc(Message, wParam, lParam)) {
            return true;
        }

        // send to toolbox modules
        {
            bool captured = false;
            for (ToolboxModule* m : tb.GetModules()) {
                if (m->WndProc(Message, wParam, lParam)) captured = true;
            }
            if (captured) return true;
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
            for (ToolboxModule* m : tb.GetModules()) {
                m->WndProc(Message, wParam, lParam);
            }
        }
        break;
    }

    return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
}

void GWToolbox::Initialize()
{
    if (initialized || must_self_destruct)
        return;

    imgui_inifile = Resources::GetPathUtf8(L"interface.ini");

    Log::Log("Creating Toolbox\n");

    GW::GameThread::RegisterGameThreadCallback(&Update_Entry, [](GW::HookStatus* a) { GWToolbox::Instance().Update(a); });

    Resources::EnsureFolderExists(Resources::GetSettingsFolderPath());
    Resources::EnsureFolderExists(Resources::GetPath(L"img"));
    Resources::EnsureFolderExists(Resources::GetPath(L"img\\bonds"));
    Resources::EnsureFolderExists(Resources::GetPath(L"img\\icons"));
    Resources::EnsureFolderExists(Resources::GetPath(L"img\\materials"));
    Resources::EnsureFolderExists(Resources::GetPath(L"img\\pcons"));
    Resources::EnsureFolderExists(Resources::GetPath(L"location logs"));
    Resources::EnsureFileExists(Resources::GetPath(L"GWToolbox.ini"),
        "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/GWToolbox.ini",
        [this](bool success, const std::wstring& error) {
        if (success) {
            OpenSettingsFile();
            LoadModuleSettings();
        }
        else {
            Log::ErrorW(L"Failed to download GWToolbox ini\n%s", error.c_str());
        }
    });

    // if the file does not exist we'll load module settings once downloaded, but we need the file open
    // in order to read defaults
    OpenSettingsFile();

    Log::Log("Creating Modules\n");
    core_modules.push_back(&CrashHandler::Instance());
    core_modules.push_back(&Resources::Instance());
    core_modules.push_back(&ToolboxTheme::Instance());
    core_modules.push_back(&ToolboxSettings::Instance());
    core_modules.push_back(&MainWindow::Instance());
    core_modules.push_back(&DialogModule::Instance());

    plugin_manager.RefreshDlls();

    for (ToolboxModule* module : core_modules) {
        module->LoadSettings(inifile);
        module->Initialize();
    }

    ToolboxSettings::Instance().LoadModules(inifile); // initialize all other modules as specified by the user

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        const auto* c = GW::CharContext::instance();
        if(c && c->player_name)
            Log::InfoW(L"Hello!");
    }
    GW::Render::SetRenderCallback([](IDirect3DDevice9* device) {
        __try {
            Instance().Draw(device);
        }
        __except (EXCEPT_EXPRESSION_ENTRY) {
        }
        });

    initialized = true;
}

void GWToolbox::OpenSettingsFile() const
{
    Log::Log("Opening ini file\n");
    if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
    inifile->Reset();
    inifile->LoadFile(Resources::GetPath(L"GWToolbox.ini").c_str());
}
void GWToolbox::LoadModuleSettings() const
{
    for (ToolboxModule* module : modules) {
        module->LoadSettings(inifile);
    }
}
void GWToolbox::SaveSettings() const
{
    if (!inifile)
        return;
    for (ToolboxModule* module : modules) {
        module->SaveSettings(inifile);
    }
    ASSERT(SaveIniToFile(inifile, Resources::GetPath(L"GWToolbox.ini")));
}

void GWToolbox::Terminate() {
    if (!initialized)
        return;
    if (inifile) {
        SaveSettings();
        inifile->Reset();
        delete inifile;
        inifile = nullptr;
    }


    GW::GameThread::RemoveGameThreadCallback(&Update_Entry);

    for (ToolboxModule* module : modules) {
        module->Terminate();
    }

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        Log::Info("Bye!");
    }
}

void GWToolbox::Draw(IDirect3DDevice9* device) {
    // === destruction ===
    if (initialized && must_self_destruct) {
        if (!GuiUtils::FontsLoaded())
            return;
        for (ToolboxModule* module : GWToolbox::Instance().modules) {
            if (!module->CanTerminate())
                return;
        }

        Instance().Terminate();
        ASSERT(DetachImgui());
        ASSERT(DetachWndProcHandler());

        GW::DisableHooks();
        initialized = false;
        tb_destroyed = true;
    }
    // === runtime ===
    if (initialized
        && !must_self_destruct
        && GW::Render::GetViewportWidth() > 0
        && GW::Render::GetViewportHeight() > 0) {
        // Attach WndProc in the render loop to make sure the window is loaded and ready
        ASSERT(AttachWndProcHandler());
        // Attach imgui if not already done so
        ASSERT(AttachImgui(device));

        if (!GW::UI::GetIsUIDrawn())
            return;

        const bool world_map_showing = GW::UI::GetIsWorldMapShowing();

        if (GW::PreGameContext::instance())
            return; // Login screen
#ifndef _DEBUG
        if (GW::Map::GetIsInCinematic())
            return;
#endif

        if (IsIconic(GW::MemoryMgr::GetGWWindowHandle()))
            return;

        if (!GuiUtils::FontsLoaded())
            return; // Fonts not loaded yet.

        Resources::Instance().DxUpdate(device);

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();

        if(!world_map_showing)
            Minimap::Render(device);

        ImGui::NewFrame();

        // Key up/down events don't get passed to gw window when out of focus, but we need the following to be correct,
        // or things like alt-tab make imgui think that alt is still down.
        auto& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_ModCtrl, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiKey_ModShift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiKey_ModAlt, (GetKeyState(VK_MENU) & 0x8000) != 0);

        for (ToolboxUIElement* uielement : GWToolbox::Instance().uielements) {
            if (world_map_showing && !uielement->ShowOnWorldMap())
                continue;
            uielement->Draw(device);
        }
        for (TBModule* mod : GWToolbox::Instance().plugins) {
            mod->Draw(device);
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


    if (tb_destroyed && defer_close) {
        // Toolbox was closed by a user closing GW - close it here for the by sending the `WM_CLOSE` message again.
        SendMessageW(gw_window_handle, WM_CLOSE, NULL, NULL);
    }
}

void GWToolbox::Update(GW::HookStatus *)
{
    static DWORD last_tick_count;
    if (last_tick_count == 0)
        last_tick_count = GetTickCount();

    if (initialized
        && imgui_initialized
        && !must_self_destruct) {

        // @Enhancement:
        // Improve precision with QueryPerformanceCounter
        DWORD tick = GetTickCount();
        DWORD delta = tick - last_tick_count;
        float delta_f = delta / 1000.f;

        for (ToolboxModule* module : modules) {
            module->Update(delta_f);
        }

        last_tick_count = tick;
    }
}
