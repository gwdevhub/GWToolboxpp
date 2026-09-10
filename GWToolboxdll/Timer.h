#pragma once

#include <ctime>

inline clock_t TIMER_INIT()
{
    return clock();
}

inline clock_t TIMER_DIFF(const clock_t t)
{
    return TIMER_INIT() - t;
}
