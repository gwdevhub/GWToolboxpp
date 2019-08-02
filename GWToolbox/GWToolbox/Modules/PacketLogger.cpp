#include "stdafx.h"
#include <Windows.h>

#include <assert.h>
#include <stdint.h>

#include <vector>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include "PacketLogger.h"

// We can forward declare, because we won't use it
struct IDirect3DDevice9;

// Taken from StoCMgr.cpp
typedef bool(__fastcall* StoCHandler_pt)(GW::Packet::StoC::PacketBase* pak);
struct StoCHandler {
    uint32_t* fields;
    uint32_t       field_count;
    StoCHandler_pt handler_func;
};
typedef GW::Array<StoCHandler> StoCHandlerArray;

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
bool logger_enabled = false;
uint32_t log_message_callback_identifier = 0;
static volatile bool running;
static StoCHandlerArray  game_server_handler;
static std::vector<bool> ignored_packets;
static void printchar(wchar_t c) {
    if (c >= L' ' && c <= L'~') {
        printf("%lc", c);
    }
    else {
        printf("0x%X ", c);
    }
}

static void InitStoC()
{
    struct GameServer {
        uint8_t h0000[8];
        struct {
            uint8_t h0000[12];
            struct {
                uint8_t h0000[12];
                void* next;
                uint8_t h0010[12];
                uint32_t ClientCodecArray[4];
                StoCHandlerArray handlers;
            } *ls_codec;
            uint8_t h0010[12];
            // Client codec
            uint32_t ClientCodecArray[4];
            StoCHandlerArray handlers;
        } *gs_codec;
    };

    uintptr_t StoCHandler_Addr;
    uintptr_t address = GW::Scanner::Find("\x50\x52\x8B\x55\x0C\xC7\x45\xF8", "xxxxxxxx", -0x23);
    StoCHandler_Addr = *(uintptr_t*)address;

    GameServer** addr = (GameServer * *)StoCHandler_Addr;
    game_server_handler = (*addr)->gs_codec->handlers;
    ignored_packets.resize(game_server_handler.size());

    ignored_packets[12] = true;
    ignored_packets[13] = true;
    ignored_packets[31] = true;
    ignored_packets[42] = true;
    ignored_packets[47] = true;
    ignored_packets[160] = true;
    ignored_packets[242] = true;
}

static FieldType GetField(uint32_t type, uint32_t size, uint32_t count)
{
    switch (type) {
    case 0: return FieldType::AgentId;
    case 1: return FieldType::Float;
    case 2: return FieldType::Vect2;
    case 3: return FieldType::Vect3;
    case 4: case 8:
        switch (count) {
        case 1: return FieldType::Byte;
        case 2: return FieldType::Word;
        case 4: return FieldType::Dword;
        }
    case 5: case 9:
        return FieldType::Blob;
    case 6: case 10:
        return FieldType::Ignore;
    case 7:
        return FieldType::String16;
    case 11:
        switch (size) {
        case 1: return FieldType::Array8;
        case 2: return FieldType::Array16;
        case 4: return FieldType::Array32;
        }
    case 12:
        return FieldType::NestedStruct;
    }

    return FieldType::Count;
}

static void PrintIndent(uint32_t indent)
{
    char buffer[64];
    assert(indent <= sizeof(buffer) - 1);
    for (uint32_t i = 0; i < indent; i++) {
        buffer[i] = ' ';
    }
    buffer[indent] = 0;
    printf("%s", buffer);
}

static void GetHexS(char* buf, uint8_t byte)
{
    uint8_t h = (byte >> 4) & 0xf;
    uint8_t l = (byte >> 0) & 0xf;
    if (h < 10)
        buf[0] = h + '0';
    else
        buf[0] = (h - 10) + 'A';
    if (l < 10)
        buf[1] = l + '0';
    else
        buf[1] = (l - 10) + 'A';
    buf[2] = 0;
}

template<typename T>
static void Serialize(uint8_t** bytes, T* val)
{
    uint8_t* b = *bytes;
    // if we want to allign
    // b = (uint8_t*)(((uintptr_t)b + (sizeof(T) - 1)) & ~(sizeof(T) - 1));
    memcpy(val, b, sizeof(T));
    *bytes = b + sizeof(T);
}

static void PrintField(FieldType field, uint32_t count, uint8_t** bytes, uint32_t indent)
{
    switch (field) {
    case FieldType::AgentId: {
        PrintIndent(indent);
        uint32_t agent_id;
        Serialize<uint32_t>(bytes, &agent_id);
        printf("AgentId(%lu)\n", agent_id);
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
        printf("Byte(%lu)\n", val);
        break;
    }
    case FieldType::Word: {
        PrintIndent(indent);
        uint32_t val;
        Serialize<uint32_t>(bytes, &val);
        printf("Word(%lu)\n", val);
        break;
    }
    case FieldType::Dword: {
        PrintIndent(indent);
        uint32_t val;
        Serialize<uint32_t>(bytes, &val);
        printf("Dword(%lu)\n", val);
        break;
    }
    case FieldType::Blob: {
        PrintIndent(indent);
        printf("Blob(%lu) => ", count);
        for (uint32_t i = 0; i < count; i++) {
            char buf[3];
            GetHexS(buf, **bytes);
            printf("%s ", buf);
            ++(*bytes);
        }
        printf("\n");
        break;
    }
    case FieldType::String16: {
        PrintIndent(indent);
        wchar_t* str = reinterpret_cast<wchar_t*>(*bytes);
        size_t length = wcsnlen(str, count);
        wprintf(L"String(%lu) \"%.*s\"\n", length, static_cast<int>(length), str);
        // PrintString(length, str);
        *bytes += (count * 2);
        break;
    }
    case FieldType::Array8: {
        PrintIndent(indent);
        uint32_t length;
        uint8_t* end = *bytes + count;
        Serialize<uint32_t>(bytes, &length);
        printf("Array8(%lu) {\n", length);
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
        uint32_t length;
        Serialize<uint32_t>(bytes, &length);
        uint8_t* end = *bytes + (count * 2);
        printf("Array16(%lu) {\n", length);
        uint16_t val;
        for (size_t i = 0; i < length; i++) {
            Serialize<uint16_t>(bytes, &val);
            PrintIndent(indent + 4);
            printf("[%zu] => %u,\n", i, val);
        }
        printf("}\n");
        *bytes = end;
        break;
    }
    case FieldType::Array32: {
        PrintIndent(indent);
        uint32_t length;
        uint8_t* end = *bytes + (count * 4);
        Serialize<uint32_t>(bytes, &length);
        printf("Array32(%lu) {\n", length);
        uint32_t val;
        for (size_t i = 0; i < length; i++) {
            Serialize<uint32_t>(bytes, &val);
            PrintIndent(indent + 4);
            printf("[%zu] => %u,\n", i, val);
        }
        printf("}\n");
        *bytes = end;
        break;
    }
    }
}

static void PrintNestedField(uint32_t* fields, uint32_t n_fields,
    uint32_t repeat, uint8_t** bytes, uint32_t indent)
{
    for (uint32_t rep = 0; rep < repeat; rep++)
    {
        PrintIndent(indent);
        printf("[%lu] => {\n", rep);
        for (uint32_t i = 0; i < n_fields; i++) {
            uint32_t field = fields[i];
            uint32_t type = (field >> 0) & 0xF;
            uint32_t size = (field >> 4) & 0xF;
            uint32_t count = (field >> 8) & 0xFFFF;

            // Just to make it easier to print
            FieldType field_type = GetField(type, size, count);

            // Used to skip field that are not printable, for instance the array end
            if (field_type == FieldType::Ignore)
                continue;

            if (field_type != FieldType::NestedStruct) {
                PrintField(field_type, count, bytes, indent + 4);
            }
            else {
                uint32_t next_field_index = i + 1;

                uint32_t struct_count;
                Serialize<uint32_t>(bytes, &struct_count);

                PrintIndent(indent + 4);
                printf("NextedStruct(%lu) {\n", struct_count);
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

static bool PacketHandler(GW::Packet::StoC::PacketBase* packet)
{
    if (!logger_enabled) return false;
    //if (packet->header == 95) return true;
    if (packet->header >= game_server_handler.size())
        return false;
    if (ignored_packets[packet->header])
        return false;

    StoCHandler handler = game_server_handler[packet->header];
    uint8_t* packet_raw = reinterpret_cast<uint8_t*>(packet);

    uint8_t** bytes = &packet_raw;
    uint32_t header;
    Serialize<uint32_t>(bytes, &header);
    assert(packet->header == header);

    printf("packet(%lu) {\n", packet->header);
    PrintNestedField(handler.fields + 1, handler.field_count - 1, 1, bytes, 4);
    printf("}\n");

    // Returns false means to forward the packet to the game
    return false;
}

void PacketLogger::Draw(IDirect3DDevice9* pDevice) {
    if (!visible)
        return;
    ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(256, 128), ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    if (ImGui::Checkbox("Enable Logging",&logger_enabled)) {
        logger_enabled = !logger_enabled;
        if (!logger_enabled)
            Enable();
        else
            Disable();
    }
    if (ImGui::Checkbox("Log Message Content", &log_message_content)) {
        if (!logger_enabled)
            log_message_content = false;
        if (log_message_callback_identifier) {
            GW::StoC::RemoveCallback(GW::Packet::StoC::MessageCore::STATIC_HEADER, log_message_callback_identifier);
            log_message_callback_identifier = 0;
        }

        if (log_message_content) {
            log_message_callback_identifier = GW::StoC::AddCallback<GW::Packet::StoC::MessageCore>([&](GW::Packet::StoC::MessageCore* pak) -> bool {
                for (int i = 0; pak->message[i] != 0; ++i) printchar(pak->message[i]);
                printf("\n");
                return false;
            });
        }
    }
    return ImGui::End();
}
void PacketLogger::Initialize() {
    ToolboxWindow::Initialize();
    InitStoC();
    if (logger_enabled) {
        logger_enabled = false;
        Enable();
    }
    /*GW::StoC::AddCallback<GW::Packet::StoC::GameSrvTransfer>([this](GW::Packet::StoC::GameSrvTransfer* pak) -> bool {
        logger_enabled = false;
        return false;
        });
    GW::StoC::AddCallback<GW::Packet::StoC::MapLoaded>([this](GW::Packet::StoC::MapLoaded* pak) -> bool {
        logger_enabled = true;
        return false;
        });*/
}
void PacketLogger::Disable() {
    if (!logger_enabled) return;
    for (size_t i = 0; i < game_server_handler.size(); i++) {
        GW::StoC::RemoveCallback(i, identifiers[i]);
    }
    logger_enabled = false;
}
void PacketLogger::Enable() {
    if (logger_enabled) return;
    for (size_t i = 0; i < game_server_handler.size(); i++) {
        identifiers[i] = GW::StoC::AddCallback(i, PacketHandler);
    }
    logger_enabled = true;
}