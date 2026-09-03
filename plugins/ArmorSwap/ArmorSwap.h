#pragma once

#include <ToolboxUIPlugin.h>
#include <PluginUtils.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Item.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ArmorSwap {
    struct HotkeyBinding {
        uint32_t key = 0;
        bool control = false;
        bool alt = false;
        bool shift = false;
    };

    struct ConfiguredItem {
        uint32_t item_id = 0;
        uint32_t model_id = 0;
        uint32_t type = 0;
        uint32_t profession = 0;
        uint32_t dye = 0;
        SettingWString encoded_name;
        std::vector<uint32_t> modifiers;
        std::string label;
    };

    struct SwapHotkey {
        uint32_t id = 0;
        std::string name;
        HotkeyBinding binding;
        std::vector<ConfiguredItem> items;
        bool enabled = true;
        bool block_gw = true;
    };

    class Plugin final : public ToolboxUIPlugin {
    public:
        Plugin()
        {
            can_show_in_main_window = true;
            show_menubutton = true;
            can_close = true;
        }
        ~Plugin() override = default;

        [[nodiscard]] const char* Name() const override { return "Armor Swap"; }
        [[nodiscard]] bool HasSettings() const override { return true; }

        void Initialize(ImGuiContext* context, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
        void SignalTerminate() override;
        bool CanTerminate() override;
        void Update(float delta) override;
        void Draw(IDirect3DDevice9* device) override;
        void DrawSettings() override;
        bool WndProc(UINT message, WPARAM wparam, LPARAM lparam) override;
        void LoadSettings(const wchar_t* folder) override;
        void SaveSettings(const wchar_t* folder) override;

    private:
        using Clock = std::chrono::steady_clock;

        enum class SwapState : uint8_t {
            Idle,
            Equipping,
            InterItemDelay,
            Failed,
        };

        struct AvailableItem {
            ConfiguredItem configured;
            std::string decoded_name;
            GW::Constants::Bag bag = GW::Constants::Bag::None;
            uint32_t slot = 0;
            bool decode_started = false;
        };

        struct ItemNameDecodeContext {
            Plugin* owner = nullptr;
            uint64_t generation = 0;
            uint32_t item_id = 0;
            std::wstring encoded_name;
        };

        static constexpr uint32_t NoHotkey = 0;

        mutable std::mutex config_mutex_;
        std::vector<SwapHotkey> hotkeys_;
        uint32_t next_hotkey_id_ = 1;

        std::atomic<uint32_t> pending_hotkey_id_{NoHotkey};
        std::atomic<uint32_t> capturing_hotkey_id_{NoHotkey};
        std::atomic_bool cancel_requested_{false};
        std::atomic_bool terminating_{false};
        std::atomic_bool game_ready_{false};
        std::atomic_bool refresh_inventory_requested_{false};
        std::atomic<uint64_t> pending_add_request_{0};

        std::atomic_int retry_interval_ms_{500};
        std::atomic_int piece_timeout_ms_{5000};
        std::atomic_int inter_item_delay_ms_{100};

        mutable std::mutex runtime_mutex_;
        SwapState state_ = SwapState::Idle;
        std::vector<uint32_t> execution_queue_;
        size_t execution_index_ = 0;
        std::string active_swap_name_;
        std::string status_ = "Create a swap hotkey and add items from your inventory.";
        Clock::time_point piece_started_{};
        Clock::time_point last_try_{};
        Clock::time_point resume_at_{};
        uint32_t equip_attempts_ = 0;
        bool last_equip_request_submitted_ = false;

        uint32_t picker_hotkey_id_ = NoHotkey;
        mutable std::mutex available_items_mutex_;
        uint64_t available_items_generation_ = 0;
        std::vector<AvailableItem> available_items_;
        std::vector<ItemNameDecodeContext*> pending_item_name_decodes_;
        std::unordered_map<uint32_t, uint32_t> manual_item_ids_;

        void StartSwap(uint32_t hotkey_id);
        void ProcessActiveSwap();
        void Execute(uint32_t item_id);
        void FinishCurrentItem();
        void Fail(const std::string& reason);
        void Cancel(const std::string& reason);

        [[nodiscard]] GW::Item* FindMatchingItem(uint32_t item_id) const;
        [[nodiscard]] GW::Item* FindMatchingItem(const ConfiguredItem& configured) const;
        [[nodiscard]] static bool IsMapContextReady();
        [[nodiscard]] static bool IsPlayerEquipmentReady();
        [[nodiscard]] static bool IsGameReady();
        [[nodiscard]] static std::vector<GW::Bag*> GetAvailableCharacterBags();
        [[nodiscard]] static const wchar_t* GetEncodedItemName(const GW::Item* item);
        [[nodiscard]] static bool ItemMatches(const GW::Item* item, const ConfiguredItem& configured);
        [[nodiscard]] static bool IsEquippable(const GW::Item* item);
        [[nodiscard]] static ConfiguredItem ConfigureItem(const GW::Item* item);
        [[nodiscard]] static uint32_t PackDye(const GW::DyeInfo& dye);
        [[nodiscard]] static GW::DyeInfo UnpackDye(uint32_t dye);

        [[nodiscard]] bool AddConfiguredItem(uint32_t hotkey_id, const GW::Item* item);
        void QueueAddConfiguredItem(uint32_t hotkey_id, uint32_t item_id);
        void ProcessPendingAdd();
        void RefreshAvailableItems();
        void BeginItemNameDecodes();
        void CompleteItemNameDecode(ItemNameDecodeContext* context, const wchar_t* decoded);
        static void OnItemNameDecoded(void* context, const wchar_t* decoded);
        void ClearAvailableItems();
        void DrawItemPicker();

        [[nodiscard]] static uint32_t HotkeyKeyFromMessage(UINT message, WPARAM wparam);
        [[nodiscard]] static bool IsHotkeyDownMessage(UINT message);
        [[nodiscard]] static bool IsModifierKey(uint32_t key);
        [[nodiscard]] static std::string HotkeyText(const HotkeyBinding& binding);
        [[nodiscard]] static const char* ItemTypeName(GW::Constants::ItemType type);
        [[nodiscard]] static const char* BagName(GW::Constants::Bag bag);
    };
}
