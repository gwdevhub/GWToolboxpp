#pragma once

namespace GW::Constants {
    enum class Language;
}

namespace GuiUtils {
    class EncString {
    protected:
        std::wstring encoded_ws;
        std::wstring decoded_ws;
        std::string decoded_s;
        bool decoding = false;
        bool decoded = false;
        bool sanitised = false;
        bool release = false;
        virtual void sanitise();
        virtual void decode();
        GW::Constants::Language language_id = static_cast<GW::Constants::Language>(0xff);
        static void OnStringDecoded(void* param, const wchar_t* decoded);

    public:
        // Set the language for decoding this encoded string. If the language has changed, resets the decoded result. Returns this for chaining.
        EncString* language(GW::Constants::Language l);
        bool IsDecoding() const { return decoding && decoded_ws.empty(); };
        // Recycle this EncString by passing a new encoded string id to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        EncString* reset(uint32_t enc_string_id = 0, bool sanitise = true);
        // Recycle this EncString by passing a new string to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        EncString* reset(const wchar_t* enc_string = nullptr, bool sanitise = true);
        std::wstring& wstring();
        std::string& string();

        // Free memory used by this EncString.
        void Release();

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

        // Disable object copying; decoded_ws is passed to GW by reference and would be bad to do this. Pass by pointer instead.
        EncString(const EncString& temp_obj) = delete;
        EncString& operator=(const EncString& temp_obj) = delete;
        ~EncString();
    };
}
