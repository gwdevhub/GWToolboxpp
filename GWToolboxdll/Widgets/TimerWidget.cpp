#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>
#include <Logger.h>
#include <Timer.h>
#include <Defines.h>

#include <Modules/GameSettings.h>
#include <Widgets/TimerWidget.h>

#include <Utils/FontLoader.h>

using namespace std::chrono;

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    steady_clock::time_point now() { return steady_clock::now(); }

    bool instance_timer_valid = true;

    bool is_valid(const steady_clock::time_point& time) { return time.time_since_epoch().count(); }

    template <typename T>
    void print_time(T duration, const int decimals, const unsigned int bufsize, char* buf)
    {
        auto time = duration_cast<milliseconds>(duration);
        const long long secs = duration_cast<seconds>(time).count() % 60;
        const auto mins = duration_cast<minutes>(time).count() % 60;
        const auto hrs = duration_cast<hours>(time).count();
        switch (decimals) {
            case 1:
                snprintf(buf, bufsize, "%d:%02d:%02lld.%01lld", hrs, mins, secs, time.count() / 100 % 10);
                break;
            case 2:
                snprintf(buf, bufsize, "%d:%02d:%02lld.%02lld", hrs, mins, secs, time.count() / 10 % 100);
                break;
            case 3:
                snprintf(buf, bufsize, "%d:%02d:%02lld.%03lld", hrs, mins, secs, time.count() % 1000);
                break;
            default: // 0
                snprintf(buf, bufsize, "%d:%02d:%02lld", hrs, mins, secs);
                break;
        }
    }

    // 这些函数写入 extra_buffer 和 extra_color。
    // 如果有内容需要绘制则返回 true。

    std::map<GW::Constants::SkillID, const char*> spirit_effects{
        {GW::Constants::SkillID::Edge_of_Extinction, "灭绝之刃"},
        {GW::Constants::SkillID::Quickening_Zephyr, "疾风"},
        {GW::Constants::SkillID::Famine, "饥荒"},
        {GW::Constants::SkillID::Symbiosis, "共生"},
        {GW::Constants::SkillID::Winnowing, "筛选"},
        {GW::Constants::SkillID::Union, "联合"},
        {GW::Constants::SkillID::Shelter, "庇护"},
        {GW::Constants::SkillID::Displacement, "位移"},
        {GW::Constants::SkillID::Life, "生命"},
        {GW::Constants::SkillID::Recuperation, "恢复"},
        {GW::Constants::SkillID::Winds, "风"},
        {GW::Constants::SkillID::Frozen_Soil, "冻土"}
    };

    TimerWidget::Settings settings;
    std::map<GW::Constants::SkillID, bool> spirit_effects_enabled{
        {GW::Constants::SkillID::Edge_of_Extinction, true},
        {GW::Constants::SkillID::Quickening_Zephyr, true}
    };

    char timer_buffer[32] = "";
    char extra_buffer[32] = "";
    char spirits_buffer[128] = "";
    ImColor extra_color = 0;

    bool reset_next_loading_screen = false;
    bool in_explorable = false;
    bool in_dungeon = false;

    std::chrono::steady_clock::time_point run_started, run_completed, instance_started;

    unsigned long cave_start = 0; // 洞穴开始时的实例计时器值
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry PreGameSrvTransfer_Entry;
    GW::HookEntry InstanceTimer_Entry;
    GW::HookEntry PostGameSrvTransfer_Entry;
    const uint32_t CAVE_SPAWN_INTERVALS[12] = {12, 12, 12, 12, 12, 12, 10, 10, 10, 10, 10, 10};

    bool GetUrgozTimer()
    {
        if (GW::Map::GetMapID() != GW::Constants::MapID::Urgozs_Warren) {
            return false;
        }
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            return false;
        }
        const unsigned long time = GW::Map::GetInstanceTime() / 1000;
        const unsigned long temp = (time - 1) % 25;
        if (temp < 15) {
            snprintf(extra_buffer, 32, "开启 - %lu", 15u - temp);
            extra_color = ImColor(0, 255, 0);
        }
        else {
            snprintf(extra_buffer, 32, "关闭 - %lu", 25u - temp);
            extra_color = ImColor(255, 0, 0);
        }
        return true;
    }

    bool GetSpiritTimer()
    {
        using namespace GW::Constants;

        if (!settings.show_spirit_timers || GW::Map::GetInstanceType() != InstanceType::Explorable) {
            return false;
        }

        GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
        if (!effects) {
            return false;
        }

        int offset = 0;
        for (auto& effect : *effects) {
            if (!effect.duration) {
                continue;
            }
            SkillID effect_id = effect.skill_id;
            auto spirit_effect_enabled = spirit_effects_enabled.find(effect_id);
            if (spirit_effect_enabled == spirit_effects_enabled.end() || !spirit_effect_enabled->second) {
                continue;
            }
            offset += snprintf(&spirits_buffer[offset], sizeof(spirits_buffer) - offset - 1, "%s%s：%d 秒", offset ? "\n" : "", spirit_effects[effect_id], effect.GetTimeRemaining() / 1000);
        }
        if (!offset) {
            return false;
        }
        spirits_buffer[offset] = 0;
        return true;
    }

    bool GetDeepTimer()
    {
        using namespace GW::Constants;

        if (GW::Map::GetMapID() != MapID::The_Deep) {
            return false;
        }
        if (GW::Map::GetInstanceType() != InstanceType::Explorable) {
            return false;
        }

        GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
        if (!effects) {
            return false;
        }

        static clock_t start = -1;
        auto skill = SkillID::No_Skill;
        for (const auto& effect : *effects) {
            const auto effect_id = effect.skill_id;
            switch (effect_id) {
            case SkillID::Aspect_of_Exhaustion:
            case SkillID::Aspect_of_Depletion_energy_loss:
            case SkillID::Scorpion_Aspect:
                skill = effect_id;
                break;
            default:
                break;
            }
            if (skill != SkillID::No_Skill) {
                break;
            }
        }
        if (skill == SkillID::No_Skill) {
            start = -1;
            return false;
        }

        if (start == -1) {
            start = TIMER_INIT();
        }

        const clock_t diff = TIMER_DIFF(start) / 1000;

        long timer = 30 - diff % 30;
        if (diff > 100) {
            timer = std::min(timer, 30 - (diff - 100) % 30);
        }
        if (diff > 200) {
            timer = std::min(timer, 30 - (diff - 200) % 30);
        }
        switch (skill) {
        case SkillID::Aspect_of_Exhaustion:
            snprintf(extra_buffer, 32, "力竭：%lu", timer);
            break;
        case SkillID::Aspect_of_Depletion_energy_loss:
            snprintf(extra_buffer, 32, "衰竭：%lu", timer);
            break;
        case SkillID::Scorpion_Aspect:
            snprintf(extra_buffer, 32, "蝎子：%lu", timer);
            break;
        default:
            break;
        }
        extra_color = ImColor(255, 255, 255);
        return true;
    }

    bool GetDhuumTimer()
    {
        // TODO：实现
        return false;
    }

    bool GetTrapTimer()
    {
        using namespace GW::Constants;
        if (GW::Map::GetInstanceType() != InstanceType::Explorable) {
            return false;
        }

        const unsigned long time = GW::Map::GetInstanceTime() / 1000;
        const unsigned long temp = time % 20;
        unsigned long timer;
        if (temp < 10) {
            timer = 10u - temp;
            extra_color = ImColor(0, 255, 0);
        }
        else {
            timer = 20u - temp;
            extra_color = ImColor(255, 0, 0);
        }

        switch (GW::Map::GetMapID()) {
        case MapID::Catacombs_of_Kathandrax_Level_1:
        case MapID::Catacombs_of_Kathandrax_Level_2:
        case MapID::Catacombs_of_Kathandrax_Level_3:
        case MapID::Bloodstone_Caves_Level_1:
        case MapID::Arachnis_Haunt_Level_2:
        case MapID::Oolas_Lab_Level_2:
            snprintf(extra_buffer, 32, "火焰喷射：%lu", timer);
            return true;
        case MapID::Heart_of_the_Shiverpeaks_Level_3:
            snprintf(extra_buffer, 32, "火焰喷涌：%lu", timer);
            return true;
        case MapID::Shards_of_Orr_Level_3:
        case MapID::Cathedral_of_Flames_Level_3:
            snprintf(extra_buffer, 32, "火焰陷阱：%lu", timer);
            return true;
        case MapID::Sepulchre_of_Dragrimmar_Level_1:
        case MapID::Ravens_Point_Level_1:
        case MapID::Ravens_Point_Level_2:
        case MapID::Heart_of_the_Shiverpeaks_Level_1:
        case MapID::Darkrime_Delves_Level_2:
            snprintf(extra_buffer, 32, "寒冰喷射：%lu", timer);
            return true;
        case MapID::Darkrime_Delves_Level_1:
        case MapID::Secret_Lair_of_the_Snowmen:
            snprintf(extra_buffer, 32, "寒冰喷涌：%lu", timer);
            return true;
        case MapID::Bogroot_Growths_Level_1:
        case MapID::Arachnis_Haunt_Level_1:
        case MapID::Shards_of_Orr_Level_1:
        case MapID::Shards_of_Orr_Level_2:
            snprintf(extra_buffer, 32, "剧毒喷射：%lu", timer);
            return true;
        case MapID::Bloodstone_Caves_Level_2:
            snprintf(extra_buffer, 32, "剧毒喷涌：%lu", timer);
            return true;
        case MapID::Cathedral_of_Flames_Level_2:
        case MapID::Bloodstone_Caves_Level_3:
            snprintf(extra_buffer, 32, "剧毒陷阱：%lu", timer);
            return true;
        default:
            return false;
        }
    }

    bool GetDoATimer()
    {
        using namespace GW::Constants;

        if (GW::Map::GetInstanceType() != InstanceType::Explorable) {
            return false;
        }
        if (GW::Map::GetMapID() != MapID::Domain_of_Anguish) {
            return false;
        }
        if (cave_start == 0) {
            return false;
        }

        const uint32_t time = GW::Map::GetInstanceTime();

        uint32_t currentWave = 0;
        uint32_t time_since_previous_wave = (time - cave_start) / 1000;
        for (size_t i = 0; i < _countof(CAVE_SPAWN_INTERVALS); i++) {
            if (time_since_previous_wave < CAVE_SPAWN_INTERVALS[i]) {
                break;
            }
            time_since_previous_wave -= CAVE_SPAWN_INTERVALS[i];
            ++currentWave;
        }

        if (currentWave >= _countof(CAVE_SPAWN_INTERVALS)) {
            return false;
        }

        uint32_t timer = 0;
        if (time_since_previous_wave < CAVE_SPAWN_INTERVALS[currentWave]) {
            timer = CAVE_SPAWN_INTERVALS[currentWave] - time_since_previous_wave;
        }

        snprintf(extra_buffer, 32, "波次 %d：%d", currentWave + 1, timer);
        extra_color = ImColor(255, 255, 255);

        return true;
    }
}

// 地图切换前调用
void TimerWidget::OnPreGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*)
{
    if (settings.print_time_zoning && in_explorable && !is_valid(run_completed)) {
        // 在真正重置之前执行
        PrintTimer();
    }
    run_completed = now();
}

// 地图切换后调用
void TimerWidget::OnPostGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* pak)
{
    cave_start = 0; // 重置痛苦领域洞穴计时器
    instance_timer_valid = false;
    const steady_clock::time_point now_tp = now();
    run_completed = steady_clock::time_point();

    instance_started = now_tp;

    // 如果 reset_next_loading_screen 为 true，则无论 never_reset 如何都重置
    if (reset_next_loading_screen) {
        run_started = now_tp;
        reset_next_loading_screen = false;
    }

    if (!settings.never_reset) {
        if (pak->is_explorable && !in_explorable) {
            // 从前哨站进入探索区域
            run_started = now_tp;
        }
        else if (!pak->is_explorable) {
            // 回到前哨站
            run_started = now_tp;
        }

        const GW::AreaInfo* info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(pak->map_id));
        if (info) {
            const bool new_in_dungeon = info->type == GW::RegionType::Dungeon;

            if (new_in_dungeon && !in_dungeon) {
                // 从探索区域进入地城
                run_started = now_tp;
            }

            in_dungeon = new_in_dungeon;
        }
    }
    if (!is_valid(run_started)) {
        run_started = now_tp;
    }

    in_explorable = pak->is_explorable;
}

void TimerWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
    for (const auto& skill_id : spirit_effects | std::views::keys) {
        spirit_effects_enabled.try_emplace(skill_id, false);
    }

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(
        &DisplayDialogue_Entry,
        [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* packet) -> void {
            if (GW::Map::GetMapID() != GW::Constants::MapID::Domain_of_Anguish) {
                return;
            }
            if (packet->message[1] != 0x5765) {
                return;
            }
            cave_start = GW::Map::GetInstanceTime();
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &PreGameSrvTransfer_Entry,
        [](GW::HookStatus* status, GW::Packet::StoC::GameSrvTransfer* pak) -> void {
            Instance().OnPreGameSrvTransfer(status, pak);
        }, -0x10);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &PostGameSrvTransfer_Entry,
        [](GW::HookStatus* status, GW::Packet::StoC::GameSrvTransfer* pak) -> void {
            Instance().OnPostGameSrvTransfer(status, pak);
        }, 0x5);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceTimer>(
        &InstanceTimer_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::InstanceTimer*) -> void {
            instance_timer_valid = true;
        }, 5);
    in_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"resettimer", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        reset_next_loading_screen = true;
        Log::Flash("将在下一次加载画面时重置计时器。");
    });
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"timerreset", [](GW::HookStatus*,const wchar_t*, const int, const LPWSTR*) {
        reset_next_loading_screen = true;
    });
    if (!is_valid(run_started)) {
        run_started = now() - milliseconds(GW::Map::GetInstanceTime());
    }
}

void TimerWidget::Terminate() {
    ToolboxWidget::Terminate();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

void TimerWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    settings.show_decimals = std::clamp(settings.show_decimals, 0, 3);
    for (const auto& effect_id : spirit_effects | std::views::keys) {
        char ini_name[32];
        snprintf(ini_name, 32, "spirit_effect_%d", effect_id);
        if (!doc.Get(Name(), ini_name, spirit_effects_enabled[effect_id]) && legacy) {
            spirit_effects_enabled[effect_id] = legacy->GetBoolValue(Name(), ini_name, spirit_effects_enabled[effect_id]);
        }
    }
}

void TimerWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    for (const auto& effect_id : spirit_effects | std::views::keys) {
        char ini_name[32];
        snprintf(ini_name, 32, "spirit_effect_%d", effect_id);
        doc.Set(Name(), ini_name, spirit_effects_enabled[effect_id]);
    }
}

void TimerWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();

    ImGui::DragFloat("文字大小", &settings.font_size, 1.0f, FontLoader::text_size_min, FontLoader::text_size_max, "%.0f");
    ImGui::Checkbox("在前哨站隐藏", &settings.hide_in_outpost);
    if (ImGui::RadioButton("实例计时器", settings.use_instance_timer)) {
        settings.use_instance_timer = true;
    }
    if (ImGui::RadioButton("实时计时器", !settings.use_instance_timer)) {
        settings.use_instance_timer = false;
    }
    ImGui::ShowHelp("实时计时器在探索区域之间传送时不会重置。\n可以使用 /resettimer 在下次加载画面时强制重置。");
    ImGui::Indent();
    ImGui::CheckboxWithHelp("永不重置", &settings.never_reset,
        "进入前哨站、探索区域（从前哨站）和地城时不重置。\n用于计时较长的跑图。\n需要取消勾选上面的\"使用实例计时器\"");
    ImGui::Checkbox("目标完成时停止", &settings.stop_at_objective_completion);
    ImGui::Checkbox("同时显示实例计时器", &settings.also_show_instance_timer);
    ImGui::Unindent();
    if (ImGui::SliderInt("显示小数位", &settings.show_decimals, 0, 3)) {
        settings.show_decimals = std::clamp(settings.show_decimals, 0, 3);
    }
    ImGui::Text("打印时间：");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Ctrl+点击计时器时", &settings.click_to_print_time);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("目标完成时", &settings.print_time_objective);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("离开探索区域时", &settings.print_time_zoning);
    ImGui::Unindent();

    ImGui::DragFloat("额外计时器文字大小", &settings.font_size_extra_timers, 1.0f, FontLoader::text_size_min, FontLoader::text_size_max, "%.0f");
    ImGui::Text("显示额外计时器：");
    ImGui::Indent();

    const std::vector<std::pair<const char*, bool*>> timers = {
        {"深渊面相", &settings.show_deep_timer},
        {"痛苦领域洞穴", &settings.show_doa_timer},
        {"杜姆", &settings.show_dhuum_timer},
        {"厄戈兹门", &settings.show_urgoz_timer},
        {"地城陷阱", &settings.show_dungeon_traps_timer}
    };
    ImGui::StartSpacedElements(140.f);
    for (size_t i = 0; i < timers.size(); i++) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox(timers[i].first, timers[i].second);
    }
    ImGui::Unindent();
    ImGui::CheckboxWithHelp("显示灵魂计时器", &settings.show_spirit_timers, "以秒为单位显示灵魂剩余时间");
    if (settings.show_spirit_timers) {
        ImGui::Indent();
        ImGui::StartSpacedElements(140.f);
        for (const auto& it : spirit_effects) {
            ImGui::NextSpacedElement();
            ImGui::Checkbox(it.second, &spirit_effects_enabled[it.first]);
        }
        ImGui::Unindent();
    }
}

milliseconds TimerWidget::GetMapTimeElapsed()
{
    if (!is_valid(instance_started)) {
        // 可能在工具箱加载后地图才加载
        instance_started = now() - milliseconds(GW::Map::GetInstanceTime());
    }
    return duration_cast<milliseconds>(now() - instance_started);
}

milliseconds TimerWidget::GetTimer()
{
    if (settings.use_instance_timer) {
        return milliseconds(instance_timer_valid ? GW::Map::GetInstanceTime() : 0);
    }
    return GetRunTimeElapsed();
}

milliseconds TimerWidget::GetRunTimeElapsed()
{
    if (!is_valid(run_started)) {
        return milliseconds(0);
    }
    if (is_valid(run_completed)) {
        return duration_cast<milliseconds>(run_completed - run_started);
    }
    // OnPostGameSrvTransfer 未被调用的罕见情况
    if (duration_cast<milliseconds>(now() - run_started) - milliseconds{GW::Map::GetInstanceTime()} > 5000ms && !settings.never_reset) {
        const GW::AreaInfo* info = GW::Map::GetMapInfo(GW::Map::GetMapID());
        if (info) {
            if (info->type == GW::RegionType::ExplorableZone) {
                run_started = now() - milliseconds(GW::Map::GetInstanceTime());
            }
        }
    }
    return duration_cast<milliseconds>(now() - run_started);
}

unsigned long TimerWidget::GetStartPoint() const
{
    const auto time_point = settings.use_instance_timer ? instance_started : run_started;
    if (!is_valid(time_point)) {
        return static_cast<unsigned long>(-1);
    }
    const auto ms = duration_cast<milliseconds>(time_point.time_since_epoch());
    return static_cast<unsigned long>(ms.count());
}

unsigned long TimerWidget::GetTimerMs() { return static_cast<unsigned long>(GetTimer().count()); }
unsigned long TimerWidget::GetMapTimeElapsedMs() { return static_cast<unsigned long>(GetMapTimeElapsed().count()); }
unsigned long TimerWidget::GetRunTimeElapsedMs() { return static_cast<unsigned long>(GetRunTimeElapsed().count()); }


void TimerWidget::SetRunCompleted(const bool no_print)
{
    if (is_valid(run_started) && settings.stop_at_objective_completion) {
        run_completed = now();
    }
    if (settings.print_time_objective && is_valid(run_started) && is_valid(run_completed) && !no_print) {
        PrintTimer();
    }
}

void TimerWidget::PrintTimer()
{
    char buf[32];
    print_time(GetTimer(), 2, 32, buf);
    Log::Info("用时：%s", buf);
}

ImGuiWindowFlags TimerWidget::GetWinFlags(const ImGuiWindowFlags flags, const bool noinput_if_frozen) const
{
    return ToolboxWidget::GetWinFlags(flags, noinput_if_frozen) | (lock_size ? ImGuiWindowFlags_AlwaysAutoResize : 0);
}

void TimerWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }
    if (settings.hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }

    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(settings.click_to_print_time && ctrl_pressed)))) {
        const auto font = FontLoader::GetFont();
        // 主计时器
        print_time(GetTimer(), settings.show_decimals, 32, timer_buffer);
        ImGui::PushFont(font, settings.font_size);
        ImGui::TextShadowed(timer_buffer, {2, 2});
        ImGui::PopFont();

        if (settings.also_show_instance_timer) {
            print_time(milliseconds(GW::Map::GetInstanceTime()), settings.show_decimals, 32, timer_buffer);
            ImGui::PushFont(font, settings.font_size);
            ImGui::TextShadowed(timer_buffer, {2, 2});
            ImGui::PopFont();
        }

        const auto draw_timer = [](const char* buffer, const ImColor* _extra_color = nullptr) {
            const ImVec2 cur2 = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur2.x + 1, cur2.y + 1));
            ImGui::TextColored(ImColor(0, 0, 0), buffer);
            ImGui::SetCursorPos(cur2);
            if (_extra_color) {
                ImGui::TextColored(*_extra_color, buffer);
            }
            else {
                ImGui::Text(buffer);
            }
        };
        if (settings.font_size_extra_timers > 0.f) {
            const auto extra_timer_font = FontLoader::GetFont();
            ImGui::PushFont(extra_timer_font, settings.font_size_extra_timers);

            if (settings.show_deep_timer && GetDeepTimer()) {
                draw_timer(extra_buffer, &extra_color);
            }
            if (settings.show_urgoz_timer && GetUrgozTimer()) {
                draw_timer(extra_buffer, &extra_color);
            }
            if (settings.show_doa_timer && GetDoATimer()) {
                draw_timer(extra_buffer, &extra_color);
            }
            if (settings.show_dungeon_traps_timer && GetTrapTimer()) {
                draw_timer(extra_buffer, &extra_color);
            }
            if (settings.show_dhuum_timer && GetDhuumTimer()) {
                draw_timer(extra_buffer, &extra_color);
            }
            if (settings.show_spirit_timers && GetSpiritTimer()) {
                draw_timer(spirits_buffer);
            }

            ImGui::PopFont();
        }

        if (settings.click_to_print_time) {
            const ImVec2 size = ImGui::GetWindowSize();
            const ImVec2 min = ImGui::GetWindowPos();
            const ImVec2 max(min.x + size.x, min.y + size.y);
            if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
                if (!GameSettings::GetSettingBool("auto_age2_on_age")) {
                    PrintTimer();
                }
                GW::Chat::SendChat('/', "age");
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
