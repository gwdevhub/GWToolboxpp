#include <io.h>

#include <commonIncludes.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <algorithm>

namespace {
    const std::string endOfStringSignifier{"<"};
    constexpr std::array<char, 4> separatorTokens = {0x7F, 0x4, 0x5, 0x6};

    constexpr bool isInvalidDecodedCharacter(char c)
    {
        if ('a' <= c && c <= 'z')
            return false;
        else if ('A' <= c && c <= 'Z')
            return false;
        else if ('0' <= c && c <= '9')
            return false;
        return c != '_' && c != '#';
    }
} // namespace

int InputStream::peek()
{
    stream >> std::ws;
    return stream.peek();
}
int InputStream::get()
{
    stream >> std::ws;
    return stream.get();
}
InputStream::operator bool() const
{
    return stream.good();
}
void InputStream::proceedPastSeparator(int lvl)
{
    if (lvl < 1) lvl = 1;
    if (lvl > 4) lvl = 4;
    while (stream.good() && get() != separatorTokens[lvl-1]) {}
    stream >> std::ws;
}
bool InputStream::isAtSeparator()
{
    stream >> std::ws;
    return std::ranges::any_of(separatorTokens, [&](auto separatorToken) { return peek() == separatorToken; });
}
void OutputStream::writeSeparator(int lvl)
{
    if (lvl < 1) lvl = 1;
    if (lvl > 4) lvl = 4;
    stream << separatorTokens[lvl-1];
}
std::string OutputStream::str()
{
    return stream.str();
}
OutputStream::operator bool() const
{
    return stream.good();
}
std::string readStringWithSpaces(InputStream& stream)
{
    if (stream.isAtSeparator()) return "";
    std::string result;
    std::string word;
    while (stream >> word) {
        if (word == endOfStringSignifier) break;
        result += word + " ";
    }
    if (!result.empty()) {
        result.erase(result.size() - 1, 1); // last character is space
    }

    return result;
}

void writeStringWithSpaces(OutputStream& stream, const std::string& word)
{
    stream << word << endOfStringSignifier;
}

std::vector<GW::Vec2f> readPositions(InputStream& stream)
{
    int size;
    stream >> size;

    std::vector<GW::Vec2f> result;

    for (int i = 0; i < size; ++i) {
        GW::Vec2f pos;
        stream >> pos.x;
        stream >> pos.y;
        result.push_back(std::move(pos));
    }

    return result;
}
void writePositions(OutputStream& stream, const std::vector<GW::Vec2f>& positions)
{
    stream << positions.size();
    for (const auto& pos : positions) {
        stream << pos.x << pos.y;
    }
}

enum FrequentChar { B = 26 + 23, Exclamation, Quote, Underscore, Hashtag, Plus, Minus, Equals, Star, Apostroph, Ampersand, Slash, EndOfAC, EndOfString, EndOfList };
constexpr uint8_t frequentCharValue(char c)
{
    if ('a' <= c && c <= 'z') return c - 'a';
    if ('D' <= c && c <= 'Z') return c - 'D' + 26;
    switch (c) {
        case 'B':
            return FrequentChar::B;
        case '!':
            return FrequentChar::Exclamation;
        case '"':
            return FrequentChar::Quote;
        case '_':
            return FrequentChar::Underscore;
        case '#':
            return FrequentChar::Hashtag;
        case '+':
            return FrequentChar::Plus;
        case '-':
            return FrequentChar::Minus;
        case '=':
            return FrequentChar::Equals;
        case '*':
            return FrequentChar::Star;
        case '\'':
            return FrequentChar::Apostroph;
        case '&':
            return FrequentChar::Ampersand;
        case '/':
            return FrequentChar::Slash;
        case 0x7F:
            return FrequentChar::EndOfAC;
        case '<':
            return FrequentChar::EndOfString;
        case '>':
            return FrequentChar::EndOfList;
        default:
            return 0xFF; // No result
    }
}

constexpr char valueToFrequentChar(uint8_t i)
{
    if (i < 26) return 'a' + i;
    if (i < 26 + 23) return 'D' + (i - 26);
    switch (i) {
        case FrequentChar::B:
            return 'B';
        case FrequentChar::Exclamation:
            return '!';
        case FrequentChar::Quote:
            return '"';
        case FrequentChar::Underscore:
            return '_';
        case FrequentChar::Hashtag:
            return '#';
        case FrequentChar::Plus:
            return '+';
        case FrequentChar::Minus:
            return '-';
        case FrequentChar::Equals:
            return '=';
        case FrequentChar::Star:
            return '*';
        case FrequentChar::Apostroph:
            return '\'';
        case FrequentChar::EndOfAC:
            return 0x7F;
        case FrequentChar::Slash:
            return '/';
        case FrequentChar::Ampersand:
            return '&';
        case FrequentChar::EndOfString:
            return '<';
        case FrequentChar::EndOfList:
            return '>';
        default:
            throw std::runtime_error("invalid value");
    }
}

constexpr std::vector<bool> makeBitSequence(uint8_t i, int8_t length)
{
    std::vector<bool> result;
    for (int8_t shift = length - 1; shift >= 0; --shift) {
        result.push_back((i >> shift) & 0x1);
    }
    return result;
}

constexpr std::vector<bool> huffmanEncode(const std::string& str)
{
    std::vector<bool> result;
    auto append = [&](std::vector<bool>&& v) {
        result.reserve(result.size() + v.size());
        for (char c : v)
            result.push_back(c);
    };
    auto encoding = [&](char c) -> std::vector<bool> {
        switch (c) {
            case ' ':
                return {0, 0};
            case '0':
                return {0, 1, 0, 0};
            case '1':
                return {0, 1, 0, 1};
            case '2':
                return {0, 1, 1, 0, 0, 0};
            case '3':
                return {0, 1, 1, 0, 0, 1};
            case '4':
                return {0, 1, 1, 0, 1, 0};
            case '5':
                return {0, 1, 1, 0, 1, 1};
            case '6':
                return {0, 1, 1, 1, 0, 0};
            case '7':
                return {0, 1, 1, 1, 0, 1};
            case '8':
                return {0, 1, 1, 1, 1, 0};
            case '9':
                return {0, 1, 1, 1, 1, 1};
            case 'A':
                return {1, 0, 0, 0};
            case 'C':
                return {1, 0, 0, 1};
            case '.':
                return {1, 0, 1, 0};
            default:
                return {};
        }
    };
    for (char c : str) {
        // Most common characters, give a short encoding
        if (auto e = encoding(c); !e.empty()) {
            append(std::move(e));
        }
        // Reasonably frequent characters should at least not bloat length
        else if (const auto f = frequentCharValue(c); f < 64) {
            append({1, 1});
            append(makeBitSequence(f, 6));
        }
        // Write raw bit representation of least common characters
        else {
            append({1, 0, 1, 1});
            append(makeBitSequence(c, 8)); // Raw bits
        }
    }
    return result;
}

std::string splitIntoReadableChars(std::vector<bool>&& seq)
{
    while (seq.size() % 12 != 0) {
        seq.push_back(false); // Pad with spaces
    }
    auto makeInt = [&](bool a, bool b, bool c, bool d, bool e, bool f) -> uint8_t {
        return (a ? 32 : 0) + (b ? 16 : 0) + (c ? 8 : 0) + (d ? 4 : 0) + (e ? 2 : 0) + (f ? 1 : 0);
    };
    auto makeReadableChar = [](uint8_t i) -> char {
        if (i < 26) return 'a' + i;
        if (i < 2 * 26) return 'A' + (i - 26);
        if (i < 2 * 26 + 10) return '0' + (i - 2 * 26);
        if (i == 62) return '_';
        if (i == 63) return '#';
        throw std::runtime_error("invalid value");
    };

    std::string result;
    for (auto i = 0u; i < seq.size(); i += 6) {
        result += makeReadableChar(makeInt(seq[i + 0], seq[i + 1], seq[i + 2], seq[i + 3], seq[i + 4], seq[i + 5]));
    }
    return result;
}

std::optional<std::string> encodeString(const std::string& str)
{
    try {
        return splitIntoReadableChars(huffmanEncode(str));
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<bool> combineIntoBitSequence(std::string&& str)
{
    std::vector<bool> result;
    result.reserve(6 * str.size());

    auto append = [&](std::vector<bool>&& v) {
        result.reserve(result.size() + v.size());
        for (char c : v)
            result.push_back(c);
    };

    for (char c : str) {
        if ('a' <= c && c <= 'z') {
            append(makeBitSequence(c - 'a', 6));
        }
        else if ('A' <= c && c <= 'Z') {
            append(makeBitSequence(c - 'A' + 26, 6));
        }
        else if ('0' <= c && c <= '9') {
            append(makeBitSequence(c - '0' + 2 * 26, 6));
        }
        else if (c == '_') {
            append(makeBitSequence(62, 6));
        }
        else if (c == '#') {
            append(makeBitSequence(63, 6));
        }
        else {
            throw std::runtime_error("invalid value");
        }
    }

    return result;
}

std::string huffmanDecode(std::vector<bool>&& seq)
{
    std::string result = "";

    auto index = 0u;
    while (index < seq.size()) {
        if (seq[index] == 0) {
            if (seq[index + 1] == 0) {
                result += ' ';
                index += 2;
            }
            else {
                if (seq[index + 2] == 0) {
                    if (seq[index + 3] == 0) {
                        result += '0';
                        index += 4;
                    }
                    else {
                        result += '1';
                        index += 4;
                    }
                }
                else {
                    // Read 3 bit number starting at 2
                    result += '2' + 4 * seq[index + 3] + 2 * seq[index + 4] + 1 * seq[index + 5];
                    index += 6;
                }
            }
        }
        else {
            if (seq[index + 1] == 0) {
                if (seq[index + 2] == 0) {
                    if (seq[index + 3] == 0) {
                        result += 'A';
                        index += 4;
                    }
                    else {
                        result += 'C';
                        index += 4;
                    }
                }
                else {
                    if (seq[index + 3] == 0) {
                        result += '.';
                        index += 4;
                    }
                    else {
                        // Read raw 8 bit character
                        char raw = 128 * seq[index + 4] + 64 * seq[index + 5] + 32 * seq[index + 6] + 16 * seq[index + 7] + 8 * seq[index + 8] + 4 * seq[index + 9] + 2 * seq[index + 10] + 1 * seq[index + 11];
                        result += raw;
                        index += 12;
                    }
                }
            }
            else {
                uint8_t value = 32 * seq[index + 2] + 16 * seq[index + 3] + 8 * seq[index + 4] + 4 * seq[index + 5] + 2 * seq[index + 6] + 1 * seq[index + 7];
                result += valueToFrequentChar(value);
                index += 8;
            }
        }
    }

    return result;
}

std::optional<std::string> decodeString(std::string&& str)
{
    std::erase_if(str, isInvalidDecodedCharacter);
    try {
        return huffmanDecode(combineIntoBitSequence(std::move(str)));
    } catch (...) {
        return std::nullopt;
    }
}

void logMessage(std::string_view message)
{
    const auto wMessage = std::wstring{message.begin(), message.end()};
    const size_t len = 30 + wcslen(wMessage.c_str());
    auto to_send = new wchar_t[len];
    swprintf(to_send, len - 1, L"<a=1>%s</a><c=#%6X>: %s</c>", L"SST", 0xFFFFFF, wMessage.c_str());
    GW::GameThread::Enqueue([to_send] {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA2, to_send, nullptr);
        delete[] to_send;
    });
}
