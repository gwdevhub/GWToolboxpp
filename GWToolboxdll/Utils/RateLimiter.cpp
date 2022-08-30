#include <stdafx.h>

#include "RateLimiter.h"

// Get more information here: https://www.codeofhonor.com/blog/using-transaction-rate-limiting-to-improve-service-reliability

RateLimiter::RateLimiter()
{
    m_time = GetTickCount();
}

bool RateLimiter::AddTime(uint32_t cost_ms, uint32_t max_cost_ms)
{
    uint64_t current_time = GetTickCount();
    if (current_time > m_time)
        m_time = current_time;

    uint64_t new_time = m_time + cost_ms;
    if ((current_time >= new_time) ||
        (new_time - current_time) >= max_cost_ms) {

        return false;
    }

    m_time = new_time;
    return true;
}
