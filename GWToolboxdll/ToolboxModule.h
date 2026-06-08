#pragma once

#include <unordered_map>

#include <GWCA/Managers/UIMgr.h>

using SectionDrawCallback = std::function<void(const std::string& section, bool is_showing)>;
class ToolboxModule;
class ToolboxIni;
struct IDirect3DDevice9;

struct SectionDrawCallbackInfo {
    float weighting{};
    SectionDrawCallback callback;
    ToolboxModule* module{};
};

using SectionDrawCallbackList = std::vector<SectionDrawCallbackInfo>;

class ToolboxModule {
protected:
    ToolboxModule() = default;
    virtual ~ToolboxModule() = default;

public:
    [[nodiscard]] virtual bool IsWidget() const { return false; }
    [[nodiscard]] virtual bool IsWindow() const { return false; }
    [[nodiscard]] virtual bool IsUIElement() const { return false; }

    // name of the window and the ini section
    [[nodiscard]] virtual const char* Name() const = 0;

    // something to make sense of this module to actual human beings that don't have time to read source code
    [[nodiscard]] virtual const char* Description() const { return nullptr; }

    // Icon for this module (if any).
    [[nodiscard]] virtual const char* Icon() const { return nullptr; }

    // name of the setting section
    [[nodiscard]] virtual const char* SettingsName() const { return Name(); }

    // Type of module
    [[nodiscard]] virtual const char* TypeName() const { return "module"; }

    // register settings callbacks. Override this to add your settings into different sections.
    virtual void RegisterSettingsContent();

    // Readable array of setting callbacks registered.
    static const std::unordered_map<std::string, SectionDrawCallbackList>& GetSettingsCallbacks();
    static const std::unordered_map<std::string, const char*>& GetSettingsIcons();

    // Readable array of modules currently loaded
    static const std::unordered_map<std::string, ToolboxModule*>& GetModulesLoaded();

    // Initialize module
    virtual void Initialize();

    // Send termination signal to module.
    virtual void SignalTerminate() { }

    // Draw help section
    virtual void DrawHelp() { }

    // Can we terminate this module?
    virtual bool CanTerminate() { return true; }

    // Does this module have settings?
    virtual bool HasSettings() { return true; }

    // Terminate module
    virtual void Terminate();

    // Update. Will always be called once every frame. Delta in seconds
    virtual void Update(float) { }

    // Called once per render frame, inside the ImGui frame, for every enabled module.
    // UI elements override this to draw their window; a plain module can use it to
    // paint an overlay (e.g. on the background draw list), which it otherwise can't
    // do from Update() (that runs on the game thread, outside the ImGui frame).
    virtual void Draw(IDirect3DDevice9*) { }

    // This is provided (and called), but use ImGui::GetIO() during update/render if possible.
    virtual bool WndProc(UINT, WPARAM, LPARAM) { return false; }

    // Load what is needed from ini
    virtual void LoadSettings(ToolboxIni*) { }

    // Save what is needed to ini
    virtual void SaveSettings(ToolboxIni*) { }

    // Draw settings interface. Will be called if the setting panel is visible, calls DrawSettingsInternal()
    //virtual void DrawSettings();
    virtual void DrawSettingsInternal() { }

    // Register settings content
    void RegisterSettingsContent(
        const char* section, const char* icon, const SectionDrawCallback& callback, float weighting);

    uint64_t last_update_time_us_ = 0;
    uint64_t last_draw_time_us_ = 0;
    mutable std::unordered_map<uint32_t, uint64_t> last_ui_message_times_us_;

    // Instrumented wrapper: times each invocation and records per-message-ID into last_ui_message_times_us_
    void RegisterUIMessageCallback(GW::HookEntry* entry, GW::UI::UIMessage message_id,
                                   const GW::UI::UIMessageCallback& callback, int altitude = -0x8000);

protected:
    // Weighting used to decide where to position the DrawSettingInternal() for this module. Useful when more than 1 module has the same SettingsName().
    virtual float SettingsWeighting() { return 1.0f; }
};
