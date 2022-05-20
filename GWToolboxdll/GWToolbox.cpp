#include "stdafx.h"

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/GWCA.h>

#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <GWCA/Utilities/Scanner.h>

#include <CursorFix.h>
#include <d3dx9_dynamic.h>
#include <Defines.h>
#include <GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Modules/ChatCommands.h>
#include <Modules/GameSettings.h>
#include <Modules/ToolboxTheme.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/CrashHandler.h>
#include <Windows/MainWindow.h>
#include <Widgets/Minimap/Minimap.h>

namespace {
    HMODULE dllmodule = 0;

    long OldWndProc = 0;
    bool tb_destroyed = false;
    bool imgui_initialized = false;

    bool drawing_world = 0;
    int drawing_passes = 0;
    int last_drawing_passes = 0;

    bool defer_close = false;

    static HWND gw_window_handle = 0;
    bool SaveIniToFile(CSimpleIni* ini, std::filesystem::path location) {
        std::filesystem::path tmpFile = location;
        tmpFile += ".tmp";
        SI_Error res = ini->SaveFile(tmpFile.c_str());
        if (res < 0) {
            return false;
        }
        std::filesystem::rename(tmpFile, location);
        return true;
    }
}

HMODULE GWToolbox::GetDLLModule() {
    return dllmodule;
}

DWORD __stdcall SafeThreadEntry(LPVOID module) {
    dllmodule = (HMODULE)module;
    __try {
        ThreadEntry(nullptr);
    } __except ( EXCEPT_EXPRESSION_ENTRY ) {
        Log::Log("SafeThreadEntry __except body\n");
    }
    return EXIT_SUCCESS;
}

DWORD __stdcall ThreadEntry(LPVOID) {
    Log::Log("Initializing API\n");

    // Try to load DirectX runtime dll. Installer should have sorted this, but may not have.
    if (!Loadd3dx9()) {
        // Handle this now before we go any further - removing this check will cause a crash when modules try to use D3DX9 funcs in Draw() later and will close GW
        char title[128];
        sprintf(title, "GWToolbox++ API Error (LastError: %lu)", GetLastError());
        if (MessageBoxA(0, 
            "Failed to load d3dx9_xx.dll; this machine may not have DirectX runtime installed.\nGWToolbox++ needs this installed to continue.\n\nVisit DirectX Redistributable download page?", 
            title, MB_YESNO) == IDYES) {
            ShellExecute(0, 0, DIRECTX_REDIST_WEBSITE, 0, 0, SW_SHOW);
        }
    
        goto leave;
    }

    GW::HookBase::Initialize();
    if (!GW::Initialize()){
        if (MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0) == IDOK) {
            
        }
        goto leave;
    }

    Log::Log("Installing Cursor Fix\n");

    InstallCursorFix();

    Log::Log("Installing dx hooks\n");
    GW::Render::SetRenderCallback([](IDirect3DDevice9* device) {
        __try {
            GWToolbox::Instance().Draw(device);
        } __except ( EXCEPT_EXPRESSION_ENTRY ) {
        }
    });
    GW::Render::SetResetCallback([](IDirect3DDevice9* device) {
        UNREFERENCED_PARAMETER(device);
        ImGui_ImplDX9_InvalidateDeviceObjects();
    });


    Log::Log("Installed dx hooks\n");

    Log::InitializeChat();

    Log::Log("Installed chat hooks\n");

    GW::HookBase::EnableHooks();

    Log::Log("Hooks Enabled!\n");

    GW::GameThread::Enqueue([]() {
        GWToolbox::Instance().Initialize();
        });

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
        return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    static bool right_mouse_down = false;

    if (Message == WM_CLOSE) {
        // This is naughty, but we need to defer the closing signal until toolbox has terminated properly.
        // we can't sleep here, because toolbox modules will probably be using the render loop to close off things like hooks
        GWToolbox::Instance().StartSelfDestruct();
        defer_close = true;
        return 0;
    }

    if (!(!GW::PreGameContext::instance() && imgui_initialized && GWToolbox::Instance().IsInitialized() && !tb_destroyed)) {
        return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
    }



    if (Message == WM_RBUTTONUP) right_mouse_down = false;
    if (Message == WM_RBUTTONDOWN) right_mouse_down = true;
    if (Message == WM_RBUTTONDBLCLK) right_mouse_down = true;

    GWToolbox::Instance().right_mouse_down = right_mouse_down;

    bool skip_mouse_capture = right_mouse_down || GW::UI::GetIsWorldMapShowing();



    // === Send events to ImGui ===
    ImGuiIO& io = ImGui::GetIO();

    switch (Message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        if (!skip_mouse_capture) io.MouseDown[0] = true;
        break;
    case WM_LBUTTONUP:
        io.MouseDown[0] = false; 
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        if (!skip_mouse_capture) {
            io.KeysDown[VK_MBUTTON] = true;
            io.MouseDown[2] = true;
        }
        break;
    case WM_MBUTTONUP:
        io.KeysDown[VK_MBUTTON] = false;
        io.MouseDown[2] = false;
        break;
    case WM_MOUSEWHEEL: 
        if (!skip_mouse_capture) io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
        break;
    case WM_MOUSEMOVE:
        if (!skip_mouse_capture) {
            io.MousePos.x = (float)GET_X_LPARAM(lParam);
            io.MousePos.y = (float)GET_Y_LPARAM(lParam);
        }
        break;
    case WM_XBUTTONDOWN:
        if (!skip_mouse_capture) {
            if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) io.KeysDown[VK_XBUTTON1] = true;
            if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) io.KeysDown[VK_XBUTTON2] = true;
        }
        break;
    case WM_XBUTTONUP:
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) io.KeysDown[VK_XBUTTON1] = false;
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) io.KeysDown[VK_XBUTTON2] = false;
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        if (wParam < 256)
            io.KeysDown[wParam] = true;
        break;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        if (wParam < 256)
            io.KeysDown[wParam] = false;
        break;
    case WM_CHAR: // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if (wParam > 0 && wParam < 0x10000)
            io.AddInputCharacter((unsigned short)wParam);
        break;
    default:
        break;
    }

    

    // === Send events to toolbox ===
    GWToolbox& tb = GWToolbox::Instance();
    switch (Message) {
    // Send button up mouse events to everything, to avoid being stuck on mouse-down
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
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
        if (io.WantCaptureMouse && !skip_mouse_capture) return true;
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
    
    return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}

void GWToolbox::Initialize() {
    if (initialized || must_self_destruct)
        return;
    Log::Log("installing event handler\n");
    gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
    OldWndProc = SetWindowLongPtrW(gw_window_handle, GWL_WNDPROC, (long)SafeWndProc);
    Log::Log("Installed input event handler, oldwndproc = 0x%X\n", OldWndProc);

    imgui_inifile = Resources::GetPathUtf8(L"interface.ini");

    Log::Log("Creating Toolbox\n");

    GW::GameThread::RegisterGameThreadCallback(&Update_Entry, GWToolbox::Update);

    Resources::Instance().EnsureFolderExists(Resources::GetSettingsFolderPath());
    Resources::Instance().EnsureFolderExists(Resources::GetPath(L"img"));
    Resources::Instance().EnsureFolderExists(Resources::GetPath(L"img\\bonds"));
    Resources::Instance().EnsureFolderExists(Resources::GetPath(L"img\\icons"));
    Resources::Instance().EnsureFolderExists(Resources::GetPath(L"img\\materials"));
    Resources::Instance().EnsureFolderExists(Resources::GetPath(L"img\\pcons"));
    Resources::Instance().EnsureFolderExists(Resources::GetPath(L"location logs"));
    Resources::Instance().EnsureFileExists(Resources::GetPath(L"GWToolbox.ini"),
        "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/GWToolbox.ini",
        [](bool success, const std::wstring& error) {
        if (success) {
            GWToolbox::Instance().OpenSettingsFile();
            GWToolbox::Instance().LoadModuleSettings();
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

    for (ToolboxModule* module : core_modules) {
        module->LoadSettings(inifile);
        module->Initialize();
    }

    ToolboxSettings::Instance().LoadModules(inifile); // initialize all other modules as specified by the user

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        auto* g = GW::GameContext::instance();
        if(g && g->character && g->character->player_name)
            Log::InfoW(L"Hello!");
    }
    initialized = true;
}
void GWToolbox::FlashWindow() {
    FLASHWINFO flashInfo = { 0 };
    flashInfo.cbSize = sizeof(FLASHWINFO);
    flashInfo.hwnd = GW::MemoryMgr::GetGWWindowHandle();
    flashInfo.dwFlags = FLASHW_TIMER | FLASHW_TRAY | FLASHW_TIMERNOFG;
    flashInfo.uCount = 0;
    flashInfo.dwTimeout = 0;
    FlashWindowEx(&flashInfo);
}

void GWToolbox::OpenSettingsFile() {
    Log::Log("Opening ini file\n");
    if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
    inifile->Reset();
    inifile->LoadFile(Resources::GetPath(L"GWToolbox.ini").c_str());
}
void GWToolbox::LoadModuleSettings() {
    for (ToolboxModule* module : modules) {
        module->LoadSettings(inifile);
    }
}
void GWToolbox::SaveSettings() {
    for (ToolboxModule* module : modules) {
        module->SaveSettings(inifile);
    }
    if (inifile) {
        ASSERT(SaveIniToFile(inifile, Resources::GetPath(L"GWToolbox.ini")));
    }
}


void GWToolbox::Terminate() {
    if (!initialized)
        return;
    SaveSettings();
    inifile->Reset();
    delete inifile;



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
    auto& instance = GWToolbox::Instance();
    if (instance.initialized && instance.must_self_destruct) {
        if (!GuiUtils::FontsLoaded())
            return;
        for (ToolboxModule* module : GWToolbox::Instance().modules) {
            if (!module->CanTerminate())
                return;
        }

        GWToolbox::Instance().Terminate();
        if (imgui_initialized) {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            imgui_initialized = false;
        }


        Log::Log("Restoring input hook\n");
        SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);

        GW::DisableHooks();
        instance.initialized = false;
        tb_destroyed = true;
    }
    // === runtime ===
    if (instance.initialized
        && !instance.must_self_destruct
        && GW::Render::GetViewportWidth() > 0
        && GW::Render::GetViewportHeight() > 0) {

        if (!imgui_initialized) {
            ImGui::CreateContext();
            //ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), device);
            ImGui_ImplDX9_Init(device);
            ImGui_ImplWin32_Init(GW::MemoryMgr().GetGWWindowHandle());

            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = false;
            io.IniFilename = GWToolbox::Instance().imgui_inifile.bytes;

            Resources::Instance().EnsureFileExists(Resources::GetPath(L"Font.ttf"),
                "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/Font.ttf",
                [](bool success, const std::wstring& error) {
                    if (success) {
                        GuiUtils::LoadFonts();
                    }
                    else {
                        Log::ErrorW(L"Cannot load font!\n%s",error.c_str());
                    }
                });

            imgui_initialized = true;
        }

        if (!GW::UI::GetIsUIDrawn())
            return;

        bool world_map_showing = GW::UI::GetIsWorldMapShowing();

        if (GW::PreGameContext::instance())
            return; // Login screen

        if (GW::Map::GetIsInCinematic())
            return;

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
        ImGui::GetIO().KeysDown[VK_CONTROL] = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        ImGui::GetIO().KeysDown[VK_SHIFT] = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        ImGui::GetIO().KeysDown[VK_MENU] = (GetKeyState(VK_MENU) & 0x8000) != 0;
        
        for (ToolboxUIElement* uielement : GWToolbox::Instance().uielements) {
            if (world_map_showing && !uielement->ShowOnWorldMap())
                continue;
            uielement->Draw(device);
        }
        //for (TBModule* mod : GWToolbox::Instance().plugins) {
        //    mod->Draw(device);
        //}

#ifdef _DEBUG
        // Feel free to uncomment to play with ImGui's features
        //ImGui::ShowDemoWindow();
        //ImGui::ShowStyleEditor(); // Warning, this WILL change your theme. Back up theme.ini first!
#endif

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }


    if(tb_destroyed && defer_close) {
        // Toolbox was closed by a user closing GW - close it here for the by sending the `WM_CLOSE` message again.
        SendMessageW(gw_window_handle, WM_CLOSE, NULL, NULL);
    }
}

void GWToolbox::Update(GW::HookStatus *)
{
    static DWORD last_tick_count;
    if (last_tick_count == 0)
        last_tick_count = GetTickCount();

    GWToolbox& tb = GWToolbox::Instance();
    if (!tb.initialized)
        tb.Initialize();
    if (tb.initialized
        && imgui_initialized
        && !GWToolbox::Instance().must_self_destruct) {

        // @Enhancement:
        // Improve precision with QueryPerformanceCounter
        DWORD tick = GetTickCount();
        DWORD delta = tick - last_tick_count;
        float delta_f = delta / 1000.f;

        for (ToolboxModule* module : tb.modules) {
            module->Update(delta_f);
        }
        //for (TBModule* module : tb.plugins) {
        //    module->Update(delta_f);
        //}

        last_tick_count = tick;
    }
}
