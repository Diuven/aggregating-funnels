
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
#include <fstream>

#include "benchmarkUtils.hpp"

typedef std::tuple<int, long long> CounterOperation;
typedef std::tuple<long long, long long, std::vector<RunResult>> ResultsSummary;
class CounterOperationGenerator
{
private:
    int seed;
    static const int OP_TYPES = 2;

    int ratios_sum[OP_TYPES] = {100, 0}; // read, increment
    long long diff_range;
    std::mt19937 mtg;

public:
    CounterOperationGenerator(int seed, const int ratios[], long long diff_range)
    {
        this->seed = seed;
        this->diff_range = diff_range;

        ratios_sum[0] = ratios[0];
        for (int i = 1; i < OP_TYPES; i++)
            ratios_sum[i] = ratios_sum[i - 1] + ratios[i];
        mtg = std::mt19937(seed);
    }

    CounterOperation next()
    {
        int op_rd = mtg() % 100;
        int op = -1;
        for (int i = 0; i < OP_TYPES; i++)
        {
            if (op_rd < ratios_sum[i])
            {
                op = i;
                break;
            }
        }

        if (op == 0) // read
        {
            return CounterOperation(0, -1);
        }
        else if (op == 1) // insert
        {
            long long diff = (((1LL * mtg()) << 30) + mtg()) % diff_range + 1;
            // int diff = 1;
            return CounterOperation(1, diff);
        }
        else
            throw std::runtime_error("Invalid operation");
    }
};

ResultsSummary run_benchmark(Timer &timer, int thread_count, int run_milliseconds, int read_percent, int increment_percent, int additional_work, long long diff_range)
{
    TargetCounter *counter = get_target_counter(thread_count);

    const int ratios[2] = {read_percent, increment_percent};

    // make seed
    int core_seed = std::chrono::system_clock::now().time_since_epoch().count() % 1000000;
    std::cerr << "Seed: " << core_seed << std::endl;

    std::atomic<long long> mirror_counter(0);
    RunResult results[thread_count];
    {
        MemoryBarrier barrier = MemoryBarrier(thread_count + 1);
        std::atomic<bool> start(false);
        std::atomic<bool> stop(false);

        // define and run threads
        auto thread_func = [&](int id)
        {
            auto seed = core_seed * 1000 + id;
            std::string tid_hex = get_hex_thread_id();
            auto gen = CounterOperationGenerator(seed, ratios, diff_range);
            long long count = 0;

            int rd_work = 0;
            auto rd_gen = get_mt_generator(seed);

            RunResult result;
            barrier.wait();

            std::string s = "Thread " + std::to_string(id) + " = " + tid_hex + " started\n";
            std::cerr << s;

            while (!stop.load())
            {
                auto op = gen.next();
                if (std::get<0>(op) == 0)
                { // read
                    int res = counter->load();
                    rd_work += res;
                }
                else if (std::get<0>(op) == 1)
                { // increment
                    long long diff = std::get<1>(op);
                    long long res = counter->fetch_add(diff, id);
                    rd_work += res;
                    count += diff;
                }
                else
                    continue;
                result.op_counts[std::get<0>(op)]++;
                result.total_count++;

                if (additional_work > 1)
                {
                    int x = 1;
                    while (x % additional_work != 0)
                    {
                        x = rd_gen() % additional_work;
                        rd_work++;
                    }
                }
            }
            mirror_counter.fetch_add(count);
            result.random_work = rd_work;
#if defined(AUX_DATA) && AUX_DATA != 0
            counter->update_aux_data(id, result);
#endif
            results[id] = result;
        };

        std::cout << " --- Starting threads --- " << std::endl;

        std::vector<std::thread> threads;
        for (int i = 0; i < thread_count; i++)
        {
            threads.push_back(std::thread(thread_func, i));
        }

        // start running and wait
        timer.start();
        barrier.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(run_milliseconds - 5));

        // stop threads
        stop.store(true);
        for (auto &t : threads)
            t.join();
        timer.stop();

        std::cout << " --- Stopped all threads --- " << std::endl;
    }

    long long result = counter->load();
    std::cout << "Structure gave : " << result << std::endl;
    std::cout << "Verification gave : " << mirror_counter.load() << std::endl;

#if defined(AUX_DATA) && AUX_DATA != 0
    long long max_access = counter->max_access();
    long long root_access = counter->root_access();
#else
    long long max_access = 0;
    long long root_access = 0;
#endif

    std::vector<RunResult> results_vec;
    for (int i = 0; i < thread_count; i++)
    {
        results_vec.push_back(results[i]);
    }
    delete counter;

    return ResultsSummary(max_access, root_access, results_vec);
}

int main(int argc, char const *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " <thread_count> <run_milliseconds> [read_percent] [increment_percent] [additional_work] [diff_range]" << std::endl;
        return 1;
    }
    assert(argc > 2);
    int arg_pos = 0;

    int thread_count = std::stoi(argv[++arg_pos]);
    int run_milliseconds = std::stoi(argv[++arg_pos]);
    // arg pos = 2

    int read_percent = (argc > ++arg_pos) ? std::stoi(argv[arg_pos]) : 50;
    int increment_percent = (argc > ++arg_pos) ? std::stoi(argv[arg_pos]) : 100 - read_percent;
    int additional_work = (argc > ++arg_pos) ? std::stoi(argv[arg_pos]) : 32;
    long long diff_range = (argc > ++arg_pos) ? std::stoll(argv[arg_pos]) : 100LL;

    std::cout << "Thread count:        \t" << thread_count << std::endl;
    std::cout << "Run milliseconds:    \t" << run_milliseconds << std::endl;
    std::cout << "Read percent:        \t" << read_percent << std::endl;
    std::cout << "Increment percent:   \t" << increment_percent << std::endl;
    std::cout << "Additional work:     \t" << additional_work << std::endl;
    std::cout << "Diff range:          \t" << diff_range
              << std::endl;

    Timer timer;
    auto [max_access, root_access, results] = run_benchmark(
        timer, thread_count, run_milliseconds, read_percent, increment_percent, additional_work, diff_range);
    double ms = timer.elapsed();

    std::cout << " --- Benchmark results --- " << std::endl;

    std::cout << "Elapsed time: " << ms << "ms" << std::endl;
    std::cout << "Operation counts: " << std::endl;
    long long total_count = 0;
    long long total_update_count = 0;
    long long max_throughput = 0, min_throughput = 2e18;
    for (int i = 0; i < thread_count; i++)
    {
        total_count += results[i].total_count;
        total_update_count += results[i].op_counts[1];
        max_throughput = std::max(max_throughput, results[i].total_count);
        min_throughput = std::min(min_throughput, results[i].total_count);
        std::cerr << "Thread " << i << " : " << results[i].op_counts[0] << " " << results[i].op_counts[1] << " : " << results[i].total_count << " ___ " << results[i].random_work << std::endl;
    }
    double sum_squared_error = 0;
    for (int i = 0; i < thread_count; i++)
    {
        double diff = (double)results[i].total_count / ms - (double)total_count / thread_count / ms;
        sum_squared_error += diff * diff;
    }
    double std_dev = sqrt(sum_squared_error / thread_count);

    std::cout << "Total count: " << total_count << std::endl;

    std::cout << "Average throughput: " << std::fixed << std::setprecision(2) << (double)total_count / ms << " ops/ms" << std::endl;
    std::cout << "Standard deviation: " << std::fixed << std::setprecision(2) << std_dev << " ops/ms" << std::endl;

    std::cout << "Fairness: " << (double)min_throughput / max_throughput << std::endl;

    std::cout << "Root access ratio: " << (double)root_access / total_update_count << std::endl;
    std::cout << "Max access ratio : " << (double)max_access / total_update_count << std::endl;

    // write main data
    std::cout << "Writing to results_counter.csv" << std::endl;
    std::ofstream summary_file("results/counter_main.csv");
    summary_file << "thread_count,run_milliseconds,read_percent,increment_percent,additional_work,total_count,elapsed_time,max_access_ratio,root_access_ratio,fairness,stddev,throughput" << std::endl;
    summary_file << thread_count << "," << run_milliseconds << "," << read_percent << "," << increment_percent << "," << additional_work;
    summary_file << "," << total_count << "," << ms << "," << (double)max_access / total_update_count << "," << (double)root_access / total_update_count << "," << (double)min_throughput / max_throughput << "," << std_dev << "," << (double)total_count / timer.elapsed() << std::endl;
    summary_file.close();

    // write aux data
    std::cout << "Writing to results_aux.csv" << std::endl;
    std::ofstream aux_file("results/counter_aux.csv");
    aux_file << "thread_id,read_count,inc_count,total_count,loop_count_1,loop_count_2,root_access" << std::endl;
    for (int i = 0; i < thread_count; i++)
    {
        // i, read_count, inc_count, total_count, loop_count_1, loop_count_2, root_access
        RunResult &res = results[i];
        aux_file << i << "," << res.op_counts[0] << "," << res.op_counts[1] << "," << res.total_count << "," << res.loop_count_1 << "," << res.loop_count_2 << "," << res.root_access << std::endl;
    }
    aux_file.close();

    return 0;
}
