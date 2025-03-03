#include "stdafx.h"

#include <Modules/Resources.h>
#include <Windows/NotePadWindow.h>
#include <Utils/FontLoader.h>

constexpr auto TEXT_SIZE = 2024 * 16;
const int MIN_FONT_SIZE = 16;

float font_size = static_cast<float>(FontLoader::FontSize::widget_label);

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
        ImGui::PushFont(font);
        if (ImGui::InputTextMultiline("##source", text, TEXT_SIZE,
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
        file.get(text, TEXT_SIZE, '\0');
        file.close();
    }
}

void NotePadWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    if (filedirty) {
        std::ofstream file(Resources::GetPath(L"Notepad.txt"));
        if (file) {
            file.write(text, strlen(text));
            file.close();
            filedirty = false;
        }
    }
}

void NotePadWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::DragFloat("Text size", &font_size, 1.0, MIN_FONT_SIZE, FontLoader::text_size_max, "%.0f");
}
