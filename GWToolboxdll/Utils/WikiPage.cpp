#include "stdafx.h"

#include "WikiPage.h"

#include <algorithm>
#include <initializer_list>
#include <string>

// Plain string scanning instead of compile-time regexes: ctre made this one file cost GCC minutes and gigabytes.
// Each helper notes the regex it replaced and keeps its leftmost, lazy/greedy matching.
namespace {
    using std::string_view;
    constexpr auto npos = string_view::npos;
    // The wiki's current MediaWiki wraps level-2 headings in a div; older pages put the id on an inner span.
    constexpr string_view section_heading = R"(<div class="mw-heading mw-heading2"><h2 id=")";
    constexpr string_view legacy_section_heading = R"(<h2><span class="mw-headline" id=")";

    size_t SectionStart(const string_view html, const string_view id)
    {
        for (const auto prefix : {section_heading, legacy_section_heading}) {
            const auto heading = std::string(prefix).append(id).append("\">");
            if (const auto found = html.find(heading); found != npos) return found + heading.size();
        }
        return npos;
    }

    // (.*?)(?:terminator|...)
    std::optional<string_view> UpToFirst(const string_view html, const size_t from, const std::initializer_list<string_view> terminators)
    {
        if (from > html.size()) return std::nullopt;
        auto end = npos;
        for (const auto terminator : terminators) {
            end = std::min(end, html.find(terminator, from));
        }
        if (end == npos) return std::nullopt;
        return html.substr(from, end - from);
    }

    size_t PastTagEnd(const string_view html, const size_t from)
    {
        const auto end = html.find('>', from);
        return end == npos ? npos : end + 1;
    }

    // id="<id>">(?:.*?)<ul.*?>(.*?)(?:</ul>|next heading), or <ul(.*?) when the tag is kept
    std::optional<string_view> SectionList(const string_view html, const string_view id, const bool skip_ul_tag)
    {
        const auto ul = html.find("<ul", SectionStart(html, id));
        if (ul == npos) return std::nullopt;
        return UpToFirst(html, skip_ul_tag ? PastTagEnd(html, ul + 3) : ul + 3, {"</ul>", section_heading, legacy_section_heading});
    }

    // <a href="[^"]+" title="([^"]+)"
    std::vector<string_view> LinkTitles(const string_view html)
    {
        constexpr string_view open = R"(<a href=")";
        constexpr string_view title = R"(" title=")";
        std::vector<string_view> titles;
        for (auto at = html.find(open); at != npos; at = html.find(open, at + 1)) {
            const auto href_end = html.find('"', at + open.size());
            if (href_end == npos || href_end == at + open.size() || html.substr(href_end, title.size()) != title) continue;
            const auto value = href_end + title.size();
            const auto value_end = html.find('"', value);
            if (value_end == npos || value_end == value) continue;
            titles.push_back(html.substr(value, value_end - value));
            at = value_end;
        }
        return titles;
    }

    // <prefix(?:.*?)>(.*?)close, or <prefix>(.*?)close when the prefix already ends the tag
    std::vector<string_view> TagContents(const string_view html, const string_view prefix, const string_view close)
    {
        std::vector<string_view> contents;
        for (auto at = html.find(prefix); at != npos; at = html.find(prefix, at)) {
            const auto content = prefix.ends_with('>') ? at + prefix.size() : PastTagEnd(html, at + prefix.size());
            const auto inner = UpToFirst(html, content, {close});
            if (!inner) break;
            contents.push_back(*inner);
            at = content + inner->size() + close.size();
        }
        return contents;
    }

    constexpr bool IsArmorValueChar(const char c)
    {
        return (c >= '0' && c <= '9') || c == ' ' || c == '(' || c == ')';
    }

    // <td>.*?title="([^"]+)".*?<td>([0-9 \(\)]+).*?</td>
    std::vector<WikiPage::Pair> ArmorCells(const string_view html)
    {
        constexpr string_view title = R"(title=")";
        std::vector<WikiPage::Pair> cells;
        for (auto at = html.find("<td>"); at != npos; at = html.find("<td>", at + 1)) {
            auto title_at = html.find(title, at + 4);
            auto name_start = npos;
            auto name_end = npos;
            for (; title_at != npos; title_at = html.find(title, title_at + 1)) {
                name_start = title_at + title.size();
                name_end = html.find('"', name_start);
                if (name_end != name_start) break;
            }
            if (title_at == npos || name_end == npos) break;
            auto cell = html.find("<td>", name_end + 1);
            while (cell != npos && !(cell + 4 < html.size() && IsArmorValueChar(html[cell + 4]))) {
                cell = html.find("<td>", cell + 1);
            }
            if (cell == npos) break;
            const auto value_start = cell + 4;
            auto value_end = value_start;
            while (value_end < html.size() && IsArmorValueChar(html[value_end])) {
                value_end++;
            }
            const auto close = html.find("</td>", value_end);
            if (close == npos) break;
            cells.emplace_back(html.substr(name_start, name_end - name_start), html.substr(value_start, value_end - value_start));
            at = close + 4;
        }
        return cells;
    }

    // <table[^>]+ class="[^"]+infobox(.*?)</table>
    std::optional<string_view> Infobox(const string_view html)
    {
        constexpr string_view class_attribute = R"( class=")";
        constexpr string_view infobox = "infobox";
        for (auto at = html.find("<table"); at != npos; at = html.find("<table", at + 1)) {
            const auto tag_end = html.find('>', at + 6);
            if (tag_end == npos) return std::nullopt;
            for (auto attribute = html.rfind(class_attribute, tag_end); attribute != npos && attribute >= at + 7; attribute = attribute ? html.rfind(class_attribute, attribute - 1) : npos) {
                const auto value = attribute + class_attribute.size();
                const auto value_end = html.find('"', value);
                if (value_end == npos || value_end < value + 1 + infobox.size()) continue;
                const auto marker = html.rfind(infobox, value_end - infobox.size());
                if (marker == npos || marker < value + 1) continue;
                return UpToFirst(html, marker + infobox.size(), {"</table>"});
            }
        }
        return std::nullopt;
    }

    // <td[^>]+class="[^"]*?infobox-image.*?<a .*?href="[^"]*?File:([^"]+)
    std::optional<string_view> InfoboxImage(const string_view html)
    {
        constexpr string_view class_attribute = R"(class=")";
        constexpr string_view image_class = "infobox-image";
        constexpr string_view href = R"(href=")";
        constexpr string_view file = "File:";
        for (auto at = html.find("<td"); at != npos; at = html.find("<td", at + 1)) {
            const auto tag_end = html.find('>', at + 3);
            if (tag_end == npos) return std::nullopt;
            for (auto attribute = html.rfind(class_attribute, tag_end); attribute != npos && attribute >= at + 4; attribute = attribute ? html.rfind(class_attribute, attribute - 1) : npos) {
                const auto value = attribute + class_attribute.size();
                const auto marker = html.find(image_class, value);
                if (marker == npos || html.find('"', value) < marker) continue;
                const auto anchor = html.find("<a ", marker + image_class.size());
                if (anchor == npos) continue;
                for (auto link = html.find(href, anchor + 3); link != npos; link = html.find(href, link + 1)) {
                    const auto link_value = link + href.size();
                    const auto file_at = html.find(file, link_value);
                    if (file_at == npos || html.find('"', link_value) < file_at) continue;
                    const auto name = file_at + file.size();
                    const auto name_end = std::min(html.find('"', name), html.size());
                    if (name_end > name) return html.substr(name, name_end - name);
                }
            }
        }
        return std::nullopt;
    }

    // (?:<tr>|<tr[^>]+>).*?(?:<th>|<th[^>]+>)(.*?)</th>.*?(?:<td>|<td[^>]+>)(.*?)</td>.*?</tr>
    std::vector<WikiPage::Pair> InfoboxRows(const string_view html)
    {
        std::vector<WikiPage::Pair> rows;
        for (auto at = html.find("<tr"); at != npos; at = html.find("<tr", at)) {
            const auto header = html.find("<th", PastTagEnd(html, at + 3));
            const auto key = header == npos ? std::nullopt : UpToFirst(html, PastTagEnd(html, header + 3), {"</th>"});
            if (!key) break;
            const auto key_end = static_cast<size_t>(key->data() - html.data()) + key->size() + 5;
            const auto cell = html.find("<td", key_end);
            const auto value = cell == npos ? std::nullopt : UpToFirst(html, PastTagEnd(html, cell + 3), {"</td>"});
            if (!value) break;
            const auto row_end = html.find("</tr>", static_cast<size_t>(value->data() - html.data()) + value->size() + 5);
            if (row_end == npos) break;
            rows.emplace_back(*key, *value);
            at = row_end + 5;
        }
        return rows;
    }
}

WikiPage::Captures WikiPage::Parse(const std::string_view html)
{
    Captures page;
    if (const auto list = SectionList(html, "Skills", true)) {
        page.skill_links = LinkTitles(*list);
    }
    if (const auto section = UpToFirst(html, SectionStart(html, "Skills"), {section_heading, legacy_section_heading})) {
        for (const auto table : TagContents(*section, R"(<table class="skill-progression" )", "</table>")) {
            std::ranges::copy(LinkTitles(table), std::back_inserter(page.skill_links));
        }
    }
    if (const auto table = UpToFirst(html, SectionStart(html, "Armor_ratings"), {"</table>", section_heading, legacy_section_heading})) {
        page.armor_ratings = ArmorCells(*table);
    }
    if (const auto list = SectionList(html, "Items_dropped", false)) {
        page.item_links = LinkTitles(*list);
    }
    const auto offered = html.find("<table", SectionStart(html, "Skills_offered"));
    if (const auto table = offered == npos ? std::nullopt : UpToFirst(html, offered + 6, {"</table>"})) {
        page.skills_offered_links = LinkTitles(*table);
    }
    if (const auto list = SectionList(html, "Notes", true)) {
        page.notes = TagContents(*list, "<li>", "</li>");
    }
    if (const auto infobox = Infobox(html)) {
        page.infobox_image = InfoboxImage(*infobox);
        page.infobox_rows = InfoboxRows(*infobox);
    }
    return page;
}
