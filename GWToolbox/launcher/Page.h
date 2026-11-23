#pragma once

#include <string>

#include "generated/Pages.h"

inline std::string pageForView(const char* view)
{
    std::string page = kLaunchPage;
    const auto body = page.find("<body");
    if (body != std::string::npos) {
        page.insert(body + 5, std::string(" data-view=\"") + view + "\"");
    }
    return page;
}
