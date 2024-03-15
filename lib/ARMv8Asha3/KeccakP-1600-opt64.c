/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

The Keccak-p permutations, designed by Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche.

Implementation by Gilles Van Assche and Ronny Van Keer, hereby denoted as "the implementer".

For more information, feedback or questions, please refer to the Keccak Team website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/

---

Please refer to the XKCP for more details.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <KeccakP-1600-SnP.h>

const char * KeccakP1600_GetImplementation()
{
    return "ARMv8-A+SHA3 optimized implementation";
}

/* ---------------------------------------------------------------- */

void KeccakP1600_opt64_Initialize(void *state)
{
    memset(state, 0, 200);
}

/* ---------------------------------------------------------------- */

void KeccakP1600_opt64_AddBytesInLane(void *state, unsigned int lanePosition, const unsigned char *data, unsigned int offset, unsigned int length)
{
    uint64_t lane;

    if (length == 0)
        return;
    if (length == 1)
        lane = data[0];
    else {
        lane = 0;
        memcpy(&lane, data, length);
    }
    lane <<= offset*8;
    ((uint64_t*)state)[lanePosition] ^= lane;
}

/* ---------------------------------------------------------------- */

static void KeccakP1600_opt64_AddLanes(void *state, const unsigned char *data, unsigned int laneCount)
{
    unsigned int i = 0;

    for( ; (i+8)<=laneCount; i+=8) {
        ((uint64_t*)state)[i+0] ^= ((uint64_t*)data)[i+0];
        ((uint64_t*)state)[i+1] ^= ((uint64_t*)data)[i+1];
        ((uint64_t*)state)[i+2] ^= ((uint64_t*)data)[i+2];
        ((uint64_t*)state)[i+3] ^= ((uint64_t*)data)[i+3];
        ((uint64_t*)state)[i+4] ^= ((uint64_t*)data)[i+4];
        ((uint64_t*)state)[i+5] ^= ((uint64_t*)data)[i+5];
        ((uint64_t*)state)[i+6] ^= ((uint64_t*)data)[i+6];
        ((uint64_t*)state)[i+7] ^= ((uint64_t*)data)[i+7];
    }
    for( ; (i+4)<=laneCount; i+=4) {
        ((uint64_t*)state)[i+0] ^= ((uint64_t*)data)[i+0];
        ((uint64_t*)state)[i+1] ^= ((uint64_t*)data)[i+1];
        ((uint64_t*)state)[i+2] ^= ((uint64_t*)data)[i+2];
        ((uint64_t*)state)[i+3] ^= ((uint64_t*)data)[i+3];
    }
    for( ; (i+2)<=laneCount; i+=2) {
        ((uint64_t*)state)[i+0] ^= ((uint64_t*)data)[i+0];
        ((uint64_t*)state)[i+1] ^= ((uint64_t*)data)[i+1];
    }
    if (i<laneCount) {
        ((uint64_t*)state)[i+0] ^= ((uint64_t*)data)[i+0];
    }
}

/* ---------------------------------------------------------------- */

void KeccakP1600_opt64_AddByte(void *state, unsigned char byte, unsigned int offset)
{
    ((unsigned char*)(state))[offset] ^= byte;
}

/* ---------------------------------------------------------------- */

#define SnP_AddBytes(state, data, offset, length, SnP_AddLanes, SnP_AddBytesInLane, SnP_laneLengthInBytes) \
    { \
        if ((offset) == 0) { \
            SnP_AddLanes(state, data, (length)/SnP_laneLengthInBytes); \
            SnP_AddBytesInLane(state, \
                (length)/SnP_laneLengthInBytes, \
                (data)+((length)/SnP_laneLengthInBytes)*SnP_laneLengthInBytes, \
                0, \
                (length)%SnP_laneLengthInBytes); \
        } \
        else { \
            unsigned int _sizeLeft = (length); \
            unsigned int _lanePosition = (offset)/SnP_laneLengthInBytes; \
            unsigned int _offsetInLane = (offset)%SnP_laneLengthInBytes; \
            const unsigned char *_curData = (data); \
            while(_sizeLeft > 0) { \
                unsigned int _bytesInLane = SnP_laneLengthInBytes - _offsetInLane; \
                if (_bytesInLane > _sizeLeft) \
                    _bytesInLane = _sizeLeft; \
                SnP_AddBytesInLane(state, _lanePosition, _curData, _offsetInLane, _bytesInLane); \
                _sizeLeft -= _bytesInLane; \
                _lanePosition++; \
                _offsetInLane = 0; \
                _curData += _bytesInLane; \
            } \
        } \
    }

void KeccakP1600_opt64_AddBytes(void *state, const unsigned char *data, unsigned int offset, unsigned int length)
{
    SnP_AddBytes(state, data, offset, length, KeccakP1600_opt64_AddLanes, KeccakP1600_opt64_AddBytesInLane, 8);
}

/* ---------------------------------------------------------------- */

void KeccakP1600_opt64_ExtractBytesInLane(const void *state, unsigned int lanePosition, unsigned char *data, unsigned int offset, unsigned int length)
{
    uint64_t lane = ((uint64_t*)state)[lanePosition];
    {
        uint64_t lane1[1];
        lane1[0] = lane;
        memcpy(data, (uint8_t*)lane1+offset, length);
    }
}

/* ---------------------------------------------------------------- */

void KeccakP1600_opt64_ExtractLanes(const void *state, unsigned char *data, unsigned int laneCount)
{
    memcpy(data, state, laneCount*8);
}

/* ---------------------------------------------------------------- */

#define SnP_ExtractBytes(state, data, offset, length, SnP_ExtractLanes, SnP_ExtractBytesInLane, SnP_laneLengthInBytes) \
    { \
        if ((offset) == 0) { \
            SnP_ExtractLanes(state, data, (length)/SnP_laneLengthInBytes); \
            SnP_ExtractBytesInLane(state, \
                (length)/SnP_laneLengthInBytes, \
                (data)+((length)/SnP_laneLengthInBytes)*SnP_laneLengthInBytes, \
                0, \
                (length)%SnP_laneLengthInBytes); \
        } \
        else { \
            unsigned int _sizeLeft = (length); \
            unsigned int _lanePosition = (offset)/SnP_laneLengthInBytes; \
            unsigned int _offsetInLane = (offset)%SnP_laneLengthInBytes; \
            unsigned char *_curData = (data); \
            while(_sizeLeft > 0) { \
                unsigned int _bytesInLane = SnP_laneLengthInBytes - _offsetInLane; \
                if (_bytesInLane > _sizeLeft) \
                    _bytesInLane = _sizeLeft; \
                SnP_ExtractBytesInLane(state, _lanePosition, _curData, _offsetInLane, _bytesInLane); \
                _sizeLeft -= _bytesInLane; \
                _lanePosition++; \
                _offsetInLane = 0; \
                _curData += _bytesInLane; \
            } \
        } \
    }

void KeccakP1600_opt64_ExtractBytes(const void *state, unsigned char *data, unsigned int offset, unsigned int length)
{
    SnP_ExtractBytes(state, data, offset, length, KeccakP1600_opt64_ExtractLanes, KeccakP1600_opt64_ExtractBytesInLane, 8);
}

/* ---------------------------------------------------------------- */

/* Keccak-p[1600]×2 */

int KeccakP1600times2_IsAvailable()
{
    return 1;
}

const char * KeccakP1600times2_GetImplementation()
{
    return "ARMv8-A+SHA3 optimized implementation";
}

/* Keccak-p[1600]×4 */

int KeccakP1600times4_IsAvailable()
{
    return 0;
}

const char * KeccakP1600times4_GetImplementation()
{
    return "";
}

void KT128_Process4Leaves(const unsigned char *input, unsigned char *output)
{
}

void KT256_Process4Leaves(const unsigned char *input, unsigned char *output)
{
}

/* Keccak-p[1600]×8 */

int KeccakP1600times8_IsAvailable()
{
    return 0;
}

const char * KeccakP1600times8_GetImplementation()
{
    return "";
}

void KT128_Process8Leaves(const unsigned char *input, unsigned char *output)
{
}

void KT256_Process8Leaves(const unsigned char *input, unsigned char *output)
{
}
