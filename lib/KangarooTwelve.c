/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

KangarooTwelve, designed by Guido Bertoni, Joan Daemen, Michaël Peeters, Gilles Van Assche, Ronny Van Keer and Benoît Viguier.

Implementation by Gilles Van Assche and Ronny Van Keer, hereby denoted as "the implementer".

For more information, feedback or questions, please refer to the Keccak Team website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include <assert.h>
#include <string.h>
#include "KangarooTwelve.h"
#include "KeccakP-1600-SnP.h"

/* ---------------------------------------------------------------- */

static void TurboSHAKE_Initialize(TurboSHAKE_Instance *instance, int securityLevel)
{
    KeccakP1600_Initialize(instance->state);
    instance->rate = (1600-(2*securityLevel));
    instance->byteIOIndex = 0;
    instance->squeezing = 0;
}

static void TurboSHAKE_Absorb(TurboSHAKE_Instance *instance, const unsigned char *data, size_t dataByteLen)
{
    size_t i, j;
    uint8_t partialBlock;
    const unsigned char *curData;

    const uint8_t rateInBytes = instance->rate/8;

    assert(instance->squeezing == 0);

    i = 0;
    curData = data;
    while(i < dataByteLen) {
        if ((instance->byteIOIndex == 0) && (dataByteLen-i >= rateInBytes)) {
#ifdef KeccakP1600_12rounds_FastLoop_supported
            /* processing full blocks first */
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
            /* normal lane: using the message queue */
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

static void TurboSHAKE_AbsorbDomainSeparationByte(TurboSHAKE_Instance *instance, unsigned char D)
{
    const unsigned int rateInBytes = instance->rate/8;

    assert(D != 0);
    assert(instance->squeezing == 0);

    /* Last few bits, whose delimiter coincides with first bit of padding */
    KeccakP1600_AddByte(instance->state, D, instance->byteIOIndex);
    /* If the first bit of padding is at position rate-1, we need a whole new block for the second bit of padding */
    if ((D >= 0x80) && (instance->byteIOIndex == (rateInBytes-1)))
        KeccakP1600_Permute_12rounds(instance->state);
    /* Second bit of padding */
    KeccakP1600_AddByte(instance->state, 0x80, rateInBytes-1);
    KeccakP1600_Permute_12rounds(instance->state);
    instance->byteIOIndex = 0;
    instance->squeezing = 1;
}

static void TurboSHAKE_Squeeze(TurboSHAKE_Instance *instance, unsigned char *data, size_t dataByteLen)
{
    size_t i, j;
    unsigned int partialBlock;
    const unsigned int rateInBytes = instance->rate/8;
    unsigned char *curData;

    if (!instance->squeezing)
        TurboSHAKE_AbsorbDomainSeparationByte(instance, 0x01);

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
            /* normal lane: using the message queue */
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

/* ---------------------------------------------------------------- */

typedef enum {
    NOT_INITIALIZED,
    ABSORBING,
    FINAL,
    SQUEEZING
} KCP_Phases;
typedef KCP_Phases KangarooTwelve_Phases;

#define K12_chunkSize       8192
#define K12_suffixLeaf      0x0B /* '110': message hop, simple padding, inner node */

#ifndef KeccakP1600_disableParallelism

// TODO: create the KT256_Process{n}Leaves variations and wire them up
void KT128_Process2Leaves(const unsigned char *input, unsigned char *output);
void KT128_Process4Leaves(const unsigned char *input, unsigned char *output);
void KT128_Process8Leaves(const unsigned char *input, unsigned char *output);

#define ProcessLeaves( Parallellism, CapacityInBytes ) \
    while (inputByteLen >= Parallellism * K12_chunkSize) { \
        unsigned char intermediate[Parallellism * CapacityInBytes]; \
        \
        KangarooTwelve_Process##Parallellism##Leaves(input, intermediate); \
        input += Parallellism * K12_chunkSize; \
        inputByteLen -= Parallellism * K12_chunkSize; \
        ktInstance->blockNumber += Parallellism; \
        TurboSHAKE_Absorb(&ktInstance->finalNode, intermediate, Parallellism * CapacityInBytes); \
    }

#endif  // KeccakP1600_disableParallelism

static unsigned int right_encode(unsigned char * encbuf, size_t value)
{
    unsigned int n, i;
    size_t v;

    for (v = value, n = 0; v && (n < sizeof(size_t)); ++n, v >>= 8)
        ; /* empty */
    for (i = 1; i <= n; ++i) {
        encbuf[i-1] = (unsigned char)(value >> (8 * (n-i)));
    }
    encbuf[n] = (unsigned char)n;
    return n + 1;
}

int KangarooTwelve_Initialize(KangarooTwelve_Instance *ktInstance, size_t outputByteLen, int securityLevel)
{
    ktInstance->fixedOutputLength = outputByteLen;
    ktInstance->queueAbsorbedLen = 0;
    ktInstance->blockNumber = 0;
    ktInstance->phase = ABSORBING;
    ktInstance->securityLevel = securityLevel;
    TurboSHAKE_Initialize(&ktInstance->finalNode, securityLevel);
    return 0;
}

int KangarooTwelve_Update(KangarooTwelve_Instance *ktInstance, const unsigned char *input, size_t inputByteLen)
{
    if (ktInstance->phase != ABSORBING)
        return 1;

    if (ktInstance->blockNumber == 0) {
        /* First block, absorb in final node */
        unsigned int len = (inputByteLen < (K12_chunkSize - ktInstance->queueAbsorbedLen)) ? (unsigned int)inputByteLen : (K12_chunkSize - ktInstance->queueAbsorbedLen);
        TurboSHAKE_Absorb(&ktInstance->finalNode, input, len);
        input += len;
        inputByteLen -= len;
        ktInstance->queueAbsorbedLen += len;
        if ((ktInstance->queueAbsorbedLen == K12_chunkSize) && (inputByteLen != 0)) {
            /* First block complete and more input data available, finalize it */
            const unsigned char padding = 0x03; /* '110^6': message hop, simple padding */
            ktInstance->queueAbsorbedLen = 0;
            ktInstance->blockNumber = 1;
            TurboSHAKE_Absorb(&ktInstance->finalNode, &padding, 1);
            ktInstance->finalNode.byteIOIndex = (ktInstance->finalNode.byteIOIndex + 7) & ~7; /* Zero padding up to 64 bits */
        }
    } else if (ktInstance->queueAbsorbedLen != 0) {
        /* There is data in the queue, absorb further in queue until block complete */
        unsigned int len = (inputByteLen < (K12_chunkSize - ktInstance->queueAbsorbedLen)) ? (unsigned int)inputByteLen : (K12_chunkSize - ktInstance->queueAbsorbedLen);
        TurboSHAKE_Absorb(&ktInstance->queueNode, input, len);
        input += len;
        inputByteLen -= len;
        ktInstance->queueAbsorbedLen += len;
        if (ktInstance->queueAbsorbedLen == K12_chunkSize) {
            int capacityInBytes = 2*(ktInstance->securityLevel)/8;
            unsigned char intermediate[capacityInBytes];
            ktInstance->queueAbsorbedLen = 0;
            ++ktInstance->blockNumber;
            TurboSHAKE_AbsorbDomainSeparationByte(&ktInstance->queueNode, K12_suffixLeaf);
            TurboSHAKE_Squeeze(&ktInstance->queueNode, intermediate, capacityInBytes);
            TurboSHAKE_Absorb(&ktInstance->finalNode, intermediate, capacityInBytes);
        }
    }

#ifndef KeccakP1600_disableParallelism
    int capacityInBytes = 2*(ktInstance->securityLevel)/8;

    if (KeccakP1600times8_IsAvailable()) {
        ProcessLeaves(8, capacityInBytes);
    }

    if (KeccakP1600times4_IsAvailable()) {
        ProcessLeaves(4, capacityInBytes);
    }

    if (KeccakP1600times2_IsAvailable()) {
        ProcessLeaves(2, capacityInBytes);
    }
#endif

    while (inputByteLen > 0) {
        unsigned int len = (inputByteLen < K12_chunkSize) ? (unsigned int)inputByteLen : K12_chunkSize;
        TurboSHAKE_Initialize(&ktInstance->queueNode, ktInstance->securityLevel);
        TurboSHAKE_Absorb(&ktInstance->queueNode, input, len);
        input += len;
        inputByteLen -= len;
        if (len == K12_chunkSize) {
            int capacityInBytes = 2*(ktInstance->securityLevel)/8;
            unsigned char intermediate[capacityInBytes];
            ++ktInstance->blockNumber;
            TurboSHAKE_AbsorbDomainSeparationByte(&ktInstance->queueNode, K12_suffixLeaf);
            TurboSHAKE_Squeeze(&ktInstance->queueNode, intermediate, capacityInBytes);
            TurboSHAKE_Absorb(&ktInstance->finalNode, intermediate, capacityInBytes);
        } else {
            ktInstance->queueAbsorbedLen = len;
        }
    }

    return 0;
}

int KangarooTwelve_Final(KangarooTwelve_Instance *ktInstance, unsigned char *output, const unsigned char *customization, size_t customByteLen)
{
    unsigned char encbuf[sizeof(size_t)+1+2];
    unsigned char padding;

    if (ktInstance->phase != ABSORBING)
        return 1;

    /* Absorb customization | right_encode(customByteLen) */
    if ((customByteLen != 0) && (KangarooTwelve_Update(ktInstance, customization, customByteLen) != 0))
        return 1;
    if (KangarooTwelve_Update(ktInstance, encbuf, right_encode(encbuf, customByteLen)) != 0)
        return 1;

    if (ktInstance->blockNumber == 0) {
        /* Non complete first block in final node, pad it */
        padding = 0x07; /*  '11': message hop, final node */
    } else {
        unsigned int n;

        if (ktInstance->queueAbsorbedLen != 0) {
            /* There is data in the queue node */
            int capacityInBytes = 2*(ktInstance->securityLevel)/8;
            unsigned char intermediate[capacityInBytes];
            ++ktInstance->blockNumber;
            TurboSHAKE_AbsorbDomainSeparationByte(&ktInstance->queueNode, K12_suffixLeaf);
            TurboSHAKE_Squeeze(&ktInstance->queueNode, intermediate, capacityInBytes);
            TurboSHAKE_Absorb(&ktInstance->finalNode, intermediate, capacityInBytes);
        }
        --ktInstance->blockNumber; /* Absorb right_encode(number of Chaining Values) || 0xFF || 0xFF */
        n = right_encode(encbuf, ktInstance->blockNumber);
        encbuf[n++] = 0xFF;
        encbuf[n++] = 0xFF;
        TurboSHAKE_Absorb(&ktInstance->finalNode, encbuf, n);
        padding = 0x06; /* '01': chaining hop, final node */
    }
    TurboSHAKE_AbsorbDomainSeparationByte(&ktInstance->finalNode, padding);
    if (ktInstance->fixedOutputLength != 0) {
        ktInstance->phase = FINAL;
        TurboSHAKE_Squeeze(&ktInstance->finalNode, output, ktInstance->fixedOutputLength);
        return 0;
    }
    ktInstance->phase = SQUEEZING;
    return 0;
}

int KangarooTwelve_Squeeze(KangarooTwelve_Instance *ktInstance, unsigned char *output, size_t outputByteLen)
{
    if (ktInstance->phase != SQUEEZING)
        return 1;
    TurboSHAKE_Squeeze(&ktInstance->finalNode, output, outputByteLen);
    return 0;
}

int KangarooTwelve(const unsigned char *input, size_t inputByteLen,
                   unsigned char *output, size_t outputByteLen,
                   const unsigned char *customization, size_t customByteLen, int securityLevel)
{
    KangarooTwelve_Instance ktInstance;

    if (outputByteLen == 0)
        return 1;
    KangarooTwelve_Initialize(&ktInstance, outputByteLen, securityLevel);
    if (KangarooTwelve_Update(&ktInstance, input, inputByteLen) != 0)
        return 1;
    return KangarooTwelve_Final(&ktInstance, output, customization, customByteLen);
}

/* ---------------------------------------------------------------- */

int KT128(const unsigned char *input, size_t inputByteLen,
                   unsigned char *output, size_t outputByteLen,
                   const unsigned char *customization, size_t customByteLen)
{
    return KangarooTwelve(input, inputByteLen, output, outputByteLen, customization, customByteLen, 128);
}

int KT256(const unsigned char *input, size_t inputByteLen,
                   unsigned char *output, size_t outputByteLen,
                   const unsigned char *customization, size_t customByteLen)
{
    return KangarooTwelve(input, inputByteLen, output, outputByteLen, customization, customByteLen, 256);
}