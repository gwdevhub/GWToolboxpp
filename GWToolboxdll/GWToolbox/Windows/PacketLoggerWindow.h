#pragma once

#include <ToolboxWindow.h>

#include <GWCA/Utilities/Hook.h>

class PacketLoggerWindow : public ToolboxWindow {
    PacketLoggerWindow() {};
    ~PacketLoggerWindow() {};
public:
    static PacketLoggerWindow& Instance() {
        static PacketLoggerWindow instance;
        return instance;
    }

    const char* Name() const override { return "Packet Logger"; }
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void SaveSettings(CSimpleIni* ini) override;
    void LoadSettings(CSimpleIni* ini) override;
	void Update(float delta) override;
    void Enable();
    void Disable();

    
private:
    uint32_t identifiers[512] = { 0 }; // Presume 512 is big enough for header size...
    GW::HookEntry hook_entry;

    void AddStoCCallback(uint32_t packet_header);
    void AddCtoSCallback(uint32_t packet_header);
    void RemoveStoCCallback(uint32_t packet_header);
    void RemoveCtoSCallback(uint32_t packet_header);

	GW::HookEntry DisplayDialogue_Entry;
	GW::HookEntry MessageCore_Entry;
    GW::HookEntry MessageLocal_Entry;
	GW::HookEntry NpcGeneralStats_Entry;
};