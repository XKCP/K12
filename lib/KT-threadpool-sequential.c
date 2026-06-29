/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

Sequential (no-threading) thread pool implementation.

This backend executes work functions immediately in the calling thread,
providing no actual parallelism. It serves as a portable fallback for
platforms without threading support and is useful for testing/debugging.

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include "KT-threadpool.h"
#include <stdlib.h>

/* Sequential pool context */
typedef struct {
    int valid;  /* Marker to detect use-after-free */
} SequentialPool;

/* Create sequential pool */
static void* sequential_create_pool(int num_threads)
{
    /* Ignore num_threads - we're always sequential */
    (void)num_threads;

    SequentialPool* pool = (SequentialPool*)malloc(sizeof(SequentialPool));
    if (!pool)
        return NULL;

    pool->valid = 0x12345678;  /* Magic number for validation */

    return pool;
}

/* Submit work to sequential pool (execute it synchronously) */
static int sequential_submit(void* pool_handle, void (*work_fn)(void*), void* work_data)
{
    SequentialPool* pool = (SequentialPool*)pool_handle;
    if (!pool || pool->valid != 0x12345678 || !work_fn)
        return 1;

    work_fn(work_data);
    return 0;
}

/* Work is completed before sequential_submit returns. */
static void sequential_wait_all(void* pool_handle)
{
    SequentialPool* pool = (SequentialPool*)pool_handle;
    if (!pool || pool->valid != 0x12345678)
        return;
}

/* Destroy sequential pool */
static void sequential_destroy(void* pool_handle)
{
    SequentialPool* pool = (SequentialPool*)pool_handle;
    if (!pool)
        return;

    pool->valid = 0;  /* Invalidate */
    free(pool);
}

/* Export sequential backend API */
const KT_ThreadPool_API KT_ThreadPool_Sequential = {
    .min_input_size_for_threading = 2097152,  /* 2 MB default threshold */
    .create = sequential_create_pool,
    .submit = sequential_submit,
    .wait_all = sequential_wait_all,
    .destroy = sequential_destroy
};
