#pragma once

#include <atomic>
#include <vector>
#include <queue>
#include <string>
#include <iostream>

#ifndef COUNTER_COMMON_HPP
#define COUNTER_COMMON_HPP
#include "./common.hpp"
#endif

#define AGG_COUNT 6

namespace SIMPLE_AGG_FUNNEL
{
    template <typename T>
    class alignas(1024) AggFunnelCounter : public Counter<T>
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
        Node child[AGG_COUNT];
        int PADDING[32] = {};

    public:
        inline static EpochBasedReclamation<MappingListNode> *ebr = new EpochBasedReclamation<MappingListNode>(max_thread_count);

        AggFunnelCounter() {}
        ~AggFunnelCounter() { delete ebr; }
        AggFunnelCounter(int thread_count) : AggFunnelCounter(0, thread_count) {}
        AggFunnelCounter(T start, int thread_count)
        {
            init(start, thread_count);
        }
        void init(T start, int thread_count)
        {
            counter.store(start);
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
            int nd_idx = thread_id % AGG_COUNT;
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
