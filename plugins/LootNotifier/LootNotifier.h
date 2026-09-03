#pragma once

#include <ToolboxUIPlugin.h>
#include <Utils/ToolboxUtils.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hook.h>

namespace GW::Packet::StoC {
    struct ItemUpdateOwner;
}

class LootNotifier final : public ToolboxUIPlugin {
public:
    enum class DropFilter : uint8_t {
        All,
        OtherPlayers,
        Self,
    };

    LootNotifier();
    ~LootNotifier() override = default;

    [[nodiscard]] const char* Name() const override { return "LootNotifier"; }

    void Initialize(ImGuiContext* context, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    void Terminate() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettings() override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;

private:
    struct TrackedItem {
        uint32_t model_id = 0;
        uint32_t model_file_id = 0;
        bool enabled = true;
    };

    struct PendingDrop {
        uint32_t item_id = 0;
        uint32_t owner_agent_id = 0;
        std::chrono::steady_clock::time_point received_at;
    };

    struct Drop {
        uint32_t item_id = 0;
        std::string item;
        std::string player;
        bool self = false;
        GW::Constants::Rarity rarity = GW::Constants::Rarity::Unknown;
    };

    struct SharedState {
        std::mutex mutex;
        bool active = true;
        uint64_t generation = 0;
        std::deque<Drop> completed;
    };

    struct DecodeRequest {
        std::mutex mutex;
        Drop drop;
        bool item_ready = false;
        bool player_ready = false;
        bool queued = false;
        uint64_t generation = 0;
    };

    void OnItemUpdateOwner(const GW::Packet::StoC::ItemUpdateOwner& packet);
    void ProcessPendingDrops();
    void BeginDecode(const PendingDrop& pending);
    void ProcessDecodedDrops();
    void Notify(const Drop& drop) const;
    void SendCongratulations(const Drop& drop) const;
    void ResetInstanceState();
    [[nodiscard]] bool IsTracked(uint32_t model_id, uint32_t model_file_id) const;
    [[nodiscard]] bool PassesFilter(DropFilter filter, bool self) const;
    [[nodiscard]] std::string Format(const std::string& pattern, const Drop& drop) const;
    [[nodiscard]] static std::string RequirementPrefix(const GW::Item& item);
    [[nodiscard]] static ImVec4 RarityColor(GW::Constants::Rarity rarity);
    static void CompleteDecode(
        const std::shared_ptr<SharedState>& state,
        const std::shared_ptr<DecodeRequest>& request,
        bool item,
        const wchar_t* decoded);

    GW::HookEntry item_owner_hook_;
    std::shared_ptr<SharedState> shared_state_;
    std::deque<PendingDrop> pending_drops_;
    std::unordered_set<uint32_t> handled_item_ids_;
    std::deque<Drop> drops_;
    std::vector<TrackedItem> tracked_items_;

    bool send_notification_ = false;
    bool send_party_chat_ = true;
    bool show_window_preview_ = false;
    DropFilter notification_filter_ = DropFilter::All;
    DropFilter party_chat_filter_ = DropFilter::All;
    std::string notification_format_ = "GZ [player] -> [item]!";
    std::string notification_format_self_ = "[item] for me!";
    std::string party_chat_format_ = "[item] dropped for [player]!";
    std::string party_chat_format_self_ = "[item] dropped for you!";
    std::string gz_format_ = "GZ [player] -> [item]!";
    std::string gz_format_self_ = "[item] for me!";
    uint32_t new_model_id_ = 0;
    uint32_t new_model_file_id_ = 0;
    uint32_t last_map_id_ = 0;
    uint32_t last_instance_time_ = 0;
    float window_pos_x_ = 100.f;
    float window_pos_y_ = 100.f;
    bool apply_window_position_ = false;
    bool terminating_ = false;
};
