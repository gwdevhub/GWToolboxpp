#pragma once

#include <ToolboxPlugin.h>

#include <IconsFontAwesome5.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>

class Slowload : public ToolboxPlugin {
public:
    ~Slowload() override = default;

    const char* Name() const override { return "Slowload"; }
    const char* Icon() const override { return ICON_FA_BED; }

    bool WndProc(UINT, WPARAM, LPARAM) override;

    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    void DrawSettings() override;
    bool HasSettings() const override { return true; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;

private:
    void stopSlowLoad();

    long shortcutKey = 0;
    long shortcutMod = 0;

    enum class Status {
        Inactive,
        WaitingForLoadScreen,
        WaitingInLoadScreen,
        StoppingSlowload,
    };
    std::atomic<Status> status = Status::Inactive;

    GW::HookEntry instanceLoadEntry;
    GW::Packet::StoC::InstanceLoadFile packet;

    char hotkeyDescription[64]{};
};
