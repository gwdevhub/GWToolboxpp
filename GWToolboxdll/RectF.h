#pragma once

#include <Windows.h>
#include <imgui.h>

struct RectF {
    ImVec2 top_left;
    ImVec2 bottom_right;

    constexpr RectF(const RECT& rect)
        : top_left({static_cast<float>(rect.left), static_cast<float>(rect.top)}),
          bottom_right({static_cast<float>(rect.right), static_cast<float>(rect.bottom)}) {}

    constexpr RectF(const ImVec2& _top_left, const ImVec2& _bottom_right)
        : top_left(_top_left), bottom_right(_bottom_right) {}

    RectF() = default;

    ImVec2 size() const
    {
        return {width(), height()};
    }

    float width() const
    {
        return bottom_right.x - top_left.x;
    }

    float height() const
    {
        return bottom_right.y - top_left.y;
    }

    void move_to(const ImVec2& new_top_left)
    {
        const auto diff_x = new_top_left.x - top_left.x;
        const auto diff_y = new_top_left.y - top_left.y;
        top_left.x += diff_x;
        top_left.y += diff_y;
        bottom_right.x += diff_x;
        bottom_right.y += diff_y;
    }

    void resize(const ImVec2& new_size)
    {
        const auto old_size = size();
        const auto diff_x = new_size.x - old_size.x;
        const auto diff_y = new_size.y - old_size.y;
        bottom_right.x += diff_x;
        bottom_right.y += diff_y;
    }

    bool contains(const ImVec2& point) const
    {
        return point.x >= top_left.x && point.x <= bottom_right.x &&
               point.y >= top_left.y && point.y <= bottom_right.y;
    }
};
