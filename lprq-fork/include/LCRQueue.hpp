/******************************************************************************
 * Copyright (c) 2022, Raed Romanov
 * Based on LCRQ implementation by Pedro Ramalhete and Andreia Correia:
 *
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
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
#include "LinkedRingQueue.hpp"
#include "RQCell.hpp"
#include "x86AtomicOps.hpp"
#include "CacheRemap.hpp"

template <typename T, typename CNT, bool padded_cells, size_t ring_size, bool cache_remap>
class CRQueue : public CustomQueueSegmentBase<T, CNT, CRQueue<T, CNT, padded_cells, ring_size, cache_remap>>
{
private:
    using Base = CustomQueueSegmentBase<T, CNT, CRQueue<T, CNT, padded_cells, ring_size, cache_remap>>;
    using Cell = detail::CRQCell<T *, padded_cells>;

    Cell array[ring_size];

    [[no_unique_address]] ConditionalCacheRemap<cache_remap, ring_size, sizeof(Cell)> remap{};

    inline uint64_t node_index(uint64_t i) const
    {
        return (i & ~(1ull << 63));
    }

    inline uint64_t set_unsafe(uint64_t i) const
    {
        return (i | (1ull << 63));
    }

    inline uint64_t node_unsafe(uint64_t i) const
    {
        return (i & (1ull << 63));
    }

public:
    static constexpr size_t RING_SIZE = ring_size;

    CRQueue(uint64_t start, int enqThreads, int deqThreads) : Base(enqThreads, deqThreads)
    {
        for (uint64_t i = start; i < start + RING_SIZE; i++)
        {
            uint64_t j = i % RING_SIZE;
            array[remap[j]].val.store(nullptr, std::memory_order_relaxed);
            array[remap[j]].idx.store(i, std::memory_order_relaxed);
        }
        Base::head.store(start, std::memory_order_relaxed);
        Base::tail.store(start, std::memory_order_relaxed);
    }

    static std::string className()
    {
        using namespace std::string_literals;
        return "CRQueue"s + (padded_cells ? "/ca" : "") + (cache_remap ? "/remap" : "") + "/" + CNT::className();
    }

    bool enqueue(T *item, [[maybe_unused]] const int tid)
    {
        int try_close = 0;

        while (true)
        {
            // uint64_t tailticket = Base::tail.fetch_add(1);
            uint64_t tailticket = Base::tail.fetch_add(1, tid);
            if (Base::isClosed(tailticket))
            {
                return false;
            }

            Cell &cell = array[remap[tailticket % RING_SIZE]];
            uint64_t idx = cell.idx.load();
            if (cell.val.load() == nullptr)
            {
                if (node_index(idx) <= tailticket)
                {
                    if ((!node_unsafe(idx) || Base::head.load() < tailticket))
                    {
                        if (CAS2((void **)&cell, nullptr, idx, item, tailticket))
                        {
                            return true;
                        }
                    }
                }
            }
            if (tailticket >= Base::head.load() + RING_SIZE)
            {
                if (Base::closeSegment(tailticket, ++try_close > 10))
                    return false;
            }
        }
    }

    T *dequeue([[maybe_unused]] const int tid)
    {
#ifdef CAUTIOUS_DEQUEUE
        if (Base::isEmpty())
            return nullptr;
#endif

        while (true)
        {
            // uint64_t headticket = Base::head.fetch_add(1);
            uint64_t headticket = Base::head.fetch_add(1, tid);
            Cell &cell = array[remap[headticket % RING_SIZE]];

            int r = 0;
            uint64_t tt = 0;

            while (true)
            {
                uint64_t cell_idx = cell.idx.load();
                uint64_t unsafe = node_unsafe(cell_idx);
                uint64_t idx = node_index(cell_idx);
                T *val = static_cast<T *>(cell.val.load());

                if (idx > headticket)
                    break;

                if (val != nullptr)
                {
                    if (idx == headticket)
                    {
                        if (CAS2((void **)&cell, val, cell_idx, nullptr, unsafe | (headticket + RING_SIZE)))
                        {
                            return val;
                        }
                    }
                    else
                    {
                        if (CAS2((void **)&cell, val, cell_idx, val, set_unsafe(idx)))
                            break;
                    }
                }
                else
                {
                    if ((r & ((1ull << 8) - 1)) == 0)
                        tt = Base::tail.load();

                    int crq_closed = Base::isClosed(tt);
                    uint64_t t = Base::tailIndex(tt);
                    if (unsafe || t < headticket + 1 || crq_closed || r > 4 * 1024)
                    {
                        if (CAS2((void **)&cell, val, cell_idx, val, unsafe | (headticket + RING_SIZE)))
                            break;
                    }
                    ++r;
                }
            }

            if (Base::tailIndex(Base::tail.load()) <= headticket + 1)
            {
                Base::fixState();
                return nullptr;
            }
        }
    }
};

template <typename T, typename CNT, bool padded_cells = false, size_t ring_size = 1024, bool cache_remap = true>
using LCRQueue = LinkedRingQueue<T, CRQueue<T, CNT, padded_cells, ring_size, cache_remap>>;
