#pragma once

#include <ToolboxModule.h>

class DownloadSpeedModule : public ToolboxModule {
    DownloadSpeedModule() = default;
    ~DownloadSpeedModule() override = default;

public:
    static DownloadSpeedModule& Instance()
    {
        static DownloadSpeedModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Download Speed"; }
    [[nodiscard]] const char* Description() const override { return "Uses the game's maximum supported 32 KiB receive chunks for Gw.dat downloads instead of adapting down to smaller chunks."; }

    void Initialize() override;
    void Terminate() override;
    void DrawSettingsInternal() override;
};
