#include "stdafx.h"

#include <Modules/Resources.h>
#include <Windows/NotePadWindow.h>
#include <Utils/FontLoader.h>

namespace {
    constexpr size_t text_buffer_length = 2024 * 16;
    constexpr size_t tab_name_length = 64;

    constexpr float text_size_min = static_cast<float>(FontLoader::FontSize::text);
    float font_size = static_cast<float>(FontLoader::FontSize::text);

    struct NotepadTab {
        char name[tab_name_length]{};
        char* text = nullptr;
        bool dirty = false;
        int id = 0;

        NotepadTab(int tab_id, const char* tab_name)
            : id(tab_id)
        {
            text = new char[text_buffer_length]{};
            if (tab_name && *tab_name) {
                strncpy_s(name, tab_name, tab_name_length - 1);
            }
        }

        ~NotepadTab() { delete[] text; }

        NotepadTab(const NotepadTab&) = delete;
        NotepadTab& operator=(const NotepadTab&) = delete;
    };

    std::vector<std::unique_ptr<NotepadTab>> tabs;
    int next_tab_id = 0;
    int renaming_tab = -1;
    int active_tab_idx = 0;
    bool rename_needs_focus = false;
    char rename_buf[tab_name_length]{};

    std::wstring GetTabFilename(int id)
    {
        return L"Notepad_" + std::to_wstring(id) + L".txt";
    }

    NotepadTab* AddTab(const char* name = nullptr, int id = -1)
    {
        if (id < 0) {
            id = next_tab_id++;
        }
        else {
            // Deduplicate: skip if a tab with this id already exists
            for (const auto& existing : tabs) {
                if (existing->id == id) {
                    return existing.get();
                }
            }
            next_tab_id = std::max(next_tab_id, id + 1);
        }
        std::string default_name;
        if (!name || !*name) {
            default_name = "Note " + std::to_string(tabs.size() + 1);
            name = default_name.c_str();
        }
        auto tab = std::make_unique<NotepadTab>(id, name);
        NotepadTab* ptr = tab.get();
        tabs.push_back(std::move(tab));
        return ptr;
    }

    void SaveTabContent(size_t idx)
    {
        if (idx >= tabs.size() || !tabs[idx]->dirty) {
            return;
        }
        auto& tab = *tabs[idx];
        const auto path = Resources::GetPath(GetTabFilename(tab.id));
        std::ofstream file(path);
        if (file) {
            file.write(tab.text, strlen(tab.text));
            tab.dirty = false;
        }
    }

    void LoadTabContent(size_t idx)
    {
        if (idx >= tabs.size()) {
            return;
        }
        auto& tab = *tabs[idx];
        const auto path = Resources::GetPath(GetTabFilename(tab.id));
        std::ifstream file(path);
        if (file) {
            file.get(tab.text, text_buffer_length, '\0');
        }
    }

    void DeleteTab(int idx)
    {
        if (idx < 0 || idx >= static_cast<int>(tabs.size()) || tabs.size() <= 1) {
            return;
        }
        const auto path = Resources::GetPath(GetTabFilename(tabs[idx]->id));
        std::filesystem::remove(path);
        tabs.erase(tabs.begin() + idx);

        if (renaming_tab == idx) {
            renaming_tab = -1;
        }
        else if (renaming_tab > idx) {
            renaming_tab--;
        }
    }
}

void NotePadWindow::Initialize()
{
    ToolboxWindow::Initialize();
}

void NotePadWindow::Terminate()
{
    ToolboxWindow::Terminate();
    tabs.clear();
    next_tab_id = 0;
    renaming_tab = -1;
    active_tab_idx = 0;
}

void NotePadWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(300.0f, 200.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (ImGui::BeginTabBar("##notepad_tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
                AddTab();
            }

            active_tab_idx = std::clamp(active_tab_idx, 0, static_cast<int>(tabs.size()) - 1);
            int delete_tab = -1;
            for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
                ImGui::PushID(i);
                auto& tab = *tabs[i];
                const ImGuiTabItemFlags flags = tab.dirty ? ImGuiTabItemFlags_UnsavedDocument : ImGuiTabItemFlags_None;
                bool tab_open = true;
                // Only show the close button (X) on the currently active tab to prevent accidental closure
                bool* const p_open = (tabs.size() > 1 && i == active_tab_idx) ? &tab_open : nullptr;
                const bool tab_visible = ImGui::BeginTabItem(tab.name, p_open, flags);

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        renaming_tab = i;
                        strncpy_s(rename_buf, tab.name, tab_name_length - 1);
                        rename_needs_focus = true;
                    }
                    if (tabs.size() > 1 && ImGui::MenuItem("Close")) {
                        delete_tab = i;
                    }
                    ImGui::EndPopup();
                }

                if (!tab_open) {
                    delete_tab = i;
                }

                if (tab_visible) {
                    active_tab_idx = i;
                    if (renaming_tab == i) {
                        ImGui::SetNextItemWidth(-1.0f);
                        if (rename_needs_focus) {
                            ImGui::SetKeyboardFocusHere(0);
                            rename_needs_focus = false;
                        }
                        const bool confirmed = ImGui::InputText("##rename", rename_buf, tab_name_length,
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                        if (confirmed) {
                            if (rename_buf[0]) {
                                strncpy_s(tab.name, rename_buf, tab_name_length - 1);
                            }
                            renaming_tab = -1;
                        }
                        else if (ImGui::IsItemDeactivated()) {
                            renaming_tab = -1;
                        }
                    }

                    const ImVec2 avail = ImGui::GetContentRegionAvail();
                    const auto font = FontLoader::GetFont();
                    ImGui::PushFont(font, font_size);
                    if (ImGui::InputTextMultiline("##text", tab.text, text_buffer_length, avail, ImGuiInputTextFlags_AllowTabInput)) {
                        tab.dirty = true;
                    }
                    ImGui::PopFont();
                    ImGui::EndTabItem();
                }
                ImGui::PopID();
            }

            if (delete_tab >= 0) {
                DeleteTab(delete_tab);
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void NotePadWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_FLOAT(font_size);

    const long tab_count = ini->GetLongValue(Name(), "tab_count", -1);
    if (tab_count < 0) {
        // Legacy single-tab config
        auto* tab = AddTab("Note 1");
        std::ifstream file(Resources::GetPath(L"Notepad.txt"));
        if (file) {
            file.get(tab->text, text_buffer_length, '\0');
        }
    }
    else {
        next_tab_id = static_cast<int>(ini->GetLongValue(Name(), "next_tab_id", tab_count));
        for (long i = 0; i < tab_count; i++) {
            char id_key[32], name_key[32];
            snprintf(id_key, sizeof(id_key), "tab_%ld_id", i);
            snprintf(name_key, sizeof(name_key), "tab_%ld_name", i);
            const int id = static_cast<int>(ini->GetLongValue(Name(), id_key, i));
            const size_t prev_size = tabs.size();
            AddTab(ini->GetValue(Name(), name_key, "Note"), id);
            if (tabs.size() > prev_size) {
                // Only load content for genuinely new tabs
                LoadTabContent(tabs.size() - 1);
            }
        }
        if (tabs.empty()) {
            AddTab("Note 1");
        }
    }
}

void NotePadWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_FLOAT(font_size);

    ini->SetLongValue(Name(), "tab_count", static_cast<long>(tabs.size()));
    ini->SetLongValue(Name(), "next_tab_id", static_cast<long>(next_tab_id));

    for (size_t i = 0; i < tabs.size(); i++) {
        char id_key[32], name_key[32];
        snprintf(id_key, sizeof(id_key), "tab_%zu_id", i);
        snprintf(name_key, sizeof(name_key), "tab_%zu_name", i);
        ini->SetLongValue(Name(), id_key, static_cast<long>(tabs[i]->id));
        ini->SetValue(Name(), name_key, tabs[i]->name);
        SaveTabContent(i);
    }
}

void NotePadWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::DragFloat("Text size", &font_size, 1.0, text_size_min, FontLoader::text_size_max, "%.0f");
}
