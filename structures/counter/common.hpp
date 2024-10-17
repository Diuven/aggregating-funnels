

#ifndef COMMON_HPP
#define COMMON_HPP
#include "../common.hpp"
#endif

#ifndef COUNTER_COMMON_HPP
#define COUNTER_COMMON_HPP
#endif

struct RunResult
{
    long long op_counts[2] = {0, 0};
    long long total_count = 0;
    long long random_work = 0;

    long long loop_count_1 = 0; // wait for other work
    long long loop_count_2 = 0; // traverse through the list

    long long root_access = 0;
};

template <typename T>
class Counter
{
public:
    virtual T fetch_add(T diff, int thread_id);
    virtual T load() const;
    virtual void store(T value, std::memory_order order = std::memory_order_seq_cst);
    virtual bool compare_exchange(T &expected, T desired);
    virtual long long max_access() const;
    virtual long long root_access() const;
    virtual void update_aux_data(int thread_id, RunResult &result) const;
};

template <typename T>
class EmptyCounter : public Counter<T>
{
public:
    EmptyCounter(int thread_count) {}
    T fetch_add(T diff, int thread_id)
    {
        return 0;
    }
    T load() const
    {
        return 0;
    }
    void store(T value, std::memory_order order = std::memory_order_seq_cst) {}
    bool compare_exchange(T &expected, T desired)
    {
        return false;
    }
    long long max_access() const
    {
        return 0;
    }
    long long root_access() const
    {
        return 0;
    }
    void update_aux_data(int thread_id, RunResult &result) const {}
};
