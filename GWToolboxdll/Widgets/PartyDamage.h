#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <Widgets/SnapsToPartyWindow.h>

class PartyDamage : public SnapsToPartyWindow {
public:
    static PartyDamage& Instance()
    {
        static PartyDamage instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Damage"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

};
