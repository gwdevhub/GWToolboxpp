#pragma once

namespace GW::Constants {
    enum class Language;
}

namespace GuiUtils {
    using SanitiseCallback = std::function<std::wstring(std::wstring)>;

    // Thread safety: EncString relies on all construction, destruction, and
    // decode callbacks running on the game thread. The DecodeContext poisons
    // the owner pointer on destruction so a late callback is a harmless no-op,
    // but there is no mutex — concurrent access from other threads is unsafe.
    class EncString final {
        struct DecodeContext {
            EncString* owner;
            std::wstring encoded_copy;
        };

        std::wstring encoded_ws;
        mutable std::wstring decoded_ws;
        mutable std::string decoded_s;
        mutable bool decoded = false;
        bool should_sanitise = true;
        SanitiseCallback sanitise_cb;
        GW::Constants::Language language_id = static_cast<GW::Constants::Language>(0xff);
        static void OnStringDecoded(void* param, const wchar_t* decoded);

        mutable DecodeContext* pending_ctx_ = nullptr;
        void AbandonDecode();

    public:
        [[nodiscard]] bool IsDecoding() const { return pending_ctx_ && decoded_ws.empty(); };
        [[nodiscard]] const std::wstring& wstring() const;
        [[nodiscard]] const std::string& string() const;

        void StartDecode() const;

        // Set a custom sanitise callback. Replaces the default tag-stripping.
        void SetSanitiseCallback(SanitiseCallback cb) { sanitise_cb = std::move(cb); }

        [[nodiscard]] const std::wstring& encoded() const
        {
            return encoded_ws;
        };

        EncString() = default;

        explicit EncString(const wchar_t* enc_string, bool sanitise = true,
            GW::Constants::Language language = static_cast<GW::Constants::Language>(0xff))
            : should_sanitise(sanitise), language_id(language)
        {
            if (enc_string) encoded_ws = enc_string;
        }

        explicit EncString(uint32_t enc_string_id, bool sanitise = true,
            GW::Constants::Language language = static_cast<GW::Constants::Language>(0xff));

        EncString(const EncString&) = delete;
        EncString& operator=(const EncString&) = delete;
        EncString(EncString&& other) noexcept;
        EncString& operator=(EncString&& other) noexcept;
        ~EncString();
    };
}
