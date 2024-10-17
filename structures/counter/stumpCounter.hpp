#include <atomic>
#include <vector>
#include <queue>
#include <string>
#include <iostream>

#ifndef COUNTER_COMMON_HPP
#define COUNTER_COMMON_HPP
#include "./common.hpp"
#endif

namespace STUMP_COUNTER
{

    struct alignas(128) RandomGenerator
    {
        int seed;
        int next()
        {
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            return seed;
        }
    };
    struct alignas(512) ThreadLocalData
    {
        long long access_count[64] = {};
        long long root_access = 0;
        long long loop_count_1 = 0;
        long long loop_count_2 = 0;
        RandomGenerator rand;
    };

    template <typename T>
    class alignas(1024) StumpCounter : public Counter<T>
    {
    private:
        struct alignas(32) MappingListNode
        {
            MappingListNode *prev = nullptr;
            T child_from = 0;
            T child_to = 0;
            T root_from = -1;
        };

        struct alignas(1024) Node
        {
            alignas(128) std::atomic<T> count = 0;
            alignas(128) std::atomic<T> sent = 0;
            std::atomic<MappingListNode *> mapping_list = new MappingListNode();
            ~Node() { delete mapping_list.load(); }
        };

        alignas(1024) std::atomic<T> counter = 0;
        int PADDING_1[32] = {};

        Node child[64]; // Max thread count is 64*64=4096
        int PADDING_2[32] = {};

        int thread_count;
        std::vector<int> starting_node;
        int PADDING_3[32] = {};

        std::vector<ThreadLocalData> aux_data;
        EpochBasedReclamation<MappingListNode> *ebr = nullptr;
        int PADDING_4[32] = {};

        int configure_fixed_fanout(int fanout, int direct = 0)
        {
            int root_fanout = fanout;
            for (int i = direct; i < thread_count; i++)
            {
                starting_node[i] = i % fanout + 1;
            }

            for (int i = 0; i < direct; i++)
            {
                root_fanout++;
                starting_node[i] = -root_fanout;
            }
            return root_fanout;
        }

        int configure_root_fanout(int direct = 0)
        {
            int block = 1; // ceil(sqrt(thread_count))
            while ((block) * (block) < (thread_count))
                block++;
            return configure_fixed_fanout(block, direct);
        }

    public:
        StumpCounter() {}
        ~StumpCounter() { delete ebr; }
        StumpCounter(int thread_count) : StumpCounter(0, thread_count) {}
        StumpCounter(T start, int thread_count)
        {
            init(start, thread_count);
        }
        void init(T start, int thread_count)
        {
            this->thread_count = thread_count;
            ebr = new EpochBasedReclamation<MappingListNode>(thread_count);
            counter.store(start);
            starting_node.resize(thread_count, 0);
            aux_data.resize(thread_count);

            int time_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            for (int i = 0; i < thread_count; i++)
            {
                aux_data[i].rand.seed = time_seed * 100 + i;
            }

#ifdef DIRECT_THREAD_COUNT
            int direct = DIRECT_THREAD_COUNT;
#else
            int direct = 0;
#endif

            int fanout = 1;

#ifdef FANOUT_COUNT
            if (FANOUT_COUNT > 0)
                fanout = FANOUT_COUNT;
#endif
#ifdef GROUP_SIZE
            if (GROUP_SIZE > 0)
                fanout = (thread_count + GROUP_SIZE - 1) / GROUP_SIZE;
#endif

#ifdef USE_ROOT_STUMP
            std::cout << "Using root stump with direct=" << direct << std::endl;
            configure_root_fanout(direct);
#elif defined USE_FIXED_STUMP
            std::cout << "Using fixed stump with fanout=" << fanout << " and direct=" << direct << std::endl;
            configure_fixed_fanout(fanout, direct);
#else

            std::cout << "(DEFAULT) Using fixed stump with fanout=" << 6 << " and direct=" << 0 << std::endl;
            configure_fixed_fanout(6, 0);
#endif

            for (int i = 0; i < thread_count; i++)
            {
                std::cerr << "Thread " << std::setw(3) << i << " goes to node " << std::setw(2) << starting_node[i] << std::endl;
            }
        }

        long long max_access() const
        {
            long long root_access = 0;
            long long node_access[64] = {};
            for (int i = 0; i < thread_count; i++)
            {
                root_access += aux_data[i].root_access;
                for (int j = 0; j < 64; j++)
                    node_access[j] += aux_data[i].access_count[j];
            }
            long long max_access = root_access;
            for (int i = 0; i < 64; i++)
                max_access = std::max(max_access, node_access[i]);
            return max_access;
        }
        long long root_access() const
        {
            long long root_access = 0;
            for (int i = 0; i < thread_count; i++)
                root_access += aux_data[i].root_access;
            return root_access;
        }
        void update_aux_data(int thread_id, RunResult &result) const
        {
            result.loop_count_1 += aux_data[thread_id].loop_count_1;
            result.loop_count_2 += aux_data[thread_id].loop_count_2;
            result.root_access += aux_data[thread_id].root_access;
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
#ifndef NO_AUX_DATA
                aux_data[thread_id].loop_count_2++;
#endif
                mapping = mapping->prev;
            }

            T root_from = mapping->root_from;
            T child_from = mapping->child_from;
            return root_from + my_child_from - child_from;
        }

        T fetch_add(T diff, int thread_id)
        {
            int nd_idx = starting_node[thread_id];
            if (nd_idx < 0)
            {
#ifndef NO_AUX_DATA
                aux_data[thread_id].root_access++;
#endif
                return counter.fetch_add(diff);
            }
            ebr->enterCritical(thread_id);

            Node *child = &this->child[nd_idx];
            T child_from = child->count.fetch_add(diff);
            T next_from = child->sent.load();
            while (next_from < child_from)
            {
#ifndef NO_AUX_DATA
                aux_data[thread_id].loop_count_1++;
#endif
                next_from = child->sent.load();
            }

            T root_from;
            if (child_from == next_from)
            {
                // I should do the work
                T child_to = child->count.load();
                root_from = update(child, child_from, child_to, thread_id);
#ifndef NO_AUX_DATA
                aux_data[thread_id].access_count[nd_idx]++;
                aux_data[thread_id].root_access++;
#endif
            }
            else
            {
                // Mine is already done
                root_from = get_my_root(child, child_from, thread_id);
#ifndef NO_AUX_DATA
                aux_data[thread_id].access_count[nd_idx]++;
#endif
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
