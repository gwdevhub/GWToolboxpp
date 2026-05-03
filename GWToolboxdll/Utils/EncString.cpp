#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Defines.h>
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

EncString::EncString(const uint32_t enc_string_id, const bool sanitise, const GW::Constants::Language language)
    : should_sanitise(sanitise), language_id(language)
{
    if (enc_string_id) {
        wchar_t out[8] = {0};
        if (GW::UI::UInt32ToEncStr(enc_string_id, out, _countof(out)))
            encoded_ws = out;
    }
}

EncString::EncString(EncString&& other) noexcept
    : encoded_ws(std::move(other.encoded_ws))
    , decoded_ws(std::move(other.decoded_ws))
    , decoded_s(std::move(other.decoded_s))
    , decoded(other.decoded)
    , should_sanitise(other.should_sanitise)
    , sanitise_cb(std::move(other.sanitise_cb))
    , language_id(other.language_id)
    , pending_ctx_(other.pending_ctx_)
{
    if (pending_ctx_) pending_ctx_->owner = this;
    other.pending_ctx_ = nullptr;
    other.decoded = false;
}

EncString& EncString::operator=(EncString&& other) noexcept
{
    if (this != &other) {
        AbandonDecode();
        encoded_ws = std::move(other.encoded_ws);
        decoded_ws = std::move(other.decoded_ws);
        decoded_s = std::move(other.decoded_s);
        decoded = other.decoded;
        should_sanitise = other.should_sanitise;
        sanitise_cb = std::move(other.sanitise_cb);
        language_id = other.language_id;
        pending_ctx_ = other.pending_ctx_;
        if (pending_ctx_) pending_ctx_->owner = this;
        other.pending_ctx_ = nullptr;
        other.decoded = false;
    }
    return *this;
}

void EncString::StartDecode() const
{
    if (!decoded && !pending_ctx_ && !encoded_ws.empty()) {
        pending_ctx_ = new DecodeContext{const_cast<EncString*>(this), encoded_ws};
        auto* ctx = pending_ctx_;
        GW::GameThread::Enqueue([ctx] {
            if (ctx->owner) {
                GW::UI::AsyncDecodeStr(ctx->encoded_copy.c_str(), OnStringDecoded, ctx, ctx->owner->language_id);
            } else {
                delete ctx;
            }
        });
    }
}

const std::wstring& EncString::wstring() const
{
    StartDecode();
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
    StartDecode();
    if (decoded && !decoded_ws.empty() && decoded_s.empty()) {
        decoded_s = TextUtils::WStringToString(decoded_ws);
    }
    return decoded_s;
}
