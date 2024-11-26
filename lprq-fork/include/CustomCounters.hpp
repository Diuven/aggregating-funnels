#pragma once

#include <atomic>
#include <vector>
#include <chrono>
#include <string>
#include <cstdlib>
#include "EpochBasedReclamation.hpp"

#define FIXED_AGG_COUNT 6
static const int max_thread_count = std::thread::hardware_concurrency();

namespace SIMPLE_AGG_FUNNEL
{
    template <typename T>
    class alignas(1024) AggFunnelCounter
    {
    private:
        struct alignas(32) MappingListNode
        {
            MappingListNode *prev = nullptr;
            T child_from = 0;
            T child_to = 0;
            T root_from = -1;
        };

        struct alignas(256) Node
        {
            alignas(128) std::atomic<T> count = 0;
            alignas(128) std::atomic<T> sent = 0;
            std::atomic<MappingListNode *> mapping_list = new MappingListNode();
            ~Node() { delete mapping_list.load(); }
        };

        alignas(1024) std::atomic<T> counter = 0;
        Node child[FIXED_AGG_COUNT];
        int PADDING[32] = {};

    public:
        inline static EpochBasedReclamation<MappingListNode> *ebr = new EpochBasedReclamation<MappingListNode>(max_thread_count);
        static std::string className()
        {
            return "AggFunnelCounter";
        }

        AggFunnelCounter()
        {
        }
        ~AggFunnelCounter() {}
        AggFunnelCounter(int thread_count) : AggFunnelCounter(0, thread_count) {}
        AggFunnelCounter(T start, int thread_count)
        {
            init(start, thread_count);
        }
        void init(T start, int thread_count)
        {
            counter.store(start);
            if (thread_count > ebr->thread_count)
                ebr = new EpochBasedReclamation<MappingListNode>(thread_count);
        }

        T update(Node *child, T child_from, T child_to, int thread_id)
        {
            T root_from = counter.fetch_add(child_to - child_from);
            // MappingListNode *new_mapping = new MappingListNode();
            MappingListNode *new_mapping = ebr->get_new(thread_id);

            MappingListNode *existing_mapping = child->mapping_list.load();
            new_mapping->prev = existing_mapping;
            new_mapping->child_from = child_from;
            new_mapping->child_to = child_to;
            new_mapping->root_from = root_from;
            child->mapping_list.store(new_mapping, std::memory_order_release);
            child->sent.store(child_to, std::memory_order_release);

            ebr->retire(existing_mapping, thread_id);
            return root_from;
        }

        T get_my_root(Node *child, T my_child_from, int thread_id)
        {
            MappingListNode *mapping = child->mapping_list.load();
            while (mapping->child_from > my_child_from)
            {
                mapping = mapping->prev;
            }

            T root_from = mapping->root_from;
            T child_from = mapping->child_from;
            return root_from + my_child_from - child_from;
        }

        T fetch_add(T diff, int thread_id)
        {
            int nd_idx = thread_id % FIXED_AGG_COUNT;
            if (nd_idx < 0)
            {
                return counter.fetch_add(diff);
            }
            ebr->enterCritical(thread_id);

            Node *child = &this->child[nd_idx];
            T child_from = child->count.fetch_add(diff);
            T next_from = child->sent.load();
            while (next_from < child_from)
            {
                next_from = child->sent.load();
            }

            T root_from;
            if (child_from == next_from)
            {
                // I should do the work
                T child_to = child->count.load();
                root_from = update(child, child_from, child_to, thread_id);
            }
            else
            {
                // Mine is already done
                root_from = get_my_root(child, child_from, thread_id);
            }
            ebr->exitCritical(thread_id);
            return root_from;
        }

        T load() const
        {
            return counter.load();
        }

        void store(T value, std::memory_order order = std::memory_order_seq_cst)
        {
            counter.store(value, order);
        }

        bool compare_exchange(T &expected, T desired)
        {
            return counter.compare_exchange_strong(expected, desired);
        }
    };
}

namespace SIMPLE_ATOMIC_COUNTER
{
    template <typename T>
    class alignas(1024) SimpleAtomicCounter
    {
    private:
        int PADDING_1[32];
        std::atomic<T> val;
        int PADDING_2[32];

    public:
        static std::string className()
        {
            return "SimpleAtomicCounter";
        }
        SimpleAtomicCounter() : val(0) {}
        SimpleAtomicCounter(int thread_count) : val(0) {}
        SimpleAtomicCounter(T start, int thread_count) : val(start) {}

        T fetch_add(T diff, int thread_id)
        {
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

namespace COMBINING_FUNNEL_COUNTER
{
    template <typename T>
    class alignas(1024) CombiningFunnelCounter
    {
    private:
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

        FunnelNode funnel[NUM_LAYERS][1 << 8]; // 512
        int PADDING_2[32] = {};

        RandomGenerator gens[1 << 8];
        int PADDING_3[32] = {};

        std::atomic<T> counter;
        int PADDING_4[32] = {};

    public:
        static std::string className()
        {
            return "CombiningFunnelCounter";
        }
        CombiningFunnelCounter(int thread_count) : CombiningFunnelCounter(0, thread_count) {}
        CombiningFunnelCounter(T start, int thread_count)
        {
            this->thread_count = thread_count;
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
                    { // If I fail here, it means that someone else has updated the counter with my update.
                        op->result.store(current);
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
            counter.store(value, order);
        }

        bool compare_exchange(T &expected, T desired)
        {
            return counter.compare_exchange_strong(expected, desired);
        }
    };
}