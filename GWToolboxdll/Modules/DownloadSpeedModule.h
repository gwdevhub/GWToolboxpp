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
    [[nodiscard]] const char* Description() const override { return "Forces Gw.dat downloads to use the maximum supported 32 KiB chunk size instead of adapting to smaller chunks."; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
};
