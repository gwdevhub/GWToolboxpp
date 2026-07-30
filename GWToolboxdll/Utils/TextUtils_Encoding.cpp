#include "stdafx.h"
#include "TextUtils_Encoding.h"

namespace TextUtils {
    namespace Encoding {
        namespace {
            constexpr uint32_t REPLACEMENT_CHAR = 0xFFFD;

            uint32_t decode_utf8_codepoint(const uint8_t* data, size_t& len, size_t max_len)
            {
                len = 0;
                if (max_len == 0) {
                    return REPLACEMENT_CHAR;
                }

                uint8_t byte0 = data[0];

                if ((byte0 & 0x80) == 0) {
                    len = 1;
                    return byte0;
                }

                if ((byte0 & 0xE0) == 0xC0) {
                    if (max_len < 2 || (data[1] & 0xC0) != 0x80) {
                        len = 1;
                        return REPLACEMENT_CHAR;
                    }
                    len = 2;
                    return (((uint32_t)(byte0 & 0x1F)) << 6) | (data[1] & 0x3F);
                }

                if ((byte0 & 0xF0) == 0xE0) {
                    if (max_len < 3 || (data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) {
                        len = 1;
                        return REPLACEMENT_CHAR;
                    }
                    len = 3;
                    return (((uint32_t)(byte0 & 0x0F)) << 12) | (((uint32_t)(data[1] & 0x3F)) << 6) | (data[2] & 0x3F);
                }

                if ((byte0 & 0xF8) == 0xF0) {
                    if (max_len < 4 || (data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 || (data[3] & 0xC0) != 0x80) {
                        len = 1;
                        return REPLACEMENT_CHAR;
                    }
                    len = 4;
                    return (((uint32_t)(byte0 & 0x07)) << 18) | (((uint32_t)(data[1] & 0x3F)) << 12) | (((uint32_t)(data[2] & 0x3F)) << 6) | (data[3] & 0x3F);
                }

                len = 1;
                return REPLACEMENT_CHAR;
            }

            size_t encode_utf8_codepoint(uint32_t codepoint, uint8_t* output, size_t max_len)
            {
                if (codepoint <= 0x7F) {
                    if (max_len < 1) return 0;
                    output[0] = static_cast<uint8_t>(codepoint);
                    return 1;
                }

                if (codepoint <= 0x7FF) {
                    if (max_len < 2) return 0;
                    output[0] = static_cast<uint8_t>(0xC0 | (codepoint >> 6));
                    output[1] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
                    return 2;
                }

                if (codepoint <= 0xFFFF) {
                    if (max_len < 3) return 0;
                    output[0] = static_cast<uint8_t>(0xE0 | (codepoint >> 12));
                    output[1] = static_cast<uint8_t>(0x80 | ((codepoint >> 6) & 0x3F));
                    output[2] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
                    return 3;
                }

                if (codepoint <= 0x10FFFF) {
                    if (max_len < 4) return 0;
                    output[0] = static_cast<uint8_t>(0xF0 | (codepoint >> 18));
                    output[1] = static_cast<uint8_t>(0x80 | ((codepoint >> 12) & 0x3F));
                    output[2] = static_cast<uint8_t>(0x80 | ((codepoint >> 6) & 0x3F));
                    output[3] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
                    return 4;
                }

                if (max_len < 3) return 0;
                output[0] = 0xEF;
                output[1] = 0xBF;
                output[2] = 0xBD;
                return 3;
            }
        }

        std::wstring Utf8ToWide(const std::string_view utf8_str)
        {
            if (utf8_str.empty()) {
                return {};
            }

            std::wstring result;
            result.reserve(utf8_str.size());

            const auto* data = reinterpret_cast<const uint8_t*>(utf8_str.data());
            size_t pos = 0;

            while (pos < utf8_str.size()) {
                size_t len = 0;
                uint32_t codepoint = decode_utf8_codepoint(data + pos, len, utf8_str.size() - pos);
                if (len == 0) break;

                if (codepoint <= 0xFFFF) {
                    result.push_back(static_cast<wchar_t>(codepoint));
                } else {
                    codepoint -= 0x10000;
                    result.push_back(static_cast<wchar_t>(0xD800 + (codepoint >> 10)));
                    result.push_back(static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF)));
                }

                pos += len;
            }

            return result;
        }

        std::string WideToUtf8(const std::wstring_view wide_str)
        {
            if (wide_str.empty()) {
                return {};
            }

            std::string result;
            result.reserve(wide_str.size() * 3);

            for (size_t i = 0; i < wide_str.size(); ++i) {
                uint32_t codepoint = wide_str[i];

                if ((codepoint & 0xFC00) == 0xD800 && i + 1 < wide_str.size() && (wide_str[i + 1] & 0xFC00) == 0xDC00) {
                    uint32_t high = codepoint & 0x3FF;
                    uint32_t low = wide_str[++i] & 0x3FF;
                    codepoint = 0x10000 + (high << 10) + low;
                }

                uint8_t buf[4];
                size_t len = encode_utf8_codepoint(codepoint, buf, 4);
                result.append(reinterpret_cast<const char*>(buf), len);
            }

            return result;
        }

        bool IsValidUtf8(const char* str, const size_t len)
        {
            if (!str || len == 0) {
                return true;
            }

            for (size_t i = 0; i < len; ++i) {
                const unsigned char c = static_cast<unsigned char>(str[i]);

                if (c < 0x80) {
                    continue;
                }
                if ((c & 0xE0) == 0xC0) {
                    if (i + 1 >= len || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80) {
                        return false;
                    }
                    i += 1;
                } else if ((c & 0xF0) == 0xE0) {
                    if (i + 2 >= len || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 || (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80) {
                        return false;
                    }
                    i += 2;
                } else if ((c & 0xF8) == 0xF0) {
                    if (i + 3 >= len || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 || (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80 || (static_cast<unsigned char>(str[i + 3]) & 0xC0) != 0x80) {
                        return false;
                    }
                    i += 3;
                } else {
                    return false;
                }
            }
            return true;
        }
    }
}
