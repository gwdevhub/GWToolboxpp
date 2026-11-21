#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace WikiPage {
    using Pair = std::pair<std::string_view, std::string_view>;

    // Raw HTML fragments from a Guild Wars Wiki page; they are views into the page, which must outlive them.
    struct Captures {
        std::vector<std::string_view> skill_links;
        std::vector<std::string_view> skills_offered_links;
        std::vector<Pair> armor_ratings;
        std::vector<std::string_view> item_links;
        std::vector<std::string_view> notes;
        std::optional<std::string_view> infobox_image;
        std::vector<Pair> infobox_rows;
    };

    Captures Parse(std::string_view html);
}
