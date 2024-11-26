/******************************************************************************
 * Copyright (c) 2022, Raed Romanov
 * Based on benchmark by Pedro Ramalhete and Andreia Correia:
 *
 * Copyright (c) 2016, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */
#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <barrier>
#include <string>
#include <optional>
#include <vector>
#include <set>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <regex>

#include "AdditionalWork.hpp"
#include "Stats.hpp"
#include "Forked.hpp"
#include "ThreadGroup.hpp"
#include "MetaprogrammingUtils.hpp"
#include "LCRQueue.hpp"
#include "LPRQueue.hpp"
#include "LSCQueue.hpp"
#include "FakeLCRQueue.hpp"
#include "LinkedRingQueue.hpp"

using namespace std;
using namespace chrono;

namespace bench
{
    /*
     * We want to keep ring size as compile-time constant for queue implementations, because it enables us to store
     * the array of cells in a node without additional indirection. But we also want to experiment with different
     * ring sizes. To make it possible we define ring size as a queue implementation template parameter and instantiate
     * each queue for all possible ring sizes at compile-time. Then we use a bit of metaprogramming to choose
     * the right instance at runtime.
     *
     * The drawback of this approach is a radical increase in compilation time :(
     * This drawback can be mitigated if we make possible ring sizes configurable with cmake.
     */
    using RingSizes = mpg::ParameterSet<size_t>::Of<32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384>;

    template <class V, class CNT, size_t ring_size, bool cache_remap>
    using RingQueues = mpg::TypeSet<
        LCRQueue<V, CNT, false, ring_size, cache_remap>,
        LPRQueue<V, CNT, false, ring_size, cache_remap>,
        FakeLCRQueue<V, false, ring_size, cache_remap>>;

    template <class Q, size_t ring_size>
    class RingSizeAdapter : public Q
    {
    public:
        static constexpr size_t RING_SIZE = ring_size;

        template <class... Args>
        explicit RingSizeAdapter(Args &&...args, int enqThreads, int deqThreads)
            : Q(std::forward<Args>(args)..., enqThreads, deqThreads) {}
    };

    template <class V, class CNT, size_t ring_size>
    using Queues = typename RingQueues<V, CNT, ring_size, false>::template Cat<RingQueues<V, CNT, ring_size, true>>::template Append<
        LSCQueue<V, ring_size>>;

    namespace
    {

        static void printThroughputSummary(const Stats<long double> stats, const std::string_view unit)
        {
            cout << "Mean " << unit << " = " << static_cast<uint64_t>(stats.mean)
                 << "; Stddev = " << static_cast<uint64_t>(stats.stddev) << endl;
        }

        static void printMetrics(const vector<Metrics> &metrics)
        {
            cout << "Metrics:\n";
            auto mStats = metricStats(metrics.begin(), metrics.end());
            for (auto [key, value] : mStats)
            {
                cout << key << ": mean = " << static_cast<uint64_t>(value.mean)
                     << "; stddev = " << static_cast<uint64_t>(value.stddev) << '\n';
            }
        }

        static void writeThroughputCsvHeader(std::ostream &stream)
        {
            // in (almost) JMH-compatible format for uniform postprocessing
            stream << "Benchmark,\"Param: queueType\",Threads,\"Param: additionalWork\",\"Param: ringSize\","
                   << "Score,\"Score Error\"" << endl;
        }

        static void writeThroughputCsvData(std::ostream &stream,
                                           const std::string_view benchmark, const std::string_view queue,
                                           const int numThreads, const double additionalWork, const size_t ringSize,
                                           const Stats<long double> stats)
        {
            stream << benchmark << ',' << queue << ',' << numThreads << ',' << static_cast<uint64_t>(additionalWork) << ','
                   << ringSize << ',' << static_cast<uint64_t>(stats.mean) << ',' << static_cast<uint64_t>(stats.stddev)
                   << endl;
        }

        static void writeMetricCsvData(std::ostream &stream,
                                       const std::string_view benchmark, const std::string_view metric,
                                       const std::string_view queue,
                                       const int numThreads, const double additionalWork, const size_t ringSize,
                                       const Stats<long double> stats)
        {
            stream << benchmark << ":get" << metric << ',' << queue << ',' << numThreads << ','
                   << static_cast<uint64_t>(additionalWork) << ',' << ringSize << ','
                   << fixed << setprecision(2) << stats.mean << ',' << stats.stddev
                   << endl;
        }

        static void writeMetricCsvData(std::ostream &stream,
                                       const std::string_view benchmark, const std::string_view queue,
                                       const int numThreads, const double additionalWork, const size_t ringSize,
                                       const vector<Metrics> &metrics)
        {
            auto mStats = metricStats(metrics.begin(), metrics.end());
            for (auto [key, value] : mStats)
            {
                writeMetricCsvData(stream, benchmark, key, queue, numThreads, additionalWork, ringSize, value);
            }
        }

        template <class F>
        static void executeBenchmarkSeries(F &&func, bool fork)
        {
            if (fork)
            {
                if (execute_in_fork_and_wait())
                {
                    std::forward<F>(func)();
                    std::exit(0);
                }
            }
            else
            {
                std::forward<F>(func)();
            }
        }

        static uint32_t gcd(uint32_t a, uint32_t b)
        {
            if (b == 0)
            {
                return a;
            }
            else
            {
                return gcd(b, a % b);
            }
        }

        template <class Q>
        size_t drainQueueAndCountElements(Q &queue, int tid)
        {
            size_t cnt = 0;
            while (queue.dequeue(tid) != nullptr)
                ++cnt;
            return cnt;
        }

        struct UserData
        {
            long long seq;
            int tid;

            UserData(long long lseq, int ltid)
            {
                this->seq = lseq;
                this->tid = ltid;
            }

            UserData()
            {
                this->seq = -2;
                this->tid = -2;
            }

            UserData(const UserData &other) : seq(other.seq), tid(other.tid) {}

            bool operator<(const UserData &other) const
            {
                return seq < other.seq;
            }
        };
    }

    class SymmetricBenchmarkQ
    {

    private:
        struct Result
        {
            nanoseconds nsEnq = 0ns;
            nanoseconds nsDeq = 0ns;
            long long numEnq = 0;
            long long numDeq = 0;
            long long totOpsSec = 0;

            Result() {}

            Result(const Result &other)
            {
                nsEnq = other.nsEnq;
                nsDeq = other.nsDeq;
                numEnq = other.numEnq;
                numDeq = other.numDeq;
                totOpsSec = other.totOpsSec;
            }

            bool operator<(const Result &other) const
            {
                return totOpsSec < other.totOpsSec;
            }
        };

        // Performance benchmark constants
        static const long long kNumPairsWarmup = 1'000'000LL; // Each thread does 1M iterations as warmup

        static const long long NSEC_IN_SEC = 1000000000LL;

        size_t numThreads;
        int additionalWork;
        bool needMetrics;

        void computeSecondaryMetrics(Metrics &m)
        {
            m["transfersPerNode"] = m["transfers"] / m["appendNode"];
            m["wasteToAppendNodeRatio"] = m["wasteNode"] / m["appendNode"];
        }

    public:
        SymmetricBenchmarkQ(size_t numThreads, int additionalWork, bool needMetrics)
            : numThreads(numThreads), additionalWork(additionalWork), needMetrics(needMetrics) {}

        /**
         * enqueue-dequeue pairs: in each iteration a thread executes an enqueue followed by a dequeue;
         * the benchmark executes 10^8 pairs partitioned evenly among all threads;
         */
        template <typename Q>
        vector<long double> enqDeqBenchmark(const size_t numPairs, const size_t numRuns, vector<Metrics> &metrics)
        {
            nanoseconds deltas[numThreads][numRuns];
            atomic<bool> startFlag = {false};
            Q *queue = nullptr;

            const char *conseq_ops_str = std::getenv("CONSEQ_OPS");
            int conseq_ops = conseq_ops_str ? std::stoi(conseq_ops_str) : 1;

            const char *init_size_str = std::getenv("INIT_SIZE");
            int init_size = init_size_str ? std::stoi(init_size_str) : 1;

            auto enqdeq_lambda = [this, &startFlag, &numPairs, &queue, &conseq_ops](const int tid)
            {
                // printf("thread tid = %03d spawned\n", tid);
                UserData ud(0, 0);
                while (!startFlag.load())
                {
                } // Spin until the startFlag is set
                // Warmup phase
                for (size_t iter = 0; iter < kNumPairsWarmup / numThreads; iter++)
                {
                    queue->enqueue(&ud, tid);
                    if (queue->dequeue(tid) == nullptr)
                        cout << "Error at warmup dequeueing iter=" << iter << "\n";
                }

                for (int i = 0; i < (init_size + tid) / numThreads; i++)
                    queue->enqueue(&ud, tid);

                queue->resetMetrics(tid);

                // Measurement phase
                auto startBeats = steady_clock::now();
                for (size_t iter = 0; iter < numPairs / numThreads / conseq_ops; iter++)
                {
                    for (int i = 0; i < conseq_ops; i++)
                    {
                        queue->enqueue(&ud, tid);
                        random_additional_work(additionalWork);
                    }

                    for (int i = 0; i < conseq_ops; i++)
                    {
                        if (queue->dequeue(tid) == nullptr)
                            cout << "Error at measurement dequeueing iter=" << iter << "\n";
                        random_additional_work(additionalWork);
                    }
                }
                auto stopBeats = steady_clock::now();
                return stopBeats - startBeats;
            };

            for (size_t irun = 0; irun < numRuns; irun++)
            {
                queue = new Q(numThreads, numThreads);
                ThreadGroup threads{};
                for (size_t i = 0; i < numThreads; ++i)
                    threads.threadWithResult(enqdeq_lambda, deltas[i][irun]);
                startFlag.store(true);
                threads.join();
                startFlag.store(false);

                metrics.emplace_back(queue->collectMetrics());

                delete (Q *)queue;
            }

            // Sum up all the time deltas of all threads so we can find the median run
            vector<long double> opsPerSec(numRuns);
            for (size_t irun = 0; irun < numRuns; irun++)
            {
                auto agg = 0ns;
                for (size_t i = 0; i < numThreads; ++i)
                {
                    agg += deltas[i][irun];
                }

                opsPerSec[irun] = (static_cast<long double>(numPairs * 2 * numThreads) / agg.count()) * NSEC_IN_SEC;

                ++metrics[irun]["appendNode"]; // 1 node always exists, but we reset metrics and lose it
                metrics[irun]["transfers"] += (numPairs / numThreads) * numThreads;
            }

            return opsPerSec;
        }

        template <class Q>
        void runEnqDeqBenchmark(std::ostream &csvFile, int numPairs, size_t numRuns)
        {
            cout << "##### " << Q::className() << " #####  \n";

            vector<Metrics> metrics;
            auto res = enqDeqBenchmark<Q>(numPairs, numRuns, metrics);
            Stats<long double> sts = stats(res.begin(), res.end());
            printThroughputSummary(sts, "ops/sec");

            if (needMetrics)
            {
                for (Metrics &m : metrics)
                    computeSecondaryMetrics(m);
                printMetrics(metrics);
                cout << endl;
            }

            writeThroughputCsvData(csvFile, "enqDeqPairs", Q::className(),
                                   numThreads, additionalWork, Q::RING_SIZE, sts);

            if (needMetrics)
            {
                writeMetricCsvData(csvFile, "enqDeqPairs", Q::className(),
                                   numThreads, additionalWork, Q::RING_SIZE, metrics);
            }
        }

    public:
        static void allThroughputTests(const std::string &csvFilename,
                                       const std::regex &queueFilter,
                                       const vector<int> &threadList,
                                       const vector<int> &additionalWorkList,
                                       const set<size_t> &ringSizeList,
                                       size_t numRuns, bool needMetrics, bool fork)
        {
            ofstream csvFile(csvFilename, ios::app);
            if (csvFile.tellp() == 0)
                writeThroughputCsvHeader(csvFile);

            // Enq-Deq Throughput benchmarks
            for (int additionalWork : additionalWorkList)
            {
                for (const int nThreads : threadList)
                {
                    const int numPairs = std::min(nThreads * 1'600'000, 80'000'000);

                    SymmetricBenchmarkQ bench(nThreads, additionalWork, needMetrics);
                    RingSizes::foreach ([&]<size_t ring_size>()
                                        {
                                            if (!ringSizeList.contains(ring_size))
                                                return;

                                            auto now = std::chrono::system_clock::now();
                                            std::time_t now_time = std::chrono::system_clock::to_time_t(now);

                                            cout << "\n----- Enq-Deq Benchmark   numThreads=" << nThreads << "   numPairs="
                                                 << numPairs / 1000000LL << "M" << "   additionalWork="
                                                 << static_cast<uint64_t>(additionalWork) << "   ringSize=" << ring_size
                                                 << "   datetime=" << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S")
                                                 << " -----" << endl;

                                            const char *counter_type_str = std::getenv("COUNTER_TYPE");
                                            int counter_type = counter_type_str ? std::stoi(counter_type_str) : -1;
                                            // 0 - SimpleAtomicCounter, 1 - StumpCounter, 2 - NestedStumpCounter, 3 - CombiningFunnelCounter

                                            auto bench_func = [&]<class Q>()
                                            {
                                                if (!regex_match(Q::className(), queueFilter))
                                                    return;

                                                executeBenchmarkSeries([&]()
                                                                       { bench.runEnqDeqBenchmark<Q>(csvFile, numPairs, numRuns); }, fork);
                                            };

                                            if (counter_type == 0)
                                                Queues<UserData, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            else if (counter_type == 1)
                                                Queues<UserData, SIMPLE_AGG_FUNNEL::AggFunnelCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            // else if (counter_type == 2)
                                            //     Queues<UserData, NESTED_STUMP_COUNTER::NestedStumpCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            else if (counter_type == 3)
                                                Queues<UserData, COMBINING_FUNNEL_COUNTER::CombiningFunnelCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            else
                                                throw std::runtime_error("Unknown counter type");
                                            // Queues<UserData, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>, ring_size>::foreach(bench_func); });
                                        });
                }
            }

            csvFile.close();
        }
    };

    /**
     *
     */
    class ProducerConsumerBenchmarkQ
    {

    private:
        // Performance benchmark constants
        static const long long kNumPairsWarmup = 1'000'000LL; // Each thread does 1M iterations as warmup

        static const long long NSEC_IN_SEC = 1000000000LL;

        size_t numProducers, numConsumers;
        int additionalWork;
        int producerAdditionalWork{}, consumerAdditionalWork{};
        bool balancedLoad;
        bool needMetrics;

        void computeSecondaryMetrics(Metrics &m)
        {
            m["transfersPerNode"] += m["transfers"] / m["appendNode"];
            m["wasteToAppendNodeRatio"] += m["wasteNode"] / m["appendNode"];
        }

    public:
        ProducerConsumerBenchmarkQ(size_t numProducers, size_t numConsumers,
                                   int additionalWork, bool balancedLoad,
                                   bool needMetrics)
            : numProducers(numProducers), numConsumers(numConsumers), additionalWork(additionalWork),
              balancedLoad(balancedLoad), needMetrics(needMetrics)
        {
            if (balancedLoad)
            {
                throw std::runtime_error("Balanced load is not supported");
                size_t total = numProducers + numConsumers;
                double ref = additionalWork * 2 / total;
                producerAdditionalWork = numProducers * ref;
                consumerAdditionalWork = numConsumers * ref;
            }
            else
            {
                producerAdditionalWork = additionalWork;
                consumerAdditionalWork = additionalWork;
            }
        }

        template <typename Q>
        vector<long double> producerConsumerBenchmark(const milliseconds runDuration, const size_t numRuns,
                                                      vector<Metrics> &metrics)
        {
            pair<uint32_t, uint32_t> transferredCount[numConsumers][numRuns];
            nanoseconds deltas[numRuns];
            Q *queue = nullptr;
            barrier<> barrier(numProducers + numConsumers + 1);
            std::atomic<bool> stopFlag{false};

            auto prod_lambda = [this, &stopFlag, &queue, &barrier](const int tid)
            {
                // printf("producer tid = %03d spawned\n", tid);
                UserData ud(0, 0);

                barrier.arrive_and_wait();
                // Warmup phase
                for (size_t iter = 0; iter < kNumPairsWarmup / numProducers; iter++)
                {
                    queue->enqueue(&ud, tid);
                }
                barrier.arrive_and_wait();
                // Finish warmup, consumers drain the queue

                queue->resetMetrics(tid);
                barrier.arrive_and_wait();
                // Measurement phase
                uint64_t iter = 0;
                while (!stopFlag.load())
                {
                    // in case of balances load slow down producers if the queue starts growing too much
                    // do not check size too often because it involves hazard pointers protection
                    // note, estimateSize is more or less accurate only if the queue consists of one segment
                    if ((iter & ((1ull << 5) - 1)) != 0 || !balancedLoad ||
                        queue->estimateSize(tid) < Q::RING_SIZE * 7 / 10)
                    {

                        queue->enqueue(&ud, tid);
                        ++iter;
                    }
                    random_additional_work(producerAdditionalWork);
                }
            };

            auto cons_lambda = [this, &stopFlag, &queue, &barrier](const int tid)
            {
                // printf("consumer tid = %03d spawned\n", tid - numProducers);
                UserData dummy;

                barrier.arrive_and_wait();
                // Warmup phase
                for (size_t iter = 0; iter < kNumPairsWarmup / numConsumers + 1; iter++)
                {
                    UserData *d = queue->dequeue(tid - numProducers);
                    if (d != nullptr && d->seq > 0)
                        // side effect to prevent DCE
                        cout << "This message must never appear; " << iter << "\n";
                }
                barrier.arrive_and_wait();
                // Finish warmup, drain the queue
                while (queue->dequeue(tid - numProducers) != nullptr)
                    ;

                queue->resetMetrics(tid);
                uint32_t successfulDeqCount = 0;
                uint32_t failedDeqCount = 0;
                barrier.arrive_and_wait();
                // Measurement phase
                while (!stopFlag.load())
                {
                    UserData *d = queue->dequeue(tid - numProducers);
                    if (d != nullptr)
                    {
                        ++successfulDeqCount;
                        if (d == &dummy)
                            // side effect to prevent DCE
                            cout << "This message will never appear \n";
                    }
                    else
                    {
                        ++failedDeqCount;
                    }
                    random_additional_work(consumerAdditionalWork);
                }

                return pair{successfulDeqCount, failedDeqCount};
            };

            for (size_t irun = 0; irun < numRuns; irun++)
            {
                queue = new Q(numProducers, numConsumers);
                ThreadGroup threads{};
                for (size_t i = 0; i < numProducers; ++i)
                    threads.thread(prod_lambda);
                for (size_t i = 0; i < numConsumers; ++i)
                    threads.threadWithResult(cons_lambda, transferredCount[i][irun]);

                stopFlag.store(false);
                barrier.arrive_and_wait(); // start warmup
                barrier.arrive_and_wait(); // finish warmup
                barrier.arrive_and_wait(); // start measurements
                auto startAll = steady_clock::now();
                std::this_thread::sleep_for(runDuration);
                stopFlag.store(true);
                auto endAll = steady_clock::now();

                deltas[irun] = duration_cast<nanoseconds>(endAll - startAll);

                threads.join();

                Metrics queueMetrics = queue->collectMetrics();
                queueMetrics["remainedElements"] += drainQueueAndCountElements(*queue, 0);
                metrics.emplace_back(std::move(queueMetrics));
                delete (Q *)queue;
            }

            // Sum up all the time deltas of all threads so we can find the median run
            vector<long double> transfersPerSec(numRuns);
            for (size_t irun = 0; irun < numRuns; irun++)
            {
                uint32_t totalTransfersCount = 0;
                uint32_t totalFailedDeqCount = 0;
                for (size_t i = 0; i < numConsumers; ++i)
                {
                    totalTransfersCount += transferredCount[i][irun].first;
                    totalFailedDeqCount += transferredCount[i][irun].second;
                }

                ++metrics[irun]["appendNode"]; // 1 node always exists, but we reset metrics and lose it
                metrics[irun]["transfers"] += totalTransfersCount;
                metrics[irun]["failedDequeues"] += totalFailedDeqCount;
                metrics[irun]["duration"] += deltas[irun].count();

                transfersPerSec[irun] = static_cast<long double>(totalTransfersCount * NSEC_IN_SEC) / deltas[irun].count();
            }

            return transfersPerSec;
        }

        template <class Q>
        void runProducerConsumerBenchmark(std::ostream &csvFile, milliseconds runDuration, size_t numRuns)
        {
            cout << "##### " << Q::className() << " #####  \n";

            vector<Metrics> metrics;
            auto res = producerConsumerBenchmark<Q>(runDuration, numRuns, metrics);
            Stats<long double> sts = stats(res.begin(), res.end());
            printThroughputSummary(sts, "transfers/sec");
            if (needMetrics)
            {
                for (Metrics &m : metrics)
                    computeSecondaryMetrics(m);
                printMetrics(metrics);
                cout << endl;
            }

            uint32_t prodConsGcd = gcd(numProducers, numConsumers);
            uint32_t prodRatio = numProducers / prodConsGcd;
            uint32_t consRatio = numConsumers / prodConsGcd;
            ostringstream benchName{};
            benchName << "producerConsumer[" << prodRatio << '/' << consRatio;
            if (balancedLoad)
            {
                benchName << ";balanced";
            }
            benchName << "]";

            writeThroughputCsvData(csvFile, benchName.str(), Q::className(),
                                   numProducers + numConsumers, additionalWork, Q::RING_SIZE, sts);
            if (needMetrics)
            {
                writeMetricCsvData(csvFile, benchName.str(), Q::className(),
                                   numProducers + numConsumers, additionalWork, Q::RING_SIZE, metrics);
            }
        }

    public:
        static void allThroughputTests(const std::string &csvFilename,
                                       const std::regex &queueFilter,
                                       const vector<pair<int, int>> &threadList,
                                       const vector<int> &additionalWorkList,
                                       const set<size_t> &ringSizeList,
                                       bool balancedLoad,
                                       size_t numRuns, bool needMetrics, bool fork)
        {
            ofstream csvFile(csvFilename, ios::app);
            if (csvFile.tellp() == 0)
                writeThroughputCsvHeader(csvFile);

            const milliseconds runDuration = 1000ms;

            for (int additionalWork : additionalWorkList)
            {
                for (auto numProdCons : threadList)
                {
                    const auto numProducers = numProdCons.first;
                    const auto numConsumers = numProdCons.second;
                    ProducerConsumerBenchmarkQ bench(numProducers, numConsumers, additionalWork, balancedLoad, needMetrics);
                    RingSizes::foreach ([&]<size_t ring_size>()
                                        {
                                            if (!ringSizeList.contains(ring_size))
                                                return;

                                            cout << "\n----- Producer-Consumer Benchmark   numProducers=" << numProducers
                                                 << "   numConsumers=" << numConsumers << "   runDuration=" << runDuration.count() << "ms"
                                                 << "   additionalWork=" << static_cast<uint64_t>(additionalWork) << "   balancedLoad="
                                                 << std::boolalpha << balancedLoad << "   ringSize=" << ring_size
                                                 << " -----" << endl;

                                            const char *counter_type_str = std::getenv("COUNTER_TYPE");
                                            int counter_type = counter_type_str ? std::stoi(counter_type_str) : -1;
                                            // 0 - SimpleAtomicCounter, 1 - StumpCounter, 2 - NestedStumpCounter, 3 - CombiningFunnelCounter

                                            auto bench_func = [&]<class Q>()
                                            {
                                                if (!regex_match(Q::className(), queueFilter))
                                                    return;

                                                executeBenchmarkSeries([&]()
                                                                       { bench.runProducerConsumerBenchmark<Q>(csvFile, runDuration, numRuns); }, fork);
                                            };

                                            if (counter_type == 0)
                                                Queues<UserData, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            else if (counter_type == 1)
                                                Queues<UserData, SIMPLE_AGG_FUNNEL::AggFunnelCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            // else if (counter_type == 2)
                                            //     Queues<UserData, NESTED_STUMP_COUNTER::NestedStumpCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            else if (counter_type == 3)
                                                Queues<UserData, COMBINING_FUNNEL_COUNTER::CombiningFunnelCounter<uint64_t>, ring_size>::foreach (bench_func);
                                            else
                                                throw std::runtime_error("Unknown counter type");
                                            // Queues<UserData, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>, ring_size>::foreach(bench_func); });
                                        });
                }
            }

            csvFile.close();
        }
    };

}
