#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Modules/Resources.h>

#include "GuiUtils.h"
#include <Utils/TextUtils.h>

namespace {
    struct AttributeConstData {
        uint32_t name_id;
        uint32_t description_id;
        uint32_t h0008;
        uint32_t h000c;
        uint32_t h0010;
    };
    const AttributeConstData& GetAttributeInfo(GW::Constants::Attribute attribute_id) {
        static constexpr auto attribute_const_data = std::to_array<AttributeConstData>({
            {0x81e, 0x81f, 0x1, 0x5, 0x1},
            {0x820, 0x821, 0x0, 0x5, 0x2},
            {0x822, 0x823, 0x0, 0x5, 0x3},
            {0x824, 0x825, 0x0, 0x4, 0x4},
            {0x826, 0x827, 0x0, 0x4, 0x5},
            {0x82a, 0x82b, 0x0, 0x4, 0x6},
            {0x82c, 0x82d, 0x1, 0x4, 0x7},
            {0x828, 0x829, 0x0, 0x6, 0x8},
            {0x82e, 0x82f, 0x0, 0x6, 0x9},
            {0x830, 0x831, 0x0, 0x6, 0xa},
            {0x834, 0x835, 0x0, 0x6, 0xb},
            {0x836, 0x837, 0x0, 0x6, 0xc},
            {0x832, 0x833, 0x1, 0x3, 0xd},
            {0x83a, 0x83b, 0x0, 0x3, 0xe},
            {0x83e, 0x83f, 0x0, 0x3, 0xf},
            {0x83c, 0x83d, 0x0, 0x3, 0x10},
            {0x838, 0x839, 0x1, 0x1, 0x11},
            {0x840, 0x841, 0x1, 0x1, 0x12},
            {0x842, 0x843, 0x0, 0x1, 0x13},
            {0x844, 0x845, 0x0, 0x1, 0x14},
            {0x846, 0x847, 0x0, 0x1, 0x15},
            {0x848, 0x849, 0x0, 0x2, 0x16},
            {0x850, 0x851, 0x0, 0x2, 0x17},
            {0x852, 0x853, 0x1, 0x2, 0x18},
            {0x854, 0x855, 0x0, 0x2, 0x19},
            {0x856, 0x857, 0x0, 0xb, 0x1a},
            {0x84a, 0x84b, 0x0, 0xb, 0x1b},
            {0x84c, 0x84d, 0x0, 0xb, 0x1c},
            {0x84e, 0x84f, 0x0, 0x7, 0x1d},
            {0x85a, 0x85b, 0x0, 0x7, 0x1e},
            {0x85c, 0x85d, 0x0, 0x7, 0x1f},
            {0x85e, 0x85f, 0x0, 0x8, 0x20},
            {0x860, 0x861, 0x0, 0x8, 0x21},
            {0x864, 0x865, 0x0, 0x8, 0x22},
            {0x866, 0x867, 0x0, 0x7, 0x23},
            {0x858, 0x859, 0x1, 0x8, 0x24},
            {0x862, 0x863, 0x1, 0x9, 0x25},
            {0x8f20, 0x8f21, 0x0, 0x9, 0x26},
            {0x84d5, 0x84d6, 0x0, 0x9, 0x27},
            {0x901a, 0x9032, 0x0, 0x9, 0x28},
            {0x9033, 0x9034, 0x1, 0xa, 0x29},
            {0x8f22, 0x8f23, 0x0, 0xa, 0x2a},
            {0x9035, 0x9036, 0x0, 0xa, 0x2b},
            {0x9037, 0x9038, 0x0, 0xa, 0x2c},
            {0x9039, 0x903a, 0x1, 0xb, 0x2d},
            {0x1192e, 0x1192f, 0x0, 0xb, 0x2e},
            {0x187a1, 0x187a2, 0x0, 0xb, 0x2f},
            {0x187a5, 0x187a6, 0x0, 0xb, 0x30},
            {0x187a3, 0x187a4, 0x0, 0xb, 0x31},
            {0x187a7, 0x187a8, 0x0, 0xb, 0x32},
            {0x187a9, 0x187aa, 0x0, 0x635c3a70, 0x5c65646f}
        });
        return attribute_const_data[static_cast<uint32_t>(attribute_id)];
    }

    const char* GetWikiPrefix()
    {
        /*uint32_t language = GW::UI::GetPreference(GW::UI::Preference_TextLanguage);
        char* wiki_prefix = "https://wiki.guildwars.com/wiki/";
        switch (static_cast<GW::Constants::MapLanguage>(language)) {
        case GW::Constants::MapLanguage::German: // German wiki
            wiki_prefix = "https://www.guildwiki.de/wiki/";
            break;
        case GW::Constants::MapLanguage::French: // French wiki
            wiki_prefix = "https://www.gwiki.fr/w/index.php";
            break;

        }
        return wiki_prefix;*/
        return "https://wiki.guildwars.com/wiki/";
    }

    const char* GetRawWikiPrefix()
    {
        return "https://wiki.guildwars.com/index.php";
    }
}

namespace GuiUtils {

    int DecimalPlaces(float value, int max_places)
    {
        const float epsilon = 0.0001f;
        for (int i = 0; i < max_places; i++) {
            float rounded = std::round(value);
            if (std::abs(value - rounded) < epsilon) {
                return i;
            }
            value *= 10.0f;
        }
        return max_places;
    }

    void DrawSkillbar(const char* build_code, bool show_attributes) {
        GW::SkillbarMgr::SkillTemplate skill_template;
        if (!(build_code && *build_code && GW::SkillbarMgr::DecodeSkillTemplate(skill_template, build_code)))
            return;

        const float text_size = ImGui::CalcTextSize(" ").y;
        const float skill_height = text_size * 2.f;
        const auto skill_size = ImVec2(skill_height, skill_height);

        if (show_attributes) {
            std::string attributes_str;

            size_t cnt = 0;
            for (size_t i = 0; i < _countof(skill_template.attribute_values); i++) {
                if (!skill_template.attribute_values[i]) continue;
                const auto attribute_data = GetAttributeInfo(skill_template.attribute_ids[i]);
                const auto attribute_str = Resources::DecodeStringId(attribute_data.name_id)->string().c_str();
                if (!attributes_str.empty()) attributes_str += ", ";
                if (cnt > 0 && (cnt % 2) == 0) {
                    attributes_str += "\n";
                }
                attributes_str += std::format("{} {}", attribute_str, skill_template.attribute_values[i]);
                cnt++;
            }
            ImGui::TextUnformatted(attributes_str.c_str());
        }



        const auto primary_icon = Resources::GetProfessionIcon(skill_template.primary);

        auto cursor_pos = ImGui::GetCursorPos();
        ImGui::ImageCropped(*primary_icon, { text_size, text_size });

        if (skill_template.secondary != GW::Constants::Profession::None) {
            cursor_pos.y += text_size;
            ImGui::SetCursorPos(cursor_pos);
            const auto secondary_icon = Resources::GetProfessionIcon(skill_template.secondary);
            ImGui::ImageCropped(*secondary_icon, { text_size, text_size });
            cursor_pos.y -= text_size;
        }
        cursor_pos.x += text_size;
        for (auto& skill : skill_template.skills) {
            ImGui::SetCursorPos(cursor_pos);
            ImGui::ImageCropped(*Resources::GetSkillImage(skill), skill_size);
            cursor_pos.x += skill_size.x;
        }
    }

    void FlashWindow(const bool force)
    {
        FLASHWINFO flashInfo = {};
        flashInfo.cbSize = sizeof(FLASHWINFO);
        flashInfo.hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!force && GetActiveWindow() == flashInfo.hwnd) {
            return; // Already in focus
        }
        if (!flashInfo.hwnd) {
            return;
        }
        flashInfo.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
        flashInfo.uCount = 5;
        flashInfo.dwTimeout = 0;
        FlashWindowEx(&flashInfo);
    }

    void FocusWindow()
    {
        const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) {
            return;
        }
        SetForegroundWindow(hwnd);
        ShowWindow(hwnd, SW_RESTORE);
    }

    std::string WikiUrl(const std::string& url_path)
    {
        return WikiUrl(TextUtils::StringToWString(url_path));
    }

    std::string WikiUrl(const std::wstring& url_path)
    {
        // @Cleanup: Should really properly url encode the string here, but modern browsers clean up after our mess. Test with Creme Brulees.
        if (url_path.empty()) {
            return GetWikiPrefix();
        }
        char cmd[256];
        const std::string encoded = TextUtils::UrlEncode(SanitizeWikiUrl(TextUtils::WStringToString(TextUtils::RemoveDiacritics(url_path))), '_');
        snprintf(cmd, _countof(cmd), "%s%s", GetWikiPrefix(), encoded.c_str());
        return cmd;
    }

    std::string WikiTemplateUrlFromTitle(const std::string& title)
    {
        return WikiTemplateUrlFromTitle(TextUtils::StringToWString(title));
    }

    std::string WikiTemplateUrlFromTitle(const std::wstring& title)
    {
        using namespace TextUtils;
        // @Cleanup: Should really properly url encode the string here, but modern browsers clean up after our mess. Test with Creme Brulees.
        if (title.empty()) {
            return GetRawWikiPrefix();
        }
        char cmd[256];
        const std::string encoded = UrlEncode(SanitizeWikiUrl(WStringToString(RemoveDiacritics(title))), '_');
        snprintf(cmd, _countof(cmd), "%s?title=%s&action=raw", GetRawWikiPrefix(), encoded.c_str());
        return cmd;
    }

    void SearchWiki(const std::wstring& term)
    {
        using namespace TextUtils;
        if (term.empty()) {
            return;
        }
        const size_t buf_len = 512;
        auto cmd = new char[buf_len];
        const std::string encoded = UrlEncode(WStringToString(RemoveDiacritics(term)));
        ASSERT(snprintf(cmd, buf_len, "%s?search=%s", GetWikiPrefix(), encoded.c_str()) != -1);
        GW::GameThread::Enqueue([cmd] {
            SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, cmd);
            delete[] cmd;
        });
    }

    std::string SanitizeWikiUrl(std::string s)
    {
        return TextUtils::ctre_regex_replace<R"([\s_]*\[.*?\][\s_]*)", "">(s);
    }

    void OpenWiki(const std::wstring& url_path)
    {
        SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)WikiUrl(url_path).c_str());
    }

    float GetGWScaleMultiplier(const bool force)
    {
        return Resources::GetGWScaleMultiplier(force);
    }

    ImVec4& ClampRect(ImVec4& rect, const ImVec4& viewport)
    {
        float correct;
        // X axis
        if (rect.x < viewport.x) {
            correct = viewport.x - rect.x;
            rect.x += correct;
            rect.z += correct;
        }
        if (rect.z > viewport.z) {
            correct = rect.z - viewport.z;
            rect.x -= correct;
            rect.z -= correct;
        }
        // Y axis
        if (rect.y < viewport.y) {
            correct = viewport.y - rect.y;
            rect.y += correct;
            rect.w += correct;
        }
        if (rect.w > viewport.w) {
            correct = rect.w - viewport.w;
            rect.y -= correct;
            rect.w -= correct;
        }
        return rect;
    }

    float GetPartyHealthbarHeight()
    {
        const auto interfacesize =
            static_cast<GW::Constants::InterfaceSize>(GetPreference(GW::UI::EnumPreference::InterfaceSize));
        switch (interfacesize) {
            case GW::Constants::InterfaceSize::SMALL:
                return GW::Constants::HealthbarHeight::Small;
            case GW::Constants::InterfaceSize::NORMAL:
                return GW::Constants::HealthbarHeight::Normal;
            case GW::Constants::InterfaceSize::LARGE:
                return GW::Constants::HealthbarHeight::Large;
            case GW::Constants::InterfaceSize::LARGER:
                return GW::Constants::HealthbarHeight::Larger;
            default:
                return GW::Constants::HealthbarHeight::Normal;
        }
    }
    void IniToBitset(const std::string& str, std::bitset<256>& key_combo) {
        key_combo.reset();  // Clear previous data before setting bits
        std::istringstream iss(str);
        std::string token;

        while (iss >> token) {  // Read each space-separated number
            try {
                int key = std::stoi(token);  // Convert to int
                if (key >= 0 && key < 256) { // Ensure valid range
                    key_combo.set(key);
                }
            }
            catch (...) {
                // Handle conversion errors (e.g., if str contains invalid characters)
            }
        }
    }
    void BitsetToIni(const std::bitset<256>& key_combo, std::string& out_str) {
        std::ostringstream oss;
        bool first = true;

        for (uint32_t key = 0; key < 256; ++key) {
            if (key_combo[key]) {
                if (!first) {
                    oss << " ";  // Space-separated values
                }
                oss << key;  // Store key as a number
                first = false;
            }
        }

        out_str = oss.str();
    }

    size_t IniToArray(const std::string& in, std::wstring& out)
    {
        out.resize((in.size() + 1) / 5);
        size_t offset = 0, pos = 0, converted = 0;
        do {
            if (pos >= in.size()) break;
            if (pos) {
                pos++;
            }
            std::string substring(in.substr(pos, 4));
            if (!TextUtils::ParseUInt(substring.c_str(), &converted, 16)) {
                break;
            }
            if (converted > 0xFFFF) {
                break;
            }
            out[offset++] = static_cast<wchar_t>(converted);
        } while ((pos = in.find(' ', pos)) != std::string::npos && offset < out.size());
        out.resize(offset);
        return offset;
    }

    size_t IniToArray(const char* in, std::vector<std::string>& out, const char separator)
    {
        const char* found = in;
        out.clear();
        while (true) {
            if (!(found && *found)) {
                break;
            }
            const auto next_token = (char*)strchr(found, separator);
            if (next_token) {
                *next_token = 0;
            }
            out.push_back(found);
            found = nullptr;
            if (next_token) {
                found = next_token + 1;
            }
        }
        return out.size();
    }

    size_t IniToArray(const std::string& in, std::vector<uint32_t>& out)
    {
        out.resize((in.size() + 1) / 9);
        out.resize(IniToArray(in, out.data(), out.size()));
        return out.size();
    }

    size_t IniToArray(const std::string& in, uint32_t* out, const size_t out_len)
    {
        if ((in.size() + 1) / 9 > out_len) {
            return 0;
        }
        size_t offset = 0, pos = 0, converted = 0;
        do {
            if (pos >= in.size()) break;
            if (pos) {
                pos++;
            }
            std::string substring(in.substr(pos, 8));
            if (!TextUtils::ParseUInt(substring.c_str(), &converted, 16)) {
                break;
            }
            out[offset++] = converted;
        } while ((pos = in.find(' ', pos)) != std::string::npos && offset < out_len);
        while (offset < out_len) {
            out[offset++] = 0;
        }
        return offset;
    }

    bool ArrayToIni(const std::wstring& in, std::string* out)
    {
        const size_t len = in.size();
        if (!len) {
            out->clear();
            return true;
        }
        out->resize(len * 5 - 1, 0);
        char* data = out->data();
        size_t offset = 0;
        for (size_t i = 0; i < len; i++) {
            if (offset > 0) {
                data[offset++] = ' ';
            }
            offset += sprintf(&data[offset], "%04x", in[i]);
        }
        return true;
    }

    bool ArrayToIni(const uint32_t* in, const size_t in_len, std::string* out)
    {
        if (!in_len) {
            out->clear();
            return true;
        }
        out->resize(in_len * 9 - 1, 0);
        char* data = out->data();
        size_t offset = 0;
        for (size_t i = 0; i < in_len; i++) {
            if (offset > 0) {
                data[offset++] = ' ';
            }
            offset += sprintf(&data[offset], "%08x", in[i]);
        }
        return true;
    }

    EncString* EncString::reset(const uint32_t _enc_string_id, const bool sanitise)
    {
        if (_enc_string_id && encoded_ws.length()) {
            const uint32_t this_id = GW::UI::EncStrToUInt32(encoded_ws.c_str());
            if (this_id == _enc_string_id) {
                return this;
            }
        }
        reset(nullptr, sanitise);
        if (_enc_string_id) {
            wchar_t out[8] = {0};
            if (!GW::UI::UInt32ToEncStr(_enc_string_id, out, _countof(out))) {
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

    EncString* EncString::reset(const wchar_t* _enc_string, const bool sanitise)
    {
        if (_enc_string && wcscmp(_enc_string, encoded_ws.c_str()) == 0) {
            return this;
        }
        ASSERT(!decoding);
        encoded_ws.clear();
        decoded_ws.clear();
        decoded_s.clear();
        decoding = decoded = false;
        sanitised = !sanitise;
        if (_enc_string) {
            encoded_ws = _enc_string;
        }
        return this;
    }

    void EncString::decode() {
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
            decoded_ws = TextUtils::StripTags(decoded_ws);
        }
    }

    void EncString::Release()
    {
        release = true;
        if (!decoding) {
            delete this;
        }
    }

    EncString::~EncString() {
        ASSERT(!decoding);
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
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
}
