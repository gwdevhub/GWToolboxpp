#pragma once

namespace GW::Constants {
    enum class Language;
}

namespace GuiUtils {
    using SanitiseCallback = std::function<std::wstring(std::wstring)>;

    class EncString final {
        struct DecodeContext {
            EncString* owner;
        };

        std::wstring encoded_ws;
        mutable std::wstring decoded_ws;
        mutable std::string decoded_s;
        mutable bool decoding = false;
        mutable bool decoded = false;
        mutable bool sanitised = false;
        SanitiseCallback sanitise_cb;
        void sanitise() const;
        void decode() const;
        GW::Constants::Language language_id = static_cast<GW::Constants::Language>(0xff);
        static void OnStringDecoded(void* param, const wchar_t* decoded);

        mutable DecodeContext* pending_ctx_ = nullptr;
        void AbandonDecode();

    public:
        // Set the language for decoding this encoded string. If the language has changed, resets the decoded result. Returns this for chaining.
        EncString* language(GW::Constants::Language l);
        [[nodiscard]] bool IsDecoding() const { return decoding && decoded_ws.empty(); };
        // Recycle this EncString by passing a new encoded string id to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        EncString* reset(uint32_t enc_string_id = 0, bool sanitise = true);
        // Recycle this EncString by passing a new string to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        EncString* reset(const wchar_t* enc_string = nullptr, bool sanitise = true);
        [[nodiscard]] const std::wstring& wstring() const;
        [[nodiscard]] const std::string& string() const;

        // Kick off async decode without waiting for the result.
        void StartDecode() const { decode(); }

        // Set a custom sanitise callback. Replaces the default tag-stripping.
        void SetSanitiseCallback(SanitiseCallback cb) { sanitise_cb = std::move(cb); }

        [[nodiscard]] const std::wstring& encoded() const
        {
            return encoded_ws;
        };

        EncString(const wchar_t* enc_string = nullptr, const bool sanitise = true)
        {
            reset(enc_string, sanitise);
        }

        EncString(const uint32_t enc_string, const bool sanitise = true)
        {
            reset(enc_string, sanitise);
        }

        EncString(const EncString&) = delete;
        EncString& operator=(const EncString&) = delete;
        ~EncString();
    };
}
