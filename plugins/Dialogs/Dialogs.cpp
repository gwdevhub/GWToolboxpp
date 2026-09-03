#include "Dialogs.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <string_view>

#include <PluginUtils.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Scanner.h>

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static Dialogs instance;
    return &instance;
}
#endif

namespace {
    using RawSendDialog = void(__cdecl*)(uint32_t dialog_id);

    struct DialogPreset {
        const char* name;
        uint32_t id;
    };

    struct CapturedDialogButton {
        uint32_t icon;
        uint32_t id;
    };

    constexpr std::array quest_dialogs{
        DialogPreset{"UW - Chamber", 0x65},
        DialogPreset{"UW - Wastes", 0x66},
        DialogPreset{"UW - UWG", 0x67},
        DialogPreset{"UW - Mnt", 0x68},
        DialogPreset{"UW - Pits", 0x69},
        DialogPreset{"UW - Planes", 0x6a},
        DialogPreset{"UW - Pools", 0x6b},
        DialogPreset{"UW - Escort", 0x6c},
        DialogPreset{"UW - Restore", 0x6d},
        DialogPreset{"UW - Vale", 0x6e},
        DialogPreset{"FoW - Defend", 0xca},
        DialogPreset{"FoW - Army Of Darkness", 0xcb},
        DialogPreset{"FoW - WailingLord", 0xcc},
        DialogPreset{"FoW - Griffons", 0xcd},
        DialogPreset{"FoW - Slaves", 0xce},
        DialogPreset{"FoW - Restore", 0xcf},
        DialogPreset{"FoW - Hunt", 0xd0},
        DialogPreset{"FoW - Forgemaster", 0xd1},
        DialogPreset{"FoW - Tos", 0xd3},
        DialogPreset{"FoW - Toc", 0xd4},
        DialogPreset{"FoW - Khobay", 0xe0},
        DialogPreset{"DoA - Gloom 1: Deathbringer Company", 0x2ed},
        DialogPreset{"DoA - Gloom 2: The Rifts Between Us", 0x2f0},
        DialogPreset{"DoA - Gloom 3: To The Rescue", 0x2f1},
        DialogPreset{"DoA - City", 0x2ef},
        DialogPreset{"DoA - Veil 1: Breaching Stygian Veil", 0x2e6},
        DialogPreset{"DoA - Veil 2: Brood Wars", 0x2f3},
        DialogPreset{"DoA - Foundry 1: Foundry Of Failed Creations", 0x2e8},
        DialogPreset{"DoA - Foundry 2: Foundry Breakout", 0x2e7},
    };

    constexpr std::array custom_dialogs{
        DialogPreset{"Craft fow armor", 0x7f},
        DialogPreset{"Prof Change - Warrior", 0x184},
        DialogPreset{"Prof Change - Ranger", 0x284},
        DialogPreset{"Prof Change - Monk", 0x384},
        DialogPreset{"Prof Change - Necro", 0x484},
        DialogPreset{"Prof Change - Mesmer", 0x584},
        DialogPreset{"Prof Change - Elementalist", 0x684},
        DialogPreset{"Prof Change - Assassin", 0x784},
        DialogPreset{"Prof Change - Ritualist", 0x884},
        DialogPreset{"Prof Change - Paragon", 0x984},
        DialogPreset{"Prof Change - Dervish", 0xa84},
        DialogPreset{"Kama -> Docks @ Hahnna", 0x85},
        DialogPreset{"Docks -> Kaineng @ Mhenlo", 0x88},
        DialogPreset{"Docks -> LA Gate @ Mhenlo", 0x89},
        DialogPreset{"LA Gate -> LA @ Neiro", 0x85},
        DialogPreset{"Faction mission outpost", 0x80000b},
        DialogPreset{"Nightfall mission outpost", 0x85},
    };

    constexpr auto quest_names = [] {
        std::array<const char*, quest_dialogs.size()> names{};
        for (size_t i = 0; i < names.size(); ++i) {
            names[i] = quest_dialogs[i].name;
        }
        return names;
    }();

    constexpr auto custom_dialog_names = [] {
        std::array<const char*, custom_dialogs.size()> names{};
        for (size_t i = 0; i < names.size(); ++i) {
            names[i] = custom_dialogs[i].name;
        }
        return names;
    }();

    constexpr uint32_t quest_dialog_flag = 0x800000;
    constexpr uint32_t quest_accept_action = 0x800001;
    constexpr uint32_t quest_reward_action = 0x800007;
    constexpr auto dialog_timeout = std::chrono::seconds{3};

    GW::HookEntry dialog_hook_entry;
    GW::HookEntry raw_command_hook_entry;
    RawSendDialog raw_send_dialog = nullptr;

    std::set<uint32_t> pending_dialogs;
    uint32_t requested_dialog = 0;
    std::chrono::steady_clock::time_point dialog_started_at{};
    uint32_t dialog_agent_id = 0;
    bool dialog_open = false;
    std::vector<CapturedDialogButton> dialog_buttons;
    float dialog_button_row_width = 0.f;

    [[nodiscard]] constexpr bool IsQuestDialog(const uint32_t dialog_id)
    {
        return (dialog_id & quest_dialog_flag) != 0;
    }

    [[nodiscard]] constexpr uint32_t QuestDialog(const uint32_t quest_id, const uint32_t action)
    {
        return quest_id << 8 | action;
    }

    [[nodiscard]] bool ParseDialogId(const char* text, uint32_t& dialog_id)
    {
        if (!text || !*text) {
            return false;
        }
        errno = 0;
        char* end = nullptr;
        const auto parsed = std::strtoul(text, &end, 0);
        if (text == end || *end != '\0' || errno == ERANGE
            || parsed > static_cast<unsigned long>(std::numeric_limits<int32_t>::max())) {
            return false;
        }
        dialog_id = static_cast<uint32_t>(parsed);
        return true;
    }

    [[nodiscard]] RawSendDialog FindRawSendDialog()
    {
        const auto address = GW::Scanner::Find("\x89\x4b\x24\x8b\x4b\x28\x83\xe9\x00", "xxxxxxxxx");
        if (!GW::Scanner::IsValidPtr(address, GW::ScannerSection::Section_TEXT)) {
            return nullptr;
        }

        const auto send_agent_dialog = GW::Scanner::FunctionFromNearCall(address + 0x15, true);
        if (!send_agent_dialog || *reinterpret_cast<const uint8_t*>(send_agent_dialog + 0x25) != 0xe9) {
            return nullptr;
        }

        const auto displacement = *reinterpret_cast<const int32_t*>(send_agent_dialog + 0x26);
        const auto target = static_cast<uintptr_t>(static_cast<intptr_t>(send_agent_dialog + 0x2a) + displacement);
        if (!GW::Scanner::IsValidPtr(target, GW::ScannerSection::Section_TEXT)) {
            return nullptr;
        }
        return reinterpret_cast<RawSendDialog>(target);
    }

    void ClearPendingDialogs()
    {
        pending_dialogs.clear();
        requested_dialog = 0;
        dialog_started_at = {};
    }

    void ResetDialogState()
    {
        ClearPendingDialogs();
        dialog_agent_id = 0;
        dialog_open = false;
        dialog_buttons.clear();
    }

    void PrepareDialogSequence(const uint32_t dialog_id)
    {
        pending_dialogs.clear();
        dialog_started_at = std::chrono::steady_clock::now();
        requested_dialog = dialog_id;

        if (IsQuestDialog(dialog_id)) {
            const auto quest_id = (dialog_id ^ quest_dialog_flag) >> 8;
            pending_dialogs.insert(QuestDialog(quest_id, 0x800004));
            pending_dialogs.insert(QuestDialog(quest_id, 0x800003));
            pending_dialogs.insert(QuestDialog(quest_id, 0x800006));
            pending_dialogs.insert(QuestDialog(quest_id, quest_reward_action));
            pending_dialogs.insert(QuestDialog(quest_id, quest_accept_action));
        }
        pending_dialogs.insert(dialog_id);
    }

    void SendDialog(const uint32_t dialog_id, const bool use_raw_sender)
    {
        if (use_raw_sender && raw_send_dialog) {
            GW::GameThread::Enqueue([dialog_id] {
                if (raw_send_dialog) {
                    raw_send_dialog(dialog_id);
                }
            });
            return;
        }
        PrepareDialogSequence(dialog_id);
    }

    void ProcessPendingDialogs()
    {
        if (pending_dialogs.empty()) {
            return;
        }
        if (std::chrono::steady_clock::now() - dialog_started_at > dialog_timeout) {
            ClearPendingDialogs();
            return;
        }

        if (!dialog_open) {
            if (!pending_dialogs.contains(requested_dialog) || dialog_agent_id == 0) {
                ClearPendingDialogs();
                return;
            }
            if (const auto agent = GW::Agents::GetAgentByID(dialog_agent_id)) {
                GW::Agents::InteractAgent(agent, false);
            }
            return;
        }
        if (dialog_buttons.empty()) {
            return;
        }

        for (auto pending = pending_dialogs.begin(); pending != pending_dialogs.end(); ++pending) {
            const auto is_available = std::ranges::any_of(dialog_buttons, [pending](const auto& button) {
                return button.id == *pending;
            });
            if (is_available && GW::Agents::SendDialog(*pending)) {
                pending_dialogs.erase(pending);
                return;
            }
        }

        for (const auto& button : dialog_buttons) {
            const auto is_quest_reward = IsQuestDialog(button.id) && (button.id & 0xff) == 7;
            const auto is_continue = button.icon == 0xb || button.icon == 0xd || button.icon == 0x18;
            if (is_quest_reward || is_continue) {
                GW::Agents::SendDialog(button.id);
                return;
            }
        }
    }

    void OnDialogUIMessage(
        GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (message_id == GW::UI::UIMessage::kDialogBody) {
            dialog_buttons.clear();
            const auto packet = static_cast<GW::UI::DialogBodyInfo*>(wparam);
            if (packet && packet->message_enc) {
                dialog_agent_id = packet->agent_id;
                dialog_open = true;
            }
            else {
                dialog_open = false;
            }
            return;
        }
        if (message_id == GW::UI::UIMessage::kDialogButton && wparam) {
            const auto packet = static_cast<GW::UI::DialogButtonInfo*>(wparam);
            dialog_buttons.push_back({packet->button_icon, packet->dialog_id});
        }
    }

    void OnSendChatMessage(
        GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (!status || status->blocked || message_id != GW::UI::UIMessage::kSendChatMessage || !wparam) {
            return;
        }
        const auto packet = static_cast<GW::UI::UIPacket::kSendChatMessage*>(wparam);
        const auto wide_message = packet->message;
        if (!wide_message || !*wide_message || GW::Chat::GetChannel(*wide_message) != GW::Chat::CHANNEL_COMMAND) {
            return;
        }

        const auto message = PluginUtils::WStringToString(wide_message);
        constexpr auto prefix = std::string_view{"/rawdialog "};
        if (!message.starts_with(prefix)) {
            return;
        }

        uint32_t dialog_id = 0;
        if (!ParseDialogId(message.c_str() + prefix.size(), dialog_id)) {
            return;
        }
        status->blocked = true;
        if (raw_send_dialog) {
            raw_send_dialog(dialog_id);
        }
    }

    void DialogButton(
        const size_t column, const size_t columns, const char* label, const char* help,
        const uint32_t dialog_id, const bool use_raw_sender)
    {
        if (column == 0) {
            dialog_button_row_width = ImGui::GetContentRegionAvail().x;
        }
        else {
            ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);
        }
        const auto spacing = ImGui::GetStyle().ItemInnerSpacing.x * static_cast<float>(columns - 1);
        const auto width = (dialog_button_row_width - spacing) / static_cast<float>(columns);
        if (ImGui::Button(label, ImVec2(width, 0.f))) {
            SendDialog(dialog_id, use_raw_sender);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", help);
        }
    }

    void ShowHelp(const char* help)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", help);
        }
    }
}

Dialogs::Dialogs()
{
    can_close = true;
    can_show_in_main_window = true;
}

void Dialogs::Initialize(
    ImGuiContext* context, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(context, allocator_fns, toolbox_dll);

    ResetDialogState();
    raw_send_dialog = FindRawSendDialog();
    GW::UI::RegisterUIMessageCallback(
        &dialog_hook_entry, GW::UI::UIMessage::kDialogBody, OnDialogUIMessage, 0x500);
    GW::UI::RegisterUIMessageCallback(
        &dialog_hook_entry, GW::UI::UIMessage::kDialogButton, OnDialogUIMessage, 0x500);
    GW::UI::RegisterUIMessageCallback(
        &raw_command_hook_entry, GW::UI::UIMessage::kSendChatMessage, OnSendChatMessage, -0x8000);
}

void Dialogs::SignalTerminate()
{
    GW::UI::RemoveUIMessageCallback(&raw_command_hook_entry, GW::UI::UIMessage::kSendChatMessage);
    GW::UI::RemoveUIMessageCallback(&dialog_hook_entry, GW::UI::UIMessage::kDialogBody);
    GW::UI::RemoveUIMessageCallback(&dialog_hook_entry, GW::UI::UIMessage::kDialogButton);
    ResetDialogState();
    raw_send_dialog = nullptr;
    ToolboxUIPlugin::SignalTerminate();
}

void Dialogs::Update(float)
{
    if (!pending_dialogs.empty()) {
        GW::GameThread::Enqueue(ProcessPendingDialogs);
    }
}

void Dialogs::Draw(IDirect3DDevice9*)
{
    const auto visible = GetVisiblePtr();
    if (!visible || !*visible) {
        return;
    }

    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300.f, 0.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), show_closebutton ? visible : nullptr, GetWinFlags())) {
        struct CommonDialog {
            const char* label;
            const char* help;
            uint32_t id;
            const bool* shown;
        };
        const std::array common_dialogs{
            CommonDialog{"Four Horseman", "Take quest in Planes", 0x806a01, &show_four_horsemen},
            CommonDialog{"Demon Assassin", "Take quest in Mountains", 0x806801, &show_demon_assassin},
            CommonDialog{"Tower of Strength", "Take quest", 0x80d301, &show_tower_of_strength},
            CommonDialog{"Foundry Reward", "Accept reward", 0x82e707, &show_foundry_reward},
            CommonDialog{"Dhuum", "Take quest", 0x846901, &show_dhuum},
        };
        const auto common_count = static_cast<size_t>(std::ranges::count_if(common_dialogs, [](const auto& dialog) {
            return *dialog.shown;
        }));
        auto common_index = size_t{0};
        for (const auto& dialog : common_dialogs) {
            if (!*dialog.shown) {
                continue;
            }
            const auto column = common_index % 2;
            const auto columns = common_index + 1 == common_count && column == 0 ? size_t{1} : size_t{2};
            DialogButton(column, columns, dialog.label, dialog.help, dialog.id, use_function_ptr);
            ++common_index;
        }
        if (common_count != 0) {
            ImGui::Separator();
        }

        if (show_uwteles) {
            DialogButton(0, 4, "Lab", "Teleport Lab", 0x8d, use_function_ptr);
            DialogButton(1, 4, "Vale", "Teleport Vale", 0x91, use_function_ptr);
            DialogButton(2, 4, "Pits", "Teleport Pits", 0x8f, use_function_ptr);
            DialogButton(3, 4, "Pools", "Teleport Pools", 0x90, use_function_ptr);
            DialogButton(0, 3, "Planes", "Teleport Planes", 0x8b, use_function_ptr);
            DialogButton(1, 3, "Wastes", "Teleport Wastes", 0x8c, use_function_ptr);
            DialogButton(
                2, 3, "Mountains", "Teleport Mountains\nThis is NOT the mountains quest", 0x8e,
                use_function_ptr);
            ImGui::Separator();
        }

        if (show_favorites) {
            for (auto i = 0; i < fav_count; ++i) {
                ImGui::PushID(i);
                const auto index = static_cast<size_t>(i);
                ImGui::SetNextItemWidth(-100.f - 2.f * ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::Combo(
                    "", &fav_index[index], quest_names.data(), static_cast<int>(quest_names.size()));
                ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Take", ImVec2(40.f, 0.f))) {
                    SendDialog(
                        QuestDialog(quest_dialogs[static_cast<size_t>(fav_index[index])].id, quest_accept_action),
                        use_function_ptr);
                }
                ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Reward", ImVec2(60.f, 0.f))) {
                    SendDialog(
                        QuestDialog(quest_dialogs[static_cast<size_t>(fav_index[index])].id, quest_reward_action),
                        use_function_ptr);
                }
                ImGui::PopID();
            }
            ImGui::Separator();
        }

        if (show_custom) {
            ImGui::SetNextItemWidth(-60.f - ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::Combo(
                "###dialogcombo", &custom_dialog_index, custom_dialog_names.data(),
                static_cast<int>(custom_dialog_names.size()));
            ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Button("Send##1", ImVec2(60.f, 0.f))) {
                SendDialog(custom_dialogs[static_cast<size_t>(custom_dialog_index)].id, use_function_ptr);
            }

            ImGui::SetNextItemWidth(-60.f - ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::InputText(
                "###dialoginput", custom_dialog_buffer.data(), custom_dialog_buffer.size(),
                ImGuiInputTextFlags_None);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "You can prefix the number by \"0x\" to specify an hexadecimal number");
            }
            ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Button("Send##2", ImVec2(60.f, 0.f))) {
                uint32_t dialog_id = 0;
                if (ParseDialogId(custom_dialog_buffer.data(), dialog_id)) {
                    SendDialog(dialog_id, use_function_ptr);
                }
            }
        }
    }
    ImGui::End();
}

void Dialogs::DrawSettings()
{
    ToolboxUIPlugin::DrawSettings();

    ImGui::SetNextItemWidth(100.f);
    if (ImGui::InputInt("Number of favorites", &fav_count)) {
        fav_count = std::clamp(fav_count, 0, 100);
        fav_index.resize(static_cast<size_t>(fav_count), 0);
    }

    ImGui::TextUnformatted("Show common dialog:");
    ImGui::SameLine();
    ImGui::Checkbox("Four Horseman", &show_four_horsemen);
    ImGui::SameLine();
    ImGui::Checkbox("Demon Assassin", &show_demon_assassin);
    ImGui::SameLine();
    ImGui::Checkbox("Tower of Strength", &show_tower_of_strength);
    ImGui::SameLine();
    ImGui::Checkbox("Foundry Reward", &show_foundry_reward);
    ImGui::SameLine();
    ImGui::Checkbox("Dhuum", &show_dhuum);

    ImGui::TextUnformatted("Show:");
    ImGui::SameLine();
    ImGui::Checkbox("UW Teles", &show_uwteles);
    ImGui::SameLine();
    ImGui::Checkbox("Favorites", &show_favorites);
    ImGui::SameLine();
    ImGui::Checkbox("Custom", &show_custom);

    ImGui::Checkbox("Send dialogs in combat", &use_function_ptr);
    ShowHelp(
        "Allows to send more dialogs, for example to allies in combat. Might flag your account, use at your own risk.");

    ImGui::Separator();
    ImGui::TextUnformatted("Example usage:");
    ImGui::TextUnformatted("Send dialog in decimal notation: /rawdialog 8416257");
    ImGui::TextUnformatted("Send dialog in hexadecimal notation: /rawdialog 0x806501");
    ImGui::TextUnformatted("Version 2.0.1");
}

void Dialogs::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);

    fav_count = 3;
    LoadSetting("fav_count", fav_count);
    fav_count = std::clamp(fav_count, 0, 100);
    fav_index.assign(static_cast<size_t>(fav_count), 0);
    for (size_t i = 0; i < fav_index.size(); ++i) {
        char key[32]{};
        std::snprintf(key, std::size(key), "Quest%zu", i);
        LoadSetting(key, fav_index[i]);
        fav_index[i] = std::clamp(fav_index[i], 0, static_cast<int>(quest_dialogs.size() - 1));
    }

    LoadSetting("show_four_horsemen", show_four_horsemen);
    LoadSetting("show_foundry_reward", show_foundry_reward);
    LoadSetting("show_tower_of_strength", show_tower_of_strength);
    LoadSetting("show_demon_assassin", show_demon_assassin);
    LoadSetting("show_dhuum", show_dhuum);
    LoadSetting("show_uwteles", show_uwteles);
    LoadSetting("show_favorites", show_favorites);
    LoadSetting("show_custom", show_custom);
    LoadSetting("useFunctionPtr", use_function_ptr);
}

void Dialogs::SaveSettings(const wchar_t* folder)
{
    SaveSetting("fav_count", fav_count);
    for (size_t i = 0; i < fav_index.size(); ++i) {
        char key[32]{};
        std::snprintf(key, std::size(key), "Quest%zu", i);
        SaveSetting(key, fav_index[i]);
    }
    SaveSetting("show_four_horsemen", show_four_horsemen);
    SaveSetting("show_foundry_reward", show_foundry_reward);
    SaveSetting("show_tower_of_strength", show_tower_of_strength);
    SaveSetting("show_demon_assassin", show_demon_assassin);
    SaveSetting("show_dhuum", show_dhuum);
    SaveSetting("show_uwteles", show_uwteles);
    SaveSetting("show_favorites", show_favorites);
    SaveSetting("show_custom", show_custom);
    SaveSetting("useFunctionPtr", use_function_ptr);

    ToolboxUIPlugin::SaveSettings(folder);
}
