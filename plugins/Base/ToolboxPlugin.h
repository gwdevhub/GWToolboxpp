#pragma once

#include <Windows.h>

#ifndef DLLAPI
#ifdef BUILD_DLL
#define DLLAPI extern "C" __declspec(dllexport)
#else
#define DLLAPI
#endif
#endif

struct IDirect3DDevice9;

struct ImGuiContext;
typedef void* (*ImGuiMemAllocFunc)(size_t sz, void* user_data);
typedef void (*ImGuiMemFreeFunc)(void* ptr, void* user_data);

namespace ImGui {
    void SetCurrentContext(ImGuiContext*);
    void SetAllocatorFunctions(ImGuiMemAllocFunc, ImGuiMemFreeFunc, void*);
}

struct ImGuiAllocFns {
    ImGuiMemAllocFunc alloc_func = nullptr;
    ImGuiMemFreeFunc free_func = nullptr;
    void* user_data = nullptr;
};

//
// Dll interface.
//
inline HMODULE plugin_handle; // set in dllmain
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
    [[nodiscard]] virtual const char* Name() const = 0;

    [[nodiscard]] virtual const char* Icon() const { return nullptr; }

    [[nodiscard]] virtual bool HasSettings() const { return true; }

    // Initialize module
    virtual void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll)
    {
        ImGui::SetCurrentContext(ctx);
        ImGui::SetAllocatorFunctions(allocator_fns.alloc_func, allocator_fns.free_func, allocator_fns.user_data);
        toolbox_handle = toolbox_dll;
    }

    // Send termination signal to module, make sure Terminate can be called.
    virtual void SignalTerminate() {}

    // Can we terminate this module?
    virtual bool CanTerminate() { return true; }

    // Terminate module. Release any resources used. Make sure to revert all callbacks
    virtual void Terminate() {}

    // Update. Will always be called once every frame. Delta is in milliseconds.
    virtual void Update(float) {}

    // Draw. Will always be called once every frame.
    virtual void Draw(IDirect3DDevice9*) {}

    // Optional. Prefer using ImGui::GetIO() during update or render, if possible.
    virtual bool WndProc(UINT, WPARAM, LPARAM) { return false; }

    // Load settings from folder
    virtual void LoadSettings(const wchar_t*) {}

    // Save settings from folder
    virtual void SaveSettings(const wchar_t*) {}

    // Will be drawn in the Settings/Plugins menu. Must use ImGui
    virtual void DrawSettings() {}

protected:
    HMODULE toolbox_handle = nullptr;
};
