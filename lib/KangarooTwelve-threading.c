/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

KangarooTwelve, designed by Guido Bertoni, Joan Daemen, Michaël Peeters, Gilles Van Assche, Ronny Van Keer and Benoît Viguier.

Threading support implementation using portable thread pool abstraction.

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include "KangarooTwelve-threading.h"
#include "KangarooTwelve.h"
#include "KT-threadpool.h"
#include "KeccakP-1600-SnP.h"
#include <stdlib.h>
#include <string.h>

/* Constants from KangarooTwelve.c */
#define K12_chunkSize       8192
#define K12_suffixLeaf      0x0B
#define KT128_capacityInBytes   32
#define KT256_capacityInBytes   64

/* Thread pool configuration */
#define MAX_THREADS 64
#define MIN_CHUNKS_PER_THREAD 4

/* Work item for chunk processing */
typedef struct {
    const unsigned char *input;
    size_t start_chunk;
    size_t end_chunk;
    unsigned char *output;
    int security_level;
    int capacity_bytes;
} ChunkWork;

/* TurboSHAKE instance for local use */
typedef struct {
    uint8_t state[KeccakP1600_stateSizeInBytes];
    unsigned int rate;
    uint8_t byteIOIndex;
    uint8_t squeezing;
} TurboSHAKE_Instance_Local;

/* Forward declarations */
static void process_chunk_range(void *work_ptr);
static void TurboSHAKE_Initialize_Local(TurboSHAKE_Instance_Local *instance, unsigned int capacity);
static void TurboSHAKE_Absorb_Local(TurboSHAKE_Instance_Local *instance, const unsigned char *data, size_t dataByteLen);
static void TurboSHAKE_AbsorbDomainSeparationByte_Local(TurboSHAKE_Instance_Local *instance, unsigned char D);
static void TurboSHAKE_Squeeze_Local(TurboSHAKE_Instance_Local *instance, unsigned char *data, size_t dataByteLen);

/* TurboSHAKE helper functions for thread-local processing */
static void TurboSHAKE_Initialize_Local(TurboSHAKE_Instance_Local *instance, unsigned int capacity)
{
    KeccakP1600_Initialize(instance->state);
    instance->rate = 1600 - capacity;
    instance->byteIOIndex = 0;
    instance->squeezing = 0;
}

static void TurboSHAKE_Absorb_Local(TurboSHAKE_Instance_Local *instance, const unsigned char *data, size_t dataByteLen)
{
    size_t i, j;
    uint8_t partialBlock;
    const unsigned char *curData;
    const uint8_t rateInBytes = instance->rate/8;

    i = 0;
    curData = data;
    while(i < dataByteLen) {
        if ((instance->byteIOIndex == 0) && (dataByteLen-i >= rateInBytes)) {
#ifdef KeccakP1600_12rounds_FastLoop_supported
            j = KeccakP1600_12rounds_FastLoop_Absorb(instance->state, instance->rate/64, curData, dataByteLen - i);
            i += j;
            curData += j;
#endif
            for(j=dataByteLen-i; j>=rateInBytes; j-=rateInBytes) {
                KeccakP1600_AddBytes(instance->state, curData, 0, rateInBytes);
                KeccakP1600_Permute_12rounds(instance->state);
                curData+=rateInBytes;
            }
            i = dataByteLen - j;
        } else {
            if (dataByteLen - i > (size_t)rateInBytes - instance->byteIOIndex) {
                partialBlock = rateInBytes-instance->byteIOIndex;
            } else {
                partialBlock = (uint8_t)(dataByteLen - i);
            }
            i += partialBlock;

            KeccakP1600_AddBytes(instance->state, curData, instance->byteIOIndex, partialBlock);
            curData += partialBlock;
            instance->byteIOIndex += partialBlock;
            if (instance->byteIOIndex == rateInBytes) {
                KeccakP1600_Permute_12rounds(instance->state);
                instance->byteIOIndex = 0;
            }
        }
    }
}

static void TurboSHAKE_AbsorbDomainSeparationByte_Local(TurboSHAKE_Instance_Local *instance, unsigned char D)
{
    const unsigned int rateInBytes = instance->rate/8;

    KeccakP1600_AddByte(instance->state, D, instance->byteIOIndex);
    if ((D >= 0x80) && (instance->byteIOIndex == (rateInBytes-1)))
        KeccakP1600_Permute_12rounds(instance->state);
    KeccakP1600_AddByte(instance->state, 0x80, rateInBytes-1);
    KeccakP1600_Permute_12rounds(instance->state);
    instance->byteIOIndex = 0;
    instance->squeezing = 1;
}

static void TurboSHAKE_Squeeze_Local(TurboSHAKE_Instance_Local *instance, unsigned char *data, size_t dataByteLen)
{
    size_t i, j;
    unsigned int partialBlock;
    const unsigned int rateInBytes = instance->rate/8;
    unsigned char *curData;

    if (!instance->squeezing)
        TurboSHAKE_AbsorbDomainSeparationByte_Local(instance, 0x01);

    i = 0;
    curData = data;
    while(i < dataByteLen) {
        if ((instance->byteIOIndex == rateInBytes) && (dataByteLen-i >= rateInBytes)) {
            for(j=dataByteLen-i; j>=rateInBytes; j-=rateInBytes) {
                KeccakP1600_Permute_12rounds(instance->state);
                KeccakP1600_ExtractBytes(instance->state, curData, 0, rateInBytes);
                curData+=rateInBytes;
            }
            i = dataByteLen - j;
        } else {
            if (instance->byteIOIndex == rateInBytes) {
                KeccakP1600_Permute_12rounds(instance->state);
                instance->byteIOIndex = 0;
            }
            if (dataByteLen-i > rateInBytes-instance->byteIOIndex)
                partialBlock = rateInBytes-instance->byteIOIndex;
            else
                partialBlock = (unsigned int)(dataByteLen - i);
            i += partialBlock;

            KeccakP1600_ExtractBytes(instance->state, curData, instance->byteIOIndex, partialBlock);
            curData += partialBlock;
            instance->byteIOIndex += partialBlock;
        }
    }
}

/* Process a range of chunks - adapted to work as thread pool job */
static void process_chunk_range(void *work_ptr)
{
    ChunkWork *work = (ChunkWork *)work_ptr;
    TurboSHAKE_Instance_Local queueNode;
    const unsigned char *chunk_ptr = work->input + (work->start_chunk * K12_chunkSize);
    unsigned char *output_ptr = work->output + (work->start_chunk * work->capacity_bytes);

    for (size_t i = work->start_chunk; i < work->end_chunk; i++) {
        /* Initialize TurboSHAKE for this chunk */
        TurboSHAKE_Initialize_Local(&queueNode, 2 * work->security_level);

        /* Absorb the chunk */
        TurboSHAKE_Absorb_Local(&queueNode, chunk_ptr, K12_chunkSize);

        /* Finalize with domain separation */
        TurboSHAKE_AbsorbDomainSeparationByte_Local(&queueNode, K12_suffixLeaf);

        /* Squeeze out the chaining value */
        TurboSHAKE_Squeeze_Local(&queueNode, output_ptr, work->capacity_bytes);

        chunk_ptr += K12_chunkSize;
        output_ptr += work->capacity_bytes;
    }
}

/* Main function to process chunks in parallel */
int KT_ProcessChunksThreaded(const KT_ThreadPool_API* threadpool_api,
                             void* threadpool_handle,
                             int thread_count,
                             const unsigned char *input,
                             size_t chunkCount,
                             unsigned char *output,
                             int securityLevel)
{
    if (chunkCount == 0)
        return 1;

    /* No threading configured - should not happen, but handle gracefully */
    if (!threadpool_api || !threadpool_handle || thread_count < 1)
        return 1;

    /* Determine capacity in bytes */
    int capacity_bytes = (securityLevel == 128) ? KT128_capacityInBytes : KT256_capacityInBytes;

    /* Determine how many threads to use */
    int threads_to_use = thread_count;
    if (chunkCount < (size_t)(threads_to_use * MIN_CHUNKS_PER_THREAD)) {
        threads_to_use = (int)(chunkCount / MIN_CHUNKS_PER_THREAD);
        if (threads_to_use < 1)
            threads_to_use = 1;
    }

    /* If only one thread, process sequentially without thread overhead */
    if (threads_to_use == 1) {
        ChunkWork work;
        work.input = input;
        work.start_chunk = 0;
        work.end_chunk = chunkCount;
        work.output = output;
        work.security_level = securityLevel;
        work.capacity_bytes = capacity_bytes;
        process_chunk_range(&work);
        return 0;
    }

    /* Allocate work items for parallel processing */
    ChunkWork* work_items = (ChunkWork*)malloc(threads_to_use * sizeof(ChunkWork));
    if (!work_items)
        return 1;

    /* Distribute work among threads */
    size_t chunks_per_thread = chunkCount / threads_to_use;
    size_t extra_chunks = chunkCount % threads_to_use;

    size_t current_chunk = 0;
    for (int i = 0; i < threads_to_use; i++) {
        size_t this_thread_chunks = chunks_per_thread + (i < (int)extra_chunks ? 1 : 0);

        work_items[i].input = input;
        work_items[i].start_chunk = current_chunk;
        work_items[i].end_chunk = current_chunk + this_thread_chunks;
        work_items[i].output = output;
        work_items[i].security_level = securityLevel;
        work_items[i].capacity_bytes = capacity_bytes;

        /* Submit work to thread pool */
        if (threadpool_api->submit(threadpool_handle, process_chunk_range, &work_items[i]) != 0) {
            free(work_items);
            return 1;
        }

        current_chunk += this_thread_chunks;
    }

    /* Wait for all work to complete */
    threadpool_api->wait_all(threadpool_handle);

    free(work_items);
    return 0;
}
