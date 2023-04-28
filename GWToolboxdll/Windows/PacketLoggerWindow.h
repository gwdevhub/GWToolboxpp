#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxWindow.h>


class PacketLoggerWindow : public ToolboxWindow {
    PacketLoggerWindow() = default;
    ~PacketLoggerWindow() { ClearMessageLog(); };
public:
    static PacketLoggerWindow& Instance() {
        static PacketLoggerWindow instance;
        return instance;
    }

    const char* Name() const override { return "Packet Logger"; }
    const char* Icon() const override { return ICON_FA_BOX; };

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingInternal() override;

    void Initialize() override;
    void SaveSettings(ToolboxIni* ini) override;
    void LoadSettings(ToolboxIni* ini) override;
    void Update(float delta) override;
    static void OnMessagePacket(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet);
    void Enable();
    void Disable();
    void AddMessageLog(const wchar_t* encoded);
    void SaveMessageLog() const;
    void ClearMessageLog();
    void PacketHandler(GW::HookStatus* status, GW::Packet::StoC::PacketBase* packet);
    void CtoSHandler(GW::HookStatus* status, void* packet);
    std::string PadLeft(std::string input, uint8_t count, char c);
    std::string PrefixTimestamp(std::string message);

private:
    enum TimestampType : int {
        TimestampType_None,
        TimestampType_Local,
        TimestampType_Instance,
    };

    std::unordered_map<std::wstring, std::wstring*> message_log;
    std::wstring* last_message_decoded = nullptr;
    uint32_t identifiers[512] = { 0 }; // Presume 512 is big enough for header size...
    GW::HookEntry hook_entry;

    struct ForTranslation {
        std::wstring in;
        std::wstring out;
    };
    std::vector<ForTranslation*> pending_translation;


    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry MessageCore_Entry;
    GW::HookEntry MessageLocal_Entry;
    GW::HookEntry NpcGeneralStats_Entry;

    int timestamp_type = TimestampType::TimestampType_None;
    bool timestamp_show_hours = true;
    bool timestamp_show_minutes = true;
    bool timestamp_show_seconds = true;
    bool timestamp_show_milliseconds = true;
};
