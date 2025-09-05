#pragma once

#include <chrono>

namespace engine
{
    class Time
    {
    public:
        void reset()
        {
            m_last = clock::now();
        }

        float tick()
        {
            auto now = clock::now();
            std::chrono::duration<float> delta = now - m_last;
            m_last = now;
            return delta.count();
        }

    private:
        using clock = std::chrono::steady_clock;
        clock::time_point m_last = clock::now();
    };
}


