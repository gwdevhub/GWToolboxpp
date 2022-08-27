#pragma once

typedef std::function<void(const std::string* section, bool is_showing)> SectionDrawCallback;
typedef std::vector<std::pair<float,SectionDrawCallback>> SectionDrawCallbackList;

class ToolboxModule {
protected:
    ToolboxModule() = default;
public:
    // name of the window and the ini section
    virtual const char* Name() const = 0;

    // Icon for this module (if any).
    virtual const char8_t* Icon() const { return nullptr; }

    // name of the setting section
    virtual const char* SettingsName() const { return Name(); }

    // Type of module
    virtual const char* TypeName() const { return "module"; }

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
    virtual void SignalTerminate() {}

    // Draw help section
    virtual void DrawHelp() {}

    // Can we terminate this module?
    virtual bool CanTerminate() { return true; }

    // Does this module have settings?
    virtual bool HasSettings() { return true; }

    // Terminate module
    virtual void Terminate() {}

    // Update. Will always be called once every frame. Delta in seconds
    virtual void Update(float) {}

    // This is provided (and called), but use ImGui::GetIO() during update/render if possible.
    virtual bool WndProc(UINT, WPARAM, LPARAM) { return false; }

    // Load what is needed from ini
    virtual void LoadSettings(CSimpleIni*) {}

    // Save what is needed to ini
    virtual void SaveSettings(CSimpleIni*) {}

    // Draw settings interface. Will be called if the setting panel is visible, calls DrawSettingsInternal()
    //virtual void DrawSettings();
    virtual void DrawSettingInternal() {}

    // Register settings content
    static void RegisterSettingsContent(
        const char* section, const char8_t* icon, const SectionDrawCallback& callback, float weighting);

protected:
    // Weighting used to decide where to position the DrawSettingInternal() for this module. Useful when more than 1 module has the same SettingsName().
    virtual float SettingsWeighting() { return 1.0f; }
};
