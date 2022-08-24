#pragma once
#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Agent.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <random>

class Obfuscator : public ToolboxModule {
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

private:
    static void OnSendChat(GW::HookStatus*, GW::Chat::Channel channel, wchar_t* message);
    static void OnPrintChat(GW::HookStatus* status, GW::Chat::Channel, wchar_t** message, FILETIME, int);

    GW::HookEntry stoc_hook;
    GW::HookEntry stoc_hook2;
    GW::HookEntry ctos_hook;
    

    bool obfuscated_login_screen = false;
    static void CALLBACK OnWindowEvent(HWINEVENTHOOK _hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

};
