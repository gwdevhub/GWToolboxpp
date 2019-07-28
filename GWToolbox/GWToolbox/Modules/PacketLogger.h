#pragma once

#include <ToolboxWindow.h>

class PacketLogger : public ToolboxWindow {
    PacketLogger() {};
    ~PacketLogger() {};
public:
    static PacketLogger& Instance() {
        static PacketLogger instance;
        return instance;
    }

    const char* Name() const override { return "Packet Logger"; }
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;

};