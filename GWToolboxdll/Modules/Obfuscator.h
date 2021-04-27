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
    wchar_t* ObfuscateMessage(GW::Chat::Channel channel, wchar_t* original_message, bool obfuscate = true);
    wchar_t* UnobfuscateMessage(GW::Chat::Channel channel, wchar_t* original_message) {
        return ObfuscateMessage(channel, original_message, false);
    }
    wchar_t* ObfuscateName(const wchar_t* original_name, wchar_t* out, int length = 20);
    wchar_t* UnobfuscateName(const wchar_t* obfuscated_name, wchar_t* out, int length = 20);
    void Reset();
    enum EnabledStatus : uint8_t {
        Disabled,
        Pending,
        Enabled
    } status = Pending;
private:
    static void OnStoCPacket(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet);
    static void OnPreUIMessage(GW::HookStatus*, uint32_t msg_id, void* wParam, void* lParam);
    static void OnPostUIMessage(GW::HookStatus*, uint32_t msg_id, void* wParam, void* lParam);
    static void OnSendChat(GW::HookStatus*, GW::Chat::Channel channel, wchar_t* message);
    static void OnPrintChat(GW::HookStatus* status, GW::Chat::Channel, wchar_t** message, FILETIME, int);
    std::unordered_map<std::wstring, std::wstring> obfuscated_by_obfuscation;
    std::unordered_map<std::wstring, std::wstring> obfuscated_by_original;
    GW::HookEntry stoc_hook;
    GW::HookEntry ctos_hook;
    size_t pool_index = 0;

    static void CALLBACK OnWindowEvent(HWINEVENTHOOK _hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

};
