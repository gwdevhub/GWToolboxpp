#include "stdafx.h"

#include <Modules/Resources.h>
#include <Windows/NotePadWindow.h>
#include <Utils/FontLoader.h>

#include <ImGuiAddons.h>

namespace {
    constexpr auto text_buffer_length = 2024 * 16;
    char* text_buffer = nullptr;

    constexpr float text_size_min = static_cast<float>(FontLoader::FontSize::text);
    float font_size = static_cast<float>(FontLoader::FontSize::text);

    bool filedirty = false;
}

void NotePadWindow::Initialize()
{
    ToolboxWindow::Initialize();
    text_buffer = new char[text_buffer_length];
    memset(text_buffer, 0, text_buffer_length);
}
void NotePadWindow::Terminate()
{
    ToolboxWindow::Terminate();
    delete[] text_buffer;
}

void NotePadWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(300.0f, 200.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        const ImVec2 cmax = ImGui::GetWindowContentRegionMax();
        const ImVec2 cmin = ImGui::GetWindowContentRegionMin();
        const auto font = FontLoader::GetFontByPx(font_size);
        ImGui::PushFont(font, font_size);
        if (ImGui::InputTextMultiline("##source", text_buffer, text_buffer_length,
                                      ImVec2(cmax.x - cmin.x, cmax.y - cmin.y), ImGuiInputTextFlags_AllowTabInput)) {
            filedirty = true;
        }
        ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void NotePadWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);

    std::ifstream file(Resources::GetPath(L"Notepad.txt"));
    if (file) {
        file.get(text_buffer, text_buffer_length, '\0');
        file.close();
    }
}

void NotePadWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    if (filedirty) {
        std::ofstream file(Resources::GetPath(L"Notepad.txt"));
        if (file) {
            file.write(text_buffer, strlen(text_buffer));
            file.close();
            filedirty = false;
        }
    }
}

void NotePadWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::DragFloat("Text size", &font_size, 1.0, text_size_min, FontLoader::text_size_max, "%.0f");
}
