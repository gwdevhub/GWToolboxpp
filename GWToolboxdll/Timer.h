#pragma once

#include <ctime>

/*
This class makes using timers easier

clock type is clock_t

create a timer with TIMER_INIT()

find the difference in milliseconds with TIMER_DIFF(clock_t timer)
*/

inline clock_t TIMER_INIT()
{
    return clock();
}

inline clock_t TIMER_DIFF(const clock_t t)
{
    return clock() - t;
}
