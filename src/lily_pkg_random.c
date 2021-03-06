/**
library random

The random package provides access to a Mersenne Twister for pseudo-random
number generation.
*/

/* This uses code from libmtwist which uses The Unlicense:
   https://github.com/dajobe/libmtwist
   https://unlicense.org/

   This randomness library is limited to 32 bit values. It's assumed that most
   ranges will be within 32 bits (4 billion-ish). */

#include <stdio.h>
#include <stdint.h>
#include <time.h>


#include "lily.h"

#define MTWIST_N             624
#define MTWIST_M             397
#define MTWIST_UPPER_MASK    ((uint32_t)0x80000000)
#define MTWIST_LOWER_MASK    ((uint32_t)0x7FFFFFFF)
#define MTWIST_FULL_MASK     ((uint32_t)0xFFFFFFFF)
#define MTWIST_MATRIX_A      ((uint32_t)0x9908B0DF)

#define MTWIST_MIXBITS(u, v) ( ( (u) & MTWIST_UPPER_MASK) | ( (v) & MTWIST_LOWER_MASK) )
#define MTWIST_TWIST(u, v)  ( (MTWIST_MIXBITS(u, v) >> 1) ^ ( (v) & UINT32_C(1) ? MTWIST_MATRIX_A : UINT32_C(0)) )

/** Begin autogen section. **/
typedef struct lily_random_Random_ {
    LILY_FOREIGN_HEADER
    uint32_t state[MTWIST_N];
    uint32_t *next;
    int remaining;
} lily_random_Random;
#define ARG_Random(state, index) \
(lily_random_Random *)lily_arg_generic(state, index)
#define ID_Random(state) lily_cid_at(state, 0)
#define INIT_Random(state)\
(lily_random_Random *) lily_push_foreign(state, ID_Random(state), (lily_destroy_func)destroy_Random, sizeof(lily_random_Random))

const char *lily_random_info_table[] = {
    "\01Random\0"
    ,"C\02Random\0"
    ,"m\0<new>\0(*Integer): Random"
    ,"m\0between\0(Random,Integer,Integer): Integer"
    ,"Z"
};
void lily_random_Random_new(lily_state *);
void lily_random_Random_between(lily_state *);
lily_call_entry_func lily_random_call_table[] = {
    NULL,
    NULL,
    lily_random_Random_new,
    lily_random_Random_between,
};
/** End autogen section. **/

static void destroy_Random(lily_random_Random *r)
{
}

/**
foreign class Random(seed: *Integer = 0) {
    layout {
        uint32_t state[MTWIST_N];
        uint32_t *next;
        int remaining;
    }
}

The `Random` class provides access to the random number generator. Each
instance is completely separate from all others.

The constructor for this class takes a seed. If the seed provided is 0 or less,
then the current time (`time(NULL)` in C) is used instead. 
*/

void lily_random_Random_new(lily_state *s)
{
    lily_random_Random* mt = INIT_Random(s);
    int64_t seed = 0;
    int i;

    if (lily_arg_count(s) == 2)
        seed = lily_arg_integer(s, 1);

    if (seed <= 0)
        seed = (int64_t)time(NULL);

    mt->state[0] = (uint32_t)(seed & MTWIST_FULL_MASK);
    for(i = 1; i < MTWIST_N; i++) {
        mt->state[i] = (((uint32_t)1812433253) * (mt->state[i - 1] ^ (mt->state[i - 1] >> 30)) + i);
        mt->state[i] &= MTWIST_FULL_MASK;
    }

    mt->remaining = 0;
    mt->next = NULL;

    lily_return_top(s);
}

static void mtwist_update(lily_random_Random* mt)
{
    int count;
    uint32_t *p = mt->state;

    for (count = (MTWIST_N - MTWIST_M + 1); --count; p++)
        *p = p[MTWIST_M] ^ MTWIST_TWIST(p[0], p[1]);

    for (count = MTWIST_M; --count; p++)
        *p = p[MTWIST_M - MTWIST_N] ^ MTWIST_TWIST(p[0], p[1]);

    *p = p[MTWIST_M - MTWIST_N] ^ MTWIST_TWIST(p[0], mt->state[0]);

    mt->remaining = MTWIST_N;
    mt->next = mt->state;
}

uint32_t mtwist_u32rand(lily_random_Random* mt)
{
    uint32_t r;

    if (mt->remaining == 0)
        mtwist_update(mt);

    r = *mt->next++;
    mt->remaining--;

    /* Tempering. */
    r ^= (r >> 11);
    r ^= (r << 7) & ((uint32_t)0x9D2C5680);
    r ^= (r << 15) & ((uint32_t)0xEFC60000);
    r ^= (r >> 18);

    r &= MTWIST_FULL_MASK;

    return (uint32_t)r;
}

/**
define Random.between(lower: Integer, upper: Integer): Integer

Generate a random `Integer` value between `lower` and `upper`.

# Errors

* `ValueError` is raised if the range is empty, or reversed.
*/
void lily_random_Random_between(lily_state *s)
{
    lily_random_Random *r = ARG_Random(s, 0);
    uint64_t rng = (uint64_t)mtwist_u32rand(r);

    int64_t start = lily_arg_integer(s, 1);
    int64_t end = lily_arg_integer(s, 2);

    if (start >= end)
        lily_ValueError(s, "Interval range is empty.");

    int64_t distance = end - start + 1;

    if (distance < INT32_MIN ||
        distance > INT32_MAX)
        lily_ValueError(s, "Interval exceeds 32 bits in size.");

    uint64_t result = start + (rng % distance);

    lily_return_integer(s, result);
}
