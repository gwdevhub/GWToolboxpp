#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/GameEntities/Map.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Windows/StringDecoderWindow.h>

static void printchar(wchar_t c)
{
    if (c >= L' ' && c <= L'~') {
        printf("%lc", c);
    } else {
        printf("0x%X ", c);
    }
}
void StringDecoderWindow::Draw(IDirect3DDevice9 *pDevice)
{
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible)
        return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(256, 128), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        delete[] encoded;
        encoded = nullptr;
        return ImGui::End();
    }
    if (!encoded) {
        encoded = new char[encoded_size];
        encoded[0] = 0;
    }
    bool decodeIt = ImGui::InputInt("Encoded string id:", &encoded_id, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsHexadecimal);
    decodeIt |= ImGui::InputText("Encoded string:", encoded, encoded_size, ImGuiInputTextFlags_EnterReturnsTrue);
    decodeIt |= ImGui::Button("Decode");
    if (decodeIt)
    {
        wchar_t buf[8];
        if (encoded_id >= 0 && GW::UI::UInt32ToEncStr(encoded_id, buf, _countof(buf))) {
            int offset = 0;
            for (size_t i = 0; buf[i]; i++) {
                offset += sprintf(&encoded[offset], offset > 0 ? " 0x%04x" : "0x%04x", buf[i]);
            }
        }
        Decode();
    }
    ImGui::InputInt("Map ID:", &map_id);
    if (ImGui::Button("Decode Map Name")) {
        GW::AreaInfo* map = GW::Map::GetMapInfo((GW::Constants::MapID)map_id);
        if (map) {
            wchar_t buf[8] = { 0 };
            if (GW::UI::UInt32ToEncStr(map->name_id, buf, 8)) {
                int offset = 0;
                for (size_t i = 0; buf[i]; i++) {
                    offset += sprintf(&encoded[offset], offset > 0 ? " 0x%04x" : "0x%04x", buf[i]);
                }
                Decode();
            }
        }
    }
    if (decoded.size()) {
        // std::wstring str = GuiUtils::StringToWString(encoded);
        Log::LogW(L"%d %ls\n",
                  GW::UI::EncStrToUInt32(GetEncodedString().c_str()),
                  decoded.c_str());
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, decoded.c_str());
        decoded.clear();
    }
    return ImGui::End();
}
void StringDecoderWindow::Initialize()
{
    ToolboxWindow::Initialize();
}
void StringDecoderWindow::PrintEncStr(const wchar_t *enc_str)
{
    for (size_t i = 0; enc_str[i] != 0; i++) {
        printf("0x%X ", enc_str[i]);
    }
    printf("\n");
}
void StringDecoderWindow::Send()
{
    GW::Chat::SendChat('#', GetEncodedString().c_str());
}
std::wstring StringDecoderWindow::GetEncodedString()
{
    std::istringstream iss(encoded);
    std::vector<std::string> results((std::istream_iterator<std::string>(iss)),
                                     std::istream_iterator<std::string>());

    std::wstring encodedW(results.size() + 1, 0);
    size_t i = 0;
    for (i=0; i < results.size(); i++) {
        Log::Log("%s\n", results[i].c_str());
        wchar_t c;
        unsigned int lval = 0;
        int base = results[i].rfind("0x", 0) == 0 ? 0 : 16;
        if (!(GuiUtils::ParseUInt(results[i].c_str(), &lval, base) && lval < 0xffff)) {
            Log::Error("Failed to ParseUInt %s", results[i].c_str());
            return L"";
        }
        c = static_cast<wchar_t>(lval);
        encodedW[i] = c;
        printchar(encodedW[i]);
        printf("\n");
    }
    encodedW[i] = 0;
    return encodedW;
}
void StringDecoderWindow::Decode()
{
    decoded.clear();
    GW::UI::AsyncDecodeStr(GetEncodedString().c_str(), &decoded);
}
void StringDecoderWindow::DecodeFromMapId()
{
    decoded.clear();
    GW::UI::AsyncDecodeStr(GetEncodedString().c_str(), &decoded);
}
