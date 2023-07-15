#pragma once

class RateLimiter {
public:
    RateLimiter();

    bool AddTime(uint32_t cost_ms, uint32_t max_cost_ms);

private:
    uint64_t m_time;
};
