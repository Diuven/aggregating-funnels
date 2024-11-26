#include "lfring_cas1.h"

void lfring_init_empty(struct lfring * ring, size_t order)
{
    struct __lfring * q = (struct __lfring *) ring;
    size_t i, n = lfring_pow2(order + 1);

    for (i = 0; i != n; i++)
        atomic_init(&q->array[i], (lfsatomic_t) -1);

    atomic_init(&q->head, 0);
    atomic_init(&q->threshold, -1);
    atomic_init(&q->tail, 0);
}

void lfring_init_full(struct lfring * ring, size_t order)
{
    struct __lfring * q = (struct __lfring *) ring;
    size_t i, half = lfring_pow2(order), n = half * 2;

    for (i = 0; i != half; i++)
        atomic_init(&q->array[__lfring_map(i, order, n)], n + __lfring_raw_map(i, order, half));
    for (; i != n; i++)
        atomic_init(&q->array[__lfring_map(i, order, n)], (lfsatomic_t) -1);

    atomic_init(&q->head, 0);
    atomic_init(&q->threshold, __lfring_threshold3(half, n));
    atomic_init(&q->tail, half);
}

void lfring_init_fill(struct lfring * ring,
                                    size_t s, size_t e, size_t order)
{
    struct __lfring * q = (struct __lfring *) ring;
    size_t i, half = lfring_pow2(order), n = half * 2;

    for (i = 0; i != s; i++)
        atomic_init(&q->array[__lfring_map(i, order, n)], 2 * n - 1);
    for (; i != e; i++)
        atomic_init(&q->array[__lfring_map(i, order, n)], n + i);
    for (; i != n; i++)
        atomic_init(&q->array[__lfring_map(i, order, n)], (lfsatomic_t) -1);

    atomic_init(&q->head, s);
    atomic_init(&q->threshold, __lfring_threshold3(half, n));
    atomic_init(&q->tail, e);
}

bool lfring_enqueue(struct lfring * ring, size_t order,
                                  size_t eidx, bool nonempty)
{
    struct __lfring * q = (struct __lfring *) ring;
    size_t tidx, half = lfring_pow2(order), n = half * 2;
    lfatomic_t tail, entry, ecycle, tcycle;

    eidx ^= (n - 1);

    while (1) {
        tail = atomic_fetch_add_explicit(&q->tail, 1, memory_order_acq_rel);
        if (tail & __LFRING_CLOSED)
            return false;

        tcycle = (tail << 1) | (2 * n - 1);
        tidx = __lfring_map(tail, order, n);
        entry = atomic_load_explicit(&q->array[tidx], memory_order_acquire);
        retry:
        ecycle = entry | (2 * n - 1);
        if (__lfring_cmp(ecycle, <, tcycle) && ((entry == ecycle) ||
                                                ((entry == (ecycle ^ n)) &&
                                                 __lfring_cmp(atomic_load_explicit(&q->head,
                                                                                   memory_order_acquire), <=, tail)))) {

            if (!atomic_compare_exchange_weak_explicit(&q->array[tidx],
                                                       &entry, tcycle ^ eidx,
                                                       memory_order_acq_rel, memory_order_acquire))
                goto retry;

            if (!nonempty && (atomic_load(&q->threshold) != __lfring_threshold3(half, n)))
                atomic_store(&q->threshold, __lfring_threshold3(half, n));
            return true;
        }
    }
}

size_t lfring_dequeue(struct lfring * ring, size_t order,
                                    bool nonempty)
{
    struct __lfring * q = (struct __lfring *) ring;
    size_t hidx, n = lfring_pow2(order + 1);
    lfatomic_t head, entry, entry_new, ecycle, hcycle, tail;
    size_t attempt;

    if (!nonempty && atomic_load(&q->threshold) < 0) {
        return LFRING_EMPTY;
    }

#ifdef CAUTIOUS_DEQUEUE
    {
        lfatomic_t h = atomic_load_explicit(&q->head, memory_order_acquire);
        lfatomic_t t = atomic_load_explicit(&q->tail, memory_order_acquire) & ~__LFRING_CLOSED;
        if (h >= t)
            return LFRING_EMPTY;
    };
#endif

    while (1) {
        head = atomic_fetch_add_explicit(&q->head, 1, memory_order_acq_rel);
        hcycle = (head << 1) | (2 * n - 1);
        hidx = __lfring_map(head, order, n);
        attempt = 0;
        again:
        entry = atomic_load_explicit(&q->array[hidx], memory_order_acquire);

        do {
            ecycle = entry | (2 * n - 1);
            if (ecycle == hcycle) {
                atomic_fetch_or_explicit(&q->array[hidx], (n - 1),
                                         memory_order_acq_rel);
                return (size_t) (entry & (n - 1));
            }

            if ((entry | n) != ecycle) {
                entry_new = entry & ~(lfatomic_t) n;
                if (entry == entry_new)
                    break;
            } else {
                if ((attempt & ((1ull << 8) - 1)) == 0)
                    tail = atomic_load_explicit(&q->tail, memory_order_acquire);
                lfatomic_t t = tail & ~__LFRING_CLOSED;
                int closed = (tail & __LFRING_CLOSED) != 0;

                if (++attempt <= 4*1024 && !closed && __lfring_cmp(t, >=, head + 1))
                    goto again;
                entry_new = hcycle ^ ((~entry) & n);
            }
        } while (__lfring_cmp(ecycle, <, hcycle) &&
                 !atomic_compare_exchange_weak_explicit(&q->array[hidx],
                                                        &entry, entry_new,
                                                        memory_order_acq_rel, memory_order_acquire));

        if (!nonempty) {
            tail = atomic_load_explicit(&q->tail, memory_order_acquire) & ~__LFRING_CLOSED;
            if (__lfring_cmp(tail, <=, head + 1)) {
                __lfring_catchup(ring, tail, head + 1);
                atomic_fetch_sub_explicit(&q->threshold, 1,
                                          memory_order_acq_rel);
                return LFRING_EMPTY;
            }

            if (atomic_fetch_sub_explicit(&q->threshold, 1,
                                          memory_order_acq_rel) <= 0)
                return LFRING_EMPTY;
        }
    }
}

void lfring_reset_threshold(struct lfring * ring, size_t order)
{
    struct __lfring * q = (struct __lfring *) ring;
    size_t half = lfring_pow2(order), n = half * 2;
    atomic_store(&q->threshold, __lfring_threshold3(half, n));
}

void lfring_close(struct lfring * ring) {
    struct __lfring * q = (struct __lfring *) ring;
    atomic_fetch_or_explicit(&q->tail, __LFRING_CLOSED, memory_order_seq_cst);
}

lfatomic_t lfring_get_head(struct lfring * ring) {
    struct __lfring * q = (struct __lfring *) ring;
    return atomic_load(&q->head);
}

lfatomic_t lfring_get_tail(struct lfring * ring) {
    struct __lfring * q = (struct __lfring *) ring;
    return atomic_load(&q->tail) & ~__LFRING_CLOSED;
}
