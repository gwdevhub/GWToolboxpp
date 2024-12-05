#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxWindow.h>

#include <Windows/Pcons.h>

class PconsWindow : public ToolboxWindow {
    PconsWindow();
    ~PconsWindow() override = default;

public:
    static PconsWindow& Instance()
    {
        static PconsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Pcons"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BIRTHDAY_CAKE; }

    void Initialize() override;
    void Terminate() override;

    bool SetEnabled(bool b);
    bool GetEnabled() const;
    bool show_storage_quantity = false;
    bool shift_click_toggles_category = false;

    void ToggleEnable();

    void Refill(bool do_refill = true) const;

    void Update(float delta) override;

    bool DrawTabButton(bool show_icon, bool show_text, bool center_align_text) override;
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void RegisterSettingsContent() override;
    static void DrawLunarsAndAlcoholSettings();

    static void OnAgentState(GW::HookStatus*, GW::Packet::StoC::AgentState* pak);
    static void OnGenericValue(GW::HookStatus*, GW::Packet::StoC::GenericValue* pak);
    static void OnAddExternalBond(GW::HookStatus* status, const GW::Packet::StoC::AddExternalBond* pak);
    static void CHAT_CMD_FUNC(CmdPcons);

    std::vector<Pcon*> pcons{};

private:
    void MapChanged(); // Called via Update() when map id changes
    // Elite area auto disable
    void CheckBossRangeAutoDisable(); // Trigger Elite area auto disable if applicable
};
