#include "LootNotifier.h"

#include <AsyncStringDecoder.h>
#include <PluginUtils.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Packets/StoC.h>

#include <cstring>

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static LootNotifier instance;
    return &instance;
}
#endif

namespace {
    constexpr auto RetryTimeout = std::chrono::seconds(5);
    constexpr size_t MaximumVisibleDrops = 50;

    GW::Constants::Rarity ItemRarity(const GW::Item* item)
    {
        if (!item) return GW::Constants::Rarity::Unknown;
        if (item->interaction & 0x10) return GW::Constants::Rarity::Green;
        if (item->interaction & 0x400000) return GW::Constants::Rarity::Purple;
        if (item->interaction & 0x20000) return GW::Constants::Rarity::Gold;
        if (item->single_item_name && item->single_item_name[0] == 0xa3f) {
            return GW::Constants::Rarity::Blue;
        }
        return GW::Constants::Rarity::White;
    }

    const GW::ItemModifier* FindModifier(const GW::Item& item, const uint32_t identifier)
    {
        if (!item.mod_struct) return nullptr;
        for (auto index = uint32_t{0}; index < item.mod_struct_size; ++index) {
            if (item.mod_struct[index].identifier() == identifier) {
                return &item.mod_struct[index];
            }
        }
        return nullptr;
    }

    const char* AttributeName(const uint8_t attribute)
    {
        switch (attribute) {
            case 0x00: return "Fast Casting";
            case 0x01: return "Illusion Magic";
            case 0x02: return "Domination Magic";
            case 0x03: return "Inspiration Magic";
            case 0x04: return "Blood Magic";
            case 0x05: return "Death Magic";
            case 0x06: return "Soul Reaping";
            case 0x07: return "Curses";
            case 0x08: return "Air Magic";
            case 0x09: return "Earth Magic";
            case 0x0a: return "Fire Magic";
            case 0x0b: return "Water Magic";
            case 0x0c: return "Energy Storage";
            case 0x0d: return "Healing Prayers";
            case 0x0e: return "Smiting Prayers";
            case 0x0f: return "Protection Prayers";
            case 0x10: return "Divine Favor";
            case 0x11: return "Strength";
            case 0x12: return "Axe Mastery";
            case 0x13: return "Hammer Mastery";
            case 0x14: return "Swordsmanship";
            case 0x15: return "Tactics";
            case 0x16: return "Beast Mastery";
            case 0x17: return "Expertise";
            case 0x18: return "Wilderness Survival";
            case 0x19: return "Marksmanship";
            case 0x1d: return "Dagger Mastery";
            case 0x1e: return "Deadly Arts";
            case 0x1f: return "Shadow Arts";
            case 0x20: return "Communing";
            case 0x21: return "Restoration Magic";
            case 0x22: return "Channeling Magic";
            case 0x23: return "Critical Strikes";
            case 0x24: return "Spawning Power";
            case 0x25: return "Spear Mastery";
            case 0x26: return "Command";
            case 0x27: return "Motivation";
            case 0x28: return "Leadership";
            case 0x29: return "Scythe Mastery";
            case 0x2a: return "Wind Prayers";
            case 0x2b: return "Earth Prayers";
            case 0x2c: return "Mysticism";
            default: return nullptr;
        }
    }

    bool IsMartialRequirement(const uint8_t attribute)
    {
        return attribute == 0x12 || attribute == 0x13 || attribute == 0x14
            || attribute == 0x19 || attribute == 0x1d || attribute == 0x25
            || attribute == 0x29;
    }

    int ResizeString(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) {
            return 0;
        }
        const auto value = static_cast<std::string*>(data->UserData);
        value->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = value->data();
        return 0;
    }

    bool InputString(const char* label, std::string& value)
    {
        if (value.capacity() < 128) {
            value.reserve(128);
        }
        const auto changed = ImGui::InputText(
            label,
            value.data(),
            value.capacity() + 1,
            ImGuiInputTextFlags_CallbackResize,
            ResizeString,
            &value);
        value.resize(std::strlen(value.c_str()));
        return changed;
    }

    const char* FilterLabel(const LootNotifier::DropFilter filter)
    {
        switch (filter) {
            case LootNotifier::DropFilter::All: return "All drops";
            case LootNotifier::DropFilter::OtherPlayers: return "Other players";
            case LootNotifier::DropFilter::Self: return "My drops";
        }
        return "All drops";
    }

    bool DrawFilter(const char* label, LootNotifier::DropFilter& filter)
    {
        auto changed = false;
        if (ImGui::BeginCombo(label, FilterLabel(filter))) {
            for (auto value : {
                     LootNotifier::DropFilter::All,
                     LootNotifier::DropFilter::OtherPlayers,
                     LootNotifier::DropFilter::Self}) {
                const auto selected = filter == value;
                if (ImGui::Selectable(FilterLabel(value), selected)) {
                    filter = value;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    void ReplaceAll(std::string& value, const std::string_view token, const std::string_view replacement)
    {
        auto offset = size_t{0};
        while ((offset = value.find(token, offset)) != std::string::npos) {
            value.replace(offset, token.size(), replacement);
            offset += replacement.size();
        }
    }
}

LootNotifier::LootNotifier()
{
    can_close = true;
    can_show_in_main_window = true;
    show_title = false;
    tracked_items_ = {
        {.model_id = 32823},
        {.model_id = 32822},
        {.model_id = 1045},
        {.model_id = 1900},
        {.model_id = 2071},
        {.model_id = 399},
    };
}

void LootNotifier::Initialize(
    ImGuiContext* context, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(context, allocator_fns, toolbox_dll);
    terminating_ = false;
    shared_state_ = std::make_shared<SharedState>();
    last_map_id_ = static_cast<uint32_t>(GW::Map::GetMapID());
    last_instance_time_ = GW::Map::GetInstanceTime();
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemUpdateOwner>(
        &item_owner_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ItemUpdateOwner* packet) {
            if (!terminating_ && packet) {
                OnItemUpdateOwner(*packet);
            }
        });
}

void LootNotifier::SignalTerminate()
{
    terminating_ = true;
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemUpdateOwner>(&item_owner_hook_);
    if (shared_state_) {
        std::scoped_lock lock(shared_state_->mutex);
        shared_state_->active = false;
        ++shared_state_->generation;
        shared_state_->completed.clear();
    }
    pending_drops_.clear();
    ToolboxUIPlugin::SignalTerminate();
}

void LootNotifier::Terminate()
{
    shared_state_.reset();
    pending_drops_.clear();
    handled_item_ids_.clear();
    drops_.clear();
    ToolboxUIPlugin::Terminate();
}

void LootNotifier::OnItemUpdateOwner(const GW::Packet::StoC::ItemUpdateOwner& packet)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable
        || !packet.item_id || !packet.owner_agent_id
        || handled_item_ids_.contains(packet.item_id)) {
        return;
    }
    const auto duplicate = std::ranges::any_of(pending_drops_, [&packet](const auto& pending) {
        return pending.item_id == packet.item_id;
    });
    if (!duplicate) {
        pending_drops_.push_back({
            .item_id = packet.item_id,
            .owner_agent_id = packet.owner_agent_id,
            .received_at = std::chrono::steady_clock::now(),
        });
    }
}

void LootNotifier::Update(float)
{
    if (terminating_) {
        return;
    }
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    const auto instance_time = GW::Map::GetInstanceTime();
    if (map_id != last_map_id_ || instance_time < last_instance_time_) {
        ResetInstanceState();
        last_map_id_ = map_id;
    }
    last_instance_time_ = instance_time;
    ProcessPendingDrops();
    ProcessDecodedDrops();
}

void LootNotifier::ResetInstanceState()
{
    pending_drops_.clear();
    handled_item_ids_.clear();
    drops_.clear();
    if (shared_state_) {
        std::scoped_lock lock(shared_state_->mutex);
        ++shared_state_->generation;
        shared_state_->completed.clear();
    }
}

bool LootNotifier::IsTracked(const uint32_t model_id, const uint32_t model_file_id) const
{
    return std::ranges::any_of(tracked_items_, [model_id, model_file_id](const auto& tracked) {
        return tracked.enabled
            && (tracked.model_file_id
                    ? tracked.model_file_id == model_file_id
                    : tracked.model_id && tracked.model_id == model_id);
    });
}

void LootNotifier::ProcessPendingDrops()
{
    const auto now = std::chrono::steady_clock::now();
    for (auto current = pending_drops_.begin(); current != pending_drops_.end();) {
        const auto item = GW::Items::GetItemById(current->item_id);
        if (item && IsTracked(item->model_id, item->model_file_id)) {
            handled_item_ids_.insert(current->item_id);
            BeginDecode(*current);
            current = pending_drops_.erase(current);
        }
        else if (item || now - current->received_at >= RetryTimeout) {
            if (item) {
                handled_item_ids_.insert(current->item_id);
            }
            current = pending_drops_.erase(current);
        }
        else {
            ++current;
        }
    }
}

void LootNotifier::BeginDecode(const PendingDrop& pending)
{
    const auto item = GW::Items::GetItemById(pending.item_id);
    if (!item || !shared_state_) {
        return;
    }
    const auto request = std::make_shared<DecodeRequest>();
    {
        std::scoped_lock lock(shared_state_->mutex);
        if (!shared_state_->active) {
            return;
        }
        request->generation = shared_state_->generation;
    }
    request->drop.item_id = pending.item_id;
    request->drop.self = pending.owner_agent_id == GW::Agents::GetControlledCharacterId();
    request->drop.rarity = ItemRarity(item);

    auto encoded_item = item->single_item_name;
    if (!encoded_item || !*encoded_item) {
        encoded_item = item->name_enc;
    }
    const auto requirement = RequirementPrefix(*item);
    const auto state = shared_state_;
    if (encoded_item && *encoded_item) {
        AsyncStringDecoder::Decode(encoded_item, [state, request, requirement](const wchar_t* decoded) {
            auto decorated = requirement + PluginUtils::WStringToString(decoded ? decoded : L"Unknown item");
            CompleteDecode(state, request, true, PluginUtils::StringToWString(decorated).c_str());
        });
    }
    else {
        CompleteDecode(state, request, true, L"Unknown item");
    }

    const auto encoded_player = GW::Agents::GetAgentEncName(pending.owner_agent_id);
    if (encoded_player && *encoded_player) {
        AsyncStringDecoder::Decode(encoded_player, [state, request](const wchar_t* decoded) {
            CompleteDecode(state, request, false, decoded ? decoded : L"Unknown player");
        });
    }
    else {
        CompleteDecode(state, request, false, request->drop.self ? L"you" : L"Unknown player");
    }
}

void LootNotifier::CompleteDecode(
    const std::shared_ptr<SharedState>& state,
    const std::shared_ptr<DecodeRequest>& request,
    const bool item,
    const wchar_t* decoded)
{
    auto ready = false;
    {
        std::scoped_lock lock(request->mutex);
        if (item) {
            request->drop.item = PluginUtils::WStringToString(decoded ? decoded : L"");
            request->item_ready = true;
        }
        else {
            request->drop.player = PluginUtils::WStringToString(decoded ? decoded : L"");
            request->player_ready = true;
        }
        ready = request->item_ready && request->player_ready && !request->queued;
        if (ready) {
            request->queued = true;
        }
    }
    if (!ready) {
        return;
    }
    std::scoped_lock lock(state->mutex);
    if (state->active && state->generation == request->generation) {
        state->completed.push_back(request->drop);
    }
}

void LootNotifier::ProcessDecodedDrops()
{
    if (!shared_state_) {
        return;
    }
    std::deque<Drop> completed;
    {
        std::scoped_lock lock(shared_state_->mutex);
        completed.swap(shared_state_->completed);
    }
    for (const auto& drop : completed) {
        drops_.push_back(drop);
        while (drops_.size() > MaximumVisibleDrops) {
            drops_.pop_front();
        }
        Notify(drop);
    }
}

bool LootNotifier::PassesFilter(const DropFilter filter, const bool self) const
{
    return filter == DropFilter::All
        || (filter == DropFilter::Self && self)
        || (filter == DropFilter::OtherPlayers && !self);
}

std::string LootNotifier::Format(const std::string& pattern, const Drop& drop) const
{
    auto result = pattern;
    ReplaceAll(result, "[player]", drop.player);
    ReplaceAll(result, "[item]", drop.item);
    return result;
}

void LootNotifier::Notify(const Drop& drop) const
{
    if (send_notification_ && PassesFilter(notification_filter_, drop.self)) {
        const auto message = PluginUtils::StringToWString(
            Format(drop.self ? notification_format_self_ : notification_format_, drop));
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, message.c_str());
    }
    if (send_party_chat_
        && PassesFilter(party_chat_filter_, drop.self)
        && GW::PartyMgr::GetPartyPlayerCount() > 1) {
        const auto message = PluginUtils::StringToWString(
            Format(drop.self ? party_chat_format_self_ : party_chat_format_, drop));
        GW::Chat::SendChat('#', message.c_str());
    }
}

void LootNotifier::SendCongratulations(const Drop& drop) const
{
    const auto message = PluginUtils::StringToWString(
        Format(drop.self ? gz_format_self_ : gz_format_, drop));
    if (GW::PartyMgr::GetPartyPlayerCount() > 1) {
        GW::Chat::SendChat('#', message.c_str());
    }
    else {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, message.c_str());
    }
}

std::string LootNotifier::RequirementPrefix(const GW::Item& item)
{
    const auto modifier = FindModifier(item, 0x2798);
    if (!modifier) {
        return {};
    }
    const auto requirement = modifier->arg2() & 0xf;
    if (!requirement) {
        return {};
    }
    const auto attribute = static_cast<uint8_t>(modifier->arg1());
    const auto attribute_name = AttributeName(attribute);
    return !attribute_name || IsMartialRequirement(attribute)
        ? std::format("q{} ", requirement)
        : std::format("q{} {} ", requirement, attribute_name);
}

ImVec4 LootNotifier::RarityColor(const GW::Constants::Rarity rarity)
{
    switch (rarity) {
        case GW::Constants::Rarity::Blue: return ImColor(GW::Chat::TextColor::ColorItemEnhance);
        case GW::Constants::Rarity::Purple: return ImColor(GW::Chat::TextColor::ColorItemUncommon);
        case GW::Constants::Rarity::Gold: return ImColor(GW::Chat::TextColor::ColorItemRare);
        case GW::Constants::Rarity::Green: return ImColor(GW::Chat::TextColor::ColorItemUnique);
        case GW::Constants::Rarity::White: return ImColor(GW::Chat::TextColor::ColorItemCommon);
        default: return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }
}

void LootNotifier::Draw(IDirect3DDevice9*)
{
    if (!*GetVisiblePtr() || (drops_.empty() && !show_window_preview_)) {
        return;
    }
    if (apply_window_position_) {
        ImGui::SetNextWindowPos({window_pos_x_, window_pos_y_}, ImGuiCond_Once);
        apply_window_position_ = false;
    }
    auto flags = GetWinFlags(ImGuiWindowFlags_AlwaysAutoResize);
    const auto open = show_closebutton ? GetVisiblePtr() : nullptr;
    if (ImGui::Begin(Name(), open, flags)) {
        if (drops_.empty()) {
            ImGui::TextDisabled("Tracked drops will appear here.");
        }
        for (auto index = size_t{0}; index < drops_.size(); ++index) {
            const auto& drop = drops_[index];
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::SmallButton("GZ")) {
                SendCongratulations(drop);
            }
            ImGui::SameLine();
            ImGui::TextColored(RarityColor(drop.rarity), "%s", drop.item.c_str());
            ImGui::SameLine();
            ImGui::Text("-> %s", drop.player.c_str());
            ImGui::PopID();
        }
        if (!drops_.empty() && ImGui::Button("Clear")) {
            drops_.clear();
        }
    }
    const auto position = ImGui::GetWindowPos();
    window_pos_x_ = position.x;
    window_pos_y_ = position.y;
    ImGui::End();
}

void LootNotifier::DrawSettings()
{
    ToolboxUIPlugin::DrawSettings();
    ImGui::TextUnformatted("Version 1.0.0 (recovered)");
    ImGui::Checkbox("Show drop window", GetVisiblePtr());
    ImGui::Checkbox("Show window preview", &show_window_preview_);
    ImGui::Checkbox("Write local notifications", &send_notification_);
    DrawFilter("Notification filter", notification_filter_);
    InputString("Notification format", notification_format_);
    InputString("Notification format (self)", notification_format_self_);
    ImGui::Checkbox("Send party chat", &send_party_chat_);
    DrawFilter("Party chat filter", party_chat_filter_);
    InputString("Party chat format", party_chat_format_);
    InputString("Party chat format (self)", party_chat_format_self_);
    InputString("GZ format", gz_format_);
    InputString("GZ format (self)", gz_format_self_);
    ImGui::TextDisabled("Format tokens: [item], [player]");

    if (ImGui::CollapsingHeader("Tracked items", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputScalar("Model ID", ImGuiDataType_U32, &new_model_id_);
        ImGui::InputScalar("Model file ID", ImGuiDataType_U32, &new_model_file_id_);
        if (ImGui::Button("Add tracked item") && (new_model_id_ || new_model_file_id_)) {
            tracked_items_.push_back({new_model_id_, new_model_file_id_, true});
            new_model_id_ = 0;
            new_model_file_id_ = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Sort")) {
            std::ranges::sort(tracked_items_, {}, [](const auto& item) {
                return std::pair{item.model_file_id, item.model_id};
            });
        }

        for (auto index = size_t{0}; index < tracked_items_.size();) {
            auto& tracked = tracked_items_[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::Checkbox("##enabled", &tracked.enabled);
            ImGui::SameLine();
            ImGui::Text(
                tracked.model_file_id ? "File ID %u%s" : "Model ID %u%s",
                tracked.model_file_id ? tracked.model_file_id : tracked.model_id,
                tracked.model_file_id && tracked.model_id
                    ? std::format(" (model {})", tracked.model_id).c_str()
                    : "");
            ImGui::SameLine();
            if (index && ImGui::SmallButton("Up")) {
                std::swap(tracked_items_[index], tracked_items_[index - 1]);
            }
            ImGui::SameLine();
            if (index + 1 < tracked_items_.size() && ImGui::SmallButton("Down")) {
                std::swap(tracked_items_[index], tracked_items_[index + 1]);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                tracked_items_.erase(tracked_items_.begin() + static_cast<ptrdiff_t>(index));
                ImGui::PopID();
                continue;
            }
            ImGui::PopID();
            ++index;
        }
    }
}

void LootNotifier::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);
    LoadSetting("send_notification", send_notification_);
    LoadSetting("send_party_chat", send_party_chat_);
    LoadSetting("show_drop_window", *GetVisiblePtr());
    LoadSetting("show_window_preview", show_window_preview_);
    LoadSetting("show_window_title", show_title);
    LoadSetting("window_pos_x", window_pos_x_);
    LoadSetting("window_pos_y", window_pos_y_);
    LoadSetting("notification_filter", notification_filter_);
    LoadSetting("party_chat_filter", party_chat_filter_);
    LoadSetting("notification_format", notification_format_);
    LoadSetting("notification_format_self", notification_format_self_);
    LoadSetting("party_chat_format", party_chat_format_);
    LoadSetting("party_chat_format_self", party_chat_format_self_);
    LoadSetting("gz_format", gz_format_);
    LoadSetting("gz_format_self", gz_format_self_);
    LoadSetting("tracked_defs", tracked_items_);
    notification_filter_ = static_cast<DropFilter>(
        std::min(static_cast<uint8_t>(notification_filter_), static_cast<uint8_t>(DropFilter::Self)));
    party_chat_filter_ = static_cast<DropFilter>(
        std::min(static_cast<uint8_t>(party_chat_filter_), static_cast<uint8_t>(DropFilter::Self)));
    apply_window_position_ = true;
}

void LootNotifier::SaveSettings(const wchar_t* folder)
{
    SaveSetting("send_notification", send_notification_);
    SaveSetting("send_party_chat", send_party_chat_);
    SaveSetting("show_drop_window", *GetVisiblePtr());
    SaveSetting("show_window_preview", show_window_preview_);
    SaveSetting("show_window_title", show_title);
    SaveSetting("window_pos_x", window_pos_x_);
    SaveSetting("window_pos_y", window_pos_y_);
    SaveSetting("notification_filter", notification_filter_);
    SaveSetting("party_chat_filter", party_chat_filter_);
    SaveSetting("notification_format", notification_format_);
    SaveSetting("notification_format_self", notification_format_self_);
    SaveSetting("party_chat_format", party_chat_format_);
    SaveSetting("party_chat_format_self", party_chat_format_self_);
    SaveSetting("gz_format", gz_format_);
    SaveSetting("gz_format_self", gz_format_self_);
    SaveSetting("tracked_defs", tracked_items_);
    SaveSetting("settings_version", 1u);
    ToolboxUIPlugin::SaveSettings(folder);
}
