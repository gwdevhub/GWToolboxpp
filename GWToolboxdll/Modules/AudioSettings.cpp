#include "stdafx.h"

#include "AudioSettings.h"
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Managers/MemoryMgr.h>

#include <ImGuiAddons.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <Utils/GuiUtils.h>

namespace {

    std::vector<std::wstring> blocked_sounds;
    std::vector<std::wstring> logged_sounds;
    bool log_sounds = false;

    std::map<GW::HookEntry*, PlaySoundCallback> play_sound_callbacks;

    static_assert(sizeof(SoundProps) == 0x78);

    using PlaySound_pt = GW::RecObject*(__cdecl*)(const wchar_t* filename, SoundProps* props);
    PlaySound_pt PlaySound_Func = nullptr, PlaySound_Ret = nullptr;

    using StopSound_pt = void(__cdecl*)(GW::RecObject* sound, uint32_t flags);
    StopSound_pt StopSound_Func = nullptr;

    using CloseHandle_pt = void(__cdecl*)(GW::RecObject* handle);
    CloseHandle_pt CloseHandle_Func = nullptr;

    bool force_play_sound = false;

    GW::RecObject* OnPlaySound(wchar_t* filename, SoundProps* props) {
        GW::Hook::EnterHook();
        GW::RecObject* ret = 0;
        GW::HookStatus status;
        for (auto& [_, cb] : play_sound_callbacks) {
            cb(&status, filename, props);
        }
        if (!status.blocked) {
            const auto found = force_play_sound ? blocked_sounds.end() : std::ranges::find_if(blocked_sounds.begin(), blocked_sounds.end(), [filename](std::wstring& snd) {
                return snd == filename;
            });
            if (found == blocked_sounds.end()) ret = PlaySound_Ret(filename, props);
        }

        if (log_sounds) {
            const auto found_log = std::ranges::find_if(logged_sounds.begin(), logged_sounds.end(), [filename](std::wstring& snd) {
                return snd == filename;
                });
            if (found_log == logged_sounds.end()) {
                logged_sounds.push_back(filename);
            }
        }
       
        GW::Hook::LeaveHook();
        return ret;
    }

    GW::HookEntry OnUIMessage_HookEntry;
    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        log_sounds = false;
        logged_sounds.clear();
    }
} // namespace

SoundProps ::~SoundProps()
{
    if (h0030) GW::MemoryMgr::MemFree(h0030);
    if (h0048) GW::MemoryMgr::MemFree(h0048);
    if (h0060) GW::MemoryMgr::MemFree(h0060);
}


bool AudioSettings::PlaySound(const wchar_t* filename, const GW::Vec3f* position, uint32_t flags, void** handle_out)
{
    if (!(PlaySound_Func && filename))
        return false;
    auto props = new SoundProps();
    if (position) {
        props->position = *position;
    }
    props->flags = flags;
    GW::GameThread::Enqueue([cpy = std::wstring(filename), props, handle_out]() {
        force_play_sound = true;
        const auto handle = PlaySound_Func(cpy.c_str(), props);
        if (handle_out)
            *handle_out = (void*)handle;
        delete props;
        force_play_sound = false;
        });
    return true;
}
bool AudioSettings::StopSound(void* handle)
{
    // This doesn't work :(
    if (!(StopSound_Func && CloseHandle_Func && handle)) return false;
    GW::GameThread::Enqueue([handle]() {
        StopSound_Func((GW::RecObject*)handle,0);
        CloseHandle_Func((GW::RecObject*)handle);
    });
    return true;
}

void AudioSettings::RegisterPlaySoundCallback(GW::HookEntry* hook_entry, PlaySoundCallback callback) {
    play_sound_callbacks[hook_entry] = callback;
}

void AudioSettings::RemovePlaySoundCallback(GW::HookEntry* hook_entry)
{
    const auto found = play_sound_callbacks.find(hook_entry);
    if (found != play_sound_callbacks.end()) {
        play_sound_callbacks.erase(found);
    }
}
void AudioSettings::Initialize()
{
    ToolboxModule::Initialize();
    PlaySound_Func = (PlaySound_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("SndMain.cpp","filename",0,0));
    if (PlaySound_Func) {
        GW::Hook::CreateHook((void**)&PlaySound_Func, OnPlaySound, reinterpret_cast<void**>(&PlaySound_Ret));
        GW::Hook::EnableHooks(PlaySound_Func);
    }
    CloseHandle_Func = (CloseHandle_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("Handle.cpp", "handle", 0x90, 0));
    StopSound_Func = (StopSound_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("SndMain.cpp", "handle", 0x3d2, 0));

    #ifdef _DEBUG
    ASSERT(PlaySound_Func);
    ASSERT(CloseHandle_Func);
    ASSERT(StopSound_Func);
    #endif
    GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kMapChange, OnPostUIMessage, 0x8000);

}
void AudioSettings::Terminate()
{
    ToolboxModule::Terminate();
    if (PlaySound_Func) {
        GW::Hook::RemoveHook(PlaySound_Func);
    }
    logged_sounds.clear();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}
void AudioSettings::LoadSettings(ToolboxIni* ini)
{
    CSimpleIni::TNamesDepend keys{};
    blocked_sounds.clear();
    if (ini->GetAllKeys(Name(), keys)) {
        for (const auto& key : keys) {
            if (strncmp(key.pItem, "blocked_sounds", 14) != 0)
                continue;
            std::wstring out;
            GuiUtils::IniToArray(ini->GetValue(Name(),key.pItem,""),out);
            if (!out.empty())
                blocked_sounds.push_back(std::move(out));
        }
    }
}
void AudioSettings::SaveSettings(ToolboxIni* ini)
{
    CSimpleIni::TNamesDepend values{};
    ini->Delete(Name(), "blocked_sounds");
    std::string buf;
    size_t i = 0;
    ini->Delete(Name(),NULL);
    for (const auto& filename : blocked_sounds) {
        GuiUtils::ArrayToIni(filename, &buf);
        if (!buf.empty()) {
            auto key = std::format("blocked_sounds{}", i++);
            ini->SetValue(Name(), key.c_str(), buf.c_str());
        } 
    }
}
void AudioSettings::DrawSettingsInternal() {
    auto log_sound = [](const std::wstring& filename) {
        ImGui::PushID(&filename);
        std::string buf;
        GuiUtils::ArrayToIni(filename, &buf);
        ImGui::TextUnformatted(buf.c_str());
        ImGui::SameLine(200.f * ImGui::FontScale());
        if (ImGui::Button("Play")) {
            PlaySound(filename.c_str());
        }
        const auto found = std::ranges::find(blocked_sounds, filename);
        ImGui::SameLine();
        if (found == blocked_sounds.end()) {
            if (ImGui::Button("Block")) {
                blocked_sounds.push_back(filename);
                ImGui::PopID();
                return true;
            }
        }
        else {
            if (ImGui::Button("Unblock")) {
                blocked_sounds.erase(found);
                ImGui::PopID();
                return true;
            }
        }
        ImGui::PopID();
        return false;
        };
    ImGui::Indent();
    if (ImGui::CollapsingHeader("Blocked In-Game Sounds")) {
        for (const auto& filename : blocked_sounds) {
            if (log_sound(filename))
                break;
        }
    }
    log_sounds = ImGui::CollapsingHeader("In-Game Sound Log");
    if (log_sounds) {
        if(ImGui::Button("Clear Logged Sounds"))
            logged_sounds.clear();
        for (const auto& filename : logged_sounds) {
            log_sound(filename);
        }
    }
    else {
        logged_sounds.clear();
    }
    ImGui::Unindent();
}
