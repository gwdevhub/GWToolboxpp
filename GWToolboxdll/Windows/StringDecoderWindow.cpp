#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/GameEntities/Map.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Windows/StringDecoderWindow.h>

namespace {
    void printchar(const wchar_t c)
    {
        if (c >= L' ' && c <= L'~') {
            printf("%lc", c);
        }
        else {
            printf("0x%X ", c);
        }
    }

    void printf_indent(const size_t indent)
    {
        printf("\n");
        for (size_t l = 0; l < indent; l++) {
            printf("  ");
        }
    }
}

void StringDecoderWindow::PrintEncStr(const wchar_t*) {}

void StringDecoderWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
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
    if (decodeIt) {
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
        const GW::AreaInfo* map = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(map_id));
        if (map) {
            wchar_t buf[8] = {0};
            if (GW::UI::UInt32ToEncStr(map->name_id, buf, 8)) {
                int offset = 0;
                for (size_t i = 0; buf[i]; i++) {
                    offset += sprintf(&encoded[offset], offset > 0 ? " 0x%04x" : "0x%04x", buf[i]);
                }
                Decode();
            }
        }
    }
    if (!decoded.empty()) {
        // std::wstring str = GuiUtils::StringToWString(encoded);
        Log::LogW(L"%d %ls\n",
                  GW::UI::EncStrToUInt32(GetEncodedString().c_str()),
                  decoded.c_str());
        WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, decoded.c_str());
        //PrintEncStr(GetEncodedString().c_str());
        decoded.clear();
    }
    return ImGui::End();
}

void StringDecoderWindow::Initialize()
{
    ToolboxWindow::Initialize();
}

void StringDecoderWindow::Send() const
{
    GW::Chat::SendChat('#', GetEncodedString().c_str());
}

std::wstring StringDecoderWindow::GetEncodedString() const
{
    std::istringstream iss(encoded);
    const std::vector results((std::istream_iterator<std::string>(iss)),
                              std::istream_iterator<std::string>());

    std::wstring encodedW(results.size() + 1, 0);
    size_t i = 0;
    for (i = 0; i < results.size(); i++) {
        //Log::Log("%s\n", results[i].c_str());
        unsigned int lval = 0;
        const auto base = results[i].rfind("0x", 0) == 0 ? 0 : 16;
        if (!(GuiUtils::ParseUInt(results[i].c_str(), &lval, base) && lval < 0xffff)) {
            Log::Error("Failed to ParseUInt %s", results[i].c_str());
            return L"";
        }
        const wchar_t c = static_cast<wchar_t>(lval);
        encodedW[i] = c;
        //printchar(encodedW[i]);
        //printf("\n");
    }
    encodedW[i] = 0;
    return encodedW;
}

void StringDecoderWindow::Decode()
{
    decoded.clear();
    const auto str = GetEncodedString();
    GW::UI::AsyncDecodeStr(str.c_str(), &decoded);
    PrintEncStr(str.c_str());
}

void StringDecoderWindow::DecodeFromMapId()
{
    decoded.clear();
    GW::UI::AsyncDecodeStr(GetEncodedString().c_str(), &decoded);
}
