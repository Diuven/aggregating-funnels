#pragma once

#include "lf/config.h"


#define LFRING_ALIGN	(_Alignof(struct __lfring))
#define LFRING_SIZE(o)	\
	(offsetof(struct __ghost_lfring, array) + (sizeof(lfatomic_t) << ((o) + 1)))

#if LFATOMIC_WIDTH == 32
# define LFRING_MIN_ORDER	(LF_CACHE_SHIFT - 2)
#elif LFATOMIC_WIDTH == 64
# define LFRING_MIN_ORDER	(LF_CACHE_SHIFT - 3)
#elif LFATOMIC_WIDTH == 128
# define LFRING_MIN_ORDER	(LF_CACHE_SHIFT - 4)
#else
# error "Unsupported LFATOMIC_WIDTH."
#endif

#define LFRING_EMPTY	(~(size_t) 0U)


/*
 * We cannot expose real __lfring to C++ code,
 * because C atomics are incompatible with C++.
 * But we need array offset in LFRING_SIZE macro.
 * So meet the ghost of __lfring.
 */
struct __ghost_lfring {
    __attribute__ ((aligned(LF_CACHE_BYTES))) lfatomic_t head;
    __attribute__ ((aligned(LF_CACHE_BYTES))) lfsatomic_t threshold;
    __attribute__ ((aligned(LF_CACHE_BYTES))) lfatomic_t tail;
    __attribute__ ((aligned(LF_CACHE_BYTES))) lfatomic_t array[1];
};

struct lfring;

void lfring_init_empty(struct lfring * ring, size_t order);

void lfring_init_full(struct lfring * ring, size_t order);

void lfring_init_fill(struct lfring * ring, size_t s, size_t e, size_t order);

bool lfring_enqueue(struct lfring * ring, size_t order, size_t eidx, bool nonempty);

size_t lfring_dequeue(struct lfring * ring, size_t order, bool nonempty);

void lfring_reset_threshold(struct lfring * ring, size_t order);

void lfring_close(struct lfring * ring);

lfatomic_t lfring_get_head(struct lfring * ring);

lfatomic_t lfring_get_tail(struct lfring * ring);
