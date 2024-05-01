#include <utils.h>

#include <imgui.h>

#include <Keys.h>

#include <vector>
#include <optional>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

namespace 
{
    const std::string endOfStringSignifier{"<"};
}

void ShowHelp(const char* help)
{
    ImGui::SameLine();
    ImGui::TextDisabled("%s", "(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", help);
    }
}

std::string readStringWithSpaces(std::istringstream& stream) 
{
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

void writeStringWithSpaces(std::ostringstream& stream, const std::string& word) 
{
    stream << word << " " << endOfStringSignifier << " ";
}

std::string_view toString(Status status)
{
    switch (status) {
        case Status::Any:
            return "Any";
        case Status::Dead:
            return "Dead";
        case Status::Alive:
            return "Alive";
    }
    return "";
}

std::string toString(Class c)
{
    switch (c) {
        case Class::Warrior:
            return "Warrior";
        case Class::Ranger:
            return "Ranger";
        case Class::Monk:
            return "Monk";
        case Class::Necro:
            return "Necro";
        case Class::Mesmer:
            return "Mesmer";
        case Class::Elementalist:
            return "Elementalist";
        case Class::Assassin:
            return "Assassin";
        case Class::Ritualist:
            return "Ritualist";
        case Class::Paragon:
            return "Paragon";
        case Class::Dervish:
            return "Dervish";
        default:
            return "Any";
    }
}
std::string_view toString(AgentType type) {
    switch (type) {
        case AgentType::Any:
            return "Any";
        case AgentType::Self:
            return "Self";
        case AgentType::PartyMember:
            return "Party member";
        case AgentType::Friendly:
            return "Friendly";
        case AgentType::Hostile:
            return "Hostile";
        default: 
            return "Any";
    }
}
std::string_view toString(Sorting sorting)
{
    switch (sorting) {
        case Sorting::AgentId:
            return "Any";
        case Sorting::ClosestToPlayer:
            return "Closest to player";
        case Sorting::FurthestFromPlayer:
            return "Furthest from player";
        case Sorting::ClosestToTarget:
            return "Closest to target";
        case Sorting::FurthestFromTarget:
            return "Furthest from target";
        case Sorting::LowestHp:
            return "Lowest HP";
        case Sorting::HighestHp:
            return "Highest HP";
        default:
            return "";
    }
}
std::string_view toString(HexedStatus status) {
    switch (status) {
        case HexedStatus::Any:
            return "Any";
        case HexedStatus::NotHexed:
            return "Not hexed";
        case HexedStatus::Hexed:
            return "Hexed";
        default:
            return "";
    }
}

std::string makeHotkeyDescription(long keyData, long modifier) 
{
    char newDescription[256];
    ModKeyName(newDescription, _countof(newDescription), modifier, keyData);
    return std::string{newDescription};
}

void drawHotkeySelector(long& keyData, long& modifier, std::string& description, float selectorWidth) 
{
    ImGui::PushItemWidth(selectorWidth);
    if (ImGui::Button(description.c_str())) {
        ImGui::OpenPopup("Select Hotkey");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change hotkey");
    if (ImGui::BeginPopup("Select Hotkey")) {
        static long newkey = 0;
        char newkey_buf[256]{};
        long newmod = 0;

        ImGui::Text("Press key");
        if (ImGui::IsKeyDown(ImGuiKey_ModCtrl)) newmod |= ModKey_Control;
        if (ImGui::IsKeyDown(ImGuiKey_ModShift)) newmod |= ModKey_Shift;
        if (ImGui::IsKeyDown(ImGuiKey_ModAlt)) newmod |= ModKey_Alt;

        if (newkey == 0) { // we are looking for the key
            for (WORD i = 0; i < 512; i++) {
                switch (i) {
                    case VK_CONTROL:
                    case VK_LCONTROL:
                    case VK_RCONTROL:
                    case VK_SHIFT:
                    case VK_LSHIFT:
                    case VK_RSHIFT:
                    case VK_MENU:
                    case VK_LMENU:
                    case VK_RMENU:
                    case VK_LBUTTON:
                    case VK_RBUTTON:
                        continue;
                    default: {
                        if (::GetKeyState(i) & 0x8000) newkey = i;
                    }
                }
            }
        }
        else if (!(::GetKeyState(newkey) & 0x8000)) { // We have a key, check if it was released
            keyData = newkey;
            modifier = newmod;
            description = makeHotkeyDescription(newkey, newmod);
            newkey = 0;
            ImGui::CloseCurrentPopup();
        }

        ModKeyName(newkey_buf, _countof(newkey_buf), newmod, newkey);
        ImGui::Text("%s", newkey_buf);

        ImGui::EndPopup();
    }
}

enum FrequentChar { B = 26 + 23, Exclamation, Quote, Underscore, Hashtag, Plus, Minus, Equals, Star, Apostroph, Ampersand, Slash, EndOfAC, EndOfString, EndOfList };
constexpr uint8_t frequentCharValue(char c) 
{
    if ('a' <= c && c <= 'z') 
        return c - 'a';
    if ('D' <= c && c <= 'Z') 
        return c - 'D' + 26;
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
            return 0xFF; //No result
    }
}

constexpr char valueToFrequentChar(uint8_t i) {
    if (i < 26) 
        return 'a' + i;
    if (i < 26+23) 
        return 'D' + (i - 26);
    switch (i) 
    {
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
            assert(false);
            return 0;
    }
}

constexpr std::vector<bool> makeBitSequence(uint8_t i, int8_t length)
{
    std::vector<bool> result;
    for (int8_t shift = length - 1; shift >= 0; --shift) 
    {
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
    auto encoding = [&](char c) -> std::vector<bool>
    {
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
    for (char c : str) 
    {
        // Most common characters, give a short encoding
        if (auto e = encoding(c); !e.empty()) 
        {
            append(std::move(e));
        }
        // Reasonably frequent characters should at least not bloat length
        else if (const auto f = frequentCharValue(c); f < 64) 
        {
            append({1, 1});
            append(makeBitSequence(f, 6));
        }
        // Write raw bit representation of least common characters
        else 
        {
            append({1, 0, 1, 1});
            append(makeBitSequence(c, 8)); // Raw bits
        }
    }
    return result;
}

 std::string splitIntoReadableChars(std::vector<bool>&& seq)
{
    while (seq.size() % 12 != 0) 
    {
        seq.push_back(false); //Pad with spaces
    }
    auto makeInt = [&](bool a, bool b, bool c, bool d, bool e, bool f) -> uint8_t
    {
        return (a ? 32 : 0) + (b ? 16 : 0) + (c ? 8 : 0) + (d ? 4 : 0) + (e ? 2 : 0) + (f ? 1 : 0);
    };
    auto makeReadableChar = [](uint8_t i) -> char
    {
        if (i < 26) 
            return 'a' + i;
        if (i < 2 * 26) 
            return 'A' + (i - 26);
        if (i < 2 * 26 + 10) 
            return '0' + (i - 2 * 26);
        if (i == 62) 
            return '_';
        if (i == 63) 
            return '-';
        assert(false);
        return 0;
    };

    std::string result;
    for (auto i = 0u; i < seq.size(); i += 6) 
    {
        result += makeReadableChar(makeInt(seq[i + 0], seq[i + 1], seq[i + 2], seq[i + 3], seq[i + 4], seq[i + 5]));
    }
    return result;
}

std::string exportString(const std::string& str) 
{
    return splitIntoReadableChars(huffmanEncode(str));
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

    for (char c : str) 
    {
        if ('a' <= c && c <= 'z') 
        {
            append(makeBitSequence(c - 'a', 6));
        }
        else if ('A' <= c && c <= 'Z') 
        {
            append(makeBitSequence(c - 'A' + 26, 6));
        }
        else if ('0' <= c && c <= '9') 
        {
            append(makeBitSequence(c - '0' + 2 * 26, 6));
        }
        else if (c == '_') 
        {
            append(makeBitSequence(62, 6));
        }
        else if (c == '-') 
        {
            append(makeBitSequence(63, 6));
        }
        else 
        {
            assert(false);
        }
    }

    return result;
}

std::string huffmanDecode(std::vector<bool>&& seq) 
{
    std::string result = "";

    auto index = 0u;
    while (index < seq.size()) 
    {
        if (seq[index] == 0) 
        {
            if (seq[index + 1] == 0) 
            {
                result += ' ';
                index += 2;
            }
            else 
            {
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
                    result += '2' + 4 * seq[index+3] + 2 * seq[index+4] + 1 * seq[index+5];
                    index += 6;
                }
            }
        }
        else 
        {
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

std::string importString(std::string&& str)
{
    return huffmanDecode(combineIntoBitSequence(std::move(str)));
}

std::vector<GW::Vec2f> readPositions(std::istringstream& stream) 
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
void writePositions(std::ostringstream& stream, const std::vector<GW::Vec2f>& positions) 
{
    stream << positions.size() << " ";
    for (const auto& pos : positions) {
        stream << pos.x << " " << pos.y << " ";
    }
}

void drawPolygonSelector(std::vector<GW::Vec2f>& polygon)
{
    ImGui::Indent();

    std::optional<int> remove_point;
    ImGui::PushItemWidth(200);
    for (auto j = 0u; j < polygon.size(); j++) {
        ImGui::PushID(j);
        ImGui::Bullet();
        ImGui::InputFloat2("", reinterpret_cast<float*>(&polygon.at(j)));
        ImGui::SameLine();
        if (ImGui::Button("x")) remove_point = j;
        ImGui::PopID();
    }
    if (remove_point) {
        polygon.erase(polygon.begin() + remove_point.value());
    }
    if (ImGui::Button("Add Polygon Point")) {
        if (const auto player = GW::Agents::GetPlayerAsAgentLiving()) {
            polygon.emplace_back(player->pos.x, player->pos.y);
        }
    }

    ImGui::Unindent();
}
bool pointIsInsidePolygon(const GW::GamePos pos, const std::vector<GW::Vec2f>& points)
{
    bool b = false;
    for (auto i = 0u, j = points.size() - 1; i < points.size(); j = i++) {
        if (points[i].y >= pos.y != points[j].y >= pos.y && pos.x <= (points[j].x - points[i].x) * (pos.y - points[i].y) / (points[j].y - points[i].y) + points[i].x) {
            b = !b;
        }
    }
    return b;
}
