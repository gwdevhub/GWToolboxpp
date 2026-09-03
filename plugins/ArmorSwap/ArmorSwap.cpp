#include "ArmorSwap.h"

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Logger.h>

#include <array>
#include <cstring>
#include <format>
#include <ranges>
#include <unordered_set>
#include <utility>

namespace {
    constexpr std::array CharacterEquipmentBags{
        GW::Constants::Bag::Equipped_Items,
        GW::Constants::Bag::Backpack,
        GW::Constants::Bag::Belt_Pouch,
        GW::Constants::Bag::Bag_1,
        GW::Constants::Bag::Bag_2,
        GW::Constants::Bag::Equipment_Pack,
    };

    constexpr uint32_t MaxBagSlots = 25;
    constexpr uint32_t MaxItemModifiers = 64;
    constexpr size_t PlayerEquipmentSlotCount = 9;
    constexpr auto ArmorSwapEquipItemMessage = static_cast<GW::UI::UIMessage>(0x100001ae);

    struct EquipItemUiPacket {
        uint32_t item_id = 0;
        uint32_t agent_id = 0;
        uint32_t reserved[2]{};
    };

    size_t EquipmentSlotForItemType(const GW::Constants::ItemType type)
    {
        switch (type) {
            case GW::Constants::ItemType::Axe:
            case GW::Constants::ItemType::Bow:
            case GW::Constants::ItemType::Hammer:
            case GW::Constants::ItemType::Wand:
            case GW::Constants::ItemType::Staff:
            case GW::Constants::ItemType::Sword:
            case GW::Constants::ItemType::Daggers:
            case GW::Constants::ItemType::Scythe:
            case GW::Constants::ItemType::Spear: return 0;
            case GW::Constants::ItemType::Offhand:
            case GW::Constants::ItemType::Shield: return 1;
            case GW::Constants::ItemType::Chestpiece: return 2;
            case GW::Constants::ItemType::Leggings: return 3;
            case GW::Constants::ItemType::Headpiece: return 4;
            case GW::Constants::ItemType::Boots: return 5;
            case GW::Constants::ItemType::Gloves: return 6;
            case GW::Constants::ItemType::Costume: return 7;
            case GW::Constants::ItemType::Costume_Headpiece: return 8;
            default: return PlayerEquipmentSlotCount;
        }
    }

    const char* EquipmentSlotName(const size_t slot)
    {
        constexpr std::array names{
            "weapon",
            "offhand",
            "chest",
            "legs",
            "head",
            "feet",
            "hands",
            "costume body",
            "costume head",
        };
        return slot < names.size() ? names[slot] : "unknown";
    }

    GW::PlayerEquipment* GetControlledPlayerEquipment()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        return player && player->equip && *player->equip
            ? static_cast<GW::PlayerEquipment*>(*player->equip)
            : nullptr;
    }

    uint32_t GetEquippedItemId(const GW::Item* item)
    {
        if (!item) return 0;
        const auto slot = EquipmentSlotForItemType(item->type);
        const auto equipment = GetControlledPlayerEquipment();
        return equipment && slot < PlayerEquipmentSlotCount
            ? equipment->item_ids[slot]
            : 0;
    }

    bool IsItemEquipped(const GW::Item* item)
    {
        if (!item) return false;
        if (GetEquippedItemId(item) == item->item_id) return true;
        return item->bag && item->bag->bag_type == GW::Constants::BagType::Equipped;
    }

    std::string WideToUtf8(const std::wstring_view value)
    {
        if (value.empty()) return {};
        const auto size = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            size,
            nullptr,
            nullptr);
        return result;
    }
}

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ArmorSwap::Plugin instance;
    return &instance;
}
#endif

namespace ArmorSwap {
    void Plugin::Initialize(
        ImGuiContext* context,
        const ImGuiAllocFns allocator_fns,
        const HMODULE toolbox_dll)
    {
        ToolboxUIPlugin::Initialize(context, allocator_fns, toolbox_dll);
        terminating_.store(false);
        game_ready_.store(false);
        refresh_inventory_requested_.store(false);
        pending_add_request_.store(0);
    }

    void Plugin::SignalTerminate()
    {
        terminating_.store(true);
        pending_hotkey_id_.store(NoHotkey);
        capturing_hotkey_id_.store(NoHotkey);
        cancel_requested_.store(false);
        game_ready_.store(false);
        refresh_inventory_requested_.store(false);
        pending_add_request_.store(0);
        ClearAvailableItems();
        {
            const std::scoped_lock lock(runtime_mutex_);
            execution_queue_.clear();
            execution_index_ = 0;
            state_ = SwapState::Idle;
            status_ = "Armor Swap terminated.";
        }
        ToolboxUIPlugin::SignalTerminate();
    }

    bool Plugin::CanTerminate()
    {
        const std::scoped_lock lock(available_items_mutex_);
        return ToolboxUIPlugin::CanTerminate() && pending_item_name_decodes_.empty();
    }

    void Plugin::LoadSettings(const wchar_t* folder)
    {
        ToolboxUIPlugin::LoadSettings(folder);

        std::vector<SwapHotkey> loaded_hotkeys;
        auto loaded_next_id = next_hotkey_id_;
        auto retry_interval = retry_interval_ms_.load();
        auto piece_timeout = piece_timeout_ms_.load();
        auto inter_item_delay = inter_item_delay_ms_.load();
        LoadSetting("hotkeys", loaded_hotkeys);
        LoadSetting("next_hotkey_id", loaded_next_id);
        LoadSetting("retry_interval_ms", retry_interval);
        LoadSetting("piece_timeout_ms", piece_timeout);
        LoadSetting("inter_item_delay_ms", inter_item_delay);

        retry_interval_ms_.store(std::clamp(retry_interval, 100, 2000));
        piece_timeout_ms_.store(std::clamp(piece_timeout, 1000, 30000));
        inter_item_delay_ms_.store(std::clamp(inter_item_delay, 0, 2000));

        std::unordered_set<uint32_t> used_ids;
        auto candidate_id = std::max(1u, loaded_next_id);
        for (auto& hotkey : loaded_hotkeys) {
            if (!hotkey.id || used_ids.contains(hotkey.id)) {
                while (!candidate_id || used_ids.contains(candidate_id)) ++candidate_id;
                hotkey.id = candidate_id++;
            }
            used_ids.insert(hotkey.id);
            candidate_id = std::max(candidate_id, hotkey.id + 1);
            if (hotkey.name.empty()) {
                hotkey.name = std::format("Armor Set {}", hotkey.id);
            }
            if (hotkey.binding.key >= 256) {
                hotkey.binding = {};
            }
            for (auto& configured : hotkey.items) {
                if (configured.label.empty()) {
                    configured.label = std::format(
                        "{} (model {})",
                        ItemTypeName(static_cast<GW::Constants::ItemType>(configured.type)),
                        configured.model_id);
                }
            }
        }
        if (loaded_hotkeys.empty()) {
            loaded_hotkeys.push_back({
                .id = candidate_id++,
                .name = "Armor Set 1",
            });
        }

        {
            const std::scoped_lock lock(config_mutex_);
            hotkeys_ = std::move(loaded_hotkeys);
            next_hotkey_id_ = candidate_id;
        }
    }

    void Plugin::SaveSettings(const wchar_t* folder)
    {
        std::vector<SwapHotkey> hotkeys;
        uint32_t next_hotkey_id;
        {
            const std::scoped_lock lock(config_mutex_);
            hotkeys = hotkeys_;
            next_hotkey_id = next_hotkey_id_;
        }
        SaveSetting("hotkeys", hotkeys);
        SaveSetting("next_hotkey_id", next_hotkey_id);
        SaveSetting("retry_interval_ms", retry_interval_ms_.load());
        SaveSetting("piece_timeout_ms", piece_timeout_ms_.load());
        SaveSetting("inter_item_delay_ms", inter_item_delay_ms_.load());
        ToolboxUIPlugin::SaveSettings(folder);
    }

    void Plugin::DrawSettings()
    {
        ToolboxUIPlugin::DrawSettings();
        ImGui::Separator();

        auto retry_interval = retry_interval_ms_.load();
        if (ImGui::SliderInt("Retry interval (ms)", &retry_interval, 100, 2000)) {
            retry_interval_ms_.store(retry_interval);
        }
        auto piece_timeout = piece_timeout_ms_.load();
        if (ImGui::SliderInt("Per-item timeout (ms)", &piece_timeout, 1000, 30000)) {
            piece_timeout_ms_.store(piece_timeout);
        }
        auto inter_item_delay = inter_item_delay_ms_.load();
        if (ImGui::SliderInt("Delay between items (ms)", &inter_item_delay, 0, 2000)) {
            inter_item_delay_ms_.store(inter_item_delay);
        }
        ImGui::TextWrapped(
            "Each configured item is resolved to a live item ID when the hotkey starts, then resolved again to a GW::Item pointer before every equip attempt.");
    }

    uint32_t Plugin::HotkeyKeyFromMessage(const UINT message, const WPARAM wparam)
    {
        switch (message) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
                return static_cast<uint32_t>(wparam);
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return VK_MBUTTON;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
            default:
                return 0;
        }
    }

    bool Plugin::IsHotkeyDownMessage(const UINT message)
    {
        return message == WM_KEYDOWN
            || message == WM_SYSKEYDOWN
            || message == WM_MBUTTONDOWN
            || message == WM_XBUTTONDOWN;
    }

    bool Plugin::IsModifierKey(const uint32_t key)
    {
        switch (key) {
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL:
            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU:
            case VK_SHIFT:
            case VK_LSHIFT:
            case VK_RSHIFT:
                return true;
            default:
                return false;
        }
    }

    bool Plugin::WndProc(const UINT message, const WPARAM wparam, const LPARAM lparam)
    {
        if (terminating_.load()) return false;

        const auto key = HotkeyKeyFromMessage(message, wparam);
        const auto key_down = IsHotkeyDownMessage(message);
        const auto capture_id = capturing_hotkey_id_.load();
        if (capture_id && key) {
            if (key_down) {
                if (key == VK_ESCAPE) {
                    capturing_hotkey_id_.store(NoHotkey);
                }
                else if (!IsModifierKey(key)) {
                    const HotkeyBinding binding{
                        .key = key,
                        .control = (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                        .alt = (GetKeyState(VK_MENU) & 0x8000) != 0,
                        .shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0,
                    };
                    const std::scoped_lock lock(config_mutex_);
                    for (auto& hotkey : hotkeys_) {
                        if (hotkey.id == capture_id) {
                            hotkey.binding = binding;
                        }
                        else if (
                            hotkey.binding.key == binding.key
                            && hotkey.binding.control == binding.control
                            && hotkey.binding.alt == binding.alt
                            && hotkey.binding.shift == binding.shift) {
                            hotkey.binding = {};
                        }
                    }
                    capturing_hotkey_id_.store(NoHotkey);
                }
            }
            return true;
        }

        if (!game_ready_.load()) return false;
        if (!key_down || !key || IsModifierKey(key)) return false;
        if (
            (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
            && (static_cast<uintptr_t>(lparam) & (1u << 30)) != 0) {
            return false;
        }
        if (
            GetForegroundWindow() != GW::MemoryMgr::GetGWWindowHandle()
            || GW::Chat::GetIsTyping()
            || ImGui::GetIO().WantTextInput
            || ImGui::GetIO().WantCaptureKeyboard) {
            return false;
        }

        const auto control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const auto alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const std::scoped_lock lock(config_mutex_);
        for (const auto& hotkey : hotkeys_) {
            if (
                hotkey.enabled
                && hotkey.binding.key == key
                && hotkey.binding.control == control
                && hotkey.binding.alt == alt
                && hotkey.binding.shift == shift) {
                pending_hotkey_id_.store(hotkey.id);
                return hotkey.block_gw;
            }
        }
        return false;
    }

    std::string Plugin::HotkeyText(const HotkeyBinding& binding)
    {
        if (!binding.key) return "Unbound";

        std::string result;
        if (binding.control) result += "Ctrl+";
        if (binding.alt) result += "Alt+";
        if (binding.shift) result += "Shift+";
        switch (binding.key) {
            case VK_MBUTTON: return result + "Middle Mouse";
            case VK_XBUTTON1: return result + "Mouse 4";
            case VK_XBUTTON2: return result + "Mouse 5";
            default: break;
        }

        const auto scan_code = MapVirtualKeyW(binding.key, MAPVK_VK_TO_VSC) << 16;
        wchar_t key_name[64]{};
        if (GetKeyNameTextW(static_cast<LONG>(scan_code), key_name, _countof(key_name))) {
            result += WideToUtf8(key_name);
        }
        else {
            result += std::format("VK {}", binding.key);
        }
        return result;
    }

    uint32_t Plugin::PackDye(const GW::DyeInfo& dye)
    {
        static_assert(sizeof(dye) <= sizeof(uint32_t));
        uint32_t packed = 0;
        std::memcpy(&packed, &dye, sizeof(dye));
        return packed;
    }

    GW::DyeInfo Plugin::UnpackDye(const uint32_t dye)
    {
        GW::DyeInfo unpacked{};
        std::memcpy(&unpacked, &dye, sizeof(unpacked));
        return unpacked;
    }

    const wchar_t* Plugin::GetEncodedItemName(const GW::Item* item)
    {
        if (!item) return nullptr;
        return item->complete_name_enc ? item->complete_name_enc : item->name_enc;
    }

    ConfiguredItem Plugin::ConfigureItem(const GW::Item* item)
    {
        ConfiguredItem configured;
        if (!item) return configured;

        configured.item_id = item->item_id;
        configured.model_id = item->model_id;
        configured.type = static_cast<uint32_t>(item->type);
        configured.profession = item->profession;
        configured.dye = PackDye(item->dye);
        if (const auto encoded_name = GetEncodedItemName(item)) {
            configured.encoded_name = SettingWString{encoded_name};
        }
        if (item->mod_struct && item->mod_struct_size) {
            configured.modifiers.reserve(item->mod_struct_size);
            for (uint32_t i = 0; i < item->mod_struct_size; ++i) {
                configured.modifiers.push_back(item->mod_struct[i].mod);
            }
        }
        configured.label = std::format(
            "{} (model {})",
            ItemTypeName(static_cast<GW::Constants::ItemType>(item->type)),
            item->model_id);
        return configured;
    }

    bool Plugin::ItemMatches(const GW::Item* item, const ConfiguredItem& configured)
    {
        if (!item || !configured.model_id) return false;
        if (item->model_id != configured.model_id) return false;
        const auto encoded_name = GetEncodedItemName(item);
        if (
            !encoded_name
            || configured.encoded_name.value.empty()
            || std::wcscmp(encoded_name, configured.encoded_name.value.c_str()) != 0) {
            return false;
        }
        if (item->mod_struct_size != configured.modifiers.size()) return false;
        if (configured.modifiers.empty()) return true;
        if (!item->mod_struct) return false;
        static_assert(sizeof(GW::ItemModifier) == sizeof(uint32_t));
        return std::memcmp(
                   item->mod_struct,
                   configured.modifiers.data(),
                   configured.modifiers.size() * sizeof(GW::ItemModifier))
            == 0;
    }

    bool Plugin::IsEquippable(const GW::Item* item)
    {
        if (!item) return false;
        switch (static_cast<GW::Constants::ItemType>(item->type)) {
            case GW::Constants::ItemType::Axe:
            case GW::Constants::ItemType::Boots:
            case GW::Constants::ItemType::Bow:
            case GW::Constants::ItemType::Chestpiece:
            case GW::Constants::ItemType::Offhand:
            case GW::Constants::ItemType::Gloves:
            case GW::Constants::ItemType::Hammer:
            case GW::Constants::ItemType::Headpiece:
            case GW::Constants::ItemType::Leggings:
            case GW::Constants::ItemType::Wand:
            case GW::Constants::ItemType::Shield:
            case GW::Constants::ItemType::Staff:
            case GW::Constants::ItemType::Sword:
            case GW::Constants::ItemType::Daggers:
            case GW::Constants::ItemType::Scythe:
            case GW::Constants::ItemType::Spear:
            case GW::Constants::ItemType::Costume_Headpiece:
            case GW::Constants::ItemType::Costume:
                return true;
            default:
                return false;
        }
    }

    bool Plugin::IsMapContextReady()
    {
        const auto game = GW::GetGameContext();
        const auto character = GW::GetCharContext();
        if (
            !game
            || !game->world
            || !game->items
            || !character
            || !character->player_name[0]
            || !GW::Map::GetIsMapLoaded()
            || GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading
            || GW::Map::GetIsObserving()
            || !GW::Agents::GetControlledCharacter()) {
            return false;
        }
        const auto inventory = GW::Items::GetInventory();
        if (!inventory) return false;
        const auto equipped = GW::Items::GetBag(GW::Constants::Bag::Equipped_Items);
        return equipped
            && equipped->inventory == inventory
            && equipped->bag_type == GW::Constants::BagType::Equipped
            && equipped->items.valid();
    }

    bool Plugin::IsPlayerEquipmentReady()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        if (!(player && player->equip && *player->equip)) return false;
        const auto equipment = static_cast<GW::PlayerEquipment*>(*player->equip);
        return equipment->IsFullyDrawn();
    }

    bool Plugin::IsGameReady()
    {
        if (!IsMapContextReady() || !IsPlayerEquipmentReady()) return false;
        const auto skillbar = GW::UI::GetFrameByLabel(L"Skillbar");
        return skillbar && skillbar->IsCreated();
    }

    GW::Item* Plugin::FindMatchingItem(const uint32_t item_id) const
    {
        if (!item_id) return nullptr;
        for (const auto bag : GetAvailableCharacterBags()) {
            for (const auto item : bag->items) {
                if (
                    item
                    && item->item_id == item_id
                    && item->bag == bag
                    && GW::Items::GetItemById(item_id) == item) {
                    return item;
                }
            }
        }
        return nullptr;
    }

    GW::Item* Plugin::FindMatchingItem(const ConfiguredItem& configured) const
    {
        if (const auto item = FindMatchingItem(configured.item_id); ItemMatches(item, configured)) {
            return item;
        }
        for (const auto bag : GetAvailableCharacterBags()) {
            for (const auto item : bag->items) {
                if (item && item->bag == bag && ItemMatches(item, configured)) return item;
            }
        }
        return nullptr;
    }

    bool Plugin::AddConfiguredItem(const uint32_t hotkey_id, const GW::Item* item)
    {
        if (!IsEquippable(item)) return false;
        const auto configured = ConfigureItem(item);
        const std::scoped_lock lock(config_mutex_);
        const auto found = std::ranges::find_if(hotkeys_, [hotkey_id](const SwapHotkey& hotkey) {
            return hotkey.id == hotkey_id;
        });
        if (found == hotkeys_.end()) return false;
        if (std::ranges::any_of(found->items, [item](const ConfiguredItem& existing) {
                return existing.item_id == item->item_id || ItemMatches(item, existing);
            })) {
            return false;
        }
        found->items.push_back(configured);
        return true;
    }

    void Plugin::QueueAddConfiguredItem(const uint32_t hotkey_id, const uint32_t item_id)
    {
        if (!(hotkey_id && item_id)) return;
        pending_add_request_.store(
            static_cast<uint64_t>(hotkey_id) << 32 | item_id,
            std::memory_order_release);
    }

    void Plugin::ProcessPendingAdd()
    {
        const auto request = pending_add_request_.exchange(0, std::memory_order_acquire);
        if (!request) return;
        const auto hotkey_id = static_cast<uint32_t>(request >> 32);
        const auto item_id = static_cast<uint32_t>(request);
        const auto item = FindMatchingItem(item_id);
        const auto added = IsEquippable(item) && AddConfiguredItem(hotkey_id, item);
        const std::scoped_lock lock(runtime_mutex_);
        status_ = added
            ? std::format("Added item ID {} to the armor swap.", item_id)
            : std::format("Item ID {} is unavailable, invalid, or already configured.", item_id);
    }

    void Plugin::RefreshAvailableItems()
    {
        if (!IsGameReady()) return;
        std::vector<AvailableItem> refreshed_items;
        std::unordered_set<uint32_t> seen;
        for (const auto bag : GetAvailableCharacterBags()) {
            for (const auto item : bag->items) {
                const auto encoded_name = GetEncodedItemName(item);
                if (
                    !item
                    || item->bag != bag
                    || !item->item_id
                    || item->slot >= bag->items.size()
                    || bag->items[item->slot] != item
                    || GW::Items::GetItemById(item->item_id) != item
                    || !encoded_name
                    || item->mod_struct_size > MaxItemModifiers
                    || (item->mod_struct_size && !item->mod_struct)
                    || !seen.insert(item->item_id).second
                    || !IsEquippable(item)) {
                    continue;
                }
                refreshed_items.push_back({
                    .configured = ConfigureItem(item),
                    .bag = bag->bag_id(),
                    .slot = static_cast<uint32_t>(item->slot) + 1,
                });
            }
        }
        const std::scoped_lock lock(available_items_mutex_);
        ++available_items_generation_;
        available_items_ = std::move(refreshed_items);
    }

    std::vector<GW::Bag*> Plugin::GetAvailableCharacterBags()
    {
        std::vector<GW::Bag*> bags;
        if (!IsMapContextReady()) return bags;
        const auto inventory = GW::Items::GetInventory();
        if (!inventory) return bags;

        for (const auto bag_id : CharacterEquipmentBags) {
            const auto bag = GW::Items::GetBag(bag_id);
            const auto expected_type = bag_id == GW::Constants::Bag::Equipped_Items
                ? GW::Constants::BagType::Equipped
                : GW::Constants::BagType::Inventory;
            if (
                !bag
                || bag->inventory != inventory
                || !bag->items.valid()
                || bag->items.size() > MaxBagSlots
                || (bag->items.size() && !bag->items.m_buffer)
                || bag->bag_type != expected_type
                || bag->bag_id() != bag_id) {
                continue;
            }
            bags.push_back(bag);
        }
        return bags;
    }

    void Plugin::BeginItemNameDecodes()
    {
        ItemNameDecodeContext* context = nullptr;
        {
            const std::scoped_lock lock(available_items_mutex_);
            if (!pending_item_name_decodes_.empty()) return;
            for (auto& available : available_items_) {
                if (available.decode_started) continue;
                available.decode_started = true;
                if (available.configured.encoded_name.value.empty()) {
                    available.decoded_name = available.configured.label;
                    continue;
                }
                context = new ItemNameDecodeContext{
                    .owner = this,
                    .generation = available_items_generation_,
                    .item_id = available.configured.item_id,
                    .encoded_name = available.configured.encoded_name.value,
                };
                pending_item_name_decodes_.push_back(context);
                break;
            }
        }
        if (!context) return;
        GW::GameThread::Enqueue([context] {
            const auto owner = context->owner;
            if (
                !owner
                || owner->terminating_.load()
                || !owner->IsMapContextReady()
                || !GW::UI::IsValidEncStr(context->encoded_name.c_str())) {
                OnItemNameDecoded(context, nullptr);
                return;
            }
            GW::UI::AsyncDecodeStr(
                context->encoded_name.c_str(),
                OnItemNameDecoded,
                context);
        });
    }

    void Plugin::CompleteItemNameDecode(ItemNameDecodeContext* context, const wchar_t* decoded)
    {
        const std::scoped_lock lock(available_items_mutex_);
        std::erase(pending_item_name_decodes_, context);
        if (context->generation != available_items_generation_) return;
        const auto found = std::ranges::find(
            available_items_,
            context->item_id,
            [](const AvailableItem& available) { return available.configured.item_id; });
        if (found == available_items_.end()) return;
        found->decoded_name = decoded && decoded[0]
            ? WideToUtf8(decoded)
            : found->configured.label;
    }

    void Plugin::OnItemNameDecoded(void* raw_context, const wchar_t* decoded)
    {
        const auto context = static_cast<ItemNameDecodeContext*>(raw_context);
        if (!context) return;
        if (context->owner) {
            context->owner->CompleteItemNameDecode(context, decoded);
        }
        delete context;
    }

    void Plugin::ClearAvailableItems()
    {
        const std::scoped_lock lock(available_items_mutex_);
        ++available_items_generation_;
        available_items_.clear();
    }

    void Plugin::StartSwap(const uint32_t hotkey_id)
    {
        {
            const std::scoped_lock lock(runtime_mutex_);
            if (state_ == SwapState::Equipping || state_ == SwapState::InterItemDelay) {
                status_ = "Another armor swap is already running.";
                return;
            }
        }
        if (!IsGameReady()) {
            const std::scoped_lock lock(runtime_mutex_);
            Fail("Cannot start an armor swap until the map, inventory, and player equipment are ready.");
            return;
        }

        SwapHotkey hotkey;
        {
            const std::scoped_lock lock(config_mutex_);
            const auto found = std::ranges::find_if(hotkeys_, [hotkey_id](const SwapHotkey& candidate) {
                return candidate.id == hotkey_id;
            });
            if (found == hotkeys_.end() || !found->enabled) return;
            hotkey = *found;
        }
        if (hotkey.items.empty()) {
            const std::scoped_lock lock(runtime_mutex_);
            Fail(std::format("{} has no configured items.", hotkey.name));
            return;
        }

        std::vector<uint32_t> item_ids;
        item_ids.reserve(hotkey.items.size());
        for (auto& configured : hotkey.items) {
            auto* item = FindMatchingItem(configured);
            if (configured.encoded_name.value.empty()) {
                const auto legacy_item = FindMatchingItem(configured.item_id);
                item = legacy_item && legacy_item->model_id == configured.model_id
                    ? legacy_item
                    : nullptr;
            }
            if (!IsEquippable(item)) {
                const std::scoped_lock lock(runtime_mutex_);
                const auto reason = configured.encoded_name.value.empty()
                    ? std::format(
                        "{} in {} was saved without canonical identity data; choose the item again.",
                        configured.label,
                        hotkey.name)
                    : std::format("Could not find {} for {}.", configured.label, hotkey.name);
                Fail(reason);
                return;
            }
            configured = ConfigureItem(item);
            if (!std::ranges::contains(item_ids, item->item_id)) {
                item_ids.push_back(item->item_id);
            }
        }

        {
            const std::scoped_lock lock(config_mutex_);
            const auto found = std::ranges::find_if(hotkeys_, [hotkey_id](const SwapHotkey& candidate) {
                return candidate.id == hotkey_id;
            });
            if (found != hotkeys_.end() && found->items.size() == hotkey.items.size()) {
                for (size_t i = 0; i < found->items.size(); ++i) {
                    found->items[i] = hotkey.items[i];
                }
            }
        }

        const std::scoped_lock lock(runtime_mutex_);
        execution_queue_ = std::move(item_ids);
        execution_index_ = 0;
        active_swap_name_ = hotkey.name;
        piece_started_ = Clock::now();
        last_try_ = {};
        equip_attempts_ = 0;
        last_equip_request_submitted_ = false;
        state_ = SwapState::Equipping;
        status_ = std::format(
            "Starting {} ({} item{}).",
            active_swap_name_,
            execution_queue_.size(),
            execution_queue_.size() == 1 ? "" : "s");
    }

    void Plugin::Update(float)
    {
        if (terminating_.load()) return;
        if (!IsMapContextReady()) {
            game_ready_.store(false);
            pending_hotkey_id_.store(NoHotkey);
            refresh_inventory_requested_.store(false);
            pending_add_request_.store(0);
            ClearAvailableItems();
            const std::scoped_lock lock(runtime_mutex_);
            if (state_ == SwapState::Equipping || state_ == SwapState::InterItemDelay) {
                Cancel("Armor swap cancelled: the playable map context is no longer available.");
            }
            return;
        }

        const auto game_ready = IsGameReady();
        game_ready_.store(game_ready);
        if (!game_ready) {
            pending_hotkey_id_.store(NoHotkey);
            refresh_inventory_requested_.store(false);
            return;
        }

        const auto inventory_refreshed = refresh_inventory_requested_.exchange(false);
        if (inventory_refreshed) {
            RefreshAvailableItems();
        }
        else {
            BeginItemNameDecodes();
        }
        ProcessPendingAdd();
        if (const auto hotkey_id = pending_hotkey_id_.exchange(NoHotkey)) {
            StartSwap(hotkey_id);
        }
        if (cancel_requested_.exchange(false)) {
            const std::scoped_lock lock(runtime_mutex_);
            if (state_ == SwapState::Equipping || state_ == SwapState::InterItemDelay) {
                Cancel("Armor swap cancelled.");
            }
        }
        ProcessActiveSwap();
    }

    void Plugin::ProcessActiveSwap()
    {
        const std::scoped_lock lock(runtime_mutex_);
        if (state_ != SwapState::Equipping && state_ != SwapState::InterItemDelay) return;
        if (!IsMapContextReady()) {
            Cancel("Armor swap cancelled: the playable map context is no longer available.");
            return;
        }
        if (execution_index_ >= execution_queue_.size()) {
            state_ = SwapState::Idle;
            return;
        }

        const auto now = Clock::now();
        if (state_ == SwapState::InterItemDelay) {
            if (now < resume_at_) return;
            piece_started_ = now;
            last_try_ = {};
            equip_attempts_ = 0;
            last_equip_request_submitted_ = false;
            state_ = SwapState::Equipping;
        }
        Execute(execution_queue_[execution_index_]);
    }

    void Plugin::Execute(const uint32_t item_id)
    {
        if (!IsGameReady()) return;
        const auto now = Clock::now();
        auto* item = FindMatchingItem(item_id);
        if (!IsEquippable(item)) {
            Fail(std::format("Lost configured item ID {} while equipping.", item_id));
            return;
        }
        if (IsItemEquipped(item)) {
            FinishCurrentItem();
            return;
        }
        if (
            std::chrono::duration_cast<std::chrono::milliseconds>(now - piece_started_).count()
            > piece_timeout_ms_.load()) {
            const auto equipment_slot = EquipmentSlotForItemType(item->type);
            const auto source_bag = item->bag
                ? item->bag->bag_id()
                : GW::Constants::Bag::None;
            Fail(std::format(
                "Timed out equipping item ID {} after {} attempts. Last request: {}; player {} slot item ID: {}; source: {} slot {} (equipped flag {}).",
                item_id,
                equip_attempts_,
                last_equip_request_submitted_ ? "submitted" : "not submitted",
                EquipmentSlotName(equipment_slot),
                GetEquippedItemId(item),
                BagName(source_bag),
                static_cast<uint32_t>(item->slot) + 1,
                item->equipped));
            return;
        }

        const auto player = GW::Agents::GetControlledCharacter();
        if (!player || !player->GetIsAlive()) {
            Fail("Cannot equip items while dead or the player agent is unavailable.");
            return;
        }
        const auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (
            player->GetIsKnockedDown()
            || player->skill
            || (skillbar && skillbar->cast_array.size())
            || !player->model_state) {
            return;
        }
        if (
            last_try_ != Clock::time_point{}
            && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_try_).count()
                < retry_interval_ms_.load()) {
            return;
        }
        last_try_ = now;

        if (!(player->GetIsIdle() || player->GetIsMoving())) {
            GW::Agents::Move(player->pos);
            status_ = std::format("Clearing the player action before equipping item ID {}.", item_id);
            return;
        }

        ++equip_attempts_;
        auto packet = EquipItemUiPacket{
            .item_id = item->item_id,
            .agent_id = player->agent_id,
        };
        last_equip_request_submitted_ = GW::UI::SendUIMessage(
            ArmorSwapEquipItemMessage,
            &packet);
        status_ = std::format(
            "Equipping item ID {} via UI message 0x100001AE ({}/{}, attempt {}, request {}).",
            item_id,
            execution_index_ + 1,
            execution_queue_.size(),
            equip_attempts_,
            last_equip_request_submitted_ ? "submitted" : "not submitted");
    }

    void Plugin::FinishCurrentItem()
    {
        ++execution_index_;
        equip_attempts_ = 0;
        last_equip_request_submitted_ = false;
        last_try_ = {};
        if (execution_index_ >= execution_queue_.size()) {
            const auto completed_name = active_swap_name_;
            execution_queue_.clear();
            execution_index_ = 0;
            state_ = SwapState::Idle;
            status_ = std::format("{} equipped.", completed_name);
            return;
        }

        resume_at_ = Clock::now()
            + std::chrono::milliseconds(std::max(0, inter_item_delay_ms_.load()));
        piece_started_ = resume_at_;
        state_ = SwapState::InterItemDelay;
        status_ = std::format(
            "Equipped item {}/{}; waiting for the next item.",
            execution_index_,
            execution_queue_.size());
    }

    void Plugin::Fail(const std::string& reason)
    {
        execution_queue_.clear();
        execution_index_ = 0;
        state_ = SwapState::Failed;
        status_ = reason;
        Log::Error("ArmorSwap: %s", reason.c_str());
    }

    void Plugin::Cancel(const std::string& reason)
    {
        execution_queue_.clear();
        execution_index_ = 0;
        state_ = SwapState::Idle;
        status_ = reason;
    }

    void Plugin::Draw(IDirect3DDevice9*)
    {
        if (!GetVisiblePtr() || !*GetVisiblePtr()) return;

        ImGui::SetNextWindowSize({620.f, 480.f}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            ImGui::End();
            return;
        }

        std::string status;
        auto running = false;
        const auto game_ready = game_ready_.load();
        {
            const std::scoped_lock lock(runtime_mutex_);
            status = status_;
            running = state_ == SwapState::Equipping || state_ == SwapState::InterItemDelay;
        }
        ImGui::TextWrapped("Status: %s", status.c_str());
        if (!game_ready) {
            ImGui::TextDisabled("Waiting for a fully loaded playable map and initialized player equipment.");
        }
        if (running) {
            if (ImGui::Button("Cancel current swap")) cancel_requested_.store(true);
        }
        ImGui::Separator();

        auto open_picker_for = NoHotkey;
        auto delete_hotkey = NoHotkey;
        auto feedback = std::string{};
        {
            const std::scoped_lock lock(config_mutex_);
            if (ImGui::Button("Add swap hotkey")) {
                const auto id = next_hotkey_id_++;
                hotkeys_.push_back({
                    .id = id,
                    .name = std::format("Armor Set {}", hotkeys_.size() + 1),
                });
            }
            ImGui::SameLine();
            ImGui::TextDisabled("One hotkey can equip any number of armor, weapon, or costume items in order.");

            if (hotkeys_.empty()) {
                ImGui::TextDisabled("No swap hotkeys configured.");
            }

            for (auto& hotkey : hotkeys_) {
                ImGui::PushID(static_cast<int>(hotkey.id));
                const auto header = std::format("{}###swap_{}", hotkey.name, hotkey.id);
                if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Enabled", &hotkey.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Block Guild Wars input", &hotkey.block_gw);

                    std::array<char, 129> name_buffer{};
                    std::snprintf(name_buffer.data(), name_buffer.size(), "%s", hotkey.name.c_str());
                    ImGui::SetNextItemWidth(280.f);
                    if (ImGui::InputText("Name", name_buffer.data(), name_buffer.size())) {
                        hotkey.name = name_buffer.data();
                    }

                    ImGui::TextUnformatted("Hotkey");
                    ImGui::SameLine(110.f);
                    const auto capturing = capturing_hotkey_id_.load() == hotkey.id;
                    if (ImGui::Button(
                            capturing ? "Press a key combination..." : HotkeyText(hotkey.binding).c_str(),
                            {220.f, 0.f})) {
                        capturing_hotkey_id_.store(capturing ? NoHotkey : hotkey.id);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear binding")) {
                        hotkey.binding = {};
                        if (capturing) capturing_hotkey_id_.store(NoHotkey);
                    }
                    ImGui::SameLine();
                    ImGui::BeginDisabled(!game_ready);
                    if (ImGui::Button("Run now")) pending_hotkey_id_.store(hotkey.id);
                    ImGui::EndDisabled();

                    ImGui::SeparatorText("Items");
                    for (size_t item_index = 0; item_index < hotkey.items.size();) {
                        auto& configured = hotkey.items[item_index];
                        ImGui::PushID(static_cast<int>(item_index));
                        ImGui::Text(
                            "%zu. %s | live item ID %u",
                            item_index + 1,
                            configured.label.c_str(),
                            configured.item_id);
                        ImGui::SameLine();
                        ImGui::BeginDisabled(item_index == 0);
                        if (ImGui::SmallButton("Up")) {
                            std::swap(hotkey.items[item_index], hotkey.items[item_index - 1]);
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        ImGui::BeginDisabled(item_index + 1 >= hotkey.items.size());
                        if (ImGui::SmallButton("Down")) {
                            std::swap(hotkey.items[item_index], hotkey.items[item_index + 1]);
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        const auto remove = ImGui::SmallButton("Remove");
                        ImGui::PopID();
                        if (remove) {
                            hotkey.items.erase(hotkey.items.begin() + static_cast<std::ptrdiff_t>(item_index));
                            continue;
                        }
                        ++item_index;
                    }
                    if (hotkey.items.empty()) {
                        ImGui::TextDisabled("No items configured.");
                    }

                    auto& manual_id = manual_item_ids_[hotkey.id];
                    ImGui::SetNextItemWidth(150.f);
                    ImGui::InputScalar("Live item ID", ImGuiDataType_U32, &manual_id);
                    ImGui::SameLine();
                    ImGui::BeginDisabled(!game_ready);
                    if (ImGui::Button("Add item ID")) {
                        QueueAddConfiguredItem(hotkey.id, manual_id);
                        feedback = std::format("Checking item ID {}...", manual_id);
                        manual_id = 0;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Choose from inventory...")) {
                        open_picker_for = hotkey.id;
                    }
                    ImGui::EndDisabled();

                    if (ImGui::Button("Delete this hotkey")) delete_hotkey = hotkey.id;
                }
                ImGui::PopID();
            }

            if (delete_hotkey) {
                std::erase_if(hotkeys_, [delete_hotkey](const SwapHotkey& hotkey) {
                    return hotkey.id == delete_hotkey;
                });
                manual_item_ids_.erase(delete_hotkey);
                if (capturing_hotkey_id_.load() == delete_hotkey) {
                    capturing_hotkey_id_.store(NoHotkey);
                }
            }
        }

        if (!feedback.empty()) {
            const std::scoped_lock lock(runtime_mutex_);
            status_ = feedback;
        }
        if (open_picker_for) {
            picker_hotkey_id_ = open_picker_for;
            refresh_inventory_requested_.store(true);
            ImGui::OpenPopup("Choose armor swap items");
        }
        DrawItemPicker();

        ImGui::End();
    }

    void Plugin::DrawItemPicker()
    {
        if (!ImGui::BeginPopupModal(
                "Choose armor swap items",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize)) {
            return;
        }

        ImGui::TextWrapped(
            "Add one or more equipped or carried items. Bag and slot are current-location details only; matching searches every available character bag by the same identity used by Equip Item hotkeys.");
        const auto game_ready = game_ready_.load();
        ImGui::BeginDisabled(!game_ready);
        if (ImGui::Button("Refresh inventory")) refresh_inventory_requested_.store(true);
        ImGui::EndDisabled();
        ImGui::Separator();

        if (ImGui::BeginChild("AvailableItems", {850.f, 420.f}, true)) {
            const std::scoped_lock lock(available_items_mutex_);
            for (const auto& available : available_items_) {
                ImGui::PushID(static_cast<int>(available.configured.item_id));
                ImGui::BeginDisabled(!game_ready);
                if (ImGui::SmallButton("Add")) {
                    QueueAddConfiguredItem(picker_hotkey_id_, available.configured.item_id);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextUnformatted(
                    available.decoded_name.empty()
                        ? "Decoding item name..."
                        : available.decoded_name.c_str());

                const auto dye = UnpackDye(available.configured.dye);
                ImGui::Indent();
                ImGui::TextDisabled(
                    "Model ID: %u | Dye tint ID: %u | Dye channels: %u/%u/%u/%u | Item ID: %u | %s slot: %u | Type: %s | Profession: %u",
                    available.configured.model_id,
                    static_cast<uint32_t>(dye.dye_tint),
                    static_cast<uint32_t>(dye.dye1),
                    static_cast<uint32_t>(dye.dye2),
                    static_cast<uint32_t>(dye.dye3),
                    static_cast<uint32_t>(dye.dye4),
                    available.configured.item_id,
                    BagName(available.bag),
                    available.slot,
                    ItemTypeName(static_cast<GW::Constants::ItemType>(available.configured.type)),
                    available.configured.profession);
                ImGui::Unindent();
                ImGui::Separator();
                ImGui::PopID();
            }
            if (available_items_.empty()) {
                ImGui::TextDisabled(
                    game_ready
                        ? "No equippable items were found."
                        : "Inventory access is paused until the playable map is ready.");
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("Done")) {
            picker_hotkey_id_ = NoHotkey;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    const char* Plugin::ItemTypeName(const GW::Constants::ItemType type)
    {
        switch (type) {
            case GW::Constants::ItemType::Axe: return "Axe";
            case GW::Constants::ItemType::Boots: return "Boots";
            case GW::Constants::ItemType::Bow: return "Bow";
            case GW::Constants::ItemType::Chestpiece: return "Chestpiece";
            case GW::Constants::ItemType::Offhand: return "Offhand";
            case GW::Constants::ItemType::Gloves: return "Gloves";
            case GW::Constants::ItemType::Hammer: return "Hammer";
            case GW::Constants::ItemType::Headpiece: return "Headpiece";
            case GW::Constants::ItemType::Leggings: return "Leggings";
            case GW::Constants::ItemType::Wand: return "Wand";
            case GW::Constants::ItemType::Shield: return "Shield";
            case GW::Constants::ItemType::Staff: return "Staff";
            case GW::Constants::ItemType::Sword: return "Sword";
            case GW::Constants::ItemType::Daggers: return "Daggers";
            case GW::Constants::ItemType::Scythe: return "Scythe";
            case GW::Constants::ItemType::Spear: return "Spear";
            case GW::Constants::ItemType::Costume_Headpiece: return "Costume headpiece";
            case GW::Constants::ItemType::Costume: return "Costume";
            default: return "Item";
        }
    }

    const char* Plugin::BagName(const GW::Constants::Bag bag)
    {
        switch (bag) {
            case GW::Constants::Bag::Equipped_Items: return "Equipped";
            case GW::Constants::Bag::Backpack: return "Backpack";
            case GW::Constants::Bag::Belt_Pouch: return "Belt Pouch";
            case GW::Constants::Bag::Bag_1: return "Bag 1";
            case GW::Constants::Bag::Bag_2: return "Bag 2";
            case GW::Constants::Bag::Equipment_Pack: return "Equipment Pack";
            default: return "Unknown bag";
        }
    }
}
