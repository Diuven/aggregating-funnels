#include <iostream>
#include <thread>
#include <latch>
#include <vector>
#include <set>
#include <algorithm>
#include <gtest/gtest.h>

#include "LPRQueue.hpp"
#include "LCRQueue.hpp"
#include "LSCQueue.hpp"
#include "FakeLCRQueue.hpp"
#include "CustomCounters.hpp"

template <class V>
using QueuesToTest = ::testing::Types<
    LCRQueue<V, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>>, LPRQueue<V, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>>,
    LSCQueue<V>, FakeLCRQueue<V>>;

template <class Q>
class QueueTest : public ::testing::Test
{
public:
    Q q{32, 32};
};

using QueuesOfInts = QueuesToTest<int>;
TYPED_TEST_SUITE(QueueTest, QueuesOfInts);

TYPED_TEST(QueueTest, Simple)
{
    TypeParam &q = this->q;
    constexpr int tid = 1;
    int x, y, z;

    EXPECT_EQ(nullptr, q.dequeue(tid));

    q.enqueue(&x, tid);
    q.enqueue(&y, tid);

    EXPECT_EQ(&x, q.dequeue(tid));
    EXPECT_EQ(&y, q.dequeue(tid));
    EXPECT_EQ(nullptr, q.dequeue(tid));

    q.enqueue(&z, tid);

    EXPECT_EQ(&z, q.dequeue(tid));
    EXPECT_EQ(nullptr, q.dequeue(tid));
}

TYPED_TEST(QueueTest, EnqDeqStress)
{
    TypeParam &q = this->q;
    constexpr int tid = 1;
    int xyz[32];

    for (uint32_t i = 0; i < 10 * 2048; ++i)
    {
        int *v = &xyz[i % 32];
        q.enqueue(v, tid);
        EXPECT_EQ(v, q.dequeue(tid)) << ">> " << i;
    }

    EXPECT_EQ(nullptr, q.dequeue(tid));
}

TYPED_TEST(QueueTest, BatchEnqDeqStress1)
{
    TypeParam &q = this->q;
    constexpr int tid = 1;
    int xyz[32];

    for (uint32_t i = 0; i < 256; ++i)
    {
        for (uint32_t j = 0; j < 128; ++j)
        {
            int *v = &xyz[j % 32];
            q.enqueue(v, tid);
            EXPECT_EQ(j + 1, q.estimateSize(0));
        }
        for (uint32_t j = 0; j < 128; ++j)
        {
            int *v = &xyz[j % 32];
            EXPECT_EQ(v, q.dequeue(tid)) << ">> " << i << ' ' << j;
            EXPECT_EQ(128 - j - 1, q.estimateSize(0));
        }
        EXPECT_EQ(nullptr, q.dequeue(tid));
        EXPECT_EQ(0, q.estimateSize(0));
    }
}

TYPED_TEST(QueueTest, BatchEnqDeqStress2)
{
    TypeParam &q = this->q;
    constexpr int tid = 1;
    int xyz[32];

    for (uint32_t i = 0; i < 10; ++i)
    {
        for (uint32_t j = 0; j < 2048; ++j)
        {
            int *v = &xyz[j % 32];
            q.enqueue(v, tid);
            EXPECT_EQ(j + 1, q.estimateSize(0)) << i << ' ' << j;
        }
        for (uint32_t j = 0; j < 2048; ++j)
        {
            int *v = &xyz[j % 32];
            EXPECT_EQ(v, q.dequeue(tid)) << ">> " << i << ' ' << j;
            EXPECT_EQ(2048 - j - 1, q.estimateSize(0)) << i << ' ' << j;
        }
        EXPECT_EQ(nullptr, q.dequeue(tid)) << i;
        EXPECT_EQ(0, q.estimateSize(0)) << i;
    }
}

struct UserData
{
    int tid;
    size_t id;

    auto operator<=>(const UserData &) const = default;
};

template <class Q>
class ConcurrentQueueTest : public QueueTest<Q>
{
};

using QueuesOfUserData = QueuesToTest<UserData>;
TYPED_TEST_SUITE(ConcurrentQueueTest, QueuesOfUserData);

TYPED_TEST(ConcurrentQueueTest, ProducerConsumer)
{
    constexpr size_t numElementsPerProducer = 400'000;
    constexpr size_t numProducers = 3;
    constexpr size_t numConsumers = 3;

    TypeParam &q = this->q;

    std::vector<std::vector<UserData>> producerData(numProducers);
    for (size_t i = 0; i < numProducers; ++i)
    {
        for (size_t j = 0; j < numElementsPerProducer; ++j)
        {
            producerData[i].push_back({static_cast<int>(i), j});
        }
    }

    std::vector<std::vector<UserData>> consumerData(numConsumers);

    int tidCnt = 0;
    std::vector<std::thread> threads;
    std::latch startBarrier{numProducers + numConsumers};
    std::latch stopBarrier{numProducers + 1};
    std::atomic<bool> stopFlag{false};

    std::transform(producerData.begin(), producerData.end(),
                   std::back_inserter(threads), [&](std::vector<UserData> &data)
                   { return std::thread([&q, &data, &startBarrier, &stopBarrier, tid = ++tidCnt]
                                        {
            startBarrier.arrive_and_wait();
            for (UserData& ud : data) {
                q.enqueue(&ud, tid);
            }
            stopBarrier.arrive_and_wait(); }); });

    std::transform(consumerData.begin(), consumerData.end(), std::back_inserter(threads), [&](std::vector<UserData> &data)
                   { return std::thread([&q, &data, &startBarrier, &stopFlag, tid = ++tidCnt]
                                        {
            startBarrier.arrive_and_wait();
            bool stop = false;
            while (true) {
                UserData* ud = q.dequeue(tid);
                if (ud)
                    data.emplace_back(*ud);
                else {
                    if (stop)
                        break;
                    stop = stopFlag.load();
                }
            } }); });

    stopBarrier.arrive_and_wait();
    stopFlag.store(true);

    for (std::thread &thread : threads)
        thread.join();

    for (std::vector<UserData> &data : consumerData)
    {
        std::stable_sort(data.begin(), data.end(), [](const auto &a, const auto &b)
                         { return a.tid < b.tid; });
        for (size_t i = 1; i < data.size(); ++i)
        {
            const UserData &d1 = data[i - 1];
            const UserData &d2 = data[i];
            if (d1.tid == d2.tid)
            {
                EXPECT_LT(d1.id, d2.id) << ">> " << d1.tid << ' ' << d1.id << ' ' << d2.id;
            }
        }
    }

    std::multiset<UserData> prodsJoined;
    for (const auto &data : producerData)
        prodsJoined.insert(data.begin(), data.end());
    std::multiset<UserData> consJoined;
    for (const auto &data : consumerData)
        consJoined.insert(data.begin(), data.end());
    EXPECT_EQ(prodsJoined.size(), consJoined.size());
    EXPECT_EQ(prodsJoined, consJoined);
}

template <class V, size_t R>
using TantrumQueuesToTest = ::testing::Types<
    CRQueue<V, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>, false, R, true>, PRQueue<V, SIMPLE_ATOMIC_COUNTER::SimpleAtomicCounter<uint64_t>, false, R, true>>;

template <class Q>
class TantrumQueueTests : public ::testing::Test
{
public:
    Q q{0, 32, 32};
};

using TantrumQueuesOfInts = TantrumQueuesToTest<int, 2>;
TYPED_TEST_SUITE(TantrumQueueTests, TantrumQueuesOfInts);

TYPED_TEST(TantrumQueueTests, Simple)
{
    TypeParam &q = this->q;
    int x[4];

    EXPECT_EQ(true, q.enqueue(&x[0], 0));
    EXPECT_EQ(true, q.enqueue(&x[1], 0));
    EXPECT_EQ(&x[0], q.dequeue(0));
    EXPECT_EQ(true, q.enqueue(&x[2], 0));
    EXPECT_EQ(&x[1], q.dequeue(0));
    EXPECT_EQ(&x[2], q.dequeue(0));
    EXPECT_EQ(true, q.enqueue(&x[3], 0));
    EXPECT_EQ(&x[3], q.dequeue(0));
}

TYPED_TEST(TantrumQueueTests, Close)
{
    TypeParam &q = this->q;
    int x[4];

    EXPECT_EQ(true, q.enqueue(&x[0], 0));
    EXPECT_EQ(true, q.enqueue(&x[1], 0));
    EXPECT_EQ(&x[0], q.dequeue(0));
    EXPECT_EQ(true, q.enqueue(&x[2], 0));
    EXPECT_EQ(false, q.enqueue(&x[3], 0));
    EXPECT_EQ(false, q.enqueue(&x[0], 0));
    EXPECT_EQ(&x[1], q.dequeue(0));
    EXPECT_EQ(false, q.enqueue(&x[0], 0));
    EXPECT_EQ(&x[2], q.dequeue(0));
    EXPECT_EQ(nullptr, q.dequeue(0));
    EXPECT_EQ(false, q.enqueue(&x[0], 0));
    EXPECT_EQ(nullptr, q.dequeue(0));
}
