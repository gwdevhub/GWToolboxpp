#include "DBBox.h"

#include <AsyncStringDecoder.h>
#include <Rendering.h>

#include <AgentPopTimer.h>
#include <ArmorSwap.h>
#include <ChestOpener.h>
#include <DeathPenaltyTimer.h>
#include <DhuumCalculator.h>
#include <Dialogs.h>
#include <HeartbeatPlugin.h>
#include <LootNotifier.h>
#include <PartyReorder.h>
#include <PitsSoulsWindow.h>
#include <ProjectileIndicator.h>
#include <ShadowstepPredictor.h>
#include <SkinChanger.h>
#include <Slowload.h>
#include <SpeedrunScriptingTools.h>
#include <TargetDetector.h>
#include <TrackerAdvancedPlugin.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <IconsFontAwesome5.h>
#include <imgui.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static DBBox instance;
    return &instance;
}

DLLAPI ToolboxPlugin* ToolboxPluginChildInstance(const size_t index)
{
    return static_cast<DBBox*>(ToolboxPluginInstance())->GetChildPlugin(index);
}

DBBox::DBBox()
{
    features_.reserve(17);
    features_.push_back({"AgentPopTimer", "Agent Pop Timer", [] { return std::make_unique<AgentPopTimer>(); }});
    features_.push_back({"ArmorSwap", "Armor Swap", [] { return std::make_unique<ArmorSwap::Plugin>(); }});
    features_.push_back({"ChestOpener", "Chest Opener", [] { return std::make_unique<ChestOpener>(); }});
    features_.push_back({"DeathPenaltyTimer", "Death Penalty Timer", [] { return std::make_unique<DeathPenaltyTimer>(); }});
    features_.push_back({"DhuumCalculator", "Dhuum Calculator", [] { return std::make_unique<DhuumCalculator>(); }});
    features_.push_back({"Dialogs", "Dialogs", [] { return std::make_unique<Dialogs>(); }});
    features_.push_back({"HeartbeatPlugin", "Heartbeat Timer", [] { return std::make_unique<HeartbeatPlugin>(); }});
    features_.push_back({"LootNotifier", "Loot Notifier", [] { return std::make_unique<LootNotifier>(); }});
    features_.push_back({"PartyReorder", "Party Reorder", [] { return std::make_unique<PartyReorder>(); }});
    features_.push_back({"PitsSoulsWindow", "Pits and Souls", [] { return std::make_unique<PitsSoulsWindow>(); }});
    features_.push_back({"ProjectileIndicator", "Projectile Indicator", [] { return std::make_unique<ProjectileIndicator>(); }});
    features_.push_back({"ShadowstepPredictor", "Shadowstep Predictor", [] { return std::make_unique<ShadowstepPredictor>(); }});
    features_.push_back({"SkinChanger", "Skin Changer", [] { return std::make_unique<SkinChanger>(); }});
    features_.push_back({"Slowload", "Slowload", [] { return std::make_unique<Slowload>(); }});
    features_.push_back({"SpeedrunScriptingTools", "Speedrun Scripting Tools", [] { return std::make_unique<SpeedrunScriptingTools>(); }});
    features_.push_back({"TargetDetector", "Target Detector", [] { return std::make_unique<TargetDetector>(); }});
    features_.push_back({"TrackerAdvanced", "Tracker Advanced", [] { return std::make_unique<TrackerAdvanced::TrackerAdvancedPlugin>(); }});
}

const char* DBBox::Icon() const
{
    return ICON_FA_TOOLBOX;
}

ToolboxPlugin* DBBox::GetChildPlugin(const size_t index)
{
    auto child_index = size_t{0};
    for (auto& feature : features_) {
        if (feature.state == FeatureState::Running && feature.instance) {
            if (child_index++ == index) {
                return feature.instance.get();
            }
        }
    }
    return nullptr;
}

void DBBox::Initialize(ImGuiContext* context, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(context, allocator_fns, toolbox_dll);
    context_ = context;
    allocator_fns_ = allocator_fns;
    toolbox_dll_ = toolbox_dll;
    initialized_ = true;
    shutting_down_ = false;
}

void DBBox::StartFeature(Feature& feature)
{
    if (!initialized_ || !settings_loaded_ || shutting_down_ || feature.instance) {
        return;
    }
    feature.instance = feature.factory();
    feature.instance->Initialize(context_, allocator_fns_, toolbox_dll_);
    feature.instance->LoadSettings(settings_folder_.c_str());
    feature.stop_barrier.reset();
    feature.state = FeatureState::Running;
}

void DBBox::StopFeature(Feature& feature, const bool save_settings)
{
    if (!feature.instance || feature.state != FeatureState::Running) {
        return;
    }
    if (save_settings) {
        feature.instance->SaveSettings(settings_folder_.c_str());
    }
    feature.instance->SignalTerminate();
    feature.stop_barrier = std::make_shared<std::atomic_bool>(false);
    GW::GameThread::Enqueue([barrier = feature.stop_barrier] {
        barrier->store(true, std::memory_order_release);
    });
    feature.state = FeatureState::Stopping;
}

void DBBox::DrainStoppingFeatures()
{
    for (auto& feature : features_) {
        if (feature.state != FeatureState::Stopping
            || !feature.instance
            || !feature.stop_barrier
            || !feature.stop_barrier->load(std::memory_order_acquire)
            || !feature.instance->CanTerminate()) {
            continue;
        }
        feature.instance->Terminate();
        feature.instance.reset();
        feature.stop_barrier.reset();
        feature.state = FeatureState::Stopped;
    }
}

void DBBox::SignalTerminate()
{
    shutting_down_ = true;
    for (auto& feature : features_) {
        StopFeature(feature, false);
    }
    DrainStoppingFeatures();
    ToolboxPlugin::SignalTerminate();
}

bool DBBox::CanTerminate()
{
    DrainStoppingFeatures();
    return AsyncStringDecoder::PendingCount() == 0
        && std::ranges::none_of(features_, [](const auto& feature) { return feature.instance != nullptr; });
}

void DBBox::Terminate()
{
    DrainStoppingFeatures();
    RenderingUtils::shutdown();
    initialized_ = false;
    settings_loaded_ = false;
    context_ = nullptr;
    toolbox_dll_ = nullptr;
    ToolboxPlugin::Terminate();
}

void DBBox::Update(const float delta)
{
    for (auto& feature : features_) {
        if (feature.instance && feature.state == FeatureState::Running) {
            feature.instance->Update(delta);
        }
    }
    DrainStoppingFeatures();
    if (shutting_down_) {
        return;
    }
    for (auto& feature : features_) {
        if (feature.enabled && feature.state == FeatureState::Stopped) {
            StartFeature(feature);
        }
    }
}

void DBBox::Draw(IDirect3DDevice9* device)
{
    const auto world_map_showing = GW::UI::GetIsWorldMapShowing();
    for (auto& feature : features_) {
        if (feature.state != FeatureState::Running || !feature.instance) {
            continue;
        }
        if (world_map_showing && !feature.instance->ShowOnWorldMap()) {
            continue;
        }
        const auto visible = feature.instance->GetVisiblePtr();
        if (!visible || *visible) {
            feature.instance->Draw(device);
        }
    }
}

bool DBBox::WndProc(const UINT message, const WPARAM wparam, const LPARAM lparam)
{
    auto captured = false;
    for (auto& feature : features_) {
        if (feature.state == FeatureState::Running && feature.instance) {
            captured |= feature.instance->WndProc(message, wparam, lparam);
        }
    }
    return captured;
}

void DBBox::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    settings_folder_ = folder;
    settings_loaded_ = true;
    if (settings.Has(Name(), "enabled_features")) {
        std::vector<std::string> enabled_features;
        LoadSetting("enabled_features", enabled_features);
        for (auto& feature : features_) {
            feature.enabled = std::ranges::contains(enabled_features, feature.id);
        }
    }

    for (auto& feature : features_) {
        if (feature.enabled) {
            if (feature.state == FeatureState::Running) {
                feature.instance->LoadSettings(settings_folder_.c_str());
            }
            else if (feature.state == FeatureState::Stopped) {
                StartFeature(feature);
            }
        }
        else {
            StopFeature(feature);
        }
    }
}

void DBBox::SaveSettings(const wchar_t* folder)
{
    settings_folder_ = folder;
    std::vector<std::string> enabled_features;
    for (auto& feature : features_) {
        if (feature.enabled) {
            enabled_features.emplace_back(feature.id);
        }
        if (feature.state == FeatureState::Running && feature.instance) {
            feature.instance->SaveSettings(folder);
        }
    }
    SaveSetting("enabled_features", enabled_features);
    ToolboxPlugin::SaveSettings(folder);
}

const char* DBBox::StateLabel(const FeatureState state)
{
    switch (state) {
        case FeatureState::Stopped: return "Disabled";
        case FeatureState::Running: return "Running";
        case FeatureState::Stopping: return "Stopping...";
    }
    return "Unknown";
}

void DBBox::DrawSettings()
{
    ToolboxPlugin::DrawSettings();
    ImGui::TextWrapped("DBBox features run independently. Disabling a feature saves its settings and waits for pending work before releasing it.");
    ImGui::Separator();

    for (auto& feature : features_) {
        ImGui::PushID(feature.id);
        const auto expanded = ImGui::CollapsingHeader(feature.label, ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 190.f);
        ImGui::TextDisabled("%s", StateLabel(feature.state));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80.f);
        auto enabled = feature.enabled;
        if (ImGui::Checkbox("Enabled", &enabled)) {
            feature.enabled = enabled;
            if (enabled) {
                if (feature.state == FeatureState::Stopped) {
                    StartFeature(feature);
                }
            }
            else {
                StopFeature(feature);
            }
        }

        if (expanded) {
            if (feature.state == FeatureState::Running && feature.instance) {
                if (const auto visible = feature.instance->GetVisiblePtr()) {
                    ImGui::Checkbox("Visible", visible);
                }
                if (feature.instance->HasSettings()) {
                    ImGui::Indent();
                    feature.instance->DrawSettings();
                    ImGui::Unindent();
                }
            }
            else {
                ImGui::TextDisabled(feature.state == FeatureState::Stopping
                    ? "Waiting for pending feature work to finish."
                    : "Enable this feature to configure it.");
            }
        }
        ImGui::PopID();
    }
}
