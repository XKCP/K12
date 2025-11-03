/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

Thread pool abstraction layer for portable threading support.

This provides a simple, application-implementable thread pool API that allows
KangarooTwelve to use custom threading implementations or fall back to
sequential execution on platforms without threading support.

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#ifndef _KT_threadpool_h_
#define _KT_threadpool_h_

#include <stddef.h>

/**
 * Abstract thread pool API.
 *
 * Applications can implement this interface to provide custom threading
 * backends (e.g., Windows thread pool, custom work-stealing scheduler, etc.).
 *
 * The API is designed for batch job processing: submit multiple jobs, then
 * wait for all to complete. This matches the KangarooTwelve tree hashing
 * pattern where chunk processing is distributed across threads.
 */
typedef struct KT_ThreadPool_API {
    /**
     * Minimum input size (in bytes) required to enable parallel processing.
     *
     * If the total input size is smaller than this threshold, KangarooTwelve
     * will not use threading for that particular Update() call, avoiding
     * thread overhead for small inputs.
     *
     * Default: 2097152 (2 MB)
     * Rationale: Threading overhead outweighs benefits for small inputs.
     *            Optimal results typically seen with inputs > 10 MB.
     */
    size_t min_input_size_for_threading;

    /**
     * Create a thread pool with the specified number of worker threads.
     *
     * @param num_threads  Number of worker threads to create.
     *                     If 1, implementation may skip thread creation overhead.
     * @return Opaque pool handle on success, NULL on failure
     * @note This is called once during KangarooTwelve initialization
     */
    void* (*create)(int num_threads);

    /**
     * Submit work to the thread pool.
     *
     * The work function will be called with work_data as its argument.
     * Multiple jobs may be submitted before calling wait_all().
     *
     * @param pool       Opaque pool handle from create()
     * @param work_fn    Function to execute (called as work_fn(work_data))
     * @param work_data  Opaque pointer passed to work_fn
     * @return 0 on success, non-zero on error
     * @note work_fn must be thread-safe and not access shared mutable state
     */
    int (*submit)(void* pool, void (*work_fn)(void*), void* work_data);

    /**
     * Wait for all submitted work to complete.
     *
     * Blocks until all jobs submitted since the last wait_all() have finished.
     * After this returns, it is safe to submit new work.
     *
     * @param pool  Opaque pool handle from create()
     * @note This may be called multiple times to wait for different batches
     */
    void (*wait_all)(void* pool);

    /**
     * Destroy the thread pool and free all resources.
     *
     * All work must be complete before calling this (call wait_all() first).
     * After destruction, the pool handle must not be used.
     *
     * @param pool  Opaque pool handle from create()
     * @note This is called during KangarooTwelve cleanup
     */
    void (*destroy)(void* pool);
} KT_ThreadPool_API;

/**
 * Built-in thread pool backend using POSIX threads (pthreads).
 *
 * Available on Linux, macOS, BSD, and other Unix-like systems with pthread support.
 * Provides true parallel execution using worker threads.
 *
 * This is the default backend on pthread-capable platforms.
 */
extern const KT_ThreadPool_API KT_ThreadPool_Pthread;

/**
 * Built-in sequential (no-threading) backend.
 *
 * Available on all platforms. Executes work functions immediately in the
 * calling thread (no actual parallelism). Useful as a fallback on platforms
 * without threading support or for testing/debugging.
 *
 * This is the default backend on platforms without pthread support.
 */
extern const KT_ThreadPool_API KT_ThreadPool_Sequential;

/**
 * Get the default thread pool API for the current platform.
 *
 * Returns pthread backend if available, otherwise sequential backend.
 *
 * @return Pointer to the default thread pool API
 */
const KT_ThreadPool_API* KT_ThreadPool_GetDefault(void);

#endif /* _KT_threadpool_h_ */
