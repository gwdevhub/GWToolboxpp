#include "stdafx.h"

#include "PluginModule.h"
#include "../plugins/Base/ToolboxPlugin.h"

#include <GWToolbox.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Defender.h>
#include <Modules/Resources.h>
#include <filesystem>
#include <string>

#include "GWCA/Managers/UIMgr.h"
#include "Utils/TextUtils.h"

namespace {
    std::wstring pluginsfoldername;

    const char* plugins_enabled_section = "已启用插件";

    std::vector<PluginModule::Plugin*> plugins_available;

    std::vector<PluginModule::Plugin*> plugins_loaded;

    bool UnloadPlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (!plugin.terminating) {
            if (plugin.instance) {
                plugin.instance->SignalTerminate();
            }
            plugin.terminating = true;
        }
        if (plugin.instance && !plugin.instance->CanTerminate()) {
            return false; // 挂起
        }

        if (plugin.instance) {
            plugin.instance->Terminate();
        }
        plugin.initialized = false;
        plugin.terminating = false;
        plugin.instance = nullptr;
        if (SUCCEEDED(FreeLibrary(plugin.dll))) {
            plugin.dll = nullptr;
        }
        std::erase_if(plugins_loaded, [plugin_ptr](auto p) { return p == plugin_ptr; });
        return true;
    }

    bool LoadPlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (plugin.instance) {
            return true;
        }
        if (!plugin.dll) {
            plugin.dll = LoadLibraryW(plugin.path.wstring().c_str());
        }
        if (!plugin.dll) {
            const DWORD err = GetLastError();
            const auto filename = plugin.path.filename();
            UnloadPlugin(plugin_ptr);
            const auto name = TextUtils::PrintFilename(filename.wstring());
            std::wstring detail;
            if ((err == ERROR_VIRUS_INFECTED || err == ERROR_VIRUS_DELETED) && FindRecentDefenderBlock(filename.wstring(), 15, detail))
                Log::ErrorW(L"加载插件 %s 失败 - Windows Defender 阻止了它：%s", name.c_str(), detail.c_str());
            else
                Log::ErrorW(L"加载插件 %s 失败 (LoadLibraryW)", name.c_str());
            return false;
        }
        using ToolboxPluginInstanceFn = ToolboxPlugin* (*)();
        const auto instance_fn = reinterpret_cast<ToolboxPluginInstanceFn>(GetProcAddress(plugin.dll, "ToolboxPluginInstance"));
        if (!instance_fn) {
            UnloadPlugin(plugin_ptr);
            Log::Error("加载插件 %s 失败 (ToolboxPluginInstance)", TextUtils::PrintFilename(plugin.path.filename().string()).c_str());
            return false;
        }

        plugin.instance = instance_fn();
        plugins_loaded.push_back(plugin_ptr);
        return true;
    }

    bool InitializePlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (plugin.terminating || !plugin.instance) {
            return false;
        }
        if (plugin.initialized) {
            return true;
        }
        const auto context = ImGui::GetCurrentContext();
        if (!context) {
            return false;
        }
        ImGuiAllocFns fns;
        ImGui::GetAllocatorFunctions(&fns.alloc_func, &fns.free_func, &fns.user_data);
        plugin.instance->Initialize(context, fns, GWToolbox::GetDLLModule());
        plugin.instance->LoadSettings(pluginsfoldername.c_str());
        plugin.initialized = true;
        return true;
    }

    void RefreshDlls()
    {
        // 刷新时，如何将已加载的模块映射到磁盘上的模块？
        // DLL文件可能已更改
        namespace fs = std::filesystem;

        const fs::path plugin_folder = pluginsfoldername;

        if (!Resources::EnsureFolderExists(plugin_folder)) {
            return;
        }

        for (auto& p : fs::directory_iterator(plugin_folder)) {
            fs::path file_path = p.path();
            fs::path ext = file_path.extension();
            if (ext == ".lnk") {
                if (SUCCEEDED(Resources::ResolveShortcut(file_path, file_path))) {
                    ext = file_path.extension();
                }
            }
            if (ext == ".dll") {
                const auto found = std::ranges::find_if(plugins_available, [file_path](const auto plugin) {
                    return plugin->path == file_path;
                });
                if (found == plugins_available.end()) {
                    plugins_available.push_back(new PluginModule::Plugin(file_path));
                }
            }
        }
    }
}

void PluginModule::DrawSettingsInternal()
{
    ImGui::PushID("Plugins");

    size_t i = 0;
    for (const auto plugin : plugins_available) {
        ImGui::PushID(i++);
        auto& style = ImGui::GetStyle();
        const auto origin_header_col = style.Colors[ImGuiCol_Header];
        style.Colors[ImGuiCol_Header] = {0, 0, 0, 0};

        static char buf[128];
        const auto has_settings = plugin->initialized && plugin->instance && plugin->instance->HasSettings();
        if (has_settings) {
            sprintf(buf, "      %s", plugin->path.filename().string().c_str());
        }
        else {
            sprintf(buf, "             %s", plugin->path.filename().string().c_str());
        }
        const auto pos = ImGui::GetCursorScreenPos();
        const bool is_showing = has_settings ? ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_AllowOverlap) : ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowOverlap);

        if (const auto icon = plugin->initialized ? plugin->instance->Icon() : nullptr) {
            const float text_offset_x = ImGui::GetTextLineHeightWithSpacing() + 4.0f; // TODO: 寻找合适的数值
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(pos.x + text_offset_x, pos.y + style.ItemSpacing.y / 2),
                ImColor(style.Colors[ImGuiCol_Text]), icon);
        }

        style.Colors[ImGuiCol_Header] = origin_header_col;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x - 128.f);
        snprintf(buf, _countof(buf), "%s###load_unload", plugin->instance ? "卸载" : "加载");
        if (ImGui::Button(buf)) {
            if (!plugin->instance) {
                LoadPlugin(plugin);
            }
            else {
                UnloadPlugin(plugin);
            }
        }
        if (plugin->instance && plugin->instance->GetVisiblePtr()) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x);
            ImGui::Checkbox("##check", plugin->instance->GetVisiblePtr());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("可见");
            }
        }

        if (is_showing && InitializePlugin(plugin) && has_settings) {
            plugin->instance->DrawSettings();
        }
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::Button("刷新")) {
        RefreshDlls();
    }

    ImGui::PopID();
}

bool PluginModule::CanTerminate()
{
    return plugins_loaded.empty();
}

bool PluginModule::WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    bool capture = false;
    for (const auto plugin : plugins_loaded) {
        if (!plugin->instance) {
            continue;
        }
        capture |= plugin->instance->WndProc(msg, wParam, lParam);
    }
    return capture;
}

std::vector<ToolboxPlugin*> PluginModule::GetPlugins()
{
    std::vector<ToolboxPlugin*> plugins;
    for (const auto plugin : plugins_loaded) {
        plugins.push_back(plugin->instance);
    }
    return plugins;
}

void PluginModule::Initialize()
{
    pluginsfoldername = Resources::GetPath(L"plugins");
    ToolboxUIElement::Initialize();
    RefreshDlls();
}

void PluginModule::Draw(IDirect3DDevice9* device)
{
    for (const auto plugin : plugins_loaded) {
        if (!InitializePlugin(plugin)) {
            continue;
        }
        if (GW::UI::GetIsWorldMapShowing() && !plugin->instance->ShowOnWorldMap()) {
            continue;
        }

        if (plugin->instance->GetVisiblePtr() && *plugin->instance->GetVisiblePtr()) {
            plugin->instance->Draw(device);
        }
    }
}

void PluginModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxUIElement::LoadSettings(doc, legacy);
    std::vector<std::string> enabled_plugins;
    if (!doc.Get(Name(), "enabled_plugins", enabled_plugins) && legacy) {
        TNamesDepend dlls_to_load;
        if (legacy->GetAllKeys(plugins_enabled_section, dlls_to_load)) {
            for (const auto& entry : dlls_to_load) {
                enabled_plugins.push_back(entry.pItem);
            }
        }
    }
    std::vector<Plugin*> plugins_enabled_from_settings;
    for (const auto& entry : enabled_plugins) {
        const auto filename = std::filesystem::path(entry).filename();
        auto matching_plugins = std::views::filter(plugins_available, [filename](auto plugin) {
            return plugin->path.filename() == filename;
        });
        for (const auto plugin : matching_plugins) {
            if (!LoadPlugin(plugin)) {
                continue;
            }
            InitializePlugin(plugin);
            plugins_enabled_from_settings.push_back(plugin);
        }
    }
    // 查找当前已加载但不应加载的插件
    auto to_unload = std::views::filter(plugins_loaded, [&](auto plugin) {
        return !std::ranges::contains(plugins_enabled_from_settings, plugin);
    }) | std::ranges::to<std::vector>();
    for (const auto plugin : std::views::reverse(to_unload)) {
        UnloadPlugin(plugin);
    }
}

void PluginModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxUIElement::SaveSettings(doc);
    std::vector<std::string> enabled_plugins;
    for (const auto plugin : plugins_loaded) {
        plugin->instance->SaveSettings(pluginsfoldername.c_str());
        enabled_plugins.push_back(plugin->path.filename().string());
    }
    doc.Set(Name(), "enabled_plugins", enabled_plugins);
}

void PluginModule::Update(const float delta)
{
    static bool message_displayed = false;
    if (!plugins_loaded.empty() && !message_displayed) {
        GW::Chat::WriteChat(
            GW::Chat::Channel::CHANNEL_GWCA2,
            L"<c=#FFFF00>检测到插件，这些插件可能不安全，且未得到 GWToolbox++ 开发者的官方支持。\n"
            "如果您信任作者，请自行承担使用风险。\n"
            "请勿在启用插件游玩时报告错误。</c>", GWTOOLBOX_SENDER, true);
        GW::Chat::WriteChat(
            GW::Chat::Channel::CHANNEL_GWCA2,
            L"<c=#FF0000>ArenaNet 不允许使用插件。</c>", GWTOOLBOX_SENDER, true);
        GW::Chat::WriteChat(
            GW::Chat::Channel::CHANNEL_WARNING,
            L"ArenaNet 不允许使用插件。", nullptr, true);
        message_displayed = true;
    }
    for (const auto plugin : plugins_loaded) {
        if (!plugin->initialized)
            continue;
        plugin->instance->Update(delta);
        if (plugin->terminating) {
            if (UnloadPlugin(plugin)) {
                break; // plugins_loaded 向量已更改，跳过一帧
            }
        }
    }
}

void PluginModule::SignalTerminate()
{
    ToolboxUIElement::SignalTerminate();
    const auto snapshot = plugins_loaded;
    for (const auto plugin : snapshot) {
        UnloadPlugin(plugin);
    }
}

void PluginModule::Terminate()
{
    ASSERT(plugins_loaded.empty());
    for (const auto p : plugins_available) {
        if (p->dll) {
            FreeLibrary(p->dll);
        }
        delete p;
    }
}
