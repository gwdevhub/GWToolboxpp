#pragma once

#include <GWCA/Managers/GameThreadMgr.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

DWORD __stdcall SafeThreadEntry(LPVOID mod) noexcept;
DWORD __stdcall ThreadEntry(LPVOID);

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) noexcept;
LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

class ToolboxWidget;
class ToolboxWindow;
class ToolboxModule;

class GWToolbox {
public:
    static GWToolbox& Instance() {
        static GWToolbox instance;
        return instance;
    }

    static HMODULE GetDLLModule();
    void Draw(IDirect3DDevice9* device);
    void Update(GW::HookStatus *);

    void Initialize();
    void Terminate();

    bool CanTerminate();

    std::filesystem::path SaveSettings(const std::filesystem::path& config = GWTOOLBOX_INI_FILENAME) const;
    std::filesystem::path LoadSettings(const std::filesystem::path& config = GWTOOLBOX_INI_FILENAME, bool fresh = false);

    void StartSelfDestruct();



    //const std::vector<ToolboxModule*>& GetModules();
    const std::vector<ToolboxModule*>& GetAllModules();
    //const std::vector<ToolboxModule*>& GetCoreModules() const { return core_modules; }
    const std::vector<ToolboxUIElement*>& GetUIElements();

    const std::vector<ToolboxModule*>& GetModules();

    const std::vector<ToolboxWindow*>& GetWindows();

    const std::vector<ToolboxWidget*>& GetWidgets();

    bool right_mouse_down = false;

    bool IsInitialized() const;

    static bool ToggleModule(ToolboxWidget& m, bool enable = true);
    static bool ToggleModule(ToolboxWindow& m, bool enable = true);
    static bool ToggleModule(ToolboxModule& m, bool enable = true);
};
