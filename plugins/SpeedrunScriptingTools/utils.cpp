#include <utils.h>

namespace 
{
    const std::string endOfStringSignifier{"ENDOFSTRING"};
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
