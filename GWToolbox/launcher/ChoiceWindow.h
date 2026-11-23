#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "WebViewWindow.h"

class ChoiceWindow : public WebViewWindow {
public:
    using Done = std::function<void(std::optional<std::size_t>)>;

    ChoiceWindow(const std::string& title, const std::string& message, const std::vector<std::string>& options, Done done);

protected:
    std::string html() const override;

private:
    Done done_;
};
