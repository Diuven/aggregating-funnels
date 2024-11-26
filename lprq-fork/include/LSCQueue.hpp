#pragma once

extern "C"
{
#include "lfring.h"
}
#include "LinkedRingQueue.hpp"

constexpr unsigned int intLog2(uint64_t v)
{
    unsigned int r = 0;
    while (v >>= 1)
        ++r;
    return r;
}

template <typename T, size_t ring_size>
class SCQueue
{
private:
    static constexpr size_t SCQD_ORDER = intLog2(ring_size);

    static_assert(SCQD_ORDER >= LFRING_MIN_ORDER);

    struct alignas(128)
    {
        char aq[LFRING_SIZE(SCQD_ORDER)];
        char fq[LFRING_SIZE(SCQD_ORDER)];
        T *val[(1U << SCQD_ORDER)];
    } q;

    alignas(128) std::atomic<SCQueue<T, ring_size> *> next{nullptr};

    size_t startIndex;

    inline struct lfring *aq()
    {
        return reinterpret_cast<struct lfring *>(q.aq);
    }

    inline struct lfring *fq()
    {
        return reinterpret_cast<struct lfring *>(q.fq);
    }

    inline bool isEmpty()
    {
        uint64_t h = lfring_get_head(aq());
        uint64_t t = lfring_get_tail(aq());
        return h >= t;
    }

    uint64_t getHeadIndex()
    {
        return lfring_get_head(aq()) + startIndex;
    }

    uint64_t getTailIndex()
    {
        return lfring_get_tail(aq()) + startIndex;
    }

    uint64_t getNextSegmentStartIndex()
    {
        return getTailIndex();
    }

public:
    static constexpr size_t RING_SIZE = ring_size;

    static std::string className()
    {
        return "SCQueue/remap";
    }

    SCQueue(uint64_t start, int enqThreads, int deqThreads) : startIndex(start)
    {
        lfring_init_empty(aq(), SCQD_ORDER);
        lfring_init_full(fq(), SCQD_ORDER);
    }

    bool enqueue(T *item, [[maybe_unused]] const int tid)
    {
        size_t eidx = lfring_dequeue(fq(), SCQD_ORDER, false);
        if (eidx == LFRING_EMPTY)
        {
            lfring_close(aq());
            return false;
        }
        q.val[eidx] = item;
        if (lfring_enqueue(aq(), SCQD_ORDER, eidx, false))
            return true;

        lfring_enqueue(fq(), SCQD_ORDER, eidx, false);
        return false;
    }

    T *dequeue([[maybe_unused]] const int tid)
    {
        size_t eidx = lfring_dequeue(aq(), SCQD_ORDER, false);
        if (eidx == LFRING_EMPTY)
            return nullptr;
        T *val = q.val[eidx];
        lfring_enqueue(fq(), SCQD_ORDER, eidx, false);
        return val;
    }

    // LSCQ requires aq threshold to be reset before the second dequeue attempt
    // after pointer to the next segment is observed.
    // See usage in LinkedRingQueue::dequeue.
    void prepareDequeueAfterNextLinked()
    {
        lfring_reset_threshold(aq(), SCQD_ORDER);
    }

    friend class LinkedRingQueue<T, SCQueue<T, ring_size>>;
};

template <typename T, size_t ring_size = 1024>
using LSCQueue = LinkedRingQueue<T, SCQueue<T, ring_size>>;
