#pragma once

#include <ctime>

/*
clock_t based timers: TIMER_DIFF(t) returns the elapsed time in milliseconds since TIMER_INIT().
*/

inline clock_t TIMER_INIT()
{
    return clock();
}

inline clock_t TIMER_DIFF(const clock_t t)
{
    return TIMER_INIT() - t;
}
