#pragma once

#include "Windows.h"
#include <cstddef>
#include <sstream>
#include <vector>
#include <array>
#include <optional>

#include "imgui.h"
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Maps.h>

template <typename T, typename U = std::enable_if_t<std::is_enum<T>::value>>
std::ostream& operator<<(std::ostream& os, T t)
{
    using ut = std::underlying_type_t<T>;
    os << static_cast<ut>(t);
    return os;
}
template <typename T, typename U = std::enable_if_t<std::is_enum<T>::value>>
std::istream& operator>>(std::istream& is, T& t)
{
    using ut = std::underlying_type_t<T>;
    ut read;
    is >> read;
    t = static_cast<T>(read);
    return is;
}

class InputStream {
public:
    InputStream(const std::string& str) : stream{str}{}
    template<typename T>
    InputStream& operator>>(T& val){
        if (!isAtSeparator())
            stream >> val;
        return *this;
    }
    int peek();
    int get();
    void proceedPastSeparator();
    operator bool() const;

private:
    bool isAtSeparator();
    std::istringstream stream;
};

class OutputStream {
public:
    OutputStream() = default;
    template <typename T>
    OutputStream& operator<<(const T& val)
    {
        stream << val << " ";
        return *this;
    }
    void writeSeparator();
    std::string str();
    operator bool() const;

private:
    std::ostringstream stream;
};


std::string readStringWithSpaces(InputStream&);
void writeStringWithSpaces(OutputStream&, const std::string& word);

std::vector<GW::Vec2f> readPositions(InputStream&);
void writePositions(OutputStream&, const std::vector<GW::Vec2f>&);

void drawPolygonSelector(std::vector<GW::Vec2f>& polygon);
bool pointIsInsidePolygon(const GW::GamePos pos, const std::vector<GW::Vec2f>& polygon);

std::string encodeString(const std::string&);
std::string decodeString(std::string&&);

void ShowHelp(const char* help);
void drawHotkeySelector(long& keyData, long& modifier, std::string& description, float selectorWidth);
std::string makeHotkeyDescription(long keyData, long modifier);

void drawSkillIDSelector(GW::Constants::SkillID& id);
void drawMapIDSelector(GW::Constants::MapID& id);

enum class Class : int { Any, Warrior, Ranger, Monk, Necro, Mesmer, Elementalist, Assassin, Ritualist, Paragon, Dervish };
enum class Status : int { Any, Dead, Alive };
enum class AgentType : int { Any, Self, PartyMember, Friendly, Hostile };
enum class Sorting : int { AgentId, ClosestToPlayer, FurthestFromPlayer, ClosestToTarget, FurthestFromTarget, LowestHp, HighestHp };
enum class HexedStatus : int { Any, NotHexed, Hexed };
enum class Channel { All, Guild, Team, Trade, Alliance, Whisper, Emote };
enum class QuestStatus : int { NotStarted, Started, Completed, Failed };
std::string_view toString(Status);
std::string_view toString(AgentType);
std::string_view toString(Sorting);
std::string_view toString(HexedStatus);
std::string_view toString(Class);
std::string_view toString(Channel channel);
std::string_view toString(QuestStatus status);

template <typename T>
void drawEnumButton(T firstValue, T lastValue, T& currentValue, int id = 0, float width = 100., std::optional<T> skipValue = std::nullopt)
{
    ImGui::PushID(id);

    if (ImGui::Button(toString(currentValue).data(), ImVec2(width, 0))) {
        ImGui::OpenPopup("Enum popup");
    }
    if (ImGui::BeginPopup("Enum popup")) {
        for (auto i = (int)firstValue; i <= (int)lastValue; ++i) {
            if (skipValue && (int)skipValue.value() == i) continue;
            if (ImGui::Selectable(toString((T)i).data())) {
                currentValue = static_cast<T>(i);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}
