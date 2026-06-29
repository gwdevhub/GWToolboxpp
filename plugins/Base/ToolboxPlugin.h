#pragma once

#include "stl.h"
#include <ToolboxIni.h>
#include <Utils/SettingsDoc.h>

#ifndef DLLAPI
#ifdef BUILD_DLL
#define DLLAPI extern "C" __declspec(dllexport)
#else
#define DLLAPI
#endif
#endif

struct IDirect3DDevice9;

struct ImGuiContext;
using ImGuiMemAllocFunc = void* (*)(size_t sz, void* user_data);
using ImGuiMemFreeFunc = void(*)(void* ptr, void* user_data);

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
class ToolboxPlugin;          // Full declaration below.
DLLAPI ToolboxPlugin* ToolboxPluginInstance();

class ToolboxPlugin {
public:
    ToolboxPlugin() = default;
    virtual ~ToolboxPlugin() = default;
    ToolboxPlugin(ToolboxPlugin&&) = delete;
    ToolboxPlugin(const ToolboxPlugin&) = delete;
    ToolboxPlugin& operator=(ToolboxPlugin&&) = delete;
    ToolboxPlugin& operator=(const ToolboxPlugin&) = delete;

    // name of the window and the settings section
    [[nodiscard]] virtual const char* Name() const = 0;

    [[nodiscard]] virtual const char* Icon() const { return nullptr; }

    [[nodiscard]] virtual bool HasSettings() const { return false; }

    // return a pointer to a bool that will be used to show/hide the window
    // if you wish to draw, you should consider inheriting from ToolboxUIPlugin.
    [[nodiscard]] virtual bool* GetVisiblePtr() { return nullptr; }

    // if a hide/close entry for this plugin should be added to the main menu
    [[nodiscard]] virtual bool ShowInMainMenu() const { return false; }

    // do we want to draw while the world map is showing?
    [[nodiscard]] virtual bool ShowOnWorldMap() const { return false; }

    // JSON settings file for this plugin: <folder>/<Name>.json
    [[nodiscard]] virtual std::filesystem::path GetSettingFile(const wchar_t* folder) const;

    // Pre-JSON settings file (<folder>/<Name>.ini), only read as a legacy fallback - never written.
    [[nodiscard]] virtual std::filesystem::path GetLegacySettingFile(const wchar_t* folder) const;

    // Initialize module
    virtual void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll);

    // Send termination signal to module, make sure Terminate can be called.
    virtual void SignalTerminate() {}

    // Can we terminate this module?
    virtual bool CanTerminate() { return true; }

    // Terminate module. Release any resources used. Make sure to revert all callbacks
    virtual void Terminate() {}

    // Update. Will be called once every frame unless the world map is showing and ShowInWorldMap returns false.
    // Delta is in milliseconds.
    virtual void Update(float) {}

    // Draw. Will always be called once every frame.
    virtual void Draw(IDirect3DDevice9*) {}

    // Optional. Prefer using ImGui::GetIO() during update or render, if possible.
    virtual bool WndProc(UINT, WPARAM, LPARAM) { return false; }

    // Called by GWToolbox when you need to (re)load any settings; the suitable settings folder is given.
    // The base implementation loads GetSettingFile() into `settings` and GetLegacySettingFile() into
    // `legacy_ini` (read-only). Override, call the base first, then read your values with LoadSetting().
    virtual void LoadSettings(const wchar_t* folder);

    // Called by GWToolbox when you need to (re)save any settings; the suitable settings folder is given.
    // The base implementation writes `settings` to GetSettingFile(). Override, stage your values with
    // SaveSetting(), then call the base last. The legacy ini file is never written.
    virtual void SaveSettings(const wchar_t* folder);

    // Will be drawn in the Settings/Plugins menu. Must use ImGui.
    virtual void DrawSettings() {}

    // Will be used to draw an open/close button in the main window.
    virtual bool DrawTabButton(bool, bool, bool) { return false; }

protected:
    // Reads `key` from this plugin's JSON settings doc, falling back to the legacy ini value.
    template <typename T>
    void LoadSetting(const char* key, T& value)
    {
        if (settings.Get(Name(), key, value)) {
            return;
        }
        if (!legacy_ini.KeyExists(Name(), key)) {
            return;
        }
        if constexpr (std::same_as<T, bool>) {
            value = legacy_ini.GetBoolValue(Name(), key, value);
        }
        else if constexpr (std::floating_point<T>) {
            value = static_cast<T>(legacy_ini.GetDoubleValue(Name(), key, static_cast<double>(value)));
        }
        else if constexpr (std::integral<T> || std::is_enum_v<T>) {
            value = static_cast<T>(legacy_ini.GetLongValue(Name(), key, static_cast<long>(value)));
        }
        else if constexpr (std::same_as<T, std::string>) {
            value = std::string(legacy_ini.GetValue(Name(), key, ""));
        }
        // other types have no legacy ini representation; JSON only
    }

    // Stages `key` into this plugin's JSON settings doc; written to disk by ToolboxPlugin::SaveSettings.
    template <typename T>
    void SaveSetting(const char* key, const T& value)
    {
        settings.Set(Name(), key, value);
    }

    HMODULE toolbox_handle = nullptr;
    SettingsDoc settings;
    ToolboxIni legacy_ini;
};
