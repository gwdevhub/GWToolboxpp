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

    struct SoundProps {
        uint32_t flags;
        uint32_t h0004[11];
        void* h0030;
        uint32_t h0034[5];
        void* h0048;
        uint32_t h004c[5];
        void* h0060;
        uint32_t h0064[5];
    };

    static_assert(sizeof(SoundProps) == 0x78);

    void FreeSoundProps(SoundProps* props) {
        if (!props) return;
        if (props->h0030)
            GW::MemoryMgr::MemFree(props->h0030);
        if (props->h0048)
            GW::MemoryMgr::MemFree(props->h0048);
        if (props->h0060)
            GW::MemoryMgr::MemFree(props->h0060);
        memset(props, 0, sizeof(*props));
    }

    using PlaySound_pt = GW::RecObject*(__cdecl*)(const wchar_t* filename, SoundProps* props);
    PlaySound_pt PlaySound_Func = nullptr, PlaySound_Ret = nullptr;

    bool force_play_sound = false;

    GW::RecObject* OnPlaySound(wchar_t* filename, SoundProps* props) {
        GW::Hook::EnterHook();
        GW::RecObject* ret = 0;
        const auto found = force_play_sound ? blocked_sounds.end() : std::ranges::find_if(blocked_sounds.begin(), blocked_sounds.end(), [filename](std::wstring& snd) {
            return snd == filename;
            });
        if (found == blocked_sounds.end())
            ret = PlaySound_Ret(filename, props);
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
}

bool AudioSettings::PlaySound(const wchar_t* filename)
{
    if (!PlaySound_Func)
        return false;
    GW::GameThread::Enqueue([filename]() {
        SoundProps props = { 0 };
        force_play_sound = true;
        PlaySound_Func(filename, &props);
        FreeSoundProps(&props);
        force_play_sound = false;
        });
    return true;
}

void AudioSettings::Initialize()
{
    ToolboxModule::Initialize();
    PlaySound_Func = (PlaySound_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("SndMain.cpp","filename",0,0));
    if (PlaySound_Func) {
        GW::Hook::CreateHook((void**)&PlaySound_Func, OnPlaySound, reinterpret_cast<void**>(&PlaySound_Ret));
        GW::Hook::EnableHooks(PlaySound_Func);
    }
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
