#pragma once

#include <deque>

template<typename T>
class RingBuffer
{

public:
    RingBuffer() {}


    std::size_t read(T* dest, std::size_t count)
    {
        T* p = dest;
        for (int i = 0; i < count; i++) {
            if (!m_data.empty()) {
                *p++ = m_data.front();
                m_data.pop_front();
            }
            else {
                return i;
            }
        }
    }

    std::size_t write(const T* src, std::size_t count)
    {
        const T* p = src;
        for (std::size_t i = 0; i < count; i++)
        {
            m_data.push_back(*p++);
        }
        return count;
    }

    std::size_t size() const { return m_data.size(); }


private:
    std::deque<T> m_data;
};

