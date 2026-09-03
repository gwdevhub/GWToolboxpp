#pragma once

#include <ToolboxUIPlugin.h>

#include <GWCA/Utilities/Hook.h>

namespace GW {
    struct PartyInfo;
}

class PartyReorder final : public ToolboxUIPlugin {
public:
    struct ProfessionSlot {
        uint8_t primary = 0;
        uint8_t secondary = 0;
        std::string label;
    };

    struct Sequence {
        std::string name;
        std::vector<uint32_t> map_ids;
        std::vector<ProfessionSlot> slots;
    };

    PartyReorder();
    ~PartyReorder() override = default;

    [[nodiscard]] const char* Name() const override { return "PartyReorder"; }

    void Initialize(ImGuiContext* context, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    void Terminate() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettings() override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;

private:
    struct Member {
        uint32_t login_number = 0;
        uint8_t primary = 0;
        uint8_t secondary = 0;
        std::wstring name;
    };

    enum class ReorderState : uint8_t {
        Idle,
        Kicking,
        Inviting,
        Complete,
        Failed,
    };

    static void ChatCommand(GW::HookStatus* status, const wchar_t* command, int argc, const LPWSTR* argv);
    static std::vector<Sequence> DefaultSequences();
    void StartBestSequence(const std::wstring& requested_name = {});
    void StartReorder(size_t sequence_index, bool ignore_map = false);
    void CancelReorder(const std::string& reason);
    void AdvanceReorder();
    void UpdateNotifications();
    void SetStatus(std::string status, bool error = false);
    [[nodiscard]] std::vector<Member> PartyMembers() const;
    [[nodiscard]] std::optional<std::vector<Member>> AssignSlots(const Sequence& sequence) const;
    [[nodiscard]] bool AssignSlotsRecursive(
        const Sequence& sequence,
        const std::vector<Member>& candidates,
        size_t slot_index,
        std::vector<bool>& used,
        std::vector<Member>& result) const;
    [[nodiscard]] bool IsSequenceForCurrentMap(const Sequence& sequence) const;
    [[nodiscard]] bool IsPartyReady(std::vector<std::string>* available = nullptr) const;
    [[nodiscard]] bool IsMemberInParty(uint32_t login_number) const;
    [[nodiscard]] bool HasExpectedOrder() const;
    [[nodiscard]] bool IsReordering() const;

    static PartyReorder* active_instance_;

    GW::HookEntry invite_response_hook_;
    GW::HookEntry chat_command_hook_;
    std::vector<Sequence> sequences_;
    std::vector<Member> reorder_members_;
    size_t selected_sequence_ = 0;
    size_t reorder_index_ = 0;
    ReorderState reorder_state_ = ReorderState::Idle;
    bool operation_pending_ = false;
    std::chrono::steady_clock::time_point operation_started_;
    std::chrono::steady_clock::time_point next_action_at_;
    uint32_t reorder_map_id_ = 0;
    std::string status_ = "Idle";
    bool status_is_error_ = false;
    bool terminating_ = false;
    bool ready_notified_ = false;
    bool ticked_notified_ = false;
    size_t last_party_signature_ = 0;

    bool auto_height_ = true;
    bool enable_chat_command_ = true;
    bool send_chat_message_on_start_ = true;
    bool send_ready_message_ = false;
    bool send_notification_when_ready_ = true;
    bool send_notification_when_all_ticked_ = true;
    bool debug_mode_ = false;
    uint32_t action_delay_ = 300;
    uint32_t reorder_timeout_ = 30'000;
};
