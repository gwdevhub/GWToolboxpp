#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/StoC.h>

#include <Utils/GuiUtils.h>
#include <Defines.h>
#include <Widgets/FavorTracker.h>
#include <Modules/AudioSettings.h>
#include <Utils/FontLoader.h>
#include <Utils/ToolboxUtils.h>
#include <Utils/ArenaNetFileParser.h>
#include <ImGuiAddons.h>
#include <Timer.h>
#include <Color.h>
#include <Modules/Resources.h>

namespace {

    constexpr uint32_t MAX_ACHIEVEMENTS_NEEDED_FOR_FAVOR = 20;

    float text_size = 24.f;
    Color text_color = IM_COL32_WHITE;
    bool hide_if_no_favor = false;

    uint32_t last_favor_minutes = 0;
    uint32_t last_favor_achievements_needed = 0;
    std::string favor_str;

    bool enabled = true;
    bool play_sound_on_favor = false;
    uint32_t favor_sound_file_id = 0;
    uint32_t default_favor_sound_file_id = 111073;

    ArenaNetFileParser::GameAssetFile favor_sound_file;

    bool isMp3(const ArenaNetFileParser::GameAssetFile& file)
    {
        const auto fileType = file.fileType();
        return fileType && fileType[0] == '\xFF' && (fileType[1] & '\xE6') == '\xE2';
    }

    int poll_interval_ms = 30000;

    GuiUtils::EncString favor_enc_str;

    uint32_t favor_minutes_remaining = 0;
    uint32_t achievements_needed_for_favor = 0;

    clock_t last_poll_time = TIMER_INIT() - poll_interval_ms;
    clock_t pending_favor = 0;
    clock_t initialised_at = 0;

    GW::HookEntry OnMessageServer_Entry;
    GW::HookEntry OnUIMessage_Entry;

    uint32_t ExtractEncodedValue(const wchar_t* msg)
    {
        if (!msg) return 0;
        for (size_t i = 0; msg[i]; i++) {
            if (msg[i] == 0x101 && msg[i + 1] >= 0x100) {
                return GW::UI::EncStrToUInt32(&msg[i + 1]);
            }
        }
        return 0;
    }

    void SetFavorActive(uint32_t minutes = 0, uint32_t achievements_needed = 0)
    {
        if (minutes == favor_minutes_remaining && achievements_needed == achievements_needed_for_favor) return;

        const bool is_active = minutes > 0;
        const bool was_active = favor_minutes_remaining > 0;
        favor_minutes_remaining = minutes;
        achievements_needed_for_favor = minutes == 0 ? achievements_needed : 0;

        if (is_active && !was_active && play_sound_on_favor && favor_sound_file_id && TIMER_DIFF(initialised_at) > 3000) {
            AudioSettings::PlaySoundFileId(favor_sound_file_id);
        }
        favor_enc_str.language(GW::UI::GetTextLanguage());
        if (favor_minutes_remaining > 0) {
            wchar_t buf[3];
            ASSERT(GW::UI::UInt32ToEncStr(favor_minutes_remaining, buf, _countof(buf)));
            favor_enc_str.reset(std::format(L"\x8102\x223f\x101{}", buf).c_str(), false);
        }
        else if (achievements_needed_for_favor > 0) {
            wchar_t buf[3];
            ASSERT(GW::UI::UInt32ToEncStr(achievements_needed_for_favor, buf, _countof(buf)));
            favor_enc_str.reset(std::format(L"\x8102\x2240\x101{}", buf).c_str(), false);
        }
        else {
            favor_enc_str.reset(L"\x101", false);
        }
    }

    bool IsRequestedFavorMessage(const wchar_t* msg)
    {
        if (!msg || !*msg) return false;
        if (wcsncmp(L"\x8102\x223f", msg, 2) == 0) {
            // "x minutes of favor of the gods remaining" (/favor)
            SetFavorActive(ExtractEncodedValue(msg));
            return true;
        }
        if (wcsncmp(L"\x8102\x2240", msg, 2) == 0) {
            // "x more achievements must be performed to earn the favor of the gods" (/favor)
            SetFavorActive(false, ExtractEncodedValue(msg));
            return true;
        }
        return false;
    }

    bool ParseFavorMessage(const wchar_t* msg)
    {
        if (!msg || !*msg) return false;
        if (IsRequestedFavorMessage(msg)) return true;
        if (wcsncmp(L"\x8101\x7b91", msg, 2) == 0) {
            // "x minutes of favor of the gods remaining" (broadcast)
            SetFavorActive(ExtractEncodedValue(msg));
            return true;
        }
        if (wcsncmp(L"\x8101\x7b92", msg, 2) == 0) {
            // "x more achievements must be performed to earn the favor of the gods" (broadcast)
            SetFavorActive(false, ExtractEncodedValue(msg));
            return true;
        }
        return false;
    }

    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        const wchar_t* message = 0;
        switch (message_id) {
            case GW::UI::UIMessage::kWriteToChatLog: {
                message = (static_cast<GW::UI::UIPacket::kWriteToChatLog*>(wparam))->message;
            } break;
            case GW::UI::UIMessage::kPrintChatMessage: {
                message = (static_cast<GW::UI::UIPacket::kPrintChatMessage*>(wparam))->message;
            } break;
            case GW::UI::UIMessage::kLogChatMessage: {
                message = (static_cast<GW::UI::UIPacket::kLogChatMessage*>(wparam))->message;
            } break;
            default:
                return;
        }
        if (ParseFavorMessage(message) && IsRequestedFavorMessage(message) && (pending_favor && TIMER_DIFF(pending_favor) < 3000)) {
            pending_favor = 0;
            status->blocked = true;
        }
    }

    void CheckFavor()
    {
        last_poll_time = pending_favor = TIMER_INIT();
        GW::Chat::SendChat('/', L"favor");
    }

} // namespace

void FavorTracker::Initialize()
{
    ToolboxWidget::Initialize();

    favor_sound_file_id = default_favor_sound_file_id;

    constexpr GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kPrintChatMessage,
        GW::UI::UIMessage::kLogChatMessage,
        GW::UI::UIMessage::kWriteToChatLog
    };
    for (const auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_Entry, message_id, OnPreUIMessage, -0x4500);
    }
}

void FavorTracker::SignalTerminate()
{
    ToolboxWidget::SignalTerminate();

    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageServer>(&OnMessageServer_Entry);
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Entry);
}

void FavorTracker::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
    if (hide_if_no_favor && !GetFavorMinutes()) return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 60.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
        const auto start_pos = ImGui::GetCursorPosY();
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        const auto label_fs = text_size * 0.55f;
        ImGui::PushFont(FontLoader::GetFontByPx(label_fs), label_fs);
        ImGui::TextShadowed(Resources::GetSkillName(GW::Constants::SkillID::Favor_of_the_Gods)->string().c_str());
        ImGui::PopFont();
        ImGui::SetCursorPosY(start_pos + text_size * 0.45f);
        ImGui::PushFont(FontLoader::GetFontByPx(text_size), text_size);
        ImGui::TextShadowed(favor_str.c_str());
        ImGui::PopFont();
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered() && !ToolboxUtils::IsExplorable()) {
            ImGui::SetTooltip(GetFavorMessage());
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void FavorTracker::Update(float)
{
    if (enabled) {
        if (!GW::Map::GetIsMapLoaded()) return;
        if (!initialised_at) initialised_at = TIMER_INIT();
        if (TIMER_DIFF(last_poll_time) >= poll_interval_ms) {
            CheckFavor();
        }
    }

    static clock_t fps_check = TIMER_INIT();
    if (!ToolboxUtils::FrameRateCheck(fps_check, 10)) return;
    const uint32_t minutes = GetFavorMinutes();
    const uint32_t achievements_needed = GetFavorAchievementsNeeded();
    if (last_favor_minutes == minutes && last_favor_achievements_needed == achievements_needed) return;
    last_favor_minutes = minutes;
    last_favor_achievements_needed = achievements_needed;
    if (minutes) {
        GW::UI::AsyncDecodeStrS(ToolboxUtils::TimeToEncString(minutes * 60).c_str(), &favor_str);
    }
    else {
        favor_str = std::format("{}/{}", MAX_ACHIEVEMENTS_NEEDED_FOR_FAVOR - achievements_needed, MAX_ACHIEVEMENTS_NEEDED_FOR_FAVOR);
    }
}

void FavorTracker::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_FLOAT(text_size);
    LOAD_BOOL(enabled);
    LOAD_BOOL(play_sound_on_favor);
    LOAD_UINT(favor_sound_file_id);
}

void FavorTracker::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_FLOAT(text_size);
    SAVE_BOOL(enabled);
    SAVE_BOOL(play_sound_on_favor);
    SAVE_UINT(favor_sound_file_id);
}

void FavorTracker::DrawSettingsInternal()
{
    ImGui::DragFloat("Text size", &text_size, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
    Colors::DrawSettingHueWheel("Text color", &text_color);
    ImGui::Checkbox("Hide when the world has no favor", &hide_if_no_favor);

    ImGui::Separator();

    ImGui::Checkbox("Enable Favor Tracking", &enabled);
    ImGui::ShowHelp("Periodically runs /favor to check Favor of the Gods status. Automated queries are hidden from chat.");

    ImGui::Checkbox("Play sound on favor activation", &play_sound_on_favor);
    if (play_sound_on_favor) {
        ImGui::InputInt("Sound File ID", (int*)&favor_sound_file_id);
        if (favor_sound_file.file_id != favor_sound_file_id) {
            favor_sound_file.readFromDat(favor_sound_file_id);
            ASSERT(favor_sound_file.file_id == favor_sound_file_id);
        }
        if (favor_sound_file.file_id) {
            ImGui::Indent();
            if (!isMp3(favor_sound_file)) {
                ImGui::TextColored(ImVec4(255, 255, 0, 255), ICON_FA_TIMES "  File ID %d is not a valid sound file", favor_sound_file.file_id);
            }
            else {
                ImGui::TextColored(ImVec4(0, 255, 0, 255), ICON_FA_CHECK "  File ID %d is a valid sound file", favor_sound_file.file_id);
            }
            ImGui::Unindent();
        }
    }
    ImGui::SameLine();
    if (favor_sound_file_id && ImGui::Button("Play")) {
        AudioSettings::PlaySoundFileId(favor_sound_file_id);
    }
    bool reset = false;
    if (default_favor_sound_file_id != favor_sound_file_id && (ImGui::SameLine(), ImGui::ConfirmButton("Reset", &reset))) {
        favor_sound_file_id = default_favor_sound_file_id;
    }
    ImGui::Separator();
    ImGui::TextUnformatted(GetFavorMessage());
    ImGui::SameLine();
    if (ImGui::Button("Check Now")) {
        CheckFavor();
    }
}

uint32_t FavorTracker::GetFavorMinutes()
{
    return favor_minutes_remaining;
}
uint32_t FavorTracker::GetFavorAchievementsNeeded()
{
    return achievements_needed_for_favor;
}

const wchar_t* FavorTracker::GetFavorMessageW()
{
    return favor_enc_str.wstring().c_str();
}

const char* FavorTracker::GetFavorMessage()
{
    return favor_enc_str.string().c_str();
}
