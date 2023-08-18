#pragma once

#include <GWCA/Managers/GameThreadMgr.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include "Windows/HeroBuildsWindow.h"

DWORD __stdcall SafeThreadEntry(LPVOID mod) noexcept;
DWORD __stdcall ThreadEntry(LPVOID);

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) noexcept;
LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

class ToolboxWidget;
class ToolboxWindow;
class ToolboxModule;

class GWToolbox {
public:
    static GWToolbox& Instance()
    {
        static GWToolbox instance;
        return instance;
    }

    static HMODULE GetDLLModule();
    static void Draw(IDirect3DDevice9* device);
    static void Update(GW::HookStatus*);

    static void Initialize();
    static void Terminate();

    static bool CanTerminate();

    static std::filesystem::path SaveSettings();
    static std::filesystem::path LoadSettings();
    static bool SetSettingsFolder(const std::filesystem::path& path);

    static void StartSelfDestruct();


    //const std::vector<ToolboxModule*>& GetModules();
    static const std::vector<ToolboxModule*>& GetAllModules();
    //const std::vector<ToolboxModule*>& GetCoreModules() const { return core_modules; }
    static const std::vector<ToolboxUIElement*>& GetUIElements();

    static const std::vector<ToolboxModule*>& GetModules();

    static const std::vector<ToolboxWindow*>& GetWindows();

    static const std::vector<ToolboxWidget*>& GetWidgets();
    static bool SettingsFolderChanged();

    bool right_mouse_down = false;

    static bool IsInitialized();

    static bool ToggleModule(ToolboxWidget& m, bool enable = true);
    static bool ToggleModule(ToolboxWindow& m, bool enable = true);
    static bool ToggleModule(ToolboxModule& m, bool enable = true);
};
