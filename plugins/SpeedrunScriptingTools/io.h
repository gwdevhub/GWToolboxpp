#pragma once
#include <sstream>
#include <optional>
#include <vector>

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
    InputStream(const std::string& str) : stream{str} {}
    template <typename T>
    InputStream& operator>>(T& val)
    {
        stream >> std::ws;
        if (!isAtSeparator()) stream >> val;
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

std::optional<std::string> encodeString(const std::string&);
std::optional<std::string> decodeString(std::string&&);

std::string readStringWithSpaces(InputStream&);
void writeStringWithSpaces(OutputStream&, const std::string& word);

namespace GW 
{
    struct Vec2f;
}
std::vector<GW::Vec2f> readPositions(InputStream&);
void writePositions(OutputStream&, const std::vector<GW::Vec2f>&);

