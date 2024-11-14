
#include <atomic>
#include <iostream>
#include <utility>
#include <random>
#include <set>
#include <cassert>
#include <functional>
#include <thread>
#include <vector>
#include <chrono>
#include <string>
#include <mutex>
#include <queue>
#include <iomanip>

#include "../bench/benchmarkUtils.hpp"

void simple_test()
{
    TargetCounter *counter = get_target_counter(1);

    std::cout << " --- Simple test --- " << std::endl;

    long long state = counter->load();
    assert(state == 0);

    state = counter->fetch_add(1, 0);
    assert(state == 0);
    state = counter->load();
    assert(state == 1);

    state = counter->fetch_add(10, 0);
    assert(state == 1);
    state = counter->fetch_add(1, 0);
    assert(state == 11);

    state = counter->load();
    assert(state == 12);

    for (int i = 12; i <= 100; i++)
    {
        state = counter->fetch_add(1, 0);
        assert(state == i);
    }
    state = counter->load();
    assert(state == 101);

    delete counter;

    std::cout << " --- End of simple test --- " << std::endl
              << std::endl;
}

int multi_test(int thread_count, int ops_count = 100000, bool print = true)
{
    TargetCounter *counter = get_target_counter(thread_count);

    std::cout << "Running multi test with " << thread_count << " threads, " << ops_count << " operations" << std::endl
              << std::endl;

    // std::set<int> keys;
    std::vector<TestOperation> ops;
    std::set<TestOperation> op_set;

    int time_seed = std::chrono::system_clock::now().time_since_epoch().count() % 1000000;
    if (print)
        std::cout << "Seed: " << time_seed << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    const int interval_size = 1000;
    // multiple threads
    {
        if (print)
            std::cout << " --- Multi test --- " << std::endl;

        std::atomic<int> count(0);
        std::vector<long long> last_elems;
        std::vector<long long> elems_5000;
        std::mutex mtx;

        auto thread_func = [&](int id)
        {
            auto seed = time_seed * 1000 + id;
            auto gen = get_mt_generator(seed);
            int my_op_count = ops_count / thread_count;
            int local_count = 0;

            long long que[128] = {}, states_5000[1024];
            int que_size = 0, state_size = 0;

            for (int i = 0; i < my_op_count; i++)
            {
                int op = gen() % 100 == 0 ? 0 : 1; // 1% read

                if (op == 0) // read
                {
                    long long res = counter->load();
                    assert(res >= que[que_size]);
                }
                else
                {
                    long long res = counter->fetch_add(1, id);
                    local_count += 1;
                    que_size = (que_size + 1) % 128;
                    que[que_size] = res;
                    if (5000 <= res && res < 5000 + interval_size)
                    {
                        states_5000[state_size++] = res;
                    }
                }
            }
            mtx.lock();
            count += local_count;
            for (int i = 0; i < state_size; i++)
            {
                elems_5000.push_back(states_5000[i]);
            }
            for (int i = 0; i < 128; i++)
            {
                last_elems.push_back(que[i]);
            }
            mtx.unlock();
        };

        if (print)
            std::cout << "Starting threads" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        threads.reserve(1024);
        for (int i = 0; i < thread_count; i++)
        {
            threads.push_back(std::thread(thread_func, i));
        }

        for (auto &t : threads)
        {
            t.join();
        }
        end = std::chrono::high_resolution_clock::now();

        if (print)
        {
            // tree->print();
            std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl
                      << std::endl;

            std::cout << "Checking count size" << std::endl;
            std::cout << "count (tracked) : " << count.load() << std::endl;
            std::cout << "count (real) :    " << counter->load() << std::endl;
            std::cout << std::endl;
        }
        assert(count.load() == counter->load());

        // check last uniqueness
        std::sort(last_elems.begin(), last_elems.end());
        auto it = std::unique(last_elems.begin(), last_elems.end());
        assert(it == last_elems.end());
        std::cout << "Last " << last_elems.size() << " elements are unique" << std::endl;

        // check 5000-5999 uniqueness
        std::sort(elems_5000.begin(), elems_5000.end());
        it = std::unique(elems_5000.begin(), elems_5000.end());
        std::cout << "Elem 5000-5999 size: " << elems_5000.size() << std::endl;
        std::cout << "Elem 5000-5999 unique size: " << std::distance(elems_5000.begin(), it) << std::endl;
        assert(it == elems_5000.end());
        assert(elems_5000.size() == interval_size);
        assert(elems_5000[0] == 5000);
        assert(elems_5000[interval_size - 1] == 5000 + interval_size - 1);
        std::cout << "5000-5999 elements are unique" << std::endl;

        delete counter;

        if (print)
            std::cout << " --- End of multi test --- " << std::endl;
    }

    // print
    if (print)
        std::cout << " --- End of all tests --- " << std::endl
                  << std::endl
                  << std::endl;
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main(int argc, char const *argv[])
{
    simple_test();

    multi_test(1, 10000);
    multi_test(2, 100000);
    multi_test(4, 100000);
    multi_test(8, 800000);
    multi_test(16, 1600000);
    multi_test(32, 1600000);
    multi_test(48, 1600000);
    multi_test(64, 6400000);

    return 0;
}
