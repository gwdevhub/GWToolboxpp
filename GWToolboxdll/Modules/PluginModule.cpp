#include "stdafx.h"

#include "PluginModule.h"

#include "GWToolbox.h"

#include <Modules/Resources.h>
#include <filesystem>
#include <string>

PluginModule::PluginModule()
{
    const auto wpath = Resources::GetPath(L"plugins");
    wcscpy_s(pluginsfoldername, wpath.c_str());
}

void PluginModule::RefreshDlls()
{
    // when we refresh, how do we map the modules that were already loaded to the ones on disk?
    // the dll file may have changed
    namespace fs = std::filesystem;

    const fs::path plugin_folder = pluginsfoldername;

    if (Resources::EnsureFolderExists(plugin_folder)) {
        for (auto& p : fs::directory_iterator(plugin_folder)) {
            fs::path file_path = p.path();
            fs::path ext = file_path.extension();
            if (ext == ".lnk") {
                if (!SUCCEEDED(Resources::ResolveShortcut(file_path, file_path)))
                    continue;
                ext = file_path.extension();
            }
            if (ext == ".dll") {
                if (!LoadDll(file_path)) {
                    Log::Error("Failed to load plugin %s", file_path.filename().string().c_str());
                }
            }
        }
    }
}

void PluginModule::DrawSettingInternal()
{
    ImGui::PushID("Plugins");

    for (auto& plugin : plugins) {
        ImGui::PushID(&plugin);
        auto& style = ImGui::GetStyle();
        const auto origin_header_col = style.Colors[ImGuiCol_Header];
        style.Colors[ImGuiCol_Header] = {0, 0, 0, 0};

        static char buf[128];
        sprintf(buf, "      %s", plugin.path.filename().string().c_str());
        const auto pos = ImGui::GetCursorScreenPos();
        const bool is_showing = ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_AllowItemOverlap);

        const auto icon = plugin.initialized ? plugin.instance->Icon() : nullptr;
        if (icon) {
            const float text_offset_x = ImGui::GetTextLineHeightWithSpacing() + 4.0f; // TODO: find a proper number
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(pos.x + text_offset_x, pos.y + style.ItemSpacing.y / 2),
                ImColor(style.Colors[ImGuiCol_Text]), icon);
        }

        style.Colors[ImGuiCol_Header] = origin_header_col;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.y * 2);
        ImGui::Checkbox("##check", &plugin.active);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Active");

        if (is_showing) {
            if (plugin.active && plugin.initialized) {
                plugin.instance->DrawSettings();
            }
        }
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::Button("Refresh")) {
        RefreshDlls();
    }

    static bool unloading = false;
    ImGui::SameLine();
    if (ImGui::Button("Unload")) {
        unloading = true;
    }
    if (unloading) {
        if (UnloadDlls())
            unloading = false;
    }

    ImGui::PopID();
}

ToolboxPlugin* PluginModule::LoadDll(const std::filesystem::path& path)
{
    for (auto& plugin : plugins) {
        if (plugin.path == path)
            return plugin.instance;
    }
    const auto dll = LoadLibraryW(path.wstring().c_str());
    if (dll == nullptr)
        return nullptr;
    using ToolboxPluginInstanceFn = ToolboxPlugin* (*)();
    const auto instance_fn = reinterpret_cast<ToolboxPluginInstanceFn>(GetProcAddress(dll, "ToolboxPluginInstance"));
    if (!instance_fn)
        return nullptr;
    Plugin p;
    p.instance = instance_fn();
    p.dll = dll;
    p.path = path;
    plugins.push_back(p);
    return p.instance;
}

bool PluginModule::UnloadDlls()
{
    const bool can_terminate = std::ranges::all_of(plugins, [](const auto& plugin) {
        return plugin.instance->CanTerminate();
    });
    if (!can_terminate) {
        for (const auto& plugin : plugins) {
            plugin.instance->SignalTerminate();
        }
        return false;
    }
    for (const auto& plugin : plugins) {
        if (plugin.initialized) {
            plugin.instance->Terminate();
        }
        const auto success = FreeLibrary(plugin.dll);
        if (!success) {
            Log::ErrorW(L"Failed to unload plugin %s", plugin.path.wstring().c_str());
            return false;
        }
    }
    plugins.clear();
    return true;
}

bool PluginModule::CanTerminate()
{
    return std::ranges::all_of(plugins, [](const auto& plugin) {
        return plugin.instance->CanTerminate();
    });
}

void PluginModule::Initialize()
{
    ToolboxUIElement::Initialize();
    RefreshDlls();
}

void PluginModule::Draw(IDirect3DDevice9* device)
{
    for (auto& plugin : plugins) {
        if (!plugin.active)
            continue;
        if (!plugin.initialized) {
            ImGuiAllocFns fns;
            ImGui::GetAllocatorFunctions(&fns.alloc_func, &fns.free_func, &fns.user_data);
            const auto context = ImGui::GetCurrentContext();
            plugin.instance->Initialize(context, fns, GWToolbox::Instance().GetDLLModule());
            plugin.instance->LoadSettings(pluginsfoldername);
            plugin.initialized = true;
        }
        plugin.instance->Draw(device);
    }
}

void PluginModule::LoadSettings(ToolboxIni*)
{
    for (const auto& plugin : GetInitializedPlugins()) {
        plugin.instance->LoadSettings(pluginsfoldername);
    }
}

void PluginModule::SaveSettings(ToolboxIni*)
{
    for (const auto& plugin : GetInitializedPlugins()) {
        plugin.instance->SaveSettings(pluginsfoldername);
    }
}

void PluginModule::Update(float delta)
{
    for (const auto& plugin : GetInitializedPlugins()) {
        plugin.instance->Update(delta);
    }
}

void PluginModule::SignalTerminate()
{
    for (const auto& plugin : GetInitializedPlugins()) {
        plugin.instance->SignalTerminate();
    }
}

void PluginModule::Terminate() { UnloadDlls(); }
