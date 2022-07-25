#pragma once
#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Agent.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <random>

class Obfuscator : public ToolboxModule {
    Obfuscator() {};
    ~Obfuscator();
public:
    static Obfuscator& Instance() {
        static Obfuscator instance;
        return instance;
    }

    const char* Name() const override { return "Obfuscator"; }
    const char* SettingsName() const override { return "Game Settings"; }

    void Initialize() override;
    void Update(float) override;
    void Terminate() override;
    void DrawSettingInternal() override;
    void SaveSettings(CSimpleIni* ini) override;
    void LoadSettings(CSimpleIni* ini) override;
    void Obfuscate(bool obfuscate);
    wchar_t* ObfuscateMessage(GW::Chat::Channel channel, wchar_t* original_message, bool obfuscate = true);
    wchar_t* UnobfuscateMessage(GW::Chat::Channel channel, wchar_t* original_message) {
        return ObfuscateMessage(channel, original_message, false);
    }
    wchar_t* getPlayerInvitedName();

    wchar_t* ObfuscateName(const wchar_t* original_name, wchar_t* out, int length = 20, bool force = false);
    wchar_t* UnobfuscateName(const wchar_t* obfuscated_name, wchar_t* out, int length = 20);
    void Reset();
    enum EnabledStatus : uint8_t {
        Disabled,
        Pending,
        Enabled
    } status = Disabled;
private:
    static void OnStoCPacket(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet);
    static void OnPreUIMessage(GW::HookStatus*, GW::UI::UIMessage msg_id, void* wParam, void* lParam);
    static void OnSendChat(GW::HookStatus*, GW::Chat::Channel channel, wchar_t* message);
    static void OnPrintChat(GW::HookStatus* status, GW::Chat::Channel, wchar_t** message, FILETIME, int);
    static void CmdObfuscate(const wchar_t* cmd, int argc, wchar_t** argv);

    // Hide or show guild member names. Note that the guild roster persists across map changes so be careful with it
    void ObfuscateGuild(bool obfuscate = true);
    std::unordered_map<std::wstring, std::wstring> obfuscated_by_obfuscation;
    std::unordered_map<std::wstring, std::wstring> obfuscated_by_original;
    GW::HookEntry stoc_hook;
    GW::HookEntry stoc_hook2;
    GW::HookEntry ctos_hook;
    size_t pool_index = 0;

    bool obfuscated_login_screen = false;

    // The name that this player was invited on in the guild window
    bool pending_guild_obfuscate = false;
    wchar_t player_guild_invited_name[20] = { 0 };
    wchar_t player_guild_obfuscated_name[20] = { 0 };
    static void CALLBACK OnWindowEvent(HWINEVENTHOOK _hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

};
