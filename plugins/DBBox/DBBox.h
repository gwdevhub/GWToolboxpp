#pragma once

#include <ToolboxPlugin.h>

#include <atomic>
#include <functional>

class DBBox final : public ToolboxPlugin {
public:
    DBBox();
    ~DBBox() override = default;

    [[nodiscard]] const char* Name() const override { return "DBBox"; }
    [[nodiscard]] const char* Icon() const override;
    [[nodiscard]] bool HasSettings() const override { return true; }
    [[nodiscard]] bool ShowOnWorldMap() const override { return true; }
    [[nodiscard]] ToolboxPlugin* GetChildPlugin(size_t index);

    void Initialize(ImGuiContext* context, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* device) override;
    bool WndProc(UINT message, WPARAM wparam, LPARAM lparam) override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;
    void DrawSettings() override;

private:
    enum class FeatureState {
        Stopped,
        Running,
        Stopping,
    };

    struct Feature {
        const char* id = nullptr;
        const char* label = nullptr;
        std::function<std::unique_ptr<ToolboxPlugin>()> factory;
        bool enabled = true;
        FeatureState state = FeatureState::Stopped;
        std::unique_ptr<ToolboxPlugin> instance;
        std::shared_ptr<std::atomic_bool> stop_barrier;
    };

    void StartFeature(Feature& feature);
    void StopFeature(Feature& feature, bool save_settings = true);
    void DrainStoppingFeatures();
    [[nodiscard]] static const char* StateLabel(FeatureState state);

    std::vector<Feature> features_;
    std::filesystem::path settings_folder_;
    ImGuiContext* context_ = nullptr;
    ImGuiAllocFns allocator_fns_{};
    HMODULE toolbox_dll_ = nullptr;
    bool initialized_ = false;
    bool settings_loaded_ = false;
    bool shutting_down_ = false;
};
