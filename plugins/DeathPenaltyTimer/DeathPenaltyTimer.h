#pragma once

#include <ToolboxUIPlugin.h>

#include <IconsFontAwesome5.h>

class DeathPenaltyTimer : public ToolboxUIPlugin {
public:
    DeathPenaltyTimer();
    ~DeathPenaltyTimer() override = default;

    const char* Name() const override { return "DeathPenaltyTimer"; }
    const char* Icon() const override { return ICON_FA_CIRCLE; }

    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    void DrawSettings() override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float) override;

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;

private:
    void drawCircleSegment(float circlePortion, float thickness) const;

    float radius = 50.f;
    ImVec4 color = {0.925f, 0.11f, 0.141f, 1.f};
    ImVec2 offset = {0.f, 0.f};
    bool showIcon = true;
    bool showText = false;
    bool showCircle = true;
    bool showBackground = true;
    int fontSizeIndex = 1;
    int imageSizeIndex = 1;
};
