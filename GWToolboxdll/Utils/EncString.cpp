#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Utils/EncString.h>
#include <Utils/TextUtils.h>

using GuiUtils::EncString;

void EncString::AbandonDecode()
{
    if (pending_ctx_) {
        pending_ctx_->owner = nullptr;
        pending_ctx_ = nullptr;
    }
}

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
    AbandonDecode();
    language_id = l;
    decoded_ws.clear();
    decoded_s.clear();
    decoded = false;
    return this;
}

EncString* EncString::reset(const wchar_t* enc_string, const bool sanitise)
{
    if (enc_string && wcscmp(enc_string, encoded_ws.c_str()) == 0) {
        return this;
    }
    AbandonDecode();
    encoded_ws.clear();
    decoded_ws.clear();
    decoded_s.clear();
    decoded = false;
    should_sanitise = sanitise;
    if (enc_string) {
        encoded_ws = enc_string;
    }
    return this;
}

void EncString::decode() const
{
    if (!decoded && !pending_ctx_ && !encoded_ws.empty()) {
        pending_ctx_ = new DecodeContext{const_cast<EncString*>(this)};
        auto* ctx = pending_ctx_;
        GW::GameThread::Enqueue([ctx] {
            if (ctx->owner) {
                GW::UI::AsyncDecodeStr(ctx->owner->encoded_ws.c_str(), OnStringDecoded, ctx, ctx->owner->language_id);
            } else {
                delete ctx;
            }
        });
    }
}

const std::wstring& EncString::wstring() const
{
    decode();
    return decoded_ws;
}

EncString::~EncString()
{
    AbandonDecode();
}

void EncString::OnStringDecoded(void* param, const wchar_t* decoded)
{
    auto* ctx = static_cast<DecodeContext*>(param);
    if (!ctx->owner) {
        delete ctx;
        return;
    }
    auto* self = ctx->owner;
    self->pending_ctx_ = nullptr;
    if (decoded && decoded[0]) {
        if (self->should_sanitise) {
            if (self->sanitise_cb) {
                self->decoded_ws = self->sanitise_cb(decoded);
            } else {
                self->decoded_ws = TextUtils::StripTags(TextUtils::Replace(decoded, L"<brx>", L"\n"));
            }
        } else {
            self->decoded_ws = decoded;
        }
    }
    self->decoded = true;
    delete ctx;
}

const std::string& EncString::string() const
{
    decode();
    if (decoded && !decoded_ws.empty() && decoded_s.empty()) {
        decoded_s = TextUtils::WStringToString(decoded_ws);
    }
    return decoded_s;
}
