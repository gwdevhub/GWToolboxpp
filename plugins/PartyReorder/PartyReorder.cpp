#include "PartyReorder.h"

#include <PluginUtils.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>

#include <cstring>

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static PartyReorder instance;
    return &instance;
}
#endif

PartyReorder* PartyReorder::active_instance_ = nullptr;

namespace {
    constexpr std::string_view StartMessage =
        "Reordering! (Enable Party Settings -> \"Automatically accept party invitations when ticked\" for fast reordering)";
    constexpr std::string_view ReadyMessage = "Party ready! All players ticked.";

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
        if (value.capacity() < 64) {
            value.reserve(64);
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

    const char* ProfessionName(const uint8_t profession)
    {
        using GW::Constants::Profession;
        if (profession > static_cast<uint8_t>(Profession::Dervish)) {
            return "?";
        }
        return GW::Constants::GetProfessionAcronym(static_cast<Profession>(profession));
    }

    bool ProfessionCombo(const char* label, uint8_t& profession, const bool allow_any)
    {
        auto changed = false;
        const auto preview = !profession && allow_any ? "*" : ProfessionName(profession);
        if (ImGui::BeginCombo(label, preview)) {
            const auto first = allow_any ? 0 : 1;
            for (auto value = first; value <= 10; ++value) {
                const auto selected = profession == value;
                const auto name = value == 0 && allow_any ? "*" : ProfessionName(static_cast<uint8_t>(value));
                if (ImGui::Selectable(name, selected)) {
                    profession = static_cast<uint8_t>(value);
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

    std::string SlotText(const PartyReorder::ProfessionSlot& slot)
    {
        auto value = std::string(ProfessionName(slot.primary));
        value += '/';
        value += slot.secondary ? ProfessionName(slot.secondary) : "*";
        if (!slot.label.empty()) {
            value += " - " + slot.label;
        }
        return value;
    }

    void LocalMessage(const std::string& message)
    {
        const auto wide = PluginUtils::StringToWString(message);
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, wide.c_str());
    }
}

PartyReorder::PartyReorder()
{
    can_close = true;
    can_show_in_main_window = true;
    sequences_ = DefaultSequences();
}

std::vector<PartyReorder::Sequence> PartyReorder::DefaultSequences()
{
    using P = GW::Constants::Profession;
    const auto slot = [](const P primary, const P secondary, const char* label) {
        return ProfessionSlot{
            static_cast<uint8_t>(primary),
            static_cast<uint8_t>(secondary),
            label,
        };
    };
    return {
        {"BogSC", {0x27e}, {slot(P::Assassin, P::Warrior, "Tank")}},
        {"DoA", {0x1da}, {
            slot(P::Assassin, P::Warrior, "Main Tank"),
            slot(P::Mesmer, P::Paragon, "VoR"),
            slot(P::Mesmer, P::None, ""),
            slot(P::Mesmer, P::None, ""),
            slot(P::Mesmer, P::Ritualist, "MLK"),
            slot(P::Monk, P::Ranger, "UA"),
            slot(P::Elementalist, P::Monk, "Emo"),
        }},
        {"DoA IT", {0x1da}, {
            slot(P::Assassin, P::Warrior, "Main Tank"),
            slot(P::Mesmer, P::Paragon, "VoR"),
            slot(P::Mesmer, P::None, ""),
            slot(P::Mesmer, P::None, ""),
            slot(P::Mesmer, P::Ritualist, "MLK"),
            slot(P::Mesmer, P::Monk, "Memo"),
            slot(P::Elementalist, P::Monk, "Emo"),
        }},
        {"KathSC", {0x288}, {slot(P::Assassin, P::Necromancer, "Gater")}},
        {"RragarSC", {0x288}, {
            slot(P::Assassin, P::None, "Gater"),
            slot(P::Assassin, P::Elementalist, "Tank"),
        }},
        {"SooSC", {0x270}, {
            slot(P::Assassin, P::Necromancer, "Gater"),
            slot(P::Assassin, P::Paragon, "Tank"),
        }},
        {"SooSC with Rit", {0x270}, {
            slot(P::Dervish, P::None, "Derv"),
            slot(P::Ritualist, P::Ranger, "Rit"),
            slot(P::Assassin, P::Necromancer, "Gater"),
            slot(P::Assassin, P::Paragon, "Tank"),
        }},
        {"UWSC", {0x8a, 0x359, 0x189, 0x11c}, {
            slot(P::Mesmer, P::Ranger, "Spiker"),
            slot(P::Ritualist, P::Ranger, "SoS"),
            slot(P::Mesmer, P::Assassin, "LT"),
            slot(P::Elementalist, P::Monk, "Emo"),
        }},
        {"UWSC 6-man", {0x8a, 0x359, 0x189, 0x11c}, {
            slot(P::Dervish, P::Ranger, "Derv DB"),
            slot(P::Mesmer, P::Elementalist, "Solo LT"),
            slot(P::Elementalist, P::Monk, "Emo"),
        }},
    };
}

void PartyReorder::Initialize(
    ImGuiContext* context, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(context, allocator_fns, toolbox_dll);
    terminating_ = false;
    active_instance_ = this;
    GW::Chat::CreateCommand(&chat_command_hook_, L"reorder", ChatCommand);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyInviteReceived_Response>(
        &invite_response_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyInviteReceived_Response*) {
            if (!terminating_ && operation_pending_) {
                next_action_at_ = std::chrono::steady_clock::now();
            }
        });
}

void PartyReorder::SignalTerminate()
{
    terminating_ = true;
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyInviteReceived_Response>(&invite_response_hook_);
    GW::Chat::DeleteCommand(&chat_command_hook_, L"reorder");
    if (active_instance_ == this) {
        active_instance_ = nullptr;
    }
    reorder_state_ = ReorderState::Failed;
    operation_pending_ = false;
    ToolboxUIPlugin::SignalTerminate();
}

void PartyReorder::Terminate()
{
    reorder_members_.clear();
    ToolboxUIPlugin::Terminate();
}

void PartyReorder::ChatCommand(
    GW::HookStatus* status, const wchar_t*, const int argc, const LPWSTR* argv)
{
    if (!active_instance_ || !active_instance_->enable_chat_command_) {
        status->blocked = false;
        return;
    }
    auto requested_name = std::wstring{};
    for (auto index = 1; index < argc; ++index) {
        if (!argv[index] || !*argv[index]) continue;
        if (!requested_name.empty()) requested_name += L' ';
        requested_name += argv[index];
    }
    active_instance_->StartBestSequence(requested_name);
}

std::vector<PartyReorder::Member> PartyReorder::PartyMembers() const
{
    std::vector<Member> result;
    const auto party = GW::PartyMgr::GetPartyInfo();
    if (!party || !party->players.valid()) {
        return result;
    }
    result.reserve(party->players.size());
    for (const auto& member : party->players) {
        if (!member.connected()) continue;
        const auto player = GW::PlayerMgr::GetPlayerByID(member.login_number);
        const auto name = GW::Agents::GetPlayerNameByLoginNumber(member.login_number);
        if (!player || !name || !*name) {
            continue;
        }
        result.push_back({
            .login_number = member.login_number,
            .primary = static_cast<uint8_t>(player->primary),
            .secondary = static_cast<uint8_t>(player->secondary),
            .name = name,
        });
    }
    return result;
}

bool PartyReorder::AssignSlotsRecursive(
    const Sequence& sequence,
    const std::vector<Member>& candidates,
    const size_t slot_index,
    std::vector<bool>& used,
    std::vector<Member>& result) const
{
    if (slot_index == sequence.slots.size()) {
        return true;
    }
    const auto& slot = sequence.slots[slot_index];
    for (auto candidate_index = size_t{0}; candidate_index < candidates.size(); ++candidate_index) {
        const auto& candidate = candidates[candidate_index];
        if (used[candidate_index]
            || candidate.primary != slot.primary
            || (slot.secondary && candidate.secondary != slot.secondary)) {
            continue;
        }
        used[candidate_index] = true;
        result.push_back(candidate);
        if (AssignSlotsRecursive(sequence, candidates, slot_index + 1, used, result)) {
            return true;
        }
        result.pop_back();
        used[candidate_index] = false;
    }
    return false;
}

std::optional<std::vector<PartyReorder::Member>> PartyReorder::AssignSlots(
    const Sequence& sequence) const
{
    auto candidates = PartyMembers();
    const auto self = GW::PlayerMgr::GetPlayerNumber();
    std::erase_if(candidates, [self](const auto& member) {
        return member.login_number == self;
    });
    std::vector<Member> result;
    std::vector used(candidates.size(), false);
    result.reserve(sequence.slots.size());
    if (!AssignSlotsRecursive(sequence, candidates, 0, used, result)) {
        return std::nullopt;
    }
    return result;
}

bool PartyReorder::IsSequenceForCurrentMap(const Sequence& sequence) const
{
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    return sequence.map_ids.empty() || std::ranges::contains(sequence.map_ids, map_id);
}

void PartyReorder::StartBestSequence(const std::wstring& requested_name)
{
    if (IsReordering()) {
        SetStatus("A party reorder is already running", true);
        return;
    }
    if (!requested_name.empty()) {
        const auto requested = PluginUtils::ToLower(PluginUtils::WStringToString(requested_name));
        for (auto index = size_t{0}; index < sequences_.size(); ++index) {
            if (PluginUtils::ToLower(sequences_[index].name) == requested) {
                StartReorder(index);
                return;
            }
        }
        SetStatus("Unknown reorder sequence: " + PluginUtils::WStringToString(requested_name), true);
        return;
    }
    std::vector<size_t> available;
    for (auto index = size_t{0}; index < sequences_.size(); ++index) {
        if (IsSequenceForCurrentMap(sequences_[index]) && AssignSlots(sequences_[index])) {
            available.push_back(index);
        }
    }
    if (available.empty()) {
        SetStatus("No sequence has its profession requirements met in this outpost", true);
    }
    else if (available.size() > 1) {
        SetStatus("Multiple sequences are available; use /reorder [sequence name]", true);
    }
    else {
        StartReorder(available.front());
    }
}

void PartyReorder::StartReorder(const size_t sequence_index, const bool ignore_map)
{
    if (sequence_index >= sequences_.size()) {
        SetStatus("No reorder sequence configured", true);
        return;
    }
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost
        || !GW::Map::GetIsMapLoaded()) {
        SetStatus("Party can only be reordered in an outpost", true);
        return;
    }
    if (!GW::PartyMgr::GetIsLeader()) {
        SetStatus("You must be the party leader", true);
        return;
    }
    const auto party = GW::PartyMgr::GetPartyInfo();
    if (!party || party->heroes.size() || party->henchmen.size()) {
        SetStatus("Party must not contain heroes or henchmen", true);
        return;
    }
    const auto& sequence = sequences_[sequence_index];
    if (!ignore_map && !IsSequenceForCurrentMap(sequence)) {
        SetStatus("This sequence is not configured for the current outpost", true);
        return;
    }
    const auto assignment = AssignSlots(sequence);
    if (!assignment) {
        SetStatus("Party does not contain enough matching professions", true);
        return;
    }
    if (assignment->empty()) {
        SetStatus("Selected sequence has no reorder slots", true);
        return;
    }
    selected_sequence_ = sequence_index;
    reorder_members_ = *assignment;
    reorder_index_ = 0;
    reorder_state_ = ReorderState::Kicking;
    operation_pending_ = false;
    reorder_map_id_ = static_cast<uint32_t>(GW::Map::GetMapID());
    next_action_at_ = std::chrono::steady_clock::now();
    SetStatus("Reordering " + sequence.name + "...");
    if (send_chat_message_on_start_) {
        const auto wide = PluginUtils::StringToWString(std::string(StartMessage));
        GW::Chat::SendChat('#', wide.c_str());
    }
}

bool PartyReorder::IsMemberInParty(const uint32_t login_number) const
{
    const auto party = GW::PartyMgr::GetPartyInfo();
    return party && party->players.valid()
        && std::ranges::any_of(party->players, [login_number](const auto& member) {
               return member.login_number == login_number && member.connected();
           });
}

bool PartyReorder::HasExpectedOrder() const
{
    const auto party = GW::PartyMgr::GetPartyInfo();
    if (!party || !party->players.valid()) return false;
    std::vector<uint32_t> connected;
    for (const auto& member : party->players) {
        if (member.connected()) connected.push_back(member.login_number);
    }
    if (connected.size() < reorder_members_.size()) return false;
    const auto offset = connected.size() - reorder_members_.size();
    for (auto index = size_t{0}; index < reorder_members_.size(); ++index) {
        if (connected[offset + index] != reorder_members_[index].login_number) return false;
    }
    return true;
}

bool PartyReorder::IsReordering() const
{
    return reorder_state_ == ReorderState::Kicking || reorder_state_ == ReorderState::Inviting;
}

void PartyReorder::CancelReorder(const std::string& reason)
{
    if (!IsReordering()) {
        return;
    }
    reorder_state_ = ReorderState::Failed;
    operation_pending_ = false;
    SetStatus("Reorder cancelled: " + reason, true);
}

void PartyReorder::AdvanceReorder()
{
    if (!IsReordering() || reorder_index_ >= reorder_members_.size()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (static_cast<uint32_t>(GW::Map::GetMapID()) != reorder_map_id_
        || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        CancelReorder("instance changed");
        return;
    }
    if (!GW::PartyMgr::GetIsLeader()) {
        CancelReorder("party leadership changed");
        return;
    }
    const auto advance_member = [this, now] {
        operation_pending_ = false;
        ++reorder_index_;
        next_action_at_ = now + std::chrono::milliseconds(action_delay_);
        if (reorder_index_ != reorder_members_.size()) {
            return;
        }
        if (reorder_state_ == ReorderState::Kicking) {
            reorder_state_ = ReorderState::Inviting;
            reorder_index_ = 0;
            SetStatus("Players removed; sending invitations...");
        }
        else {
            reorder_state_ = ReorderState::Complete;
            if (HasExpectedOrder()) {
                SetStatus("Party reorder complete!");
                LocalMessage(status_);
            }
            else {
                SetStatus("Reorder actions completed, but the final party order is incorrect", true);
            }
        }
    };
    const auto& member = reorder_members_[reorder_index_];
    const auto member_present = IsMemberInParty(member.login_number);
    if (operation_pending_) {
        const auto completed = reorder_state_ == ReorderState::Kicking ? !member_present : member_present;
        if (completed) {
            advance_member();
            return;
        }
        if (now - operation_started_ >= std::chrono::milliseconds(reorder_timeout_)) {
            CancelReorder(
                reorder_state_ == ReorderState::Kicking
                    ? "player did not leave before timeout"
                    : "player did not rejoin after invite");
        }
        return;
    }
    if (now < next_action_at_) {
        return;
    }
    operation_pending_ = true;
    operation_started_ = now;
    if (reorder_state_ == ReorderState::Kicking) {
        if (!member_present) {
            advance_member();
            return;
        }
        const auto login_number = member.login_number;
        GW::GameThread::Enqueue([login_number] {
            GW::PartyMgr::KickPlayer(login_number);
        });
        SetStatus("Removing " + PluginUtils::WStringToString(member.name) + "...");
    }
    else {
        if (member_present) {
            advance_member();
            return;
        }
        const auto name = member.name;
        GW::GameThread::Enqueue([name] {
            GW::PartyMgr::InvitePlayer(name.c_str());
        });
        SetStatus("Inviting " + PluginUtils::WStringToString(member.name) + "...");
    }
}

bool PartyReorder::IsPartyReady(std::vector<std::string>* available) const
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost
        || !GW::Map::GetIsMapLoaded()
        || !GW::PartyMgr::GetIsLeader()) {
        return false;
    }
    const auto party = GW::PartyMgr::GetPartyInfo();
    const auto map = GW::Map::GetMapInfo();
    if (!party || !map || party->heroes.size() || party->henchmen.size()
        || party->players.size() != map->max_party_size
        || PartyMembers().size() != party->players.size()) {
        return false;
    }
    auto found = false;
    for (const auto& sequence : sequences_) {
        if (!IsSequenceForCurrentMap(sequence) || !AssignSlots(sequence)) {
            continue;
        }
        found = true;
        if (available) {
            available->push_back(sequence.name);
        }
    }
    return found;
}

void PartyReorder::UpdateNotifications()
{
    const auto party = GW::PartyMgr::GetPartyInfo();
    auto signature = size_t{0};
    if (party && party->players.valid()) {
        for (const auto& member : party->players) {
            const auto member_signature = std::hash<uint32_t>{}(
                member.login_number + 0x9e3779b9u + (member.state << 16));
            signature ^= member_signature + 0x9e3779b9u + (signature << 6) + (signature >> 2);
        }
        signature ^= std::hash<size_t>{}(party->players.size())
            + 0x9e3779b9u + (signature << 6) + (signature >> 2);
    }
    if (signature != last_party_signature_) {
        last_party_signature_ = signature;
        ready_notified_ = false;
        ticked_notified_ = false;
    }
    if (send_notification_when_ready_ && !ready_notified_) {
        std::vector<std::string> available;
        if (IsPartyReady(&available)) {
            auto message = std::string("Party ready for reorder. Available: ");
            for (auto index = size_t{0}; index < available.size(); ++index) {
                if (index) message += ", ";
                message += available[index];
            }
            LocalMessage(message);
            ready_notified_ = true;
        }
    }
    const auto all_ticked = party && party->players.size() > 1 && GW::PartyMgr::GetIsPartyTicked();
    if (!all_ticked) {
        ticked_notified_ = false;
        return;
    }
    if (ticked_notified_) {
        return;
    }
    if (send_notification_when_all_ticked_) {
        LocalMessage("Party is fully ticked");
    }
    if (send_ready_message_) {
        const auto wide = PluginUtils::StringToWString(std::string(ReadyMessage));
        GW::Chat::SendChat('#', wide.c_str());
    }
    ticked_notified_ = true;
}

void PartyReorder::Update(float)
{
    if (terminating_) {
        return;
    }
    AdvanceReorder();
    UpdateNotifications();
}

void PartyReorder::SetStatus(std::string status, const bool error)
{
    status_ = std::move(status);
    status_is_error_ = error;
    if (error) {
        LocalMessage(status_);
    }
}

void PartyReorder::Draw(IDirect3DDevice9*)
{
    auto flags = GetWinFlags(auto_height_ ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None);
    const auto open = show_closebutton ? GetVisiblePtr() : nullptr;
    if (ImGui::Begin(Name(), open, flags)) {
        if (sequences_.empty()) {
            ImGui::TextDisabled("No reorder sequence configured");
        }
        else {
            selected_sequence_ = std::min(selected_sequence_, sequences_.size() - 1);
            if (ImGui::BeginCombo("Sequence", sequences_[selected_sequence_].name.c_str())) {
                for (auto index = size_t{0}; index < sequences_.size(); ++index) {
                    const auto applicable = IsSequenceForCurrentMap(sequences_[index]);
                    ImGui::BeginDisabled(!applicable);
                    const auto selected = selected_sequence_ == index;
                    if (ImGui::Selectable(sequences_[index].name.c_str(), selected)) {
                        selected_sequence_ = index;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                    ImGui::EndDisabled();
                }
                ImGui::EndCombo();
            }
            for (const auto& slot : sequences_[selected_sequence_].slots) {
                ImGui::BulletText("%s", SlotText(slot).c_str());
            }
        }
        ImGui::TextColored(
            status_is_error_ ? ImVec4(1.f, .35f, .3f, 1.f) : ImVec4(.35f, 1.f, .45f, 1.f),
            "%s",
            status_.c_str());
        ImGui::BeginDisabled(sequences_.empty() || IsReordering());
        if (ImGui::Button("Reorder Party")) {
            StartReorder(selected_sequence_);
        }
        ImGui::EndDisabled();
        if (IsReordering()) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                CancelReorder("cancelled by user");
            }
        }
        ImGui::TextDisabled("/reorder [sequence name]");
    }
    ImGui::End();
}

void PartyReorder::DrawSettings()
{
    ToolboxUIPlugin::DrawSettings();
    ImGui::TextUnformatted("Version 1.3.4 (recovered)");
    ImGui::Checkbox("Automatic window height", &auto_height_);
    ImGui::Checkbox("Enable /reorder chat command", &enable_chat_command_);
    ImGui::Checkbox("Send party chat message when reorder starts", &send_chat_message_on_start_);
    ImGui::Checkbox("Send party chat message when all players are ticked", &send_ready_message_);
    ImGui::Checkbox("Display notification when party is ready to be reordered", &send_notification_when_ready_);
    ImGui::Checkbox("Display notification when all party members are ticked", &send_notification_when_all_ticked_);
    ImGui::Checkbox("Debug mode", &debug_mode_);
    if (debug_mode_ && !sequences_.empty()) {
        ImGui::SeparatorText("Debug Controls");
        selected_sequence_ = std::min(selected_sequence_, sequences_.size() - 1);
        if (ImGui::BeginCombo("Select Reorder Sequence", sequences_[selected_sequence_].name.c_str())) {
            for (auto index = size_t{0}; index < sequences_.size(); ++index) {
                const auto selected = selected_sequence_ == index;
                if (ImGui::Selectable(sequences_[index].name.c_str(), selected)) {
                    selected_sequence_ = index;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        const auto assignment = AssignSlots(sequences_[selected_sequence_]);
        if (assignment) {
            if (assignment->size() == 1) {
                ImGui::TextUnformatted("Last slot should be:");
            }
            else {
                ImGui::Text("Last %zu slots should be:", assignment->size());
            }
            for (const auto& member : *assignment) {
                ImGui::BulletText("%ls", member.name.c_str());
            }
        }
        else {
            ImGui::TextDisabled("Requirements not met.");
        }
        ImGui::BeginDisabled(!assignment || assignment->empty() || IsReordering());
        if (ImGui::Button("Trigger Debug Reorder")) {
            StartReorder(selected_sequence_, true);
            if (IsReordering()) SetStatus("Starting debug reorder...");
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Note: /reorder still requires the configured outpost.");
    }
    ImGui::InputScalar("Action delay (ms)", ImGuiDataType_U32, &action_delay_);
    ImGui::InputScalar("Per-action timeout (ms)", ImGuiDataType_U32, &reorder_timeout_);
    action_delay_ = std::clamp(action_delay_, 50u, 10'000u);
    reorder_timeout_ = std::clamp(reorder_timeout_, 1'000u, 120'000u);

    if (!ImGui::CollapsingHeader("Manage Reorder Sequences", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (ImGui::Button("Add Sequence")) {
        sequences_.push_back({"New Sequence", {}, {}});
        selected_sequence_ = sequences_.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Defaults")) {
        sequences_ = DefaultSequences();
        selected_sequence_ = 0;
    }
    for (auto sequence_index = size_t{0}; sequence_index < sequences_.size();) {
        auto& sequence = sequences_[sequence_index];
        ImGui::PushID(static_cast<int>(sequence_index));
        const auto expanded = ImGui::TreeNode(sequence.name.empty() ? "Unnamed sequence" : sequence.name.c_str());
        ImGui::SameLine();
        if (sequence_index && ImGui::SmallButton("Up")) {
            std::swap(sequences_[sequence_index], sequences_[sequence_index - 1]);
            selected_sequence_ = sequence_index - 1;
        }
        ImGui::SameLine();
        if (sequence_index + 1 < sequences_.size() && ImGui::SmallButton("Down")) {
            std::swap(sequences_[sequence_index], sequences_[sequence_index + 1]);
            selected_sequence_ = sequence_index + 1;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete")) {
            sequences_.erase(sequences_.begin() + static_cast<ptrdiff_t>(sequence_index));
            selected_sequence_ = 0;
            if (expanded) ImGui::TreePop();
            ImGui::PopID();
            continue;
        }
        if (expanded) {
            InputString("Name", sequence.name);
            ImGui::TextUnformatted("Outposts where this sequence can be used");
            for (auto map_index = size_t{0}; map_index < sequence.map_ids.size();) {
                ImGui::PushID(static_cast<int>(map_index));
                ImGui::SetNextItemWidth(130.f);
                ImGui::InputScalar("##map", ImGuiDataType_U32, &sequence.map_ids[map_index]);
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    sequence.map_ids.erase(sequence.map_ids.begin() + static_cast<ptrdiff_t>(map_index));
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopID();
                ++map_index;
            }
            if (ImGui::SmallButton("Add outpost")) {
                sequence.map_ids.push_back(static_cast<uint32_t>(GW::Map::GetMapID()));
            }
            ImGui::TextUnformatted("Required final slots");
            for (auto slot_index = size_t{0}; slot_index < sequence.slots.size();) {
                auto& slot = sequence.slots[slot_index];
                ImGui::PushID(static_cast<int>(slot_index));
                ImGui::SetNextItemWidth(70.f);
                ProfessionCombo("##primary", slot.primary, false);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70.f);
                ProfessionCombo("##secondary", slot.secondary, true);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.f);
                InputString("##label", slot.label);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    sequence.slots.erase(sequence.slots.begin() + static_cast<ptrdiff_t>(slot_index));
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopID();
                ++slot_index;
            }
            if (ImGui::SmallButton("Add slot")) {
                sequence.slots.push_back({1, 0, {}});
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++sequence_index;
    }
}

void PartyReorder::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);
    LoadSetting("auto_height", auto_height_);
    LoadSetting("enable_chat_command", enable_chat_command_);
    LoadSetting("send_chat_message_on_start", send_chat_message_on_start_);
    LoadSetting("send_ready_message", send_ready_message_);
    LoadSetting("send_notification_when_ready", send_notification_when_ready_);
    LoadSetting("send_notification_when_all_ticked", send_notification_when_all_ticked_);
    LoadSetting("debug_mode", debug_mode_);
    LoadSetting("action_delay", action_delay_);
    LoadSetting("reorder_timeout", reorder_timeout_);
    LoadSetting("sequences", sequences_);
    LoadSetting("selected_sequence", selected_sequence_);
    if (sequences_.empty()) {
        sequences_ = DefaultSequences();
    }
    selected_sequence_ = std::min(selected_sequence_, sequences_.size() - 1);
    action_delay_ = std::clamp(action_delay_, 50u, 10'000u);
    reorder_timeout_ = std::clamp(reorder_timeout_, 1'000u, 120'000u);
}

void PartyReorder::SaveSettings(const wchar_t* folder)
{
    SaveSetting("auto_height", auto_height_);
    SaveSetting("enable_chat_command", enable_chat_command_);
    SaveSetting("send_chat_message_on_start", send_chat_message_on_start_);
    SaveSetting("send_ready_message", send_ready_message_);
    SaveSetting("send_notification_when_ready", send_notification_when_ready_);
    SaveSetting("send_notification_when_all_ticked", send_notification_when_all_ticked_);
    SaveSetting("debug_mode", debug_mode_);
    SaveSetting("action_delay", action_delay_);
    SaveSetting("reorder_timeout", reorder_timeout_);
    SaveSetting("sequences", sequences_);
    SaveSetting("selected_sequence", selected_sequence_);
    ToolboxUIPlugin::SaveSettings(folder);
}
