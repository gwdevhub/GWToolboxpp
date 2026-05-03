#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Utils/EncString.h>
#include <Utils/TextUtils.h>

using GuiUtils::EncString;

EncString* EncString::reset(const uint32_t enc_string_id, const bool sanitise)
{
    if (enc_string_id && encoded_ws.length()) {
        const uint32_t this_id = GW::UI::EncStrToUInt32(encoded_ws.c_str());
        if (this_id == enc_string_id) {
            return this;
        }
    }
    reset(nullptr, sanitise);
    if (enc_string_id) {
        wchar_t out[8] = {0};
        if (!GW::UI::UInt32ToEncStr(enc_string_id, out, _countof(out))) {
            return this;
        }
        encoded_ws = out;
    }
    return this;
}

EncString* EncString::language(const GW::Constants::Language l)
{
    if (language_id == l) {
        return this;
    }
    ASSERT(!decoding);
    language_id = l;
    decoded_ws.clear();
    decoded_s.clear();
    decoding = decoded = false;
    return this;
}

EncString* EncString::reset(const wchar_t* enc_string, const bool sanitise)
{
    if (enc_string && wcscmp(enc_string, encoded_ws.c_str()) == 0) {
        return this;
    }
    ASSERT(!decoding);
    encoded_ws.clear();
    decoded_ws.clear();
    decoded_s.clear();
    decoding = decoded = false;
    sanitised = !sanitise;
    if (enc_string) {
        encoded_ws = enc_string;
    }
    return this;
}

void EncString::decode()
{
    if (!decoded && !decoding && !encoded_ws.empty()) {
        decoding = true;
        GW::GameThread::Enqueue([&] {
            GW::UI::AsyncDecodeStr(encoded_ws.c_str(), OnStringDecoded, this, language_id);
        });
    }
}

std::wstring& EncString::wstring()
{
    decode();
    sanitise();
    return decoded_ws;
}

void EncString::sanitise()
{
    if (!sanitised && !decoded_ws.empty()) {
        sanitised = true;
        decoded_ws = TextUtils::StripTags(TextUtils::Replace(decoded_ws, L"<brx>", L"\n"));
    }
}

void EncString::Release()
{
    release = true;
    if (!decoding) {
        delete this;
    }
}

EncString::~EncString()
{
    ASSERT(!decoding);
}

void EncString::OnStringDecoded(void* param, const wchar_t* decoded)
{
    const auto context = static_cast<EncString*>(param);
    if (!(context && context->decoding && !context->decoded)) {
        return; // Not expecting a decoded string; may have been reset() before response was received.
    }
    if (decoded && decoded[0]) {
        context->decoded_ws = decoded;
    }
    context->decoded = true;
    context->decoding = false;
    if (context->release) {
        delete context;
    }
}

std::string& EncString::string()
{
    wstring();
    if (sanitised && !decoded_ws.empty() && decoded_s.empty()) {
        decoded_s = TextUtils::WStringToString(decoded_ws);
    }
    return decoded_s;
}
