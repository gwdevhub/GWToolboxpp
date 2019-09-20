#pragma once

#include <ToolboxWindow.h>

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
    
    void AddCallback(uint32_t packet_header);
    void RemoveCallback(uint32_t packet_header);
};