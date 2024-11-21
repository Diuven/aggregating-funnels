#pragma once

#include <atomic>

#ifndef COUNTER_COMMON_HPP
#define COUNTER_COMMON_HPP
#include "./common.hpp"
#endif

namespace HARDWARE_ATOMIC
{
    struct ThreadLocalData
    {
        long long inc_count = 0;
    };

    template <typename T>
    class alignas(1024) HardwareCounter : public Counter<T>
    {
    private:
        int PADDING_1[32];
        std::atomic<T> val;
        int PADDING_2[32];
        std::vector<ThreadLocalData> aux_data;

    public:
        HardwareCounter() : val(0) {}
        HardwareCounter(int thread_count) : val(0)
        {
            aux_data.resize(thread_count);
        }
        HardwareCounter(T start, int thread_count) : val(start)
        {
            aux_data.resize(thread_count);
        }

        long long max_access() const
        {
            return root_access();
        }
        long long root_access() const
        {
            long long access = 0;
            for (int i = 0; i < aux_data.size(); i++)
                access += aux_data[i].inc_count;
            return access;
        }
        void update_aux_data(int thread_id, RunResult &result) const
        {
            result.root_access += aux_data[thread_id].inc_count;
        }

        T fetch_add(T diff, int thread_id)
        {
#if defined(AUX_DATA) && AUX_DATA != 0
            aux_data[thread_id].inc_count++;
#endif
            return val.fetch_add(diff);
        }

        T load() const
        {
            return val.load();
        }

        void store(T value, std::memory_order order = std::memory_order_seq_cst)
        {
            val.store(value, order);
        }

        bool compare_exchange(T &expected, T desired)
        {
            return val.compare_exchange_strong(expected, desired);
        }
    };
}