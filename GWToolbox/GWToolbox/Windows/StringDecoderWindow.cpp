#include "stdafx.h"
#include <Windows.h>
#include <bitset>

#include "logger.h"

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include "GuiUtils.h"

#include "StringDecoderWindow.h"

static void printchar(wchar_t c) {
    if (c >= L' ' && c <= L'~') {
        printf("%lc", c);
    }
    else {
        printf("0x%X ", c);
    }
}
void StringDecoderWindow::Draw(IDirect3DDevice9* pDevice) {
    if (!visible)
        return;
    ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(256, 128), ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
    ImGui::InputText("Encoded string:", encoded, 2048);
    if (ImGui::Button("Decode")) {
        Decode();
    }
    if (decoded.size()) {
		//std::wstring str = GuiUtils::StringToWString(encoded);
        Log::LogW(L"%d %ls\n",GW::UI::EncStrToUInt32(GetEncodedString().c_str()), decoded.c_str());
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, decoded.c_str());
        decoded.clear();
    }
    return ImGui::End();
}
void StringDecoderWindow::Initialize() {
    ToolboxWindow::Initialize();
}
void StringDecoderWindow::PrintEncStr(const wchar_t* enc_str) {
    char enc_str_chars[256] = { 0 };
    for (size_t i = 0; enc_str[i] != 0; i++) {
        printf("0x%X ", enc_str[i]);
    }
    printf("\n");
}
void StringDecoderWindow::Send() {
    GW::Chat::SendChat('#', GetEncodedString().c_str());
}
std::wstring StringDecoderWindow::GetEncodedString() {
    std::istringstream iss(encoded);
    std::vector<std::string> results((std::istream_iterator<std::string>(iss)),
        std::istream_iterator<std::string>());

    unsigned long hex_value = 0;
    std::wstring encodedW;

    for (size_t i = 0; i < results.size(); i++) {
        Log::Log("%s\n", results[i]);
        encodedW[i] = std::strtoul(results[i].c_str(), 0, 16);
        printchar(encodedW[i]);
        printf("\n");
    }
    return encodedW;
}
void StringDecoderWindow::Decode() {
    decoded.clear();
    try {
        GW::UI::AsyncDecodeStr(GetEncodedString().c_str(), &decoded);
    }
    catch (...) {
        Log::Error("Failed to decode; invalid.");
    }
}