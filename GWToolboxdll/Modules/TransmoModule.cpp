#include <GWCA/Constants/Constants.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>

#include <Modules/TransmoModule.h>

namespace {

    struct TransmoEntry {
        char name[64] = "";
        int npc_id = 0;
        int scale = 100;
        int model_file_id = 0;
        int model_file_data = 0;
        int flags = 0;

        TransmoModule::PendingTransmo ToPending() const
        {
            return {static_cast<DWORD>(npc_id), static_cast<DWORD>(scale) << 24,
                    static_cast<DWORD>(model_file_id), static_cast<DWORD>(model_file_data),
                    static_cast<DWORD>(flags)};
        }
    };

    std::vector<TransmoEntry*> npc_transmo_list;

    TransmoModule::PendingTransmo pending_transmo_add_data;
    std::unique_ptr<GuiUtils::EncString> pending_transmo_add_name;

    GW::HookEntry TransmoCmd_HookEntry;

    void ResetNpcTransmoListToDefaults()
    {
        for (auto* e : npc_transmo_list) {
            delete e;
        }
        npc_transmo_list.clear();

        struct DefaultEntry {
            const char* name;
            uint32_t npc_id;
            uint32_t model_file_id;
            uint32_t model_file_data;
            uint32_t flags;
        };
        static constexpr DefaultEntry defaults[] = {
            {"charr",      163,   0x0004c409, 0,      98820},
            {"reindeer",   5,     277573,     277576, 32780},
            {"gwenpre",    244,   116377,     116759, 98820},
            {"gwenchan",   245,   116377,     283392, 98820},
            {"eye",        0x1f4, 0x9d07,     0,      0},
            {"zhu",        298,   170283,     170481, 98820},
            {"kuunavang",  309,   157438,     157527, 98820},
            {"beetle",     329,   207331,     279211, 98820},
            {"polar",      313,   277551,     277556, 98820},
            {"celepig",    331,   279205,     0,      0},
            {"mallyx",     315,   243812,     0,      98820},
            {"bonedragon", 231,   16768,      0,      0},
            {"destroyer",  312,   285891,     285900, 98820},
            {"destroyer2", 146,   285886,     285890, 32780},
            {"koss",       250,   243282,     245053, 98820},
            {"smite",      346,   129664,     0,      98820},
            {"dorian",     8299,  86510,      0,      98820},
            {"kanaxai",    317,   184176,     185319, 98820},
            {"skeletonic", 359,   52356,      0,      98820},
            {"moa",        504,   16689,      0,      98820},
        };
        for (const auto& d : defaults) {
            auto* e = new TransmoEntry;
            snprintf(e->name, sizeof(e->name), "%s", d.name);
            e->npc_id = static_cast<int>(d.npc_id);
            e->scale = 100;
            e->model_file_id = static_cast<int>(d.model_file_id);
            e->model_file_data = static_cast<int>(d.model_file_data);
            e->flags = static_cast<int>(d.flags);
            npc_transmo_list.push_back(e);
        }
    }

    bool AddCurrentTargetToTransmoList()
    {
        const auto target = GW::Agents::GetTargetAsAgentLiving();
        if (!target) {
            Log::Error("No target selected");
            return false;
        }
        if (target->IsPlayer()) {
            Log::Error("Target must be an NPC");
            return false;
        }
        DWORD npc_id = target->player_number;
        if (target->transmog_npc_id & 0x20000000) {
            npc_id = target->transmog_npc_id ^ 0x20000000;
        }
        pending_transmo_add_data = {};
        pending_transmo_add_data.npc_id = npc_id;
        pending_transmo_add_data.scale = 0x64000000;
        const auto* game_ctx = GW::GetGameContext();
        const auto* world_ctx = game_ctx ? game_ctx->world : nullptr;
        if (world_ctx && npc_id < world_ctx->npcs.size()) {
            const auto& npc = world_ctx->npcs[npc_id];
            pending_transmo_add_data.npc_model_file_id = npc.model_file_id;
            if (npc.files_count > 0 && npc.model_files) {
                pending_transmo_add_data.npc_model_file_data = npc.model_files[0];
            }
            pending_transmo_add_data.flags = npc.npc_flags;
        }
        const auto enc_name = GW::Agents::GetAgentEncName(target);
        if (!enc_name || !enc_name[0]) {
            auto* e = new TransmoEntry;
            snprintf(e->name, sizeof(e->name), "npc_%u", npc_id);
            e->npc_id = static_cast<int>(pending_transmo_add_data.npc_id);
            e->scale = 100;
            e->model_file_id = static_cast<int>(pending_transmo_add_data.npc_model_file_id);
            e->model_file_data = static_cast<int>(pending_transmo_add_data.npc_model_file_data);
            e->flags = static_cast<int>(pending_transmo_add_data.flags);
            npc_transmo_list.push_back(e);
            Log::Info("Transmo entry '%s' added", e->name);
            return true;
        }
        pending_transmo_add_name = std::make_unique<GuiUtils::EncString>(enc_name);
        return true;
    }

} // namespace

void TransmoModule::Initialize()
{
    ToolboxModule::Initialize();
    ResetNpcTransmoListToDefaults();

    const std::vector<std::pair<const wchar_t*, GW::Chat::ChatCommandCallback>> transmo_commands = {
        {L"transmo", CmdTransmo},
        {L"transmotarget", CmdTransmoTarget},
        {L"transmoparty", CmdTransmoParty},
        {L"transmoagent", CmdTransmoAgent},
    };
    for (const auto& [cmd, fn] : transmo_commands) {
        GW::Chat::CreateCommand(&TransmoCmd_HookEntry, cmd, fn);
    }
}

void TransmoModule::Terminate()
{
    GW::Chat::DeleteCommand(&TransmoCmd_HookEntry);
    for (const auto it : npc_transmo_list) {
        delete it;
    }
    npc_transmo_list.clear();
    pending_transmo_add_name.reset();
}

void TransmoModule::Update(float)
{
    if (pending_transmo_add_name && !pending_transmo_add_name->IsDecoding()) {
        std::string entry_name = pending_transmo_add_name->string();
        if (entry_name.empty()) {
            entry_name = std::format("npc_{}", pending_transmo_add_data.npc_id);
        }
        else {
            entry_name = TextUtils::ToLower(entry_name);
            std::ranges::replace(entry_name, ' ', '_');
        }
        auto* e = new TransmoEntry;
        snprintf(e->name, sizeof(e->name), "%s", entry_name.c_str());
        e->npc_id = static_cast<int>(pending_transmo_add_data.npc_id);
        e->scale = 100;
        e->model_file_id = static_cast<int>(pending_transmo_add_data.npc_model_file_id);
        e->model_file_data = static_cast<int>(pending_transmo_add_data.npc_model_file_data);
        e->flags = static_cast<int>(pending_transmo_add_data.flags);
        npc_transmo_list.push_back(e);
        Log::Info("Transmo entry '%s' added", e->name);
        pending_transmo_add_name.reset();
    }
}

void TransmoModule::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Transmo Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    constexpr auto transmo_hint = "NPC names are configured in Chat Settings > Transmo NPC List";
    ImGui::Bullet();
    ImGui::Text("'/transmo <npc_name> [size (6-255)]' to change your appearance into an NPC.\n"
        "'/transmo' to change your appearance into target NPC.\n"
        "'/transmo add' to add the current target as a named transmo entry.\n"
        "'/transmo reset' to reset your appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet();
    ImGui::Text("'/transmoparty <npc_name> [size (6-255)]' to change your party's appearance into an NPC.\n"
        "'/transmoparty' to change your party's appearance into target NPC.\n"
        "'/transmoparty reset' to reset your party's appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet();
    ImGui::Text("'/transmotarget <npc_name> [size (6-255)]' to change your target's appearance into an NPC.\n"
        "'/transmotarget reset' to reset your target's appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::TreePop();
}

void TransmoModule::DrawSettingsInternal()
{
    ImGui::TextUnformatted("Transmo NPC List");
    ImGui::ShowHelp("Named NPC models usable with '/transmo <name>', '/transmoparty <name>' etc.\nGet model data from the Info window (Advanced > Target section).");

    static auto OnConfirmDeleteTransmo = [](bool result, void* wparam) {
        if (!result) {
            return;
        }
        auto* entry = static_cast<TransmoEntry*>(wparam);
        const auto found = std::ranges::find(npc_transmo_list, entry);
        if (found != npc_transmo_list.end()) {
            npc_transmo_list.erase(found);
            delete entry;
        }
    };

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("###transmo_npc_table", 7, table_flags)) {
        ImGui::TableSetupColumn("Name",          ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("NPC ID",        ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("Scale",         ImGuiTableColumnFlags_WidthFixed, 45.f);
        ImGui::TableSetupColumn("Model File ID", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("Model File",    ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("Flags",         ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("##del",         ImGuiTableColumnFlags_WidthFixed, 30.f);
        ImGui::TableHeadersRow();

        for (auto* entry : npc_transmo_list) {
            ImGui::PushID(entry);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##name", entry->name, sizeof(entry->name));
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##npc_id", &entry->npc_id, 0);
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##scale", &entry->scale, 0);
            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##mfid", &entry->model_file_id, 0);
            ImGui::TableSetColumnIndex(4);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##mfd", &entry->model_file_data, 0);
            ImGui::TableSetColumnIndex(5);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##flags", &entry->flags, 0);
            ImGui::TableSetColumnIndex(6);
            ImGui::SmallConfirmButton("X", "Delete this transmo entry?", OnConfirmDeleteTransmo, entry);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (pending_transmo_add_name) {
        ImGui::TextDisabled("Adding target... (decoding name)");
    }

    if (ImGui::Button("Add Entry")) {
        auto* e = new TransmoEntry;
        snprintf(e->name, sizeof(e->name), "new_entry_%zu", npc_transmo_list.size());
        npc_transmo_list.push_back(e);
    }
    ImGui::SameLine();
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    const bool has_npc_target = target && !target->IsPlayer();
    if (!has_npc_target) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Add current target")) {
        AddCurrentTargetToTransmoList();
    }
    if (!has_npc_target) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::SmallConfirmButton("Reset to defaults", "Reset the transmo list to built-in defaults?\nAll custom entries will be lost.", [](bool result, void*) {
        if (result) {
            ResetNpcTransmoListToDefaults();
        }
    });
}

void TransmoModule::LoadSettings(ToolboxIni* ini)
{
    // Load custom transmo NPC list; only override defaults if we've previously saved settings
    const auto transmo_section = "Transmo NPC List";
    if (ini->GetValue(transmo_section, "_saved", nullptr) != nullptr) {
        for (auto* e : npc_transmo_list) {
            delete e;
        }
        npc_transmo_list.clear();
        ToolboxIni::TNamesDepend transmo_keys;
        ini->GetAllKeys(transmo_section, transmo_keys);
        for (const auto& key : transmo_keys) {
            if (!key.pItem[0] || strcmp(key.pItem, "_saved") == 0) {
                continue;
            }
            const std::string val = ini->GetValue(transmo_section, key.pItem, "");
            if (val.empty()) {
                continue;
            }
            int npc_id = 0, scale = 100, model_file_id = 0, model_file_data = 0, flags = 0;
            if (sscanf(val.c_str(), "%d,%d,%d,%d,%d", &npc_id, &scale, &model_file_id, &model_file_data, &flags) < 1) {
                continue;
            }
            std::string entry_name = key.pItem;
            static constexpr ctll::fixed_string transmo_index_regex = "(\\d+):(.+)";
            if (const auto match = ctre::match<transmo_index_regex>(entry_name)) {
                entry_name = match.get<2>().to_string();
            }
            auto* e = new TransmoEntry;
            snprintf(e->name, sizeof(e->name), "%s", entry_name.c_str());
            e->npc_id = npc_id;
            e->scale = scale;
            e->model_file_id = model_file_id;
            e->model_file_data = model_file_data;
            e->flags = flags;
            npc_transmo_list.push_back(e);
        }
    }
}

void TransmoModule::SaveSettings(ToolboxIni* ini)
{
    const auto transmo_section = "Transmo NPC List";
    ini->Delete(transmo_section, nullptr);
    ini->SetValue(transmo_section, "_saved", "1");
    for (const auto& [index, entry] : npc_transmo_list | std::views::enumerate) {
        const auto key = std::format("{}:{}", index, entry->name);
        const auto val = std::format("{},{},{},{},{}", entry->npc_id, entry->scale,
                                     entry->model_file_id, entry->model_file_data, entry->flags);
        ini->SetValue(transmo_section, key.c_str(), val.c_str());
    }
}

void TransmoModule::TransmoAgent(DWORD agent_id, PendingTransmo& transmo)
{
    if (!transmo.npc_id || !agent_id) {
        return;
    }
    const auto a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
    if (!a || !a->GetIsLivingType()) {
        return;
    }
    DWORD& npc_id = transmo.npc_id;
    DWORD& scale = transmo.scale;
    const GW::NPCArray& npcs = GW::GetGameContext()->world->npcs;
    if (npc_id == static_cast<DWORD>(std::numeric_limits<int>::max() - 1)) {
        // Scale only
        npc_id = a->player_number;
        if (a->transmog_npc_id & 0x20000000) {
            npc_id = a->transmog_npc_id ^ 0x20000000;
        }
    }
    else if (npc_id == static_cast<DWORD>(std::numeric_limits<int>::max())) {
        // Reset
        npc_id = 0;
        scale = 0x64000000;
    }
    else if (npc_id >= npcs.size() || !npcs[npc_id].model_file_id) {
        const DWORD& npc_model_file_id = transmo.npc_model_file_id;
        const DWORD& npc_model_file_data = transmo.npc_model_file_data;
        const DWORD& flags = transmo.flags;
        if (!npc_model_file_id) {
            return;
        }
        // Need to create the NPC.
        // Those 2 packets (P074 & P075) are used to create a new model, for instance if we want to "use" a tonic.
        // We have to find the data that are in the NPC structure and feed them to those 2 packets.
        GW::NPC npc = {0};
        npc.model_file_id = npc_model_file_id;
        npc.npc_flags = flags;
        npc.primary = (GW::Constants::Profession)1;
        npc.visual_adjustment = *(GW::CharAdjustment*)(&scale);
        npc.default_level = 0;
        GW::GameThread::Enqueue([npc_id, npc] {
            GW::Packet::StoC::NpcGeneralStats packet{};
            packet.npc_id = npc_id;
            packet.file_id = npc.model_file_id;
            packet.data1 = 0;
            packet.scale = *(uint32_t*)(&npc.visual_adjustment);
            packet.data2 = 0;
            packet.flags = npc.npc_flags;
            packet.profession = (uint32_t)npc.primary;
            packet.level = npc.default_level;
            packet.name[0] = 0;
            GW::StoC::EmulatePacket(&packet);
        });
        if (npc_model_file_data) {
            GW::GameThread::Enqueue([npc_id, npc_model_file_data] {
                GW::Packet::StoC::NPCModelFile packet;
                packet.npc_id = npc_id;
                packet.count = 1;
                packet.data[0] = npc_model_file_data;
                GW::StoC::EmulatePacket(&packet);
            });
        }
    }
    GW::GameThread::Enqueue([npc_id, agent_id, scale] {
        if (npc_id) {
            const GW::NPCArray& npcs = GW::GetGameContext()->world->npcs;
            const GW::NPC npc = npcs[npc_id];
            if (!npc.model_file_id) {
                return;
            }
        }
        GW::Packet::StoC::AgentScale packet1;
        packet1.header = GW::Packet::StoC::AgentScale::STATIC_HEADER;
        packet1.agent_id = agent_id;
        packet1.scale = scale;
        GW::StoC::EmulatePacket(&packet1);

        GW::Packet::StoC::AgentModel packet2;
        packet2.header = GW::Packet::StoC::AgentModel::STATIC_HEADER;
        packet2.agent_id = agent_id;
        packet2.model_id = npc_id;
        GW::StoC::EmulatePacket(&packet2);
    });
}

bool TransmoModule::GetNPCInfoByName(const std::string& name, PendingTransmo& transmo)
{
    for (const auto* entry : npc_transmo_list) {
        if (std::string_view(entry->name).find(name) == std::string_view::npos) {
            continue;
        }
        transmo = entry->ToPending();
        return true;
    }
    return false;
}

bool TransmoModule::GetNPCInfoByName(const std::wstring& name, PendingTransmo& transmo)
{
    return GetNPCInfoByName(TextUtils::WStringToString(name), transmo);
}

bool TransmoModule::ParseTransmoArgs(int argc, const LPWSTR* argv, int name_arg_index, PendingTransmo& transmo)
{
    const wchar_t* name_arg = argv[name_arg_index];
    int iscale;
    if (wcsncmp(name_arg, L"reset", 5) == 0) {
        transmo.npc_id = std::numeric_limits<int>::max();
    }
    else if (TextUtils::ParseInt(name_arg, &iscale)) {
        if (!ParseScale(iscale, transmo)) return false;
    }
    else if (!GetNPCInfoByName(name_arg, transmo)) {
        Log::Error("Unknown transmo '%ls'", name_arg);
        return false;
    }
    const int scale_arg_index = name_arg_index + 1;
    if (argc > scale_arg_index && TextUtils::ParseInt(argv[scale_arg_index], &iscale)) {
        if (!ParseScale(iscale, transmo)) return false;
    }
    return true;
}

bool TransmoModule::ParseScale(const int scale, PendingTransmo& transmo)
{
    if (scale < 6 || scale > 255) {
        Log::Error("scale must be between [6, 255]");
        return false;
    }
    transmo.scale = static_cast<DWORD>(scale) << 24;
    if (!transmo.npc_id) {
        transmo.npc_id = std::numeric_limits<int>::max() - 1;
    }
    return true;
}

bool TransmoModule::GetTargetTransmoInfo(PendingTransmo& transmo)
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) {
        return false;
    }
    transmo.npc_id = target->player_number;
    if (target->transmog_npc_id & 0x20000000) {
        transmo.npc_id = target->transmog_npc_id ^ 0x20000000;
    }
    else if (target->IsPlayer()) {
        return false;
    }
    return true;
}

void CHAT_CMD_FUNC(TransmoModule::CmdTransmoParty)
{
    GW::PartyInfo* pInfo = GW::PartyMgr::GetPartyInfo();
    if (!pInfo) return;

    PendingTransmo transmo;
    if (argc > 1) {
        if (!ParseTransmoArgs(argc, argv, 1, transmo)) return;
    }
    else {
        if (!GetTargetTransmoInfo(transmo)) return;
    }

    for (const GW::HeroPartyMember& p : pInfo->heroes)
        TransmoAgent(p.agent_id, transmo);
    for (const GW::HenchmanPartyMember& p : pInfo->henchmen)
        TransmoAgent(p.agent_id, transmo);
    for (const GW::PlayerPartyMember& p : pInfo->players) {
        const auto player = GW::PlayerMgr::GetPlayerByID(p.login_number);
        if (player) TransmoAgent(player->agent_id, transmo);
    }
}

void CHAT_CMD_FUNC(TransmoModule::CmdTransmoTarget)
{
    if (argc < 2) return Log::Error("Missing /transmotarget argument");
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return Log::Error("Invalid /transmotarget target");
    PendingTransmo transmo;
    if (!ParseTransmoArgs(argc, argv, 1, transmo)) return;
    TransmoAgent(target->agent_id, transmo);
}

void CHAT_CMD_FUNC(TransmoModule::CmdTransmo)
{
    PendingTransmo transmo;
    if (argc > 1) {
        if (wcscmp(argv[1], L"add") == 0) {
            AddCurrentTargetToTransmoList();
            return;
        }
        if (wcsncmp(argv[1], L"model", 5) == 0) {
            if (argc < 6) {
                Log::Info("HELP for /transmo model");
                Log::Info("Usage: /transmo model NPC_ID MODEL_FILE_ID MODEL_FILE FLAGS [SCALE]");
                Log::Info("Example, transmo as Gehraz: /transmo model 4581 204020 245127 540 [15]");
                Log::Info("The numbers required by the command can be obtained from the GWToolbox 'Info' window, in the 'Advanced' section under the 'Target' menu. Note: the numbers must be converted from hexadecimal to decimal.");
                return;
            }
            int npc_id = transmo.npc_id, model_file_id = transmo.npc_model_file_id, model_file_data = transmo.npc_model_file_data, flags = transmo.flags;
            unsigned int scale = transmo.scale;
            if (!TextUtils::ParseInt(argv[2], &npc_id)) return Log::Error("Transmo model: invalid NPC ID '%ls', expected an integer. Example: 4581", argv[2]);
            if (!TextUtils::ParseInt(argv[3], &model_file_id)) return Log::Error("Transmo model: invalid NPC ModelFileID '%ls', expected an integer. Example: 204020", argv[3]);
            if (!TextUtils::ParseInt(argv[4], &model_file_data)) return Log::Error("Transmo model: invalid NPC ModelFile '%ls', expected an integer. Example: 245127", argv[4]);
            if (!TextUtils::ParseInt(argv[5], &flags)) return Log::Error("Transmo model: invalid NPC Flags '%ls', expected an integer. Example: 540", argv[5]);
            if (argc > 6 && !TextUtils::ParseUInt(argv[6], &scale)) return Log::Error("Transmo model: invalid scale '%ls', expected an integer between 6 and 255", argv[6]);
            transmo.npc_id = npc_id;
            transmo.npc_model_file_id = model_file_id;
            transmo.npc_model_file_data = model_file_data;
            transmo.flags = flags;
            transmo.scale = argc > 6 ? scale << 24 : scale;
        }
        else if (!ParseTransmoArgs(argc, argv, 1, transmo)) {
            return;
        }
    }
    else {
        if (!GetTargetTransmoInfo(transmo)) return;
    }
    TransmoAgent(GW::Agents::GetControlledCharacterId(), transmo);
}

void CHAT_CMD_FUNC(TransmoModule::CmdTransmoAgent)
{
    if (argc < 3) return Log::Error("Missing /transmoagent argument");
    uint32_t agent_id;
    if (!TextUtils::ParseUInt(argv[1], &agent_id)) return Log::Error("Invalid /transmoagent agent_id");
    PendingTransmo transmo;
    if (!ParseTransmoArgs(argc, argv, 2, transmo)) return;
    TransmoAgent(agent_id, transmo);
}
