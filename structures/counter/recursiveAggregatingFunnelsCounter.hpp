#include <atomic>
#include <vector>
#include <queue>
#include <string>
#include <iostream>

#include "./common.hpp"
#include "./simpleAggregatingFunnelsCounter.hpp"
#include "./hardwareCounter.hpp"

namespace RECURSIVE_AGG_FUNNEL
{
    template <typename T>
    class RecursiveAggFunnelsCounter : public Counter<T>
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

        alignas(1024) SIMPLE_AGG_FUNNEL::SimpleAggFunnelsCounter<T> main_counter;
        int PADDING_1[32] = {};

        Node child[64]; // Max thread count is 64*64=4096
        int PADDING_2[32] = {};

        int thread_count;
        std::vector<int> starting_node;
        int PADDING_3[32] = {};

        EpochBasedReclamation<MappingListNode> *ebr = nullptr;
        int PADDING_4[32] = {};

    public:
        int configure_fixed_fanout(int fanout)
        {
            int root_fanout = fanout;
            for (int i = 0; i < thread_count; i++)
            {
                starting_node[i] = i % fanout + 1;
            }
            return root_fanout;
        }
        RecursiveAggFunnelsCounter(int thread_count) : RecursiveAggFunnelsCounter(0, thread_count) {}
        ~RecursiveAggFunnelsCounter() { delete ebr; }
        RecursiveAggFunnelsCounter(T start, int thread_count)
        {
            this->thread_count = thread_count;
            ebr = new EpochBasedReclamation<MappingListNode>(thread_count);
            starting_node.resize(thread_count, 0);
            int my_fanout = (thread_count + 5) / 6; // ceil(thread_count / 6)
            configure_fixed_fanout(my_fanout);
            main_counter.init(start, my_fanout);
            for (int i = 0; i < thread_count; i++)
            {
                std::cerr << "In Nested, Thread " << std::setw(3) << i << " goes to node " << std::setw(2) << starting_node[i] << std::endl;
            }
        }

        long long max_access() const
        {
            // throw std::runtime_error("Not implemented");
            return 0;
        }
        long long root_access() const
        {
            // throw std::runtime_error("Not implemented");
            return 0;
        }
        void update_aux_data(int thread_id, RunResult &result) const
        {
            // throw std::runtime_error("Not implemented");
            return;
        }

        T update(int nd_idx, T child_from, T child_to, int thread_id)
        {
            Node *child = &this->child[nd_idx];
            T root_from = main_counter.fetch_add(child_to - child_from, nd_idx - 1);

            // MappingListNode *new_mapping = new MappingListNode();
            MappingListNode *new_mapping = ebr->get_new(thread_id);
            new_mapping->prev = child->mapping_list.load();
            new_mapping->child_from = child_from;
            new_mapping->child_to = child_to;
            new_mapping->root_from = root_from;
            child->mapping_list.store(new_mapping, std::memory_order_release);
            child->sent.store(child_to, std::memory_order_release);

            ebr->retire(new_mapping->prev, thread_id);
            return root_from;
        }

        T get_my_root(Node *child, T my_child_from)
        {
            MappingListNode *mapping = child->mapping_list.load();
            while (mapping->child_from > my_child_from)
                mapping = mapping->prev;

            T root_from = mapping->root_from;
            T child_from = mapping->child_from;
            return root_from + my_child_from - child_from;
        }

        T fetch_add(T diff, int thread_id)
        {
            int nd_idx = starting_node[thread_id];
            ebr->enterCritical(thread_id);

            Node *child = &this->child[nd_idx];
            T child_from = child->count.fetch_add(diff);
            T next_from = child->sent.load();
            while (next_from < child_from)
                next_from = child->sent.load();

            T root_from;
            if (child_from == next_from)
            {
                // I should do the work
                T child_to = child->count.load();
                root_from = update(nd_idx, child_from, child_to, thread_id);
            }
            else
            {
                // Mine is already done
                root_from = get_my_root(child, child_from);
            }
            ebr->exitCritical(thread_id);
            return root_from;
        }

        T load() const
        {
            return main_counter.load();
        }

        void store(T value, std::memory_order order = std::memory_order_seq_cst)
        {
            main_counter.store(value, order);
        }

        bool compare_exchange(T &expected, T desired)
        {
            return main_counter.compare_exchange(expected, desired);
        }
    };
}
