#pragma once

#include <atomic>

#include "./common.hpp"
#include <stack>
#include <mutex>

namespace COMB_FUNNEL
{
    struct alignas(128) ThreadLocalData
    {
        long long root_access = 0;
    };

    template <typename T>
    class alignas(1024) CombiningFunnelCounter : public Counter<T>
    {
    private:
    public:
        struct alignas(16) OperationType
        {
            std::atomic<T> result = -1; // -1 : empty
            std::atomic<T> sum = 0;
        };

        struct alignas(128) OperationStatus
        {
            std::atomic<int> status = 0; // instead of location. can be (0, 1, 2)
            std::atomic<OperationType *> operation = new OperationType();
            ~OperationStatus() { delete operation.load(); }
        };

        struct alignas(128) FunnelNode
        {
            std::atomic<OperationStatus *> statp = nullptr;
        };

        struct alignas(128) RandomGenerator
        {
            int seed;
            int next()
            {
                seed = (seed * 1103515245 + 12345) & 0x7fffffff;
                return seed;
            }
        };

        static const int NUM_LAYERS = 10;

        int layer_width[NUM_LAYERS];
        int thread_count;
        int layer_count;
        int PADDING_1[32] = {};

        std::vector<ThreadLocalData> aux_data;
        FunnelNode funnel[NUM_LAYERS][1 << 8]; // 256
        int PADDING_2[32] = {};

        RandomGenerator gens[1 << 8];
        int PADDING_3[32] = {};

        std::atomic<T> counter;
        int PADDING_4[32] = {};

    public:
        CombiningFunnelCounter(int thread_count) : CombiningFunnelCounter(0, thread_count) {}
        CombiningFunnelCounter(T start, int thread_count)
        {
            this->thread_count = thread_count;
            aux_data.resize(thread_count);
            counter.store(start);

            int time_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            time_seed %= 1000007;
            for (int i = 0; i < 1 << 8; i++)
                gens[i].seed = time_seed * 100 + i;

            int cur = 1;
            layer_count = 0;
            while (2 * cur < thread_count)
            {
                cur *= 2;
                layer_count++;
            }

            layer_width[layer_count] = 1; // target object
            for (int i = layer_count - 1; i >= 0; i--)
                layer_width[i] = layer_width[i + 1] * 2;

            std::cerr << "layer_count: " << layer_count << std::endl;
            std::cerr << "layer_width: ";
            for (int i = 0; i <= layer_count; i++)
                std::cerr << layer_width[i] << " ";
            std::cerr << std::endl;
        }

        long long max_access() const
        {
            // throw std::runtime_error("Not implemented");
            return 0;
        }
        long long root_access() const
        {
            // throw std::runtime_error("Not implemented");
            long long sum = 0;
            for (int i = 0; i < thread_count; i++)
                sum += aux_data[i].root_access;
            return sum;
        }
        void update_aux_data(int thread_id, RunResult &result) const
        {
            return;
        }

        T fetch_add(T diff, int thread_id)
        {
            OperationType *op = new OperationType();
            op->sum = diff;
            OperationStatus *my_status = new OperationStatus();
            my_status->status.store(1);
            my_status->operation = op;
            RandomGenerator &gen = gens[thread_id];

            std::vector<std::pair<OperationStatus *, T>> collisions;
            int _tmp = 1;

            while (true)
            {
                for (int i = 0; i < layer_count; i++)
                {
                    int r = gen.next() % layer_width[i];
                    OperationStatus *q = funnel[i][r].statp.exchange(my_status);

                    _tmp = 1;
                    if (my_status->status.compare_exchange_strong(_tmp, 2))
                    {
                        _tmp = 1;
                        if (q != nullptr && q->status.compare_exchange_strong(_tmp, 2))
                        {
                            T q_diff = q->operation.load()->sum.load();
                            collisions.push_back(std::make_pair(q, q_diff));
                            op->sum += q_diff;
                        }
                        my_status->status.store(1);
                    }
                    else
                        goto distribute;

                    for (int i = 0; i < 100; i++)
                        if (my_status->status.load() == 2)
                            goto distribute;
                }

                _tmp = 1;
                if (my_status->status.compare_exchange_strong(_tmp, 2))
                {
                    T current = counter.load();
                    T tmp = current;
                    if (counter.compare_exchange_strong(tmp, current + op->sum.load()))
                    {
                        op->result.store(current);
                        // assert(my_status->status.load() == 2);
                        aux_data[thread_id].root_access++;
                        goto distribute;
                    }
                    my_status->status.store(1);
                }
            }

        distribute:
            while (op->result.load() == -1)
                ;

            T subtotal = diff;
            T prior = op->result.load();
            for (auto &[q, qsum] : collisions)
            {
                q->operation.load()->result = prior + subtotal;
                subtotal += qsum;
            }

            long long result = op->result.load();
            return result;
        }

        T load() const
        {
            return counter.load();
        }

        void store(T value, std::memory_order order = std::memory_order_seq_cst)
        {
            throw std::runtime_error("Not implemented");
            // counter.store(value, order);
        }

        bool compare_exchange(T &expected, T desired)
        {
            return counter.compare_exchange_strong(expected, desired);
        }
    };
}