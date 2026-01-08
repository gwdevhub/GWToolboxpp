#pragma once

#include <GWCA/Managers/GameThreadMgr.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include "Windows/HeroBuildsWindow.h"

class ToolboxWidget;
class ToolboxWindow;
class ToolboxModule;

namespace GW {
    namespace Constants {
        enum class MapID : uint32_t;
    }
}

class GWToolbox {
public:
    static GWToolbox& Instance()
    {
        static GWToolbox instance;
        return instance;
    }
    static bool ShouldDisableToolbox(GW::Constants::MapID = (GW::Constants::MapID)0);

    static HMODULE GetDLLModule();
    static DWORD __stdcall MainLoop(LPVOID module) noexcept;

    static void Draw(IDirect3DDevice9* device);

    static void Initialize(LPVOID);
    static void SignalTerminate(bool detach_dll = true);
    static void Update(GW::HookStatus* status = nullptr);
    static void Enable();
    static void Disable();
    static bool CanTerminate();

    static ToolboxIni* OpenSettingsFile();
    static std::filesystem::path SaveSettings();
    static void ForceTerminate(bool detach_wndproc_handler = true);
    static std::filesystem::path LoadSettings();
    static bool SetSettingsFolder(const std::filesystem::path& path);

    static bool IsModuleEnabled(ToolboxModule* m);
    static bool IsModuleEnabled(const char* name);

    //const std::vector<ToolboxModule*>& GetModules();
    static const std::vector<ToolboxModule*>& GetAllModules();
    //const std::vector<ToolboxModule*>& GetCoreModules() const { return core_modules; }
    static const std::vector<ToolboxUIElement*>& GetUIElements();
    // Get modules that are currently enabled, that are not windows or widgets
    static const std::vector<ToolboxModule*>& GetModules();

    static const std::vector<ToolboxWindow*>& GetWindows();

    static const std::vector<ToolboxWidget*>& GetWidgets();
    static bool SettingsFolderChanged();

    bool right_mouse_down = false;

    static bool IsInitialized();

    static bool ToggleModule(ToolboxModule& m, bool enable = true);

private:
    static void DrawInitialising(IDirect3DDevice9* device);
    static void DrawTerminating(IDirect3DDevice9* device);
    static void UpdateInitialising(float);
    static void UpdateModulesTerminating(float);
    static void UpdateTerminating(float);
};
