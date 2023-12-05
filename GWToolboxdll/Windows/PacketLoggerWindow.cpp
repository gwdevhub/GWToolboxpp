#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Maps.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Windows/PacketLoggerWindow.h>

#include <GWToolbox.h>

namespace {
    wchar_t* GetMessageCore()
    {
        GW::Array<wchar_t>* buff = &GW::GetGameContext()->world->message_buff;
        return buff ? buff->begin() : nullptr;
    }

    struct MapInfo {
        wchar_t enc_name[8]{};
        std::wstring name;
        wchar_t enc_desc[8]{};
        std::wstring description;
        GW::AreaInfo* map_info{};

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            if (!name.empty()) {
                json["name"] = GuiUtils::WStringToString(name);
            }
            if (!description.empty()) {
                json["description"] = GuiUtils::WStringToString(description);
            }
            json["campaign"] = map_info->campaign;
            json["type"] = map_info->type;
            json["region"] = map_info->region;
            json["flags"] = map_info->flags;
            json["continent"] = map_info->continent;
            return json;
        };
    };

    std::map<uint32_t, MapInfo*> maps;

    void FetchMapInfo()
    {
        if (!maps.empty()) {
            return;
        }
        for (uint32_t map_id = 1; map_id < static_cast<uint32_t>(GW::Constants::MapID::Count); map_id++) {
            const auto map_info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(map_id));
            if (!map_info) {
                continue;
            }
            auto map = new MapInfo();
            map->map_info = map_info;
            GW::UI::UInt32ToEncStr(map_info->name_id, map->enc_name, 8);
            GW::UI::AsyncDecodeStr(map->enc_name, &map->name);
            GW::UI::UInt32ToEncStr(map_info->description_id, map->enc_desc, 8);
            GW::UI::AsyncDecodeStr(map->enc_desc, &map->description);
            maps.emplace(map_id, map);
        }
        Log::Info("Fetching map info now");
    }

    void ExportMapInfo()
    {
        nlohmann::json json;
        for (const auto& it : maps) {
            json[it.first] = it.second->ToJson();
            delete it.second;
        }
        maps.clear();
        const std::wstring file_location = Resources::GetPath(L"maps.json");
        if (PathFileExistsW(file_location.c_str())) {
            DeleteFileW(file_location.c_str());
        }

        std::ofstream out(file_location);
        out << json.dump();
        out.close();
        Log::Info("Maps exported to %ls", file_location.c_str());
    }

    // Taken from StoCMgr.cpp
    using StoCHandler_pt = bool(__fastcall*)(GW::Packet::StoC::PacketBase* pak);

    struct StoCHandler {
        uint32_t* fields;
        uint32_t field_count;
        StoCHandler_pt handler_func;
    };

    StoCHandler** gwca_original_functions = nullptr;


    void TooltipHandlerInfo(StoCHandler& handler) {
        ImGui::SetTooltip("Field Count: %d\nHandler Addr: %08x", handler.field_count, handler.handler_func);
    }

    using StoCHandlerArray = GW::Array<StoCHandler>;

    enum class FieldType {
        Ignore,
        AgentId,
        Float,
        Vect2,
        Vect3,
        Byte,
        Word,
        Dword,
        Blob,
        String16,
        Array8,
        Array16,
        Array32,
        NestedStruct,
        Count
    };

    bool log_message_content = false;
    bool log_npc_dialogs = false;

    struct NPCDialog {
        wchar_t msg_in[122] = {0};
        wchar_t sender_in[32] = {0};
        std::wstring msg_out = L"";
        std::wstring sender_out = L"";
    } npc_dialog;

    bool logger_enabled = false;
    bool log_packet_content = false;
    bool auto_ignore_packets = false;
    bool debug = false;
    uint32_t log_message_callback_identifier = 0;
    volatile bool running;

    StoCHandlerArray game_server_handler;
    constexpr size_t packet_max = 512; // Increase if number of StoC packets exceeds this.
    bool ignored_packets[packet_max] = {false};
    bool blocked_packets[packet_max] = {false};
    GW::HookEntry hook_entry;



    void printchar(const wchar_t c)
    {
        if (c >= L' ' && c <= L'~') {
            printf("%lc", c);
        }
        else {
            printf("0x%X ", c);
        }
    }

    uintptr_t game_srv_object_addr;





    StoCHandlerArray* GetStoCHandlerArray() {

        uintptr_t address = GW::Scanner::Find("\x75\x04\x33\xC0\x5D\xC3\x8B\x41\x08\xA8\x01\x75", "xxxxxxxxxxxx", -6);
        const uintptr_t StoCHandler_Addr = *(uintptr_t*)address;

        struct GameServer {
            uint8_t h0000[8];

            struct {
                uint8_t h0000[12]{};

                struct {
                    uint8_t h0000[12]{};
                    void* next{};
                    uint8_t h0010[12]{};
                    uint32_t ClientCodecArray[4]{};
                    StoCHandlerArray handlers;
                }* ls_codec{};

                uint8_t h0010[12]{};
                // Client codec
                uint32_t ClientCodecArray[4]{};
                StoCHandlerArray handlers;
            } * gs_codec;
        };


        const auto addr = (GameServer* *)StoCHandler_Addr;
        if (!(addr && *addr)) {
            return nullptr;
        }
        return &(*addr)->gs_codec->handlers;

    }
    bool stoc_initialised = false;
    void InitStoC()
    {
        if (stoc_initialised) {
            return;
        }
        const auto original_handler_arr = GetStoCHandlerArray();
        if (!original_handler_arr) {
            return;
        }

        auto& test_handler = original_handler_arr->at(1);

        const auto original_handler_func = test_handler.handler_func;

        // Hook packet 1 to grab the address for GWCA's function hook
        GW::HookEntry entry;
        GW::StoC::PacketCallback c;

        GW::StoC::RegisterPacketCallback(&entry, 1, c);
        if (original_handler_func == test_handler.handler_func) {
            GW::StoC::RemoveCallback(1, &entry);
            return; // GWCA not ready yet
        }

        StoCHandler_pt GWCA_StoCHandler_Func = test_handler.handler_func;
        GW::StoC::RemoveCallback(1, &entry);

        ASSERT(original_handler_func != test_handler.handler_func);

        // Using GWCA_StoCHandler_Func, offset to find original callback array
        gwca_original_functions = *(StoCHandler***)(((uintptr_t)GWCA_StoCHandler_Func) + 0x110);
        ASSERT(gwca_original_functions && *gwca_original_functions);

        StoCHandler* original_functions = *gwca_original_functions;

        // Final sanity check; ensure the pointer for packet 1 is the same as what we grabbed from memory
        ASSERT(original_functions[1].handler_func == original_handler_func);

        // Copy gs handler array; we're going to swap out m_buffer.
        memcpy(&game_server_handler, original_handler_arr, sizeof(*original_handler_arr));

        // Redirect our handler array to pont to the originals
        game_server_handler.m_buffer = original_functions;

        ignored_packets[12] = true;
        ignored_packets[13] = true;
        ignored_packets[31] = true;
        ignored_packets[42] = true;
        ignored_packets[47] = true;
        ignored_packets[160] = true;
        ignored_packets[242] = true;

        stoc_initialised = true;
    }


    FieldType GetField(const uint32_t type, const uint32_t size, const uint32_t count)
    {
        switch (type) {
        case 0:
            return FieldType::AgentId;
        case 1:
            return FieldType::Float;
        case 2:
            return FieldType::Vect2;
        case 3:
            return FieldType::Vect3;
        case 4:
        case 8:
            switch (count) {
            case 1:
                return FieldType::Byte;
            case 2:
                return FieldType::Word;
            case 4:
                return FieldType::Dword;
            }
        case 5:
        case 9:
            return FieldType::Blob;
        case 6:
        case 10:
            return FieldType::Ignore;
        case 7:
            return FieldType::String16;
        case 11:
            switch (size) {
            case 1:
                return FieldType::Array8;
            case 2:
                return FieldType::Array32;
            case 4:
                return FieldType::Array32;
            }
        case 12:
            return FieldType::NestedStruct;
        }

        return FieldType::Count;
    }

    void PrintIndent(const uint32_t indent)
    {
        char buffer[64];
        ASSERT(indent <= sizeof(buffer) - 1);
        for (auto i = 0u; i < indent; i++) {
            buffer[i] = ' ';
        }
        buffer[indent] = 0;
        printf("%s", buffer);
    }

    void GetHexS(char* buf, const uint8_t byte)
    {
        const uint8_t h = byte >> 4 & 0xfu;
        const uint8_t l = byte >> 0 & 0xfu;
        if (h < 10) {
            buf[0] = h + '0';
        }
        else {
            buf[0] = h - 10 + 'A';
        }
        if (l < 10) {
            buf[1] = l + '0';
        }
        else {
            buf[1] = l - 10 + 'A';
        }
        buf[2] = 0;
    }

    template <typename T>
    void Serialize(uint8_t** bytes, T* val)
    {
        uint8_t* b = *bytes;
        // if we want to allign
        // b = (uint8_t*)(((uintptr_t)b + (sizeof(T) - 1)) & ~(sizeof(T) - 1));
        memcpy(val, b, sizeof(T));
        *bytes = b + sizeof(T);
    }

    void PrintString(const int length, const wchar_t* str)
    {
        for (auto i = 0; i < length && str[i]; i++) {
            printf(i > 0 ? " %04x" : "%04x", str[i]);
        }
    }

    void PrintField(const FieldType field, const uint32_t count, uint8_t** bytes, const uint32_t indent)
    {
        switch (field) {
        case FieldType::AgentId: {
            PrintIndent(indent);
            uint32_t agent_id;
            Serialize<uint32_t>(bytes, &agent_id);
            printf("AgentId(%u)\n", agent_id);
            break;
        }
        case FieldType::Float: {
            PrintIndent(indent);
            float f;
            Serialize<float>(bytes, &f);
            printf("Float(%f)\n", f);
            break;
        }
        case FieldType::Vect2: {
            PrintIndent(indent);
            float x, y;
            Serialize<float>(bytes, &x);
            Serialize<float>(bytes, &y);
            printf("Vect2(%f, %f)\n", x, y);
            break;
        }
        case FieldType::Vect3: {
            PrintIndent(indent);
            float x, y, z;
            Serialize<float>(bytes, &x);
            Serialize<float>(bytes, &y);
            Serialize<float>(bytes, &z);
            printf("Vect3(%f, %f, %f)\n", x, y, z);
            break;
        }
        case FieldType::Byte: {
            PrintIndent(indent);
            uint32_t val;
            Serialize<uint32_t>(bytes, &val);
            printf("Byte(%u)\n", val);
            break;
        }
        case FieldType::Word: {
            PrintIndent(indent);
            uint32_t val;
            Serialize<uint32_t>(bytes, &val);
            printf("Word(%u)\n", val);
            break;
        }
        case FieldType::Dword: {
            PrintIndent(indent);
            uint32_t val;
            Serialize<uint32_t>(bytes, &val);
            printf("Dword(%u)\n", val);
            break;
        }
        case FieldType::Blob: {
            PrintIndent(indent);
            printf("Blob(%u) => ", count);
            for (auto i = 0u; i < count; i++) {
                char buf[3];
                GetHexS(buf, **bytes);
                printf("%s ", buf);
                ++*bytes;
            }
            printf("\n");
            break;
        }
        case FieldType::String16: {
            PrintIndent(indent);
            const auto str = reinterpret_cast<wchar_t*>(*bytes);
            const size_t length = wcsnlen(str, count);
            printf("String(%zu) \"", length);
            PrintString(length, str);
            printf("\"\n");
            *bytes += count * 2;
            break;
        }
        case FieldType::Array8: {
            PrintIndent(indent);
            uint32_t length;
            uint8_t* end = *bytes + count;
            Serialize<uint32_t>(bytes, &length);
            printf("Array8(%u) {\n", length);
            uint8_t val;
            for (size_t i = 0; i < length; i++) {
                Serialize<uint8_t>(bytes, &val);
                PrintIndent(indent + 4);
                printf("[%zu] => %u,\n", i, val);
            }
            printf("}\n");
            *bytes = end;
            break;
        }
        case FieldType::Array16: {
            PrintIndent(indent);
            uint32_t length = count;
            Serialize<uint32_t>(bytes, &length);
            uint8_t* end = *bytes + count * 2;
            printf("Array16(%u of %u) {\n", length, count);
            if (length < 64) {
                uint16_t val;
                for (size_t i = 0; i < length; i++) {
                    Serialize<uint16_t>(bytes, &val);
                    PrintIndent(indent + 4);
                    printf("[%zu] => %u,\n", i, val);
                }
            }
            printf("}\n");
            *bytes = end;
            break;
        }
        case FieldType::Array32: {
            PrintIndent(indent);
            uint32_t length = count;
            Serialize<uint32_t>(bytes, &length);
            uint8_t* end = *bytes + count * 4;
            printf("Array32(%u of %u) {\n", length, count);
            if (length < 128) {
                uint32_t val;
                for (size_t i = 0; i < length; i++) {
                    Serialize<uint32_t>(bytes, &val);
                    PrintIndent(indent + 4);
                    printf("[%zu] => %u,\n", i, val);
                }
            }
            printf("}\n");
            *bytes = end;
            break;
        }
        default:
            break;
        }
    }

    void PrintNestedField(uint32_t* fields, const uint32_t n_fields,
        const uint32_t repeat, uint8_t** bytes, const uint32_t indent)
    {
        for (uint32_t rep = 0; rep < repeat; rep++) {
            PrintIndent(indent);
            printf("[%u] => {\n", rep);
            for (auto i = 0u; i < n_fields; i++) {
                const uint32_t field = fields[i];
                const uint32_t type = field >> 0 & 0xF;
                const uint32_t size = field >> 4 & 0xF;
                const uint32_t count = field >> 8 & 0xFFFF;

                // Just to make it easier to print
                const FieldType field_type = GetField(type, size, count);

                // Used to skip field that are not printable, for instance the array end
                if (field_type == FieldType::Ignore) {
                    continue;
                }

                if (field_type != FieldType::NestedStruct) {
                    PrintField(field_type, count, bytes, indent + 4);
                }
                else {
                    const uint32_t next_field_index = i + 1;

                    uint32_t struct_count;
                    Serialize<uint32_t>(bytes, &struct_count);

                    PrintIndent(indent + 4);
                    printf("NextedStruct(%u) {\n", struct_count);
                    PrintNestedField(fields + next_field_index,
                        n_fields - next_field_index, struct_count, bytes, indent + 8);
                    PrintIndent(indent + 4);
                    printf("}\n");

                    // This isn't necessary, but Guild Wars always have the nested struct at the end and once max
                    break;
                }
            }
            PrintIndent(indent);
            printf("}\n");
        }
    }

}


void PacketLoggerWindow::CtoSHandler(const GW::HookStatus*, void* packet) const
{
    if (!logger_enabled) {
        return;
    }
    printf(PrefixTimestamp("CtoS packet(%u 0x%X) {\n").c_str(), *static_cast<uint32_t*>(packet), *static_cast<uint32_t*>(packet));
}

void PacketLoggerWindow::PacketHandler(GW::HookStatus* status, GW::Packet::StoC::PacketBase* packet) const
{
    if (blocked_packets[packet->header]) {
        status->blocked = true;
    }
    if (!logger_enabled) {
        return;
    }
    InitStoC();
    if (!game_server_handler.m_buffer) {
        return;
    }
    //if (packet->header == 95) return true;
    if (packet->header >= game_server_handler.size()) {
        return;
    }
    if (auto_ignore_packets) {
        ignored_packets[packet->header] = true;
    }
    if (ignored_packets[packet->header]) {
        return;
    }

    const StoCHandler handler = game_server_handler.at(packet->header);
    auto packet_raw = reinterpret_cast<uint8_t*>(packet);

    uint8_t** bytes = &packet_raw;
    uint32_t header;
    Serialize<uint32_t>(bytes, &header);
    ASSERT(packet->header == header);

    if (log_packet_content) {
        printf(PrefixTimestamp("StoC packet(%u 0x%X) {\n").c_str(), packet->header, packet->header);
        PrintNestedField(handler.fields + 1, handler.field_count - 1, 1, bytes, 4);
        printf("} endpacket(%u 0x%X)\n", packet->header, packet->header);
    }
    else {
        printf(PrefixTimestamp("StoC packet(%u 0x%X) {\n").c_str(), packet->header, packet->header);
    }
}

std::string PacketLoggerWindow::PadLeft(std::string input, const uint8_t count, const char c)
{
    input.insert(input.begin(), count - input.size(), c);
    return input;
}

std::string PacketLoggerWindow::PrefixTimestamp(std::string message) const
{
    if (timestamp_type == TimestampType_None) {
        return message;
    }

    switch (timestamp_type) {
        case TimestampType_Local: {
            SYSTEMTIME time;
            GetLocalTime(&time);
            bool prependColon = false;
            char t[4];
            std::string time_s = "[";
            if (timestamp_show_hours) {
                snprintf(t, 4, "%02d", time.wHour);
                time_s.append(t);
                prependColon = true;
            }
            if (timestamp_show_minutes) {
                if (prependColon) {
                    time_s.append(":");
                }
                snprintf(t, 4, "%02d", time.wMinute);
                time_s.append(t);
                prependColon = true;
            }
            if (timestamp_show_seconds) {
                if (prependColon) {
                    time_s.append(":");
                }
                snprintf(t, 4, "%02d", time.wSecond);
                time_s.append(t);
                prependColon = true;
            }
            if (timestamp_show_milliseconds) {
                if (prependColon) {
                    time_s.append(".");
                }
                snprintf(t, 4, "%03d", time.wMilliseconds);
                time_s.append(t);
            }
            return time_s + "] " + message;
        }
        case TimestampType_Instance: {
            auto ms = std::chrono::milliseconds(GW::Map::GetInstanceTime());
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(ms);
            ms -= std::chrono::duration_cast<std::chrono::milliseconds>(secs);
            auto mins = std::chrono::duration_cast<std::chrono::minutes>(secs);
            secs -= std::chrono::duration_cast<std::chrono::seconds>(mins);
            const auto hours = std::chrono::duration_cast<std::chrono::hours>(mins);
            mins -= std::chrono::duration_cast<std::chrono::minutes>(hours);
            bool prependColon = false;
            std::string time_s = "[";
            if (timestamp_show_hours) {
                time_s.append(PadLeft(std::to_string(hours.count()), 2, '0'));
                prependColon = true;
            }
            if (timestamp_show_minutes) {
                if (prependColon) {
                    time_s.append(":");
                }
                time_s.append(PadLeft(std::to_string(mins.count()), 2, '0'));
                prependColon = true;
            }
            if (timestamp_show_seconds) {
                if (prependColon) {
                    time_s.append(":");
                }
                time_s.append(PadLeft(std::to_string(secs.count()), 2, '0'));
                prependColon = true;
            }
            if (timestamp_show_milliseconds) {
                if (prependColon) {
                    time_s.append(".");
                }
                time_s.append(PadLeft(std::to_string(ms.count()), 3, '0'));
            }
            return time_s + "] " + message;
        }
        default: {
            return message;
        }
    }
}

void PacketLoggerWindow::AddMessageLog(const wchar_t* encoded)
{
    const std::wstring encoded_ws(encoded);
    if (!encoded || message_log.contains(encoded_ws)) {
        return;
    }
    const auto t = new ForTranslation();
    t->in = encoded;
    pending_translation.push_back(t);
    GW::UI::AsyncDecodeStr(encoded, &t->out);
}

void PacketLoggerWindow::SaveMessageLog() const
{
    const utf8::string filename = Resources::GetPathUtf8(L"message_log.csv");
    std::wofstream myFile(filename.bytes);

    // Send column names to the stream
    for (const auto& it : message_log) {
        if (!it.second || !it.second->length()) {
            continue;
        }
        myFile << it.first.c_str();
        myFile << L",";
        myFile << it.second->c_str();
        myFile << "\n";
    }
    // Close the file
    myFile.close();
}

void PacketLoggerWindow::ClearMessageLog()
{
    for (const auto& val : message_log | std::views::values) {
        delete val;
    }
    message_log.clear();
}

void PacketLoggerWindow::Draw(IDirect3DDevice9*)
{

    if (!visible) {
        return;
    }
    InitStoC();
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(256, 128), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }
    if (ImGui::Checkbox("Enable Packet Logging", &logger_enabled)) {
        logger_enabled = !logger_enabled;
        if (!logger_enabled) {
            Enable();
        }
        else {
            Disable();
        }
    }
    ImGui::ShowHelp("Log outgoing and incoming packet contents in debug console");
    ImGui::SameLine();
    ImGui::Checkbox("Log Packet Content", &log_packet_content);
    ImGui::SameLine();
    ImGui::Checkbox("Auto ignore incoming packets", &auto_ignore_packets);
    ImGui::ShowHelp("While ticked, any StoC packets received will be added to the ignore list.");
    /*if ( ImGui::Button("Export Map Info")) {
        if (maps.empty()) {
            FetchMapInfo();
        }
        else {
            ExportMapInfo();
        }
    }
    ImGui::ShowHelp("Export current map info to disk");
    */
    ImGui::Checkbox("Log NPC Dialogs", &log_npc_dialogs);
    ImGui::ShowHelp("Log encoded strings and their translated output to debug console");
    if (ImGui::CollapsingHeader("Ignored Packets")) {
        if (ImGui::Button("Select All")) {
            for (size_t i = 0; i < game_server_handler.size(); i++) {
                ignored_packets[i] = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All")) {
            for (size_t i = 0; i < game_server_handler.size(); i++) {
                ignored_packets[i] = false;
            }
        }
        float offset = 0.0f;
        for (size_t i = 0; i < game_server_handler.size(); i++) {
            if (i % 12 == 0) {
                offset = 0.0f;
                ImGui::NewLine();
            }
            ImGui::SameLine(offset, 0);
            offset += 80.0f * ImGui::GetIO().FontGlobalScale;
            char buf[30];
            sprintf(buf, "%zu###ignore_packet_%zu", i, i);
            bool p = ignored_packets[i];
            if (ImGui::Checkbox(buf, &p)) {
                ignored_packets[i] = p;
            }
            if (ImGui::IsItemHovered()) {
                TooltipHandlerInfo(game_server_handler[i]);
            }
        }
    }
    if (ImGui::CollapsingHeader("Blocked Packets")) {
        if (ImGui::Button("Select All")) {
            for (size_t i = 0; i < game_server_handler.size(); i++) {
                blocked_packets[i] = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All")) {
            for (size_t i = 0; i < game_server_handler.size(); i++) {
                blocked_packets[i] = false;
            }
        }
        float offset = 0.0f;
        for (size_t i = 0; i < game_server_handler.size(); i++) {
            if (i % 12 == 0) {
                offset = 0.0f;
                ImGui::NewLine();
            }
            ImGui::SameLine(offset, 0);
            offset += 80.0f * ImGui::GetIO().FontGlobalScale;
            char buf[30];
            sprintf(buf, "%zu###block_packet_%zu", i, i);
            bool p = blocked_packets[i];
            if (ImGui::Checkbox(buf, &p)) {
                blocked_packets[i] = p;
            }
            if (ImGui::IsItemHovered()) {
                TooltipHandlerInfo(game_server_handler[i]);
            }
        }
    }
    return ImGui::End();
}

void PacketLoggerWindow::Initialize()
{
    ToolboxWindow::Initialize();
    GW::GameThread::Enqueue([]() {
        InitStoC();
        });
    if (logger_enabled) {
        logger_enabled = false;
        Enable();
    }
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_DIALOG_BODY, OnMessagePacket);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_DOA_COMPLETE_ZONE, OnMessagePacket);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_SPEECH_BUBBLE, OnMessagePacket);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_DIALOG_BUTTON, OnMessagePacket);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_MISSION_OBJECTIVE_UPDATE_STRING, OnMessagePacket);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_MISSION_OBJECTIVE_ADD, OnMessagePacket);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_AGENT_DISPLAY_DIALOG, OnMessagePacket);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_CHAT_MESSAGE_NPC, OnMessagePacket);
}

void PacketLoggerWindow::OnMessagePacket(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet)
{
    if (!log_npc_dialogs) {
        return;
    }
    const auto pak = (uint32_t*)packet;
    uint32_t string_offset = 0;
    switch (pak[0]) {
        case GAME_SMSG_DIALOG_BODY:
        case GAME_SMSG_DOA_COMPLETE_ZONE:
            string_offset = 1;
            break;
        case GAME_SMSG_SPEECH_BUBBLE:
        case GAME_SMSG_DIALOG_BUTTON:
        case GAME_SMSG_MISSION_OBJECTIVE_UPDATE_STRING:
            string_offset = 2;
            break;
        case GAME_SMSG_MISSION_OBJECTIVE_ADD:
            string_offset = 3;
            break;
        case GAME_SMSG_AGENT_DISPLAY_DIALOG:
            string_offset = 19;
            break;
        case GAME_SMSG_CHAT_MESSAGE_NPC:
            Instance().AddMessageLog(GetMessageCore());
            break;
    }
    if (string_offset) {
        Instance().AddMessageLog((wchar_t*)(pak + string_offset));
    }
}

void PacketLoggerWindow::Update(const float)
{


    for (auto it = pending_translation.begin(); it != pending_translation.end(); ++it) {
        ForTranslation& t = **it;
        if (t.out.empty()) {
            continue;
        }
        wchar_t enc_str[1024];
        int len = 0;
        for (size_t i = 0; i < t.in.size(); i++) {
            len += swprintf(&enc_str[len], _countof(enc_str) - len, L"0x%X ", t.in[i]);
        }
        Log::LogW(enc_str);
        Log::LogW(t.out.c_str());
        delete*it;
        pending_translation.erase(it);
        break;
    }
    if (message_log.size() > 100) {
        SaveMessageLog();
    }
    if (last_message_decoded && !last_message_decoded->empty()) {
        SaveMessageLog();
    }
}

void PacketLoggerWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    SAVE_BOOL(timestamp_type);
    SAVE_BOOL(timestamp_show_hours);
    SAVE_BOOL(timestamp_show_seconds);
    SAVE_BOOL(timestamp_show_milliseconds);

    std::bitset<packet_max> ignored_packets_bitset;
    for (size_t i = 0; i < packet_max; i++) {
        ignored_packets_bitset[i] = ignored_packets[i] ? 1 : 0;
    }
    ini->SetValue(Name(), VAR_NAME(ignored_packets), ignored_packets_bitset.to_string().c_str());
    SaveMessageLog();
    ClearMessageLog();
}

void PacketLoggerWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);






    const char* ignored_packets_bits = ini->GetValue(Name(), VAR_NAME(ignored_packets), "-");
    if (strcmp(ignored_packets_bits, "-") == 0) {
        return;
    }
    const std::bitset<packet_max> ignored_packets_bitset(ignored_packets_bits);
    for (size_t i = 0; i < packet_max; i++) {
        ignored_packets[i] = ignored_packets_bitset[i] == 1;
    }
}

void PacketLoggerWindow::Disable()
{
    if (!logger_enabled || !game_server_handler.m_buffer) {
        return;
    }
    for (size_t i = 0; i < game_server_handler.size(); i++) {
        GW::StoC::RemoveCallback(i, &hook_entry);
    }
    for (size_t i = 0; i < 180; i++) {
        GW::CtoS::RemoveCallback(i, &hook_entry);
    }
    logger_enabled = false;
}
void PacketLoggerWindow::Terminate() {
    ClearMessageLog();
}
void PacketLoggerWindow::Enable()
{
    if (logger_enabled) {
        return;
    }
    for (size_t i = 0; i < game_server_handler.size(); i++) {
        GW::StoC::RegisterPacketCallback(
            &hook_entry, i, [this](GW::HookStatus* status, GW::Packet::StoC::PacketBase* packet) -> void {
                PacketHandler(status, packet);
            }, -0x9000
        );
    }
    for (size_t i = 0; i < 180; i++) {
        GW::CtoS::RegisterPacketCallback(
            &hook_entry, i, [this](const GW::HookStatus* status, void* packet) -> void {
                CtoSHandler(status, packet);
            }
        );
    }
    logger_enabled = true;
}
void PacketLoggerWindow::DrawSettingsInternal()
{
    ImGui::RadioButton("No timestamp", &timestamp_type, TimestampType_None);
    ImGui::RadioButton("Local timestmap", &timestamp_type, TimestampType_Local);
    ImGui::RadioButton("Instance timestamp", &timestamp_type, TimestampType_Instance);
    ImGui::Checkbox("Show hours", &timestamp_show_hours);
    ImGui::Checkbox("Show minutes", &timestamp_show_minutes);
    ImGui::Checkbox("Show seconds", &timestamp_show_seconds);
    ImGui::Checkbox("Show milliseconds", &timestamp_show_milliseconds);
}
