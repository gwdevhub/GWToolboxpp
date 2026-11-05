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
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
};
