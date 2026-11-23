#include "ChoiceWindow.h"

#include <cstddef>

#include "Page.h"

namespace {

WindowOptions choiceWindowOptions(const std::string& title)
{
    WindowOptions options;
    options.cooldown = std::chrono::milliseconds::zero();
    options.title = title;
    options.width = 320;
    options.height = 150;
    options.resizable = true;
    options.autosize = true;
    options.transparent = false;
    options.centered = true;
    return options;
}

}

ChoiceWindow::ChoiceWindow(const std::string& title, const std::string& message, const std::vector<std::string>& options, Done done)
    : WebViewWindow(choiceWindowOptions(title))
    , done_(std::move(done))
{
    Json choices = Json::array();
    for (const std::string& option : options) {
        choices.push_back(option);
    }
    const Json content{{"title", title}, {"message", message}, {"options", choices}};
    handler().on("choice", [content](WebViewWindow&, const Json&, const Reply& reply) {
        reply.result(content);
    });

    handler().on("fitToPage", [](WebViewWindow& window, const Json& args, const Reply& reply) {
        window.fitToPage(args.at(0).get<int>(), args.at(1).get<int>());
        reply.result(nullptr);
    });
    handler().on("answer", [this](WebViewWindow& window, const Json& args, const Reply& reply) {
        const auto index = args.at(0).get<std::size_t>();
        if (done_) {
            done_(index);
            done_ = nullptr;
        }
        window.close();
        reply.result(nullptr);
    });
    handler().on("close", [this](WebViewWindow& window, const Json&, const Reply& reply) {
        if (done_) {
            done_(std::nullopt);
            done_ = nullptr;
        }
        window.close();
        reply.result(nullptr);
    });
    handler().on("beginMove", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.beginMove();
        reply.result(nullptr);
    });
}

std::string ChoiceWindow::html() const
{
    return pageForView("choice");
}
