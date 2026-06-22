#include "stdafx.h"

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include <Utf8.h>

#include <GWCA/GWCA.h>
#include <GWCA/GWCAVersion.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/PreGameContext.h>

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <Defender.h>
#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <Utils/TeamBuild.h>

#include <Modules/CameraUnlockModule.h>
#include <Modules/ChatCommands.h>
#include <Modules/ChatSettings.h>
#include <Modules/CrashHandler.h>
#include <Modules/DialogModule.h>
#include <Modules/GameSettings.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/HallOfMonumentsModule.h>
#include <Modules/InventoryManager.h>
#include <Modules/ItemDescriptionHandler.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/ToolboxTheme.h>
#include <Modules/TransmoModule.h>
#include <Modules/Updater.h>
#include <Windows/SettingsWindow.h>

#include <Widgets/Minimap/Minimap.h>
#include <Windows/MainWindow.h>

#include <Utils/TextUtils.h>
#include "Utils/FontLoader.h"

#include <EmbeddedResource.h>
#include "resource.h"

#include <DelayImp.h>

// declare method here as recommended by imgui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    HMODULE gwcamodule = nullptr;
    HMODULE dllmodule = nullptr;
    WNDPROC OldWndProc = nullptr;
    HWND gw_window_handle = nullptr;

    utf8::string imgui_inifile;
    bool imgui_inifile_changed = false;
    bool settings_folder_changed = false;

    bool must_self_destruct = false; // is true when toolbox should quit
    GW::HookEntry Update_Entry;

    bool profiling_enabled = false;

    utf8::string GetImguiIniPath()
    {
        const auto path = Resources::GetSettingFile(L"interface.ini");
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            // Carry the window layout over from the pre-configs/default location
            const auto legacy = Resources::GetLegacySettingFile(L"interface.ini");
            if (std::filesystem::exists(legacy, ec)) {
                Resources::EnsureFolderExists(path.parent_path());
                std::filesystem::copy_file(legacy, path, ec);
            }
        }
        return Unicode16ToUtf8(path.c_str());
    }

    uint64_t QpcToMicroseconds(LONGLONG ticks)
    {
        static LARGE_INTEGER freq = [] {
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            return f;
        }();
        return static_cast<uint64_t>(ticks * 1000000 / freq.QuadPart);
    }
    std::recursive_mutex module_management_mutex;


    GW::UI::UIInteractionCallback OnUiRoot_UICallback_Func = nullptr, OnUiRoot_UICallback_Ret = nullptr;

    // Hook the "Exit Game" button on logout prompt to ensure toolbox closes gracefully
    void OnUiRoot_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) {
            GWToolbox::ForceTerminate();
        }
        OnUiRoot_UICallback_Ret(message, wparam, lparam);
        GW::Hook::LeaveHook();
    }

    bool HookUiRoot()
    {
        if (OnUiRoot_UICallback_Func) return true;
        const auto frame = GW::UI::GetParentFrame(GW::UI::GetFrameByLabel(L"Game"));
        if (!(frame && frame->frame_callbacks.size())) return false;
        OnUiRoot_UICallback_Func = frame->frame_callbacks[0].callback;
        GW::Hook::CreateHook((void**)&OnUiRoot_UICallback_Func, OnUiRoot_UICallback, (void**)&OnUiRoot_UICallback_Ret);
        GW::Hook::EnableHooks(OnUiRoot_UICallback_Func);
        return true;
    }

    bool render_callback_attached = false;

    bool AttachRenderCallback()
    {
        if (!render_callback_attached) {
            GW::Render::SetRenderCallback(GWToolbox::Draw);
            render_callback_attached = true;
        }
        return render_callback_attached;
    }

    bool DetachRenderCallback()
    {
        if (render_callback_attached) {
            GW::Render::SetRenderCallback(nullptr);
            render_callback_attached = false;
        }
        return !render_callback_attached;
    }

    bool game_loop_callback_attached = false;
    GW::HookEntry game_loop_callback_entry;

    bool AttachGameLoopCallback()
    {
        GW::GameThread::EnableHooks();
        if (!game_loop_callback_attached) {
            GW::GameThread::RegisterGameThreadCallback(&game_loop_callback_entry, GWToolbox::Update);
            game_loop_callback_attached = true;
        }
        return game_loop_callback_attached;
    }

    bool DetachGameLoopCallback()
    {
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

        auto& io = ImGui::GetIO();


        io.MouseDrawCursor = false;
        io.IniFilename = imgui_inifile.bytes;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.ConfigNavCaptureKeyboard = false;

        // ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), device);
        ImGui_ImplDX9_Init(device);
        ImGui_ImplWin32_Init(GW::MemoryMgr::GetGWWindowHandle());

        GW::Render::SetResetCallback([](IDirect3DDevice9*) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
        });

        imgui_initialized = true;

        Resources::EnsureFileExists(Resources::GetPath(L"Font.ttf"), "https://raw.githubusercontent.com/gwdevhub/GWToolboxpp/master/resources/Font.ttf", [](const bool success, const std::wstring& error) {
            FontLoader::LoadFonts();
            if (!success) {
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
        FontLoader::Terminate();
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imgui_initialized = false;
        return true;
    }

    // All modules including widgets and windows
    std::vector<ToolboxModule*> modules_enabled{};
    // Widgets
    std::vector<ToolboxWidget*> widgets_enabled{};
    // Windows
    std::vector<ToolboxWindow*> windows_enabled{};
    // Modules that aren't widgets or windows
    std::vector<ToolboxModule*> other_modules_enabled{};

    std::vector<ToolboxUIElement*> ui_elements_enabled{};

    std::vector<ToolboxModule*> modules_terminating{};

    bool minimap_enabled = false;

    bool is_right_clicking = false;
    bool mouse_moved_whilst_right_clicking = false;
    LPARAM right_click_lparam;

    enum class GWToolboxState { Initialising, UpdateInitialising, DrawInitialising, Initialised, DrawTerminating, Terminating, Terminated, Disabled };

    auto gwtoolbox_state = GWToolboxState::Terminated;
    bool gwtoolbox_disabled = false;

    bool pending_detach_dll = false;

    bool greeted = false;

    std::recursive_mutex initialize_mutex;

    void ReorderModules(std::vector<ToolboxModule*>& modules)
    {
        std::ranges::sort(modules, [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
            return std::string(lhs->SettingsName()).compare(rhs->SettingsName()) < 0;
        });
    }

    // Lowercase hex sha256 of a byte buffer; empty on failure.
    std::string Sha256Hex(const void* data, const size_t size)
    {
        unsigned char hash[32];
        if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, static_cast<PUCHAR>(const_cast<void*>(data)),
                       static_cast<ULONG>(size), hash, sizeof(hash)) != 0)
            return {};
        char hex[2 * sizeof(hash) + 1];
        for (size_t i = 0; i < sizeof(hash); ++i)
            sprintf_s(hex + i * 2, 3, "%02x", hash[i]);
        return hex;
    }

    std::string Sha256HexFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return Sha256Hex(bytes.data(), bytes.size());
    }

    bool IsValidGWCADll(const std::filesystem::path& dll_path_str, [[maybe_unused]] const EmbeddedResource& resource_dll)
    {
        if (!std::filesystem::exists(dll_path_str)) return false;

#ifndef _DEBUG
        // The on-disk gwca.dll must be byte-for-byte the one embedded in this build.
        if (!resource_dll.data()) return false;
        const std::string expected = Sha256Hex(resource_dll.data(), resource_dll.size());
        return !expected.empty() && Sha256HexFile(dll_path_str) == expected;
#else
        // Debug builds have no embedded resource; fall back to a version-number check.
        DWORD handle;
        DWORD version_info_size = GetFileVersionInfoSizeW(dll_path_str.c_str(), &handle);
        if (!version_info_size) return false;
        std::vector<BYTE> version_data(version_info_size);
        if (!GetFileVersionInfoW(dll_path_str.c_str(), handle, version_info_size, version_data.data())) return false;
        VS_FIXEDFILEINFO* file_info;
        UINT len;
        if (!VerQueryValueA(version_data.data(), "\\", (LPVOID*)&file_info, &len)) return false;
        WORD file_version_major = HIWORD(file_info->dwFileVersionMS);
        WORD file_version_minor = LOWORD(file_info->dwFileVersionMS);
        WORD file_version_patch = HIWORD(file_info->dwFileVersionLS);
        return (file_version_major == GWCA::VersionMajor && file_version_minor == GWCA::VersionMinor && file_version_patch == GWCA::VersionPatch);
#endif
    }

    void TerminateExistingGWCADll(HMODULE existing_gwca)
    {
        typedef void(__cdecl * GWFnVoid)();
        const auto disable_hooks = (GWFnVoid)GetProcAddress(existing_gwca, "?DisableHooks@GW@@YAXXZ");
        const auto terminate = (GWFnVoid)GetProcAddress(existing_gwca, "?Terminate@GW@@YAXXZ");
        // The old gwca.dll's hooks may point into a previously-unloaded toolbox dll;
        // calling DisableHooks/Terminate can therefore crash. Swallow it via SEH
        // and continue to FreeLibrary.
        if (disable_hooks) {
            Log::Log("[LoadGWCADll] Calling DisableHooks on old gwca.dll");
            __try {
                disable_hooks();
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log::Log("[LoadGWCADll] DisableHooks on old gwca.dll crashed; continuing");
            }
        }
        if (terminate) {
            Log::Log("[LoadGWCADll] Calling Terminate on old gwca.dll");
            __try {
                terminate();
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log::Log("[LoadGWCADll] Terminate on old gwca.dll crashed; continuing");
            }
        }
        Sleep(160);
        // Unload: may need multiple calls if ref count > 1
        while (GetModuleHandleW(L"gwca.dll") == existing_gwca) {
            if (!FreeLibrary(existing_gwca)) {
                Log::Log("[LoadGWCADll] FreeLibrary failed for old gwca.dll: %d", GetLastError());
                break;
            }
        }
    }

    HMODULE LoadGWCADll(HMODULE resource_module)
    {
        if (gwcamodule) return gwcamodule;
        const auto gwca_dll_path = Resources::GetPath("gwca.dll");
        const auto dll_path_str = gwca_dll_path.wstring();

        const EmbeddedResource resource(IDR_GWCA_DLL, RT_RCDATA, resource_module);
        if (!resource.data()) {
            Log::Log("[LoadGWCADll] resource fail, couldn't get dll from resources");
            return nullptr;
        }

        // If a gwca.dll is already loaded that isn't our expected version, terminate and unload it first
        const HMODULE existing_gwca = GetModuleHandleW(L"gwca.dll");
        if (existing_gwca) {
            wchar_t existing_path[MAX_PATH] = {};
            GetModuleFileNameW(existing_gwca, existing_path, MAX_PATH);
            if (_wcsicmp(existing_path, dll_path_str.c_str()) != 0 || !IsValidGWCADll(gwca_dll_path, resource)) {
                Log::Log("[LoadGWCADll] Found existing gwca.dll at %ls; terminating before loading correct version", existing_path);
                TerminateExistingGWCADll(existing_gwca);
            }
        }

        // Defender returns these from CreateFile/LoadLibrary when it blocks or quarantines a file.
        const auto warn_if_antivirus = [](const DWORD err) {
            if (err != ERROR_VIRUS_INFECTED && err != ERROR_VIRUS_DELETED) return;
            std::wstring message =
                L"Windows Defender (or another anti-virus) blocked gwca.dll, which GWToolbox needs to run.\n\n"
                L"Add an exclusion for your GWToolbox folder in Windows Security and re-launch.";
            std::wstring detail;
            if (FindRecentDefenderBlock(L"gwca.dll", 15, detail))
                message += L"\n\nWindows Defender reported:\n" + detail;
            MessageBoxW(nullptr, message.c_str(), L"GWToolbox", MB_OK | MB_ICONERROR | MB_TOPMOST);
        };

        if (!IsValidGWCADll(gwca_dll_path, resource)) {
            std::wstring err;
            if (!Resources::ResourceToFile(IDR_GWCA_DLL, gwca_dll_path, err)) {
                const DWORD code = GetLastError();
                Log::Log("[LoadGWCADll] ResourceToFile fail: %ls (%lu)", err.c_str(), code);
                warn_if_antivirus(code);
            }
            if (!IsValidGWCADll(gwca_dll_path, resource)) {
                Log::Log("[LoadGWCADll] resource fail, GWCA not valid after replacing");
                return nullptr;
            }
        }

        gwcamodule = LoadLibraryW(gwca_dll_path.wstring().c_str());
        if (!gwcamodule) {
            const DWORD code = GetLastError();
            Log::Log("[LoadGWCADll] LoadLibraryW fail, %lu", code);
            warn_if_antivirus(code);
            return nullptr;
        }
        Log::Log("[LoadGWCADll] success, module ptr %p", gwcamodule);
        return gwcamodule;
    }

    bool UnloadGWCADll()
    {
        ASSERT(!gwcamodule || FreeLibrary(gwcamodule));
        return true;
    }

    bool CanRenderToolbox()
    {
        const auto device = GW::Render::GetDevice();
        const HRESULT hr = device ? device->TestCooperativeLevel() : D3DERR_DEVICELOST;
        if (hr != D3D_OK) {
            // Device is lost or not ready - skip all rendering
            return false;
        }
        return !gwtoolbox_disabled && !GW::GetPreGameContext() && !GW::Map::GetIsInCinematic() && !IsIconic(GW::MemoryMgr::GetGWWindowHandle()) &&
               (!ToolboxSettings::hide_on_loading_screen || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) && FontLoader::FontsLoaded();
    }

    bool ToggleTBModule(ToolboxModule& m, std::vector<ToolboxModule*>& vec, const bool enable)
    {
        const auto found = std::ranges::find(vec, &m);
        if (found != vec.end()) {
            // Module found
            if (enable) {
                return true;
            }
            m.SaveSettings(*GWToolbox::GetSettingsDoc());
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
        if (std::ranges::contains(modules_terminating, &m)) {
            return false; // Not finished terminating
        }
        vec.push_back(&m);
        Log::Log("ToggleTBModule: Initializing %s...", m.Name());
        m.Initialize();
        m.LoadSettings(*GWToolbox::GetSettingsDoc(), GWToolbox::OpenSettingsFile());
        Log::Log("ToggleTBModule: Initialised %s !!", m.Name());
        ReorderModules(vec);
        return true; // Added successfully
    }

    void UpdateEnabledWidgetVectors(ToolboxModule* m, bool added)
    {
        const auto found = std::ranges::find(modules_enabled, m);
        if (added) {
            if (found == modules_enabled.end()) {
                modules_enabled.push_back(m);
            }
        }
        else {
            if (found != modules_enabled.end()) {
                modules_enabled.erase(found);
            }
        }

        ui_elements_enabled.clear();
        widgets_enabled.clear();
        windows_enabled.clear();
        other_modules_enabled.clear();
        for (auto module : modules_enabled) {
            if (module->IsWidget()) {
                widgets_enabled.push_back(static_cast<ToolboxWidget*>(module));
                ui_elements_enabled.push_back(static_cast<ToolboxUIElement*>(module));
            }
            else if (module->IsWindow()) {
                windows_enabled.push_back(static_cast<ToolboxWindow*>(module));
                ui_elements_enabled.push_back(static_cast<ToolboxUIElement*>(module));
            }
            else
                other_modules_enabled.push_back(module);
        }
        minimap_enabled = GWToolbox::IsModuleEnabled(&Minimap::Instance());
    }

    LRESULT CALLBACK WndProc(const HWND hWnd, const UINT Message, const WPARAM wParam, const LPARAM lParam)
    {
        static bool right_mouse_down = false;

        if (Message == WM_CLOSE) {
            GWToolbox::ForceTerminate(false);
            return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
        }

        if (!(CanRenderToolbox() && GWToolbox::IsInitialized())) {
            return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
        }

        auto& io = ImGui::GetIO();

        auto& tb = GWToolbox::Instance();

        if (Message == WM_RBUTTONUP) {
            if (right_mouse_down && !mouse_moved_whilst_right_clicking && !io.WantCaptureMouse) {
                // Tell imgui that the mouse cursor is in its original clicked position - GW messes with the cursor in-game
                SendMessage(hWnd, WM_GW_RBUTTONCLICK, 0, right_click_lparam);
            }
            mouse_moved_whilst_right_clicking = false;
            right_mouse_down = false;
        }
        if (Message == WM_RBUTTONDOWN) {
            right_mouse_down = true;
            right_click_lparam = lParam;
            mouse_moved_whilst_right_clicking = false;
        }
        if (Message == WM_RBUTTONDBLCLK) {
            right_mouse_down = true;
        }

        GWToolbox::Instance().right_mouse_down = right_mouse_down;

        // === Send events to ImGui ===

        const bool skip_mouse_capture = right_mouse_down || /* GW::UI::GetIsWorldMapShowing() || */ GW::Map::GetIsInCinematic();
        if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam) && !skip_mouse_capture) return TRUE;

        // === Send events to toolbox ===

        /* GW Deliberately makes a WM_MOUSEMOVE event right after right button is pressed.
            Does this to "hide" the cursor when looking around.

            To easily send a "rmb clicked" event to toolbox modules, figure the logic out ourselves and send a custom message WM_GW_RBUTTONCLICK
         */


        switch (Message) {
            case WM_MOUSELEAVE:
            case WM_NCMOUSELEAVE:
                if (GetCapture() == nullptr && ImGui::IsMouseDown(ImGuiMouseButton_Left)) SetCapture(hWnd);
                break;
            // Send button up mouse events to everything, to avoid being stuck on mouse-down
            case WM_INPUT: {
                if (right_mouse_down && !mouse_moved_whilst_right_clicking && GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT && lParam) {
                    BYTE lpb[128];
                    UINT dwSize = _countof(lpb);
                    ASSERT(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) < dwSize);

                    const RAWINPUT* raw = (RAWINPUT*)lpb;
                    if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0 && raw->data.mouse.lLastX && raw->data.mouse.lLastY) {
                        // If its a relative mouse move, process the action
                        mouse_moved_whilst_right_clicking = true;
                    }
                }

                for (const auto m : tb.GetAllModules()) {
                    m->WndProc(Message, wParam, lParam);
                }
            } break;

            // Other mouse events:
            // - If right mouse down, leave it to gw
            // - ImGui first (above), if WantCaptureMouse that's it
            // - Toolbox module second (e.g.: minimap), if captured, that's it
            // - otherwise pass to gw
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
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
            // if (!skip_mouse_capture) {

            //}
            break;

            // keyboard messages
            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (io.WantCaptureKeyboard || io.WantTextInput) {
                    break; // make sure key up events are passed through to gw, already handled in imgui
                }
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
                if (io.WantCaptureKeyboard || io.WantTextInput) {
                    return true; // if imgui wants them, send just to imgui (above)
                }

                // send input to chat commands for camera movement
                if (CameraUnlockModule::Instance().WndProc(Message, wParam, lParam)) {
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
                if (Message >= 0x8000 && Message <= 0xFFFF) {
                    for (const auto m : tb.GetAllModules()) {
                        m->WndProc(Message, wParam, lParam);
                    }
                }
                break;
        }

        return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
    }

    LRESULT CALLBACK SafeWndProc(const HWND hWnd, const UINT Message, const WPARAM wParam, const LPARAM lParam) noexcept
    {
        __try {
            return WndProc(hWnd, Message, wParam, lParam);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return CallWindowProc(OldWndProc, hWnd, Message, wParam, lParam);
        }
    }

    bool event_handler_attached = false;

    RAWINPUTDEVICE rid[2];
    // RegisterRawInputDevices to be able to receive WM_INPUT via WndProc
    bool RegisterRawInputs(bool enable = true)
    {
        if (!gw_window_handle) return false;

        // Mouse
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x02;
        rid[0].dwFlags = enable ? RIDEV_INPUTSINK : RIDEV_REMOVE;
        rid[0].hwndTarget = enable ? gw_window_handle : nullptr;


        // Keyboard
        rid[1].usUsagePage = 0x01;
        rid[1].usUsage = 0x06;
        rid[1].dwFlags = enable ? RIDEV_INPUTSINK : RIDEV_REMOVE;
        rid[1].hwndTarget = enable ? gw_window_handle : nullptr;

        return RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
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

        // NB: Inputs are being listened to in reforged
        // DEBUG_ASSERT(RegisterRawInputs(true));

        event_handler_attached = true;
        return true;
    }

    bool DetachWndProcHandler()
    {
        if (!event_handler_attached) {
            return true;
        }
        Log::Log("Restoring input hook\n");
        // NB: Don't unregister the raw input - Guild Wars needs it
        // DEBUG_ASSERT(RegisterRawInputs(false));

        SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, reinterpret_cast<LONG>(OldWndProc));
        event_handler_attached = false;
        return true;
    }

    FARPROC WINAPI CustomDliHook(const unsigned dliNotify, PDelayLoadInfo pdli)
    {
        switch (dliNotify) {
            case dliNotePreLoadLibrary: {
                if (_stricmp(pdli->szDll, "gwca.dll") != 0) break;
                const auto loaded = LoadGWCADll(dllmodule);
                ASSERT(loaded);
                return (FARPROC)loaded;
            } break;
            case dliFailGetProc: {
                if (_stricmp(pdli->szDll, "gwca.dll") != 0) break;
                ASSERT(pdli && *(pdli->dlp.szProcName));
                Log::Log("Something is trying to access %ls, but failed", pdli->dlp.szProcName);
                ASSERT(false && "Failed to find a valid proc address in GWCA. Ensure you're running the correct version, or delete gwca.dll in your toolbox folder!");
            } break;
                // Add other sliNotify cases for debugging if you need to later
        }
        return nullptr;
    }
} // namespace

extern const PfnDliHook __pfnDliFailureHook2 = CustomDliHook;
extern const PfnDliHook __pfnDliNotifyHook2 = CustomDliHook;

const std::vector<ToolboxModule*>& GWToolbox::GetAllModules()
{
    return modules_enabled;
}

const std::vector<ToolboxUIElement*>& GWToolbox::GetUIElements()
{
    return ui_elements_enabled;
}

const std::vector<ToolboxModule*>& GWToolbox::GetModules()
{
    return other_modules_enabled;
}

const std::vector<ToolboxWindow*>& GWToolbox::GetWindows()
{
    return windows_enabled;
}

const std::vector<ToolboxWidget*>& GWToolbox::GetWidgets()
{
    return widgets_enabled;
}

void GWToolbox::SetProfilingEnabled(bool enabled)
{
    profiling_enabled = enabled;
}

bool GWToolbox::IsProfilingEnabled()
{
    return profiling_enabled;
}

bool GWToolbox::ShouldDisableToolbox(GW::Constants::MapID map_id)
{
    const auto m = GW::Map::GetMapInfo(map_id);
    return m && (m->GetIsPvP() || m->GetIsGuildHall());
}

bool GWToolbox::IsInitialized()
{
    return gwtoolbox_state == GWToolboxState::Initialised;
}

bool GWToolbox::ToggleModule(ToolboxModule& m, const bool enable)
{
    std::lock_guard lock(module_management_mutex);
    if (IsModuleEnabled(&m) == enable) return enable;
    const bool added = ToggleTBModule(m, modules_enabled, enable);
    UpdateEnabledWidgetVectors(&m, added);
    return added;
}

HMODULE GWToolbox::GetDLLModule()
{
    return dllmodule;
}

DWORD __stdcall GWToolbox::MainLoop(LPVOID module) noexcept
{
    dllmodule = static_cast<HMODULE>(module);
    __try {
        Initialize(module);
        while (gwtoolbox_state != GWToolboxState::Terminated) {
            // wait until destruction
            Sleep(100);

            // Feel free to uncomment to get this behavior for testing, but don't commit.
            // #ifdef _DEBUG
            //        if (GetAsyncKeyState(VK_END) & 1) {
            //            GWToolbox::Instance().StartSelfDestruct();
            //        }
            // #endif
        }

        // @Remark:
        // Hooks are disable from Guild Wars thread (safely), so we just make sure we exit the last hooks
        GW::DisableHooks();
        while (GW::Hook::GetInHookCount()) {
            Sleep(16);
        }

        // @Remark:
        // We can't guarantee that the code in Guild Wars thread isn't still in the trampoline, but
        // practically a short sleep is fine.
        Sleep(16);

        Log::Log("Destroying API\n");
        Log::Log("Closing log/console, bye!\n");
        Log::Terminate();
        GW::Terminate();

        Sleep(160);

        UnloadGWCADll();
    } __except (EXCEPT_EXPRESSION_ENTRY) {
        Log::Log("SafeThreadEntry __except body\n");
    }
    return EXIT_SUCCESS;
}

void GWToolbox::Initialize(LPVOID module)
{
    std::lock_guard<std::recursive_mutex> lock(initialize_mutex);
    if (module) {
        dllmodule = static_cast<HMODULE>(module);
    }
    if (gwtoolbox_state != GWToolboxState::Terminated) return;
    gwtoolbox_state = GWToolboxState::Initialising;

    Log::InitializeLog();

    // @Cleanup: atm its just an ASSERT - this could be a valid issue where the user can't write gwca.dll to disk, so need to handle it better later.
    ASSERT(LoadGWCADll(dllmodule));

    Log::InitializeGWCALog();
    GW::RegisterPanicHandler(CrashHandler::GWCAPanicHandler, nullptr);
    GW::Initialize();
    Log::InitializeChat();

    HookUiRoot();
    AttachRenderCallback();
    GW::EnableHooks();

    UpdateInitialising(.0f);
    AttachGameLoopCallback();
    pending_detach_dll = false;
}

std::filesystem::path GWToolbox::LoadSettings()
{
    const auto ini = OpenSettingsFile();
    const auto doc = GetSettingsDoc();
    // Reset the flag so nested OpenSettingsFile() calls (via ToggleTBModule) don't
    // free and reallocate the ini we just loaded, causing a use-after-free.
    settings_folder_changed = false;
    ToolboxSettings::Instance().LoadSettings(*doc, ini);
    ToolboxSettings::LoadModules(ini);
    if (!ini->location_on_disk.empty()) {
        // Loaded sequentially on purpose, not threaded: a fixed load order keeps config loading deterministic, so a shared settings folder always reproduces the same behaviour and bugs aren't order-dependent. The ~100ms cost is negligible; revisit only if it ever causes noticeable lag.
        for (const auto m : modules_enabled) {
            m->LoadSettings(*doc, ini);
        }
        for (const auto m : widgets_enabled) {
            m->LoadSettings(*doc, ini);
        }
        for (const auto m : windows_enabled) {
            m->LoadSettings(*doc, ini);
        }
    }
    return doc->location_on_disk;
}

bool GWToolbox::SetSettingsFolder(const std::filesystem::path& path)
{
    static auto last_path = std::filesystem::path{};
    if (last_path != path) {
        if (Resources::SetSettingsFolder(path)) {
            imgui_inifile = GetImguiIniPath();
            settings_folder_changed = true;
            imgui_inifile_changed = true;
            last_path = path;
            return true;
        }
        return false;
    }
    return true;
}

bool GWToolbox::IsModuleEnabled(ToolboxModule* m)
{
    std::lock_guard<std::recursive_mutex> lock(module_management_mutex);
    return m && std::ranges::find(modules_enabled, m) != modules_enabled.end();
}

bool GWToolbox::IsModuleEnabled(const char* name)
{
    std::lock_guard<std::recursive_mutex> lock(module_management_mutex);
    return name && std::ranges::find_if(modules_enabled, [name](ToolboxModule* m) {
                       return strcmp(m->Name(), name) == 0;
                   }) != modules_enabled.end();
}

bool GWToolbox::SettingsFolderChanged()
{
    return settings_folder_changed;
}

ToolboxIni* GWToolbox::OpenSettingsFile(bool fresh)
{
    static ToolboxIni* inifile = nullptr;
    const auto full_path = Resources::GetLegacySettingFile(GWTOOLBOX_INI_FILENAME);
    if (!SettingsFolderChanged() && inifile && !fresh) {
        return inifile;
    }
    auto tmp = new ToolboxIni(false, false, false);
    ASSERT(tmp->LoadIfExists(full_path) == SI_OK);
    tmp->location_on_disk = full_path;
    if (inifile) delete inifile;
    inifile = tmp;
    return inifile;
}

SettingsDoc* GWToolbox::GetSettingsDoc(bool fresh)
{
    static SettingsDoc* doc = nullptr;
    const auto modules_path = Resources::GetSettingFile(GWTOOLBOX_MODULES_FOLDERNAME);
    if (!SettingsFolderChanged() && doc && !fresh) {
        return doc;
    }
    auto tmp = new SettingsDoc();
    tmp->LoadFolder(modules_path);
    if (tmp->Empty()) {
        // One-time seed: pre-split installs kept all sections in a single GWToolbox.json
        auto seed_path = Resources::GetSettingFile(GWTOOLBOX_JSON_FILENAME);
        std::error_code ec;
        if (!std::filesystem::exists(seed_path, ec)) {
            seed_path = Resources::GetLegacySettingFile(GWTOOLBOX_JSON_FILENAME);
        }
        if (std::filesystem::exists(seed_path, ec)) {
            tmp->LoadFile(seed_path);
            tmp->location_on_disk = modules_path;
        }
    }
    if (doc) delete doc;
    doc = tmp;
    return doc;
}

std::filesystem::path GWToolbox::SaveSettings()
{
    const auto ini = OpenSettingsFile();
    const auto doc = GetSettingsDoc(true);
    for (const auto m : modules_enabled) {
        m->SaveSettings(*doc);
    }
    for (const auto m : widgets_enabled) {
        m->SaveSettings(*doc);
    }
    for (const auto m : windows_enabled) {
        m->SaveSettings(*doc);
    }
    ToolboxSettings::LoadModules(ini);
    ASSERT(doc->SaveFolder());
    if (ImGui::GetCurrentContext()) {
        auto& io = ImGui::GetIO();
        if (io.IniFilename) {
            ImGui::SaveIniSettingsToDisk(io.IniFilename);
        }
    }
    const auto dir = doc->location_on_disk.parent_path();
    const auto dirstr = dir.wstring();
    const auto printable = TextUtils::str_replace_all(dirstr, LR"(\\)", L"/");
    Log::LogW(L"Toolbox settings saved to %s", printable.c_str());
    settings_folder_changed = false;
    return doc->location_on_disk;
}

void GWToolbox::ForceTerminate(bool detach_wndproc_handler)
{
    if (gwtoolbox_state == GWToolboxState::Terminated) return;
    ASSERT(DetachGameLoopCallback());
    ASSERT(!detach_wndproc_handler || DetachWndProcHandler());

    SignalTerminate();
    DrawTerminating(nullptr);
    UpdateTerminating(0.f, true);

    GW::DisableHooks();

    gwtoolbox_state = GWToolboxState::Terminated;
}

void GWToolbox::SignalTerminate(bool detach_dll)
{
    switch (gwtoolbox_state) {
        case GWToolboxState::Disabled:
        case GWToolboxState::Terminating:
        case GWToolboxState::Initialised:
        case GWToolboxState::Initialising:
        case GWToolboxState::DrawInitialising:
            gwtoolbox_state = GWToolboxState::DrawTerminating;
            AttachGameLoopCallback();
            AttachRenderCallback();
            pending_detach_dll = detach_dll;
    }
}

void GWToolbox::Enable()
{
    if (!gwtoolbox_disabled) return;
    GW::EnableHooks();
    gwtoolbox_disabled = false;
}

void GWToolbox::Disable()
{
    if (gwtoolbox_disabled) return;
    GW::DisableHooks();
    GW::Render::EnableHooks();
    if (OnUiRoot_UICallback_Func) GW::Hook::EnableHooks(OnUiRoot_UICallback_Func);
    AttachRenderCallback();
    gwtoolbox_disabled = true;
}

bool GWToolbox::CanTerminate()
{
    return modules_terminating.empty() && FontLoader::FontsLoaded() && modules_enabled.empty() && !imgui_initialized && !event_handler_attached;
}

void GWToolbox::Update(GW::HookStatus*)
{
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

    UpdateModulesTerminating(delta_f);

    Build::Update();

    // Update loop
    for (const auto m : modules_enabled) {
        if (profiling_enabled) {
            LARGE_INTEGER t0, t1;
            QueryPerformanceCounter(&t0);
            m->Update(delta_f);
            QueryPerformanceCounter(&t1);
            m->last_update_time_us_ = QpcToMicroseconds(t1.QuadPart - t0.QuadPart);
        }
        else {
            m->Update(delta_f);
        }
    }

    if (!greeted && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        const auto* c = GW::GetCharContext();
        if (c && c->player_name) {
            Log::Flash("Hello!");
            greeted = true;

            // we wants here to alert the player if he inits GWToolbox++ in a PvP area that it's disabled
            // check here if the player is in a PvP area
            if (ShouldDisableToolbox()) {
                GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA2, L"<c=#FF0000>GWToolbox++ is disabled in PvP areas. (It includes Guild Halls)</c>", GWTOOLBOX_SENDER, true);
                GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_WARNING, L"Toolbox is disabled in PvP areas.", nullptr, true);
            }
        }
    }

    last_tick_count = tick;
}

void GWToolbox::Draw(IDirect3DDevice9* device)
{
    HookUiRoot();
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
    if (ShouldDisableToolbox()) {
        Disable();
        return;
    }

    // Draw loop
    Resources::DxUpdate(device);

    if (!CanRenderToolbox()) return;

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();

    const bool world_map_showing = GW::UI::GetIsWorldMapShowing();

    if (!world_map_showing && GW::UI::GetIsUIDrawn()) {
        GameWorldRenderer::Render(device);
    }

    ImGui::NewFrame();

    // Key up/down events don't get passed to gw window when out of focus, but we need the following to be correct,
    // or things like alt-tab make imgui think that alt is still down.
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_LeftCtrl, (GetKeyState(VK_LCONTROL) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiKey_LeftShift, (GetKeyState(VK_LSHIFT) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiKey_LeftAlt, (GetKeyState(VK_LMENU) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiKey_RightCtrl, (GetKeyState(VK_RCONTROL) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiKey_RightShift, (GetKeyState(VK_RSHIFT) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiKey_RightAlt, (GetKeyState(VK_RMENU) & 0x8000) != 0);

    io.AddKeyEvent(ImGuiMod_Ctrl, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (GetKeyState(VK_MENU) & 0x8000) != 0);

    if (GW::UI::GetIsUIDrawn()) {
        ToolboxUIElement::UpdateCachedFrameStates();
        std::lock_guard lock(module_management_mutex);
        // NB: Don't use an iterator here, because it could be invalidated during draw
        for (size_t i = 0; i < ui_elements_enabled.size(); i++) {
            const auto uielement = ui_elements_enabled[i];
            if (world_map_showing && !uielement->ShowOnWorldMap()) {
                continue;
            }
            uielement->UpdateLocationAgainstSnappedFrame();
            uielement->DrawBreakoutButton(device);
            if (profiling_enabled) {
                LARGE_INTEGER t0, t1;
                QueryPerformanceCounter(&t0);
                uielement->Draw(device);
                QueryPerformanceCounter(&t1);
                uielement->last_draw_time_us_ = QpcToMicroseconds(t1.QuadPart - t0.QuadPart);
            }
            else {
                uielement->Draw(device);
            }
        }

        // Non-UI modules have no window of their own but may still paint an overlay
        // this frame (e.g. Texmod's recording banner).
        for (size_t i = 0; i < other_modules_enabled.size(); i++) {
            other_modules_enabled[i]->Draw(device);
        }

#ifdef _DEBUG
        // Feel free to uncomment to play with ImGui's features
        // ImGui::ShowDemoWindow();
        // ImGui::ShowStyleEditor(); // Warning, this WILL change your theme. Back up theme.ini first!
#endif
        ToolboxSettings::DrawSettingsCogButtons();
        ImGui::DrawContextMenu();
        ImGui::DrawConfirmDialog();
        if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0) ImGui::ClampAllWindowsToScreen(gwtoolbox_state < GWToolboxState::DrawTerminating && ToolboxSettings::clamp_windows_to_screen);
    }
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    // The Toolbox windows are now drawn into the back buffer; if the user
    // clicked a title-bar camera button last frame, this is where we
    // actually read the pixels out and save them to disk.
    ToolboxSettings::FlushPendingScreenshot(device);

    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        // TODO for OpenGL: restore current GL context.
    }
}

void GWToolbox::DrawInitialising(IDirect3DDevice9* device)
{
    ASSERT(gwtoolbox_state == GWToolboxState::DrawInitialising);

    if (!imgui_inifile.bytes) imgui_inifile = GetImguiIniPath();

    // Attach WndProc in the render loop to make sure the window is loaded and ready
    ASSERT(AttachWndProcHandler());
    // Attach imgui if not already done so
    ASSERT(AttachImgui(device));

    if (!FontLoader::FontsLoaded()) {
        Resources::Instance().Update(0.f); // necessary, because this won't be called in
        Resources::DxUpdate(device);
        return; // GWToolbox::Update() until fonts are initialised
    }

    gwtoolbox_state = GWToolboxState::Initialised;
}

void GWToolbox::UpdateInitialising(float)
{
    ASSERT(gwtoolbox_state == GWToolboxState::Initialising);

    Log::Log("Creating Toolbox\n");

    Resources::EnsureFolderExists(Resources::GetComputerFolderPath());
    Resources::EnsureFolderExists(Resources::GetPath(L"img"));
    Resources::EnsureFolderExists(Resources::GetPath(L"location logs"));
    Resources::EnsureFolderExists(Resources::GetSettingsFolderPath());

    // if the file does not exist we'll load module settings once downloaded, but we need the file open
    // in order to read defaults
    const auto ini = OpenSettingsFile();

    Log::Log("Creating Modules\n");
    ToggleModule(CrashHandler::Instance());
    ToggleModule(Resources::Instance());
    ToggleModule(ToolboxTheme::Instance());
    ToggleModule(ItemDescriptionHandler::Instance());
    ToggleModule(ToolboxSettings::Instance());
    ToggleModule(MainWindow::Instance());
    ToggleModule(DialogModule::Instance());

    ToggleModule(GwDatTextureModule::Instance());
    ToggleModule(Updater::Instance());
    ToggleModule(ChatCommands::Instance());
    ToggleModule(TransmoModule::Instance());
    ToggleModule(GameSettings::Instance());
    ToggleModule(ChatSettings::Instance());
    ToggleModule(InventoryManager::Instance());
    ToggleModule(HallOfMonumentsModule::Instance());
    ToggleModule(SettingsWindow::Instance());

    ToolboxSettings::LoadModules(ini); // initialize all other modules as specified by the user

    gwtoolbox_state = GWToolboxState::DrawInitialising;
}

void GWToolbox::UpdateModulesTerminating(float delta_f, bool panicking)
{
terminate_modules:
    for (const auto m : modules_terminating) {
        if (m->CanTerminate() || panicking) {
            m->Terminate();
            const auto found = std::ranges::find(modules_terminating, m);
            ASSERT(found != modules_terminating.end());
            modules_terminating.erase(found);
            goto terminate_modules;
        }
        m->Update(delta_f);
    }
}

void GWToolbox::UpdateTerminating(float delta_f, bool panicking)
{
    ASSERT(gwtoolbox_state == GWToolboxState::Terminating);

    while (modules_enabled.size()) {
        ASSERT(ToggleModule(*modules_enabled[0], false) == false);
    }
    ASSERT(modules_enabled.empty());
    UpdateModulesTerminating(delta_f, panicking);
    if (!modules_terminating.empty()) return;

    ASSERT(DetachWndProcHandler());

    if (!CanTerminate()) return;

    GW::DisableHooks();

    gwtoolbox_state = GWToolboxState::Terminated;
}

void GWToolbox::DrawTerminating(IDirect3DDevice9*)
{
    ASSERT(gwtoolbox_state == GWToolboxState::DrawTerminating);
    // Save settings on the draw loop otherwise theme won't be saved
    SaveSettings();
    ASSERT(DetachImgui());
    gwtoolbox_state = GWToolboxState::Terminating;
}
