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
#include <Timer.h>

namespace {

    std::vector<std::wstring> blocked_sounds;
    std::vector<std::wstring> logged_sounds;
    std::vector<std::wstring> logged_music;
    std::map<std::wstring, std::string> saved_sounds;
    bool log_sounds = false;

    std::map<GW::HookEntry*, PlaySoundCallback> play_sound_callbacks;
    std::map<GW::HookEntry*, PlaySoundCallback> play_music_callbacks;

    std::map<std::wstring, clock_t> blocked_sounds_until;

struct MusicData {
        uint32_t h0000[10];
        GW::Array<wchar_t*> music_files;
        wchar_t* base_directory;
        // ...
    };
    static_assert(sizeof(MusicData) == 0x3C, "MusicData size mismatch");

    struct SoundScriptContext {
        uint32_t h0000;
        uint32_t h0004;
        MusicData* music_data_ptr;
        uint32_t flags;
        uint32_t h0010[10];
        void* h0038;
        uint32_t h003C[18];
        GW::Array<GW::RecObject*> handle_array;
        uint32_t h0094[4];
        int32_t state;
        // ...
    };
    static_assert(sizeof(SoundScriptContext) == 0xA8, "SoundScriptContext size mismatch");

    static_assert(sizeof(SoundProps) == 0x78);
    using PlaySound_pt = GW::RecObject*(__cdecl*)(const wchar_t* filename, SoundProps* props);
    PlaySound_pt PlaySound_Func = nullptr, PlaySound_Ret = nullptr;

    using StopSound_pt = void(__cdecl*)(GW::RecObject* sound, uint32_t flags);
    StopSound_pt StopSound_Func = nullptr;

    using CloseHandle_pt = void(__cdecl*)(GW::RecObject* handle);
    CloseHandle_pt CloseHandle_Func = nullptr, CloseHandle_Ret = nullptr;

    using PlayMusicFromSoundScript_pt = void(__cdecl*)(SoundScriptContext* sound_script, uint32_t flags1, uint32_t flags2, uint32_t flags3, uint32_t music_idx);
    PlayMusicFromSoundScript_pt PlayMusicFromSoundScript_Func = 0, PlayMusicFromSoundScript_Ret = 0;


    SoundScriptContext* current_sound_script_context = 0;
    void OnPlayMusicFromSoundScript(SoundScriptContext* sound_script, uint32_t flags1, uint32_t flags2, uint32_t flags3, uint32_t music_idx) {
        GW::Hook::EnterHook();
        current_sound_script_context = sound_script;
        if (sound_script && sound_script->music_data_ptr && music_idx < sound_script->music_data_ptr->music_files.size()) {
            const auto filename = sound_script->music_data_ptr->music_files[music_idx];
            if (std::ranges::find(logged_music, filename) == logged_music.end()) {
                logged_music.push_back(filename);
            }
            if (std::ranges::find(blocked_sounds, filename) != blocked_sounds.end()) {
                GW::Hook::LeaveHook();
                return;
            }
        }
        PlayMusicFromSoundScript_Ret(sound_script, flags1, flags2, flags3, music_idx);
        GW::Hook::LeaveHook();
    }

    bool force_play_sound = false;

    // Helper function to handle common logic
    template <typename CallbackMap>
    GW::RecObject* PlayAudioInternal(wchar_t* filename, SoundProps* props, CallbackMap& callbacks, PlaySound_pt ret_func)
    {
        GW::Hook::EnterHook();
        GW::RecObject* ret = nullptr;
        GW::HookStatus status;

        // Execute callbacks
        for (auto& [_, cb] : callbacks) {
            cb(&status, filename, props);
        }

        // Check if sound should be played
        if (!status.blocked) {
            if (!force_play_sound) {
                bool found = std::ranges::find(blocked_sounds, filename) != blocked_sounds.end();
                if (!found) {
                    found = blocked_sounds_until.contains(filename);
                }
                if (!found) {
                    ret = ret_func(filename, props);
                }
            }
            else {
                ret = ret_func(filename, props);
            }
        }

        GW::Hook::LeaveHook();
        return ret;
    }

    GW::RecObject* OnPlaySound(wchar_t* filename, SoundProps* props)
    {
        auto handle = PlayAudioInternal(filename, props, play_sound_callbacks, PlaySound_Ret);
        // Log sound if enabled
        if (log_sounds && std::ranges::find(logged_sounds, filename) == logged_sounds.end()) {
            logged_sounds.push_back(filename);
        }
        return handle;
    }

    // Avoids assertion issues when handle->h0000 is freed already e.g. by toolbox
    void OnCloseHandle(GW::RecObject* handle)
    {
        GW::Hook::EnterHook();
        if (handle && handle->vtable) CloseHandle_Ret(handle);
        GW::Hook::LeaveHook();
    }

    GW::HookEntry OnUIMessage_HookEntry;
    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        log_sounds = false;
        logged_sounds.clear();
        current_sound_script_context = 0;
    }
} // namespace

SoundProps ::~SoundProps()
{
    if (h0030) GW::MemoryMgr::MemFree(h0030);
    if (h0048) GW::MemoryMgr::MemFree(h0048);
    if (h0060) GW::MemoryMgr::MemFree(h0060);
}

bool AudioSettings::PlayMusic(const wchar_t* filename, uint32_t flags)
{
    if (!(PlayMusicFromSoundScript_Func && filename && current_sound_script_context)) return false;
    GW::GameThread::Enqueue([cpy = std::wstring(filename), flags]() {
        const auto old_flags = current_sound_script_context->flags;
        current_sound_script_context->flags = flags;
        const auto old_filename = current_sound_script_context->music_data_ptr->music_files[1];
        current_sound_script_context->music_data_ptr->music_files[1] = (wchar_t*)cpy.data();
        PlayMusicFromSoundScript_Func(current_sound_script_context, 0, 0, 0, 1);
        current_sound_script_context->music_data_ptr->music_files[1] = old_filename;
        current_sound_script_context->flags = old_flags;
    });
    return true;
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
void AudioSettings::BlockSoundForMs(const wchar_t* filename, clock_t ms) {
    blocked_sounds_until[filename] = TIMER_INIT() + ms;
}
void AudioSettings::Initialize()
{
    ToolboxModule::Initialize();
    PlaySound_Func = (PlaySound_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("SndMain.cpp","filename",0,0));
    if (PlaySound_Func) {
        GW::Hook::CreateHook((void**)&PlaySound_Func, OnPlaySound, reinterpret_cast<void**>(&PlaySound_Ret));
        GW::Hook::EnableHooks(PlaySound_Func);
    }
    PlayMusicFromSoundScript_Func = (PlayMusicFromSoundScript_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x8d\x77\x0c\x83\xe0\xf3", "xxxxxx", 0));
    if (PlayMusicFromSoundScript_Func) {
        GW::Hook::CreateHook((void**)&PlayMusicFromSoundScript_Func, OnPlayMusicFromSoundScript, reinterpret_cast<void**>(&PlayMusicFromSoundScript_Ret));
        GW::Hook::EnableHooks(PlayMusicFromSoundScript_Func);
    }
    

    CloseHandle_Func = (CloseHandle_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("Handle.cpp", "s_allocCount", 0xa7, 0));
    if (CloseHandle_Func) {
        GW::Hook::CreateHook((void**)&CloseHandle_Func, OnCloseHandle, reinterpret_cast<void**>(&CloseHandle_Ret));
        GW::Hook::EnableHooks(CloseHandle_Func);
    }
    StopSound_Func = (StopSound_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("SndMain.cpp", "handle", 0x3d8, 0));

    #ifdef _DEBUG
    ASSERT(PlaySound_Func);
    ASSERT(CloseHandle_Func);
    ASSERT(StopSound_Func);
    ASSERT(PlayMusicFromSoundScript_Func);
    #endif
    GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kMapChange, OnPostUIMessage, 0x8000);

}
void AudioSettings::Update(float) {
    for (auto& it : blocked_sounds_until) {
        if (it.second < TIMER_INIT()) {
            blocked_sounds_until.erase(it.first);
            break;
        }
    }
}
void AudioSettings::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    if (PlayMusicFromSoundScript_Func) {
        GW::Hook::RemoveHook(PlayMusicFromSoundScript_Func);
        PlayMusicFromSoundScript_Func = 0;
    }
    if (PlaySound_Func) {
        GW::Hook::RemoveHook(PlaySound_Func);
        PlaySound_Func = 0;
    }
    if (CloseHandle_Func) {
        GW::Hook::RemoveHook(CloseHandle_Func);
        PlaySound_Func = 0;
    }
    logged_sounds.clear();
    logged_music.clear();
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

    using PlaySoundInt_pt = bool(__cdecl*)(const wchar_t*, uint32_t, uint32_t, uint32_t);

    auto log_sound = [](const std::wstring& filename, PlaySoundInt_pt PlaySound_pt, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
        ImGui::PushID(&filename);
        std::string buf;
        GuiUtils::ArrayToIni(filename, &buf);
        ImGui::TextUnformatted(buf.c_str());
        ImGui::SameLine(200.f * ImGui::FontScale());
        if (ImGui::Button("Play")) {
            PlaySound_pt(filename.c_str(),arg1,arg2,arg3);
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
            if (log_sound(filename, (PlaySoundInt_pt)PlaySound,0,0,0))
                break;
        }
    }
    log_sounds = ImGui::CollapsingHeader("In-Game Sound Log");
    if (log_sounds) {
        if(ImGui::Button("Clear Logged Sounds"))
            logged_sounds.clear();
        for (const auto& filename : logged_sounds) {
            log_sound(filename, (PlaySoundInt_pt)PlaySound, 0, 0, 0);
        }
    }
    else {
        logged_sounds.clear();
    }
    if (ImGui::CollapsingHeader("In-Game Music Log")) {
            if (ImGui::Button("Clear Logged Music")) logged_music.clear();
            for (const auto& filename : logged_music) {
                log_sound(filename, (PlaySoundInt_pt)PlayMusic, 0x83, 0, 0);
            }
    }

    ImGui::Unindent();
}
