#pragma once

#include <ToolboxModule.h>

class TransmoModule : public ToolboxModule {
    TransmoModule() = default;
    ~TransmoModule() override = default;

public:
    static TransmoModule& Instance()
    {
        static TransmoModule instance;
        return instance;
    }

    struct PendingTransmo {
        PendingTransmo(const DWORD _npcid = 0, const DWORD _scale = 0x64000000, const DWORD _npcmfid = 0, const DWORD _npcmfd = 0, const DWORD _flags = 0)
            : npc_id(_npcid)
            , scale(_scale)
            , npc_model_file_id(_npcmfid)
            , npc_model_file_data(_npcmfd)
            , flags(_flags) {}
        DWORD npc_id = 0;
        DWORD scale = 0x64000000;
        DWORD npc_model_file_id = 0;
        DWORD npc_model_file_data = 0;
        DWORD flags = 0;
    };

    [[nodiscard]] const char* Name() const override { return "Transmo"; }
    [[nodiscard]] const char* SettingsName() const override { return "Chat Settings"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void DrawHelp() override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    static void TransmoAgent(DWORD agent_id, PendingTransmo& transmo);
    static bool GetNPCInfoByName(const std::string& name, PendingTransmo& transmo);
    static bool GetNPCInfoByName(const std::wstring& name, PendingTransmo& transmo);
    static bool ParseScale(int scale, PendingTransmo& transmo);
    static bool GetTargetTransmoInfo(PendingTransmo& transmo);
    static bool ParseTransmoArgs(int argc, const LPWSTR* argv, int name_arg_index, PendingTransmo& transmo);

private:
    static void CHAT_CMD_FUNC(CmdTransmo);
    static void CHAT_CMD_FUNC(CmdTransmoTarget);
    static void CHAT_CMD_FUNC(CmdTransmoParty);
    static void CHAT_CMD_FUNC(CmdTransmoAgent);
};
