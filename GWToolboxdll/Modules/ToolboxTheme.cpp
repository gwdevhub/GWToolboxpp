#include "stdafx.h"

#include <Color.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxTheme.h>

#define IniFilename L"Theme.ini"
// @Enhancement: Allow users to save different layouts by changing this variable in settings
#define WindowPositionsFilename L"Layout.ini"
#define IniSection "Theme"

namespace {
    ToolboxIni* LoadIni(ToolboxIni** out, const wchar_t* filename, bool reload_from_disk = false) {
        if (!*out) {
            *out = new ToolboxIni(false, false, false);
            reload_from_disk = true;
        }
        if (!reload_from_disk)
            return *out;
        const auto path = Resources::GetPath(filename);
        if (!std::filesystem::exists(path)) {
            Log::LogW(L"File %s doesn't exist.", path.c_str());
            return *out;
        }
        const auto tmp = new ToolboxIni(false, false, false);
        ASSERT(Resources::LoadIniFromFile(path, tmp) == 0);
        delete* out;
        *out = tmp;
        return *out;
    }
}

ToolboxTheme::ToolboxTheme()
{
    ini_style = DefaultTheme();
}

ImGuiStyle ToolboxTheme::DefaultTheme()
{
    ImGuiStyle style = ImGuiStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 2.0f;
    style.ScrollbarSize = 16.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabMinSize = 17.0f;
    style.GrabRounding = 2.0f;
    // fixme: rewrite the colors to remove the need for SwapRB
    style.Colors[ImGuiCol_WindowBg] = ImColor(0xD0000000);
    style.Colors[ImGuiCol_TitleBg] = ImColor(0xD7282828);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImColor(0x82282828);
    style.Colors[ImGuiCol_TitleBgActive] = ImColor(0xFF282828);
    style.Colors[ImGuiCol_MenuBarBg] = ImColor(0xFF000000);
    style.Colors[ImGuiCol_ScrollbarBg] = ImColor(0xCC141414);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(0xB3424954);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0xFF424954);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0xFF565D68);
    style.Colors[ImGuiCol_SliderGrabActive] = ImColor(0xFFC8C8FF);
    style.Colors[ImGuiCol_Button] = ImColor(0x99344870);
    style.Colors[ImGuiCol_ButtonHovered] = ImColor(0xFF344870);
    style.Colors[ImGuiCol_ButtonActive] = ImColor(0xFF283C68);
    style.Colors[ImGuiCol_Header] = ImColor(0x73E62800);
    style.Colors[ImGuiCol_HeaderHovered] = ImColor(0xCCF03200);
    style.Colors[ImGuiCol_HeaderActive] = ImColor(0xCCFA3C00);
    return style;
}

void ToolboxTheme::Terminate()
{
    ToolboxUIElement::Terminate();
    delete layout_ini;
    delete theme_ini;
    layout_ini = theme_ini = nullptr;
}

void ToolboxTheme::LoadSettings(ToolboxIni* ini)
{
    ToolboxUIElement::LoadSettings(ini);

    const auto inifile = GetThemeIni();
    if (!inifile) {
        ini_style = DefaultTheme();
        return;
    }

    font_global_scale = static_cast<float>(inifile->GetDoubleValue(IniSection, "FontGlobalScale", font_global_scale));

    ini_style.Alpha = static_cast<float>(inifile->GetDoubleValue(IniSection, "GlobalAlpha", ini_style.Alpha));
    ini_style.Alpha = std::min(std::max(ini_style.Alpha, 0.2f), 1.0f); // clamp to [0.2, 1.0]
    ini_style.WindowPadding.x = static_cast<float>(inifile->GetDoubleValue(IniSection, "WindowPaddingX", ini_style.WindowPadding.x));
    ini_style.WindowPadding.y = static_cast<float>(inifile->GetDoubleValue(IniSection, "WindowPaddingY", ini_style.WindowPadding.y));
    ini_style.WindowRounding = static_cast<float>(inifile->GetDoubleValue(IniSection, "WindowRounding", ini_style.WindowRounding));
    ini_style.FramePadding.x = static_cast<float>(inifile->GetDoubleValue(IniSection, "FramePaddingX", ini_style.FramePadding.x));
    ini_style.FramePadding.y = static_cast<float>(inifile->GetDoubleValue(IniSection, "FramePaddingY", ini_style.FramePadding.y));
    ini_style.FrameRounding = static_cast<float>(inifile->GetDoubleValue(IniSection, "FrameRounding", ini_style.FrameRounding));
    ini_style.ItemSpacing.x = static_cast<float>(inifile->GetDoubleValue(IniSection, "ItemSpacingX", ini_style.ItemSpacing.x));
    ini_style.ItemSpacing.y = static_cast<float>(inifile->GetDoubleValue(IniSection, "ItemSpacingY", ini_style.ItemSpacing.y));
    ini_style.ItemInnerSpacing.x = static_cast<float>(inifile->GetDoubleValue(IniSection, "ItemInnerSpacingX", ini_style.ItemInnerSpacing.x));
    ini_style.ItemInnerSpacing.y = static_cast<float>(inifile->GetDoubleValue(IniSection, "ItemInnerSpacingY", ini_style.ItemInnerSpacing.y));
    ini_style.IndentSpacing = static_cast<float>(inifile->GetDoubleValue(IniSection, "IndentSpacing", ini_style.IndentSpacing));
    ini_style.ScrollbarSize = static_cast<float>(inifile->GetDoubleValue(IniSection, "ScrollbarSize", ini_style.ScrollbarSize));
    ini_style.ScrollbarRounding = static_cast<float>(inifile->GetDoubleValue(IniSection, "ScrollbarRounding", ini_style.ScrollbarRounding));
    ini_style.GrabMinSize = static_cast<float>(inifile->GetDoubleValue(IniSection, "GrabMinSize", ini_style.GrabMinSize));
    ini_style.GrabRounding = static_cast<float>(inifile->GetDoubleValue(IniSection, "GrabRounding", ini_style.GrabRounding));
    ini_style.WindowTitleAlign.x = static_cast<float>(inifile->GetDoubleValue(IniSection, "WindowTitleAlignX", ini_style.WindowTitleAlign.x));
    ini_style.WindowTitleAlign.y = static_cast<float>(inifile->GetDoubleValue(IniSection, "WindowTitleAlignY", ini_style.WindowTitleAlign.y));
    ini_style.ButtonTextAlign.x = static_cast<float>(inifile->GetDoubleValue(IniSection, "ButtonTextAlignX", ini_style.ButtonTextAlign.x));
    ini_style.ButtonTextAlign.y = static_cast<float>(inifile->GetDoubleValue(IniSection, "ButtonTextAlignY", ini_style.ButtonTextAlign.y));
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const char* name = ImGui::GetStyleColorName(i);
        const Color color = Colors::Load(inifile, IniSection, name, ImColor(ini_style.Colors[i]));
        ini_style.Colors[i] = ImColor(color);
    }

    layout_dirty = true;
}
void ToolboxTheme::SaveUILayout()
{
    if (!ImGui::GetCurrentContext() || !imgui_style_loaded)
        return;
    const auto ini = GetLayoutIni(false);
    const auto window_ini_section = "Windows";
    ImVector<ImGuiWindow*>& windows = ImGui::GetCurrentContext()->Windows;
    for (const ImGuiWindow* window : windows) {
        char key[128];
        snprintf(key, 128, "_%s_X", window->Name);
        ini->SetDoubleValue(window_ini_section, key, window->Pos.x);
        snprintf(key, 128, "_%s_Y", window->Name);
        ini->SetDoubleValue(window_ini_section, key, window->Pos.y);
        snprintf(key, 128, "_%s_W", window->Name);
        ini->SetDoubleValue(window_ini_section, key, window->SizeFull.x);
        snprintf(key, 128, "_%s_H", window->Name);
        ini->SetDoubleValue(window_ini_section, key, window->SizeFull.y);
        snprintf(key, 128, "_%s_Collapsed", window->Name);
        ini->SetBoolValue(window_ini_section, key, window->Collapsed);
    }
    ASSERT(Resources::SaveIniToFile(WindowPositionsFilename, ini) == 0);
}
ToolboxIni* ToolboxTheme::GetLayoutIni(const bool reload)
{
    return LoadIni(&layout_ini, WindowPositionsFilename, reload);
}
ToolboxIni* ToolboxTheme::GetThemeIni(const bool reload)
{
    return LoadIni(&theme_ini, IniFilename, reload);
}
void ToolboxTheme::LoadUILayout()
{
    if (!ImGui::GetCurrentContext())
        return;
    // Copy theme over
    ImGui::GetStyle() = ini_style;
    // Copy window positions over
    ImGui::GetIO().FontGlobalScale = font_global_scale;
    const auto ini = GetLayoutIni();
    ImVector<ImGuiWindow*>& windows = ImGui::GetCurrentContext()->Windows;
    const auto window_ini_section = "Windows";
    for (ImGuiWindow* window : windows) {
        if (!window)
            continue;
        ImVec2 pos = window->Pos;
        ImVec2 size = window->Size;
        char key[128];
        snprintf(key, 128, "_%s_X", window->Name);
        pos.x = static_cast<float>(ini->GetDoubleValue(window_ini_section, key, pos.x));
        snprintf(key, 128, "_%s_Y", window->Name);
        pos.y = static_cast<float>(ini->GetDoubleValue(window_ini_section, key, pos.y));
        snprintf(key, 128, "_%s_W", window->Name);
        size.x = static_cast<float>(ini->GetDoubleValue(window_ini_section, key, size.x));
        snprintf(key, 128, "_%s_H", window->Name);
        size.y = static_cast<float>(ini->GetDoubleValue(window_ini_section, key, size.y));
        snprintf(key, 128, "_%s_Collapsed", window->Name);
        const bool collapsed = ini->GetBoolValue(window_ini_section, key, false);
        ImGui::SetWindowPos(window, pos);
        ImGui::SetWindowSize(window, size);
        ImGui::SetWindowCollapsed(window, collapsed);
    }
    layout_dirty = false;
    imgui_style_loaded = true;
}

void ToolboxTheme::SaveSettings(ToolboxIni* ini)
{
    ToolboxUIElement::SaveSettings(ini);

    if (!ImGui::GetCurrentContext() || !imgui_style_loaded)
        return; // Imgui not initialised, can happen if destructing before first draw

    const ImGuiStyle& style = ImGui::GetStyle();
    const auto inifile = GetThemeIni(false);

    inifile->SetDoubleValue(IniSection, "FontGlobalScale", ImGui::GetIO().FontGlobalScale);
    inifile->SetDoubleValue(IniSection, "GlobalAlpha", style.Alpha);
    inifile->SetDoubleValue(IniSection, "WindowPaddingX", style.WindowPadding.x);
    inifile->SetDoubleValue(IniSection, "WindowPaddingY", style.WindowPadding.y);
    inifile->SetDoubleValue(IniSection, "WindowRounding", style.WindowRounding);
    inifile->SetDoubleValue(IniSection, "FramePaddingX", style.FramePadding.x);
    inifile->SetDoubleValue(IniSection, "FramePaddingY", style.FramePadding.y);
    inifile->SetDoubleValue(IniSection, "FrameRounding", style.FrameRounding);
    inifile->SetDoubleValue(IniSection, "ItemSpacingX", style.ItemSpacing.x);
    inifile->SetDoubleValue(IniSection, "ItemSpacingY", style.ItemSpacing.y);
    inifile->SetDoubleValue(IniSection, "ItemInnerSpacingX", style.ItemInnerSpacing.x);
    inifile->SetDoubleValue(IniSection, "ItemInnerSpacingY", style.ItemInnerSpacing.y);
    inifile->SetDoubleValue(IniSection, "IndentSpacing", style.IndentSpacing);
    inifile->SetDoubleValue(IniSection, "ScrollbarSize", style.ScrollbarSize);
    inifile->SetDoubleValue(IniSection, "ScrollbarRounding", style.ScrollbarRounding);
    inifile->SetDoubleValue(IniSection, "GrabMinSize", style.GrabMinSize);
    inifile->SetDoubleValue(IniSection, "GrabRounding", style.GrabRounding);
    inifile->SetDoubleValue(IniSection, "WindowTitleAlignX", style.WindowTitleAlign.x);
    inifile->SetDoubleValue(IniSection, "WindowTitleAlignY", style.WindowTitleAlign.y);
    inifile->SetDoubleValue(IniSection, "ButtonTextAlignX", style.ButtonTextAlign.x);
    inifile->SetDoubleValue(IniSection, "ButtonTextAlignY", style.ButtonTextAlign.y);
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const char* name = ImGui::GetStyleColorName(i);
        const Color color = ImColor(style.Colors[i]);
        Colors::Save(inifile, IniSection, name, color);
    }

    ASSERT(Resources::SaveIniToFile(IniFilename, inifile) == 0);

    SaveUILayout();
}

void ToolboxTheme::Draw(IDirect3DDevice9*)
{
    if (layout_dirty)
        LoadUILayout();
}

void ToolboxTheme::DrawSettingInternal()
{
    ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::SmallButton("Restore Default")) {
        style = DefaultTheme();
    }
    ImGui::Text("Note: window position/size is stored in 'Layout.ini' in settings folder. You can share the file or parts of it with other people.");
    ImGui::Text("Note: theme is stored in 'Theme.ini' in settings folder. You can share the file or parts of it with other people.");
    ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f");
    style.Alpha = std::clamp(style.Alpha, 0.2f, 1.f);
    ImGui::DragFloat("Global Font Scale", &ImGui::GetIO().FontGlobalScale, 0.005f, 0.3f, 2.0f, "%.1f");
    ImGui::Text("Sizes");
    ImGui::SliderFloat2("Window Padding", reinterpret_cast<float*>(&style.WindowPadding), 0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("Window Rounding", &style.WindowRounding, 0.0f, 16.0f, "%.0f");
    ImGui::SliderFloat2("Frame Padding", reinterpret_cast<float*>(&style.FramePadding), 0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("Frame Rounding", &style.FrameRounding, 0.0f, 16.0f, "%.0f");
    ImGui::SliderFloat2("Item Spacing", reinterpret_cast<float*>(&style.ItemSpacing), 0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat2("Item InnerSpacing", reinterpret_cast<float*>(&style.ItemInnerSpacing), 0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("Indent Spacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f");
    ImGui::SliderFloat("Scrollbar Size", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("Scrollbar Rounding", &style.ScrollbarRounding, 0.0f, 16.0f, "%.0f");
    ImGui::SliderFloat("Grab MinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("Grab Rounding", &style.GrabRounding, 0.0f, 16.0f, "%.0f");
    ImGui::Text("Alignment");
    ImGui::SliderFloat2("Window Title Align", reinterpret_cast<float*>(&style.WindowTitleAlign), 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat2("Button Text Align", reinterpret_cast<float*>(&style.ButtonTextAlign), 0.0f, 1.0f, "%.2f");
    ImGui::Text("Colors");
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const char* name = ImGui::GetStyleColorName(i);
        ImGui::PushID(i);
        ImGui::ColorEdit4(name, reinterpret_cast<float*>(&style.Colors[i]));
        const Color cur = ImColor(style.Colors[i]);
        if (cur != ImColor(ini_style.Colors[i])) {
            ImGui::SameLine();
            if (ImGui::Button("Revert")) {
                style.Colors[i] = ImColor(Colors::Load(GetThemeIni(), IniSection, name, ImColor(ini_style.Colors[i])));
            }
        }
        ImGui::PopID();
    }
}
