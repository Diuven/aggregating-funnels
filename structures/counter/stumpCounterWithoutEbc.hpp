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
        };

        alignas(1024) std::atomic<T> counter = 0;
        int PADDING_1[32] = {};

        Node child[64]; // Max thread count is 64*64=4096
        int PADDING_2[32] = {};

        int thread_count;
        std::vector<int> starting_node;
        int PADDING_3[32] = {};

    public:
        StumpCounter() {}
        StumpCounter(int thread_count) : StumpCounter(0, thread_count) {}
        StumpCounter(T start, int thread_count, int contention_cutoff = 14)
        {
            init(start, thread_count, contention_cutoff);
        }
        void init(T start, int thread_count, int contention_cutoff = 14)
        {
            this->thread_count = thread_count;
            counter.store(start);
            for (int i = 0; i < thread_count; i++)
                starting_node.push_back(0);

            int block = 0;
            while (block * block <= thread_count)
                block++;
            block = std::max(block, 6);

            int child_count = 0;
            int cur = thread_count;
            while (cur > 0)
            {
                if (cur + child_count < std::min(2 * block, contention_cutoff))
                {
                    child_count += 1;
                    starting_node[--cur] = 0;
                }
                else
                {
                    child_count += 1;
                    for (int i = 0; i < block; i++)
                        starting_node[--cur] = child_count;
                }
            }

            for (int i = 0; i < thread_count; i++)
            {
                std::cerr << "Thread " << std::setw(3) << i << " goes to node " << std::setw(2) << starting_node[i] << std::endl;
            }
        }

        T update(Node *child, T child_from, T child_to)
        {
            T root_from = counter.fetch_add(child_to - child_from);
            MappingListNode *new_mapping = new MappingListNode();
            new_mapping->prev = child->mapping_list.load();
            new_mapping->child_from = child_from;
            new_mapping->child_to = child_to;
            new_mapping->root_from = root_from;
            child->mapping_list.store(new_mapping, std::memory_order_release);
            child->sent.store(child_to, std::memory_order_release);
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
            if (nd_idx == 0)
                return counter.fetch_add(diff);

            Node *child = &this->child[nd_idx];
            T child_from = child->count.fetch_add(diff);
            T next_from = child->sent.load();
            while (next_from < child_from)
                next_from = child->sent.load();

            if (child_from == next_from)
            {
                // I should do the work
                T child_to = child->count.load();
                T root_from = update(child, child_from, child_to);
                return root_from;
            }
            else
            {
                // Mine is already done
                T root_from = get_my_root(child, child_from);
                return root_from;
            }
        }

        T load() const
        {
            return counter.load();
        }

        void store(T value)
        {
            counter.store(value);
        }

        bool compare_exchange(T &expected, T desired)
        {
            return counter.compare_exchange_strong(expected, desired);
        }
    };
}
