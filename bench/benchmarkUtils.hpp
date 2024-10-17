
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
#include <sstream>

#include "../structures/counter/targetCounter.hpp"

int get_thread_id()
{
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

std::string get_hex_thread_id()
{
    int tid = get_thread_id();
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(8) << std::hex << tid;
    std::string tid_hex(stream.str());
    return tid_hex;
}

TargetCounter *get_target_counter(int thread_count)
{
    return new TargetCounter(thread_count);
}

std::mt19937 get_mt_generator(int seed)
{
    return std::mt19937(seed);
}

typedef std::tuple<int, int, int> TestOperation;
class OperationGenerator
{
private:
    int seed;
    int mn_key;
    int mx_key;

    // int iratio;
    int ratios_sum[4] = {100, 0, 0, 0}; // insert, erase, find, sum
    const int MAX_VALUE = 1000000;
    std::mt19937 mtg;

public:
    OperationGenerator(int seed, int mn, int mx, int iratio)
    {
        this->seed = seed;
        this->mn_key = mn;
        this->mx_key = mx;
        // this->iratio = iratio;
        ratios_sum[0] = iratio;
        ratios_sum[1] = 100;
        mtg = std::mt19937(seed);
    }

    OperationGenerator(int seed, int mn_key, int mx_key, const int ratios[])
    {
        this->seed = seed;
        this->mn_key = mn_key;
        this->mx_key = mx_key;

        ratios_sum[0] = ratios[0];
        for (int i = 1; i < 4; i++)
            ratios_sum[i] = ratios_sum[i - 1] + ratios[i];
        mtg = std::mt19937(seed);
    }

    TestOperation next()
    {
        int op_rd = mtg() % 100;
        int key = mtg() % (mx_key - mn_key) + mn_key;
        int key_2 = mtg() % (mx_key - mn_key) + mn_key;
        int value = mtg() % MAX_VALUE;

        int op = -1;
        for (int i = 0; i < 4; i++)
        {
            if (op_rd < ratios_sum[i])
            {
                op = i;
                break;
            }
        }

        if (op == 0) // insert
        {
            return TestOperation(0, key, value);
        }
        else if (op == 1) // erase
        {
            return TestOperation(1, key, -1);
        }
        else if (op == 2) // find
        {
            return TestOperation(2, key, -1);
        }
        else if (op == 3) // sum
        {
            if (key > key_2)
                std::swap(key, key_2);
            return TestOperation(3, key, key_2);
        }
        else
            throw std::runtime_error("Invalid operation");
    }

    TestOperation next_sum() // TODO : refactor
    {
        int key_l = mtg() % (mx_key - mn_key) + mn_key;
        int key_r = mtg() % (mx_key - mn_key) + mn_key;
        if (key_l > key_r)
        {
            std::swap(key_l, key_r);
        }
        return TestOperation(-1, key_l, key_r);
    }
};

class MemoryBarrier
{
public:
    std::mutex mtx;
    std::atomic<int> count_to;
    // std::condition_variable cv;
    MemoryBarrier(int count_to)
    {
        this->count_to = count_to;
    }

    void wait()
    {
        count_to--;
        while (count_to.load() > 0)
        {
            // std::cout << "Wait!" << std::endl;
            ;
        }
    }
    /*
    void wait()
    {
        std::cout << "Start waiting!" << std::endl;
        std::unique_lock<std::mutex> lock(mtx);
        count_to--;
        cv.notify_all();
        while (count_to.load() > 0)
        {
            std::cout << "Waiting CV in loop!" << std::endl;
            cv.wait(lock);
        }
        std::cout << "End waiting!" << std::endl;
    }
    */
};

class Timer
{
private:
    std::chrono::high_resolution_clock::time_point start_ts;
    std::chrono::high_resolution_clock::time_point end_ts;

public:
    Timer() {}

    void start()
    {
        start_ts = std::chrono::high_resolution_clock::now();
    }

    void stop()
    {
        end_ts = std::chrono::high_resolution_clock::now();
    }

    long long elapsed()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts).count();
    }
};
