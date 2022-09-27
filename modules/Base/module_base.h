#pragma once

#include <Windows.h>

#ifndef DLLAPI
#ifdef BUILD_DLL
#define DLLAPI extern "C" __declspec(dllexport)
#else
#define DLLAPI
#endif
#endif

struct ImGuiContext;
struct IDirect3DDevice9;

namespace ImGui {
    void SetCurrentContext(ImGuiContext*);
}

//
// Dll interface.
//
extern HMODULE plugin_handle; // set in dllmain
class ToolboxPlugin; // Full declaration below.
DLLAPI ToolboxPlugin* ToolboxPluginInstance();

class ToolboxPlugin {
public:
    ToolboxPlugin() = default;
    virtual ~ToolboxPlugin() = default;
    ToolboxPlugin(ToolboxPlugin&&) = delete;
    ToolboxPlugin(const ToolboxPlugin&) = delete;
    ToolboxPlugin& operator=(ToolboxPlugin&&) = delete;
    ToolboxPlugin& operator=(const ToolboxPlugin&) = delete;

    // name of the window and the ini section
    virtual const char* Name() const = 0;

    // Initialize module
    virtual void Initialize(ImGuiContext* ctx, HMODULE toolbox_dll)
    {
        if (ctx) ImGui::SetCurrentContext(ctx);
        toolbox_handle = toolbox_dll;
    }

    // Send termination signal to module.
    virtual void SignalTerminate() {}

    // Can we terminate this module?
    virtual bool CanTerminate() { return true; }

    // Terminate module. Release any resources used.
    virtual void Terminate() {}

    // Update. Will always be called once every frame. Delta is in seconds.
    virtual void Update(float) {}

    // Draw. Will always be called once every frame.
    virtual void Draw(IDirect3DDevice9*) {}

    // Optional. Prefer using ImGui::GetIO() during update or render, if possible.
    virtual bool WndProc(UINT, WPARAM, LPARAM) { return false; }

    // Load settings from ini
    virtual void LoadSettings() {}

    // Save settings to ini
    virtual void SaveSettings() {}

    // Draw settings.
    virtual void DrawSettings() {}

protected:
    HMODULE plugin_handle = nullptr;
    HMODULE toolbox_handle = nullptr;
};
