#pragma once

#include <GWCA/Managers/UIMgr.h>

#include <ToolboxModule.h>


class ResignLogModule : public ToolboxModule {
    ResignLogModule() = default;
    ~ResignLogModule() override = default;

public:
    static ResignLogModule& Instance()
    {
        static ResignLogModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Resign Log"; }
    [[nodiscard]] const char* Description() const override { return "Tracks player resign state in an explorable area, adds /resignlog command"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void SignalTerminate() override;
    void Update(float) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    void RegisterSettingsContent() override;

    static bool PrintResignStatus(const uint32_t player_number, std::wstring& out, bool include_timestamp = false);
};
