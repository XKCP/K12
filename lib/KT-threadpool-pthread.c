/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

Thread pool implementation using POSIX threads (pthreads).

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include "KT-threadpool.h"
#include <stdlib.h>
#include <string.h>

/* Only compile pthread backend if pthreads are available */
#if defined(_POSIX_THREADS) || defined(__unix__) || defined(__unix) || \
    (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__) || \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

#include <pthread.h>

#define MAX_THREADS 64
#define MAX_JOBS 256

/* Job structure */
typedef struct {
    void (*work_fn)(void*);
    void* work_data;
} Job;

/* Thread pool context */
typedef struct {
    pthread_t* threads;
    int num_threads;

    /* Job queue */
    Job job_queue[MAX_JOBS];
    int job_count;
    int jobs_grabbed;    /* Number of jobs grabbed by workers */
    int jobs_finished;   /* Number of jobs actually completed */

    /* Synchronization */
    pthread_mutex_t mutex;
    pthread_cond_t work_available;
    pthread_cond_t work_complete;

    /* Thread IDs for passing to worker threads */
    int* thread_ids;

    /* Lifecycle */
    int shutdown;
} PthreadPool;

/* Worker thread function */
static void* worker_thread(void* arg)
{
    PthreadPool* pool = (PthreadPool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        /* Wait for work or shutdown */
        while (!pool->shutdown && pool->jobs_grabbed >= pool->job_count) {
            pthread_cond_wait(&pool->work_available, &pool->mutex);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        /* Get next available job atomically */
        Job job;
        int has_job = 0;
        if (pool->jobs_grabbed < pool->job_count) {
            job = pool->job_queue[pool->jobs_grabbed];
            pool->jobs_grabbed++;
            has_job = 1;
        }

        pthread_mutex_unlock(&pool->mutex);

        /* Execute job outside the lock */
        if (has_job && job.work_fn) {
            job.work_fn(job.work_data);

            /* Mark job as finished */
            pthread_mutex_lock(&pool->mutex);
            pool->jobs_finished++;
            if (pool->jobs_finished >= pool->job_count) {
                pthread_cond_signal(&pool->work_complete);
            }
            pthread_mutex_unlock(&pool->mutex);
        }
    }

    return NULL;
}

/* Create pthread pool */
static void* pthread_create_pool(int num_threads)
{
    if (num_threads < 1 || num_threads > MAX_THREADS)
        return NULL;

    PthreadPool* pool = (PthreadPool*)malloc(sizeof(PthreadPool));
    if (!pool)
        return NULL;

    memset(pool, 0, sizeof(PthreadPool));
    pool->num_threads = num_threads;

    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->work_available, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->work_complete, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->work_available);
        free(pool);
        return NULL;
    }

    /* Allocate thread array */
    pool->threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    if (!pool->threads) {
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->work_available);
        pthread_cond_destroy(&pool->work_complete);
        free(pool);
        return NULL;
    }

    pool->thread_ids = (int*)malloc(num_threads * sizeof(int));
    if (!pool->thread_ids) {
        free(pool->threads);
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->work_available);
        pthread_cond_destroy(&pool->work_complete);
        free(pool);
        return NULL;
    }

    /* Create worker threads */
    pool->shutdown = 0;
    pool->job_count = 0;
    pool->jobs_grabbed = 0;
    pool->jobs_finished = 0;

    for (int i = 0; i < num_threads; i++) {
        pool->thread_ids[i] = i;
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            /* Failed to create thread - clean up */
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->work_available);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->thread_ids);
            free(pool->threads);
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->work_available);
            pthread_cond_destroy(&pool->work_complete);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

/* Submit work to pthread pool */
static int pthread_submit(void* pool_handle, void (*work_fn)(void*), void* work_data)
{
    PthreadPool* pool = (PthreadPool*)pool_handle;
    if (!pool || !work_fn)
        return 1;

    pthread_mutex_lock(&pool->mutex);

    if (pool->job_count >= MAX_JOBS) {
        pthread_mutex_unlock(&pool->mutex);
        return 1;  /* Job queue full */
    }

    pool->job_queue[pool->job_count].work_fn = work_fn;
    pool->job_queue[pool->job_count].work_data = work_data;
    pool->job_count++;

    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

/* Wait for all work to complete */
static void pthread_wait_all(void* pool_handle)
{
    PthreadPool* pool = (PthreadPool*)pool_handle;
    if (!pool)
        return;

    pthread_mutex_lock(&pool->mutex);

    /* Reset counters and wake up workers */
    pool->jobs_grabbed = 0;
    pool->jobs_finished = 0;
    pthread_cond_broadcast(&pool->work_available);

    /* Wait for all jobs to finish execution */
    while (pool->jobs_finished < pool->job_count) {
        pthread_cond_wait(&pool->work_complete, &pool->mutex);
    }

    /* Reset for next batch */
    pool->job_count = 0;
    pool->jobs_grabbed = 0;
    pool->jobs_finished = 0;

    pthread_mutex_unlock(&pool->mutex);
}

/* Destroy pthread pool */
static void pthread_destroy(void* pool_handle)
{
    PthreadPool* pool = (PthreadPool*)pool_handle;
    if (!pool)
        return;

    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->work_available);
    pthread_mutex_unlock(&pool->mutex);

    /* Wait for all threads to finish */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    /* Cleanup */
    free(pool->thread_ids);
    free(pool->threads);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->work_available);
    pthread_cond_destroy(&pool->work_complete);
    free(pool);
}

/* Export pthread backend API */
const KT_ThreadPool_API KT_ThreadPool_Pthread = {
    .min_input_size_for_threading = 2097152,  /* 2 MB default threshold */
    .create = pthread_create_pool,
    .submit = pthread_submit,
    .wait_all = pthread_wait_all,
    .destroy = pthread_destroy
};

#else /* !HAVE_PTHREADS */

/* Pthread not available on this platform - provide stub */
static void* pthread_stub_create(int num_threads) {
    (void)num_threads;
    return NULL;
}

static int pthread_stub_submit(void* pool, void (*work_fn)(void*), void* work_data) {
    (void)pool; (void)work_fn; (void)work_data;
    return 1;
}

static void pthread_stub_wait_all(void* pool) {
    (void)pool;
}

static void pthread_stub_destroy(void* pool) {
    (void)pool;
}

const KT_ThreadPool_API KT_ThreadPool_Pthread = {
    .min_input_size_for_threading = 2097152,  /* 2 MB default threshold */
    .create = pthread_stub_create,
    .submit = pthread_stub_submit,
    .wait_all = pthread_stub_wait_all,
    .destroy = pthread_stub_destroy
};

#endif /* HAVE_PTHREADS */
