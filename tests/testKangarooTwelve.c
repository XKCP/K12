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

#include "KangarooTwelve.h"
#include "KeccakP-1600-SnP.h"

/* #define OUTPUT */
/* #define VERBOSE */

#define SnP_width               1600
#define inputByteSize           (80*1024)
#define outputByteSize          256
#define customizationByteSize   32
#define checksumByteSize        16
#define cChunkSize              8192

#if (defined(OUTPUT) || defined(VERBOSE) || !defined(EMBEDDED))
#include <stdio.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(EMBEDDED)
static void assert(int condition)
{
    if (!condition)
    {
        for ( ; ; ) ;
    }
}
#else
#include <assert.h>
#endif

static void generateSimpleRawMaterial(unsigned char* data, unsigned int length, unsigned char seed1, unsigned int seed2)
{
    unsigned int i;

    for(i=0; i<length; i++) {
        unsigned char iRolled;
        unsigned char byte;
        seed2 = seed2 % 8;
        iRolled = ((unsigned char)i << seed2) | ((unsigned char)i >> (8-seed2));
        byte = seed1 + 161*length - iRolled + i;
        data[i] = byte;
    }
}

static void performTestKangarooTwelveOneInput(unsigned int inputLen, unsigned int outputLen, unsigned int customLen, KangarooTwelve_Instance *pSpongeChecksum, unsigned int mode, unsigned int useSqueeze)
{
    unsigned char input[inputByteSize];
    unsigned char output[outputByteSize];
    unsigned char customization[customizationByteSize];
    int result;
    unsigned int i;

    generateSimpleRawMaterial(customization, customizationByteSize, customLen, 97);
    generateSimpleRawMaterial(input, inputLen, outputLen, inputLen + customLen);

    #ifdef VERBOSE
    printf( "outputLen %5u, inputLen %5u, customLen %3u\n", outputLen, inputLen, customLen);
    #endif
    if (!useSqueeze)
    {
        if (mode == 0)
        {
            /* Input/Output full size in one call */
            result = KangarooTwelve( input, inputLen, output, outputLen, customization, customLen );
            assert(result == 0);
        }
        else if (mode == 1)
        {
            /* Input/Output one byte per call */
            KangarooTwelve_Instance kt;
            result = KangarooTwelve_Initialize(&kt, outputLen);
            assert(result == 0);
            for (i = 0; i < inputLen; ++i )
            {
                result = KangarooTwelve_Update(&kt, input + i, 1);
                assert(result == 0);
            }
            result =  KangarooTwelve_Final(&kt, output, customization, customLen );
            assert(result == 0);
        }
        else if (mode == 2)
        {
            /* Input/Output random number of bytes per call */
            KangarooTwelve_Instance kt;
            unsigned char *pInput = input;
            result = KangarooTwelve_Initialize(&kt, outputLen);
            assert(result == 0);
            while (inputLen)
            {
                unsigned int len = ((rand() << 15) ^ rand()) % (inputLen + 1);
                result = KangarooTwelve_Update(&kt, pInput, len);
                assert(result == 0);
                pInput += len;
                inputLen -= len;
            }
            result =  KangarooTwelve_Final(&kt, output, customization, customLen);
            assert(result == 0);
        }
    }
    else
    {
        if (mode == 0)
        {
            KangarooTwelve_Instance kt;
            result = KangarooTwelve_Initialize(&kt, 0);
            assert(result == 0);
            result = KangarooTwelve_Update(&kt, input, inputLen);
            assert(result == 0);
            result = KangarooTwelve_Final(&kt, 0, customization, customLen);
            assert(result == 0);
            result = KangarooTwelve_Squeeze(&kt, output, outputLen);
            assert(result == 0);
        }
        else if (mode == 1)
        {
            KangarooTwelve_Instance kt;
            result = KangarooTwelve_Initialize(&kt, 0);
            assert(result == 0);
            result = KangarooTwelve_Update(&kt, input, inputLen);
            assert(result == 0);
            result = KangarooTwelve_Final(&kt, 0, customization, customLen);
            assert(result == 0);

            for (i = 0; i < outputLen; ++i)
            {
                result =  KangarooTwelve_Squeeze(&kt, output + i, 1);
                assert(result == 0);
            }
        }
        else if (mode == 2)
        {
            KangarooTwelve_Instance kt;
            unsigned int len;
            result = KangarooTwelve_Initialize(&kt, 0);
            assert(result == 0);
            result = KangarooTwelve_Update(&kt, input, inputLen);
            assert(result == 0);
            result = KangarooTwelve_Final(&kt, 0, customization, customLen);
            assert(result == 0);

            for (i = 0; i < outputLen; i += len)
            {
                len = ((rand() << 15) ^ rand()) % ((outputLen-i) + 1);
                result = KangarooTwelve_Squeeze(&kt, output+i, len);
                assert(result == 0);
            }
        }
    }

    #ifdef VERBOSE
    {
        unsigned int i;

        printf("KangarooTwelve\n");
        printf("Input of %d bytes:", inputLen);
        for(i=0; (i<inputLen) && (i<16); i++)
            printf(" %02x", (int)input[i]);
        if (inputLen > 16)
            printf(" ...");
        printf("\n");
        printf("Output of %d bytes:", outputLen);
        for(i=0; i<outputLen; i++)
            printf(" %02x", (int)output[i]);
        printf("\n\n");
        fflush(stdout);
    }
    #endif

    KangarooTwelve_Update(pSpongeChecksum, output, outputLen);
}

static void performTestKangarooTwelve(unsigned char *checksum, unsigned int mode, unsigned int useSqueeze)
{
    unsigned int inputLen, outputLen, customLen;

    /* Acumulated test vector */
    KangarooTwelve_Instance spongeChecksum;
    KangarooTwelve_Initialize(&spongeChecksum, 0);

    outputLen = 256/8;
    customLen = 0;
    for(inputLen=0; inputLen<=cChunkSize*9+123; inputLen++) {
        assert(inputLen <= inputByteSize);
        performTestKangarooTwelveOneInput(inputLen, outputLen, customLen, &spongeChecksum, mode, useSqueeze);
    }
    
    for(outputLen = 128/8; outputLen <= 512/8; outputLen <<= 1)
    for(inputLen = 0; inputLen <= (3*cChunkSize) && inputLen <= inputByteSize; inputLen = inputLen ? (inputLen + 167) : 1)
    for(customLen = 0; customLen <= customizationByteSize; customLen += 7) 
    {
        assert(inputLen <= inputByteSize);
        performTestKangarooTwelveOneInput(inputLen, outputLen, customLen, &spongeChecksum, 0, useSqueeze);
    }
    KangarooTwelve_Final(&spongeChecksum, 0, (const unsigned char *)"", 0);
    KangarooTwelve_Squeeze(&spongeChecksum, checksum, checksumByteSize);

    #ifdef VERBOSE
    {
        unsigned int i;
        printf("KangarooTwelve\n" );
        printf("Checksum: ");
        for(i=0; i<checksumByteSize; i++)
            printf("\\x%02x", (int)checksum[i]);
        printf("\n\n");
    }
    #endif
}

void selfTestKangarooTwelve(const unsigned char *expected)
{
    unsigned char checksum[checksumByteSize];
    unsigned int mode, useSqueeze;

    for(useSqueeze = 0; useSqueeze <= 1; ++useSqueeze)
    for(mode = 0; mode <= 2; ++mode) {
        #ifndef EMBEDDED
        printf("Testing KangarooTwelve %u %u...", useSqueeze, mode);
        fflush(stdout);
        #endif
        performTestKangarooTwelve(checksum, mode, useSqueeze);
        assert(memcmp(expected, checksum, checksumByteSize) == 0);
        #ifndef EMBEDDED
        printf(" - OK.\n");
        #endif
    }
}

#ifdef OUTPUT
void writeTestKangarooTwelveOne(FILE *f)
{
    unsigned char checksum[checksumByteSize];
    unsigned int offset;

    performTestKangarooTwelve(checksum, 0, 0);
    fprintf(f, "    selfTestKangarooTwelve(\"");
    for(offset=0; offset<checksumByteSize; offset++)
        fprintf(f, "\\x%02x", checksum[offset]);
    fprintf(f, "\");\n");
}

void writeTestKangarooTwelve(const char *filename)
{
    FILE *f = fopen(filename, "w");
    assert(f != NULL);
    writeTestKangarooTwelveOne(f);
    fclose(f);
}
#endif

static void outputHex(const unsigned char *data, unsigned char length)
{
    #ifndef EMBEDDED
    unsigned int i;
    for(i=0; i<length; i++)
        printf("%02x ", (int)data[i]);
    printf("\n\n");
    #endif
}

void printKangarooTwelveTestVectors()
{
    unsigned char *M, *C;
    unsigned char output[10032];
    unsigned int i, j, l;

    printf("KangarooTwelve(M=empty, C=empty, 32 output bytes):\n");
    KangarooTwelve(0, 0, output, 32, 0, 0);
    outputHex(output, 32);
    printf("KangarooTwelve(M=empty, C=empty, 64 output bytes):\n");
    KangarooTwelve(0, 0, output, 64, 0, 0);
    outputHex(output, 64);
    printf("KangarooTwelve(M=empty, C=empty, 10032 output bytes), last 32 bytes:\n");
    KangarooTwelve(0, 0, output, 10032, 0, 0);
    outputHex(output+10000, 32);
    for(l=1, i=0; i<7; i++, l=l*17) {
        M = malloc(l);
        for(j=0; j<l; j++)
            M[j] = j%251;
        printf("KangarooTwelve(M=pattern 0x00 to 0xFA for 17^%d bytes, C=empty, 32 output bytes):\n", i);
        KangarooTwelve(M, l, output, 32, 0, 0);
        outputHex(output, 32);
        free(M);
    }
    for(l=1, i=0; i<4; i++, l=l*41) {
        unsigned int ll = (1 << i)-1;
        M = malloc(ll);
        memset(M, 0xFF, ll);
        C = malloc(l);
        for(j=0; j<l; j++)
            C[j] = j%251;
        printf("KangarooTwelve(M=%d times byte 0xFF, C=pattern 0x00 to 0xFA for 41^%d bytes, 32 output bytes):\n", ll, i);
        KangarooTwelve(M, ll, output, 32, C, l);
        outputHex(output, 32);
        free(M);
        free(C);
    }
}

#ifndef KeccakP1600_disableParallelism
void testKangarooTwelveWithChangingCpuFeatures()
{
    uint8_t M[289];
    uint8_t output[32];
    const size_t l = 289;
    const uint8_t expected[32] = {
        0x0c, 0x31, 0x5e, 0xbc, 0xde, 0xdb, 0xf6, 0x14, 0x26, 0xde, 0x7d, 0xcf, 0x8f, 0xb7, 0x25, 0xd1,
        0xe7, 0x46, 0x75, 0xd7, 0xf5, 0x32, 0x7a, 0x50, 0x67, 0xf3, 0x67, 0xb1, 0x08, 0xec, 0xb6, 0x7c };
    KangarooTwelve_Instance k12;

    printf("\n * Testing KangarooTwelve interleaved with changing CPU features\n");
    for(size_t j=0; j<l; j++)
        M[j] = j%251;
    KangarooTwelve_Initialize(&k12, 32);
    for(size_t j=0; j<l; j++) {
        // Pseudo-randomly switch on/off the CPU features
        uint8_t features = expected[j % 32] ^ M[j];
        KangarooTwelve_EnableAllCpuFeatures();
        if (features & 1) KangarooTwelve_DisableAVX512();
        if (features & 2) KangarooTwelve_DisableAVX2();
        if (features & 4) KangarooTwelve_DisableSSSE3();
        KangarooTwelve_Update(&k12, &M[j], 1);
    }
    KangarooTwelve_Final(&k12, output, "", 0);
    assert(memcmp(expected, output, 32) == 0);
    printf("   - OK\n");
}
#endif

void testKangarooTwelve(void)
{
#ifdef OUTPUT
    printKangarooTwelveTestVectors();
    writeTestKangarooTwelve("KangarooTwelve.txt");
#endif
    const unsigned char* checksum = (const unsigned char*)"\x61\x4d\x7a\xf8\xd5\xcc\xd0\xe1\x02\x53\x7d\x21\x5e\x39\x05\xed";

#ifndef KeccakP1600_disableParallelism
    // Read feature availability
    KangarooTwelve_EnableAllCpuFeatures();
    int cpu_has_AVX512 = KangarooTwelve_DisableAVX512();
    int cpu_has_AVX2 = KangarooTwelve_DisableAVX2();
    int cpu_has_SSSE3 = KangarooTwelve_DisableSSSE3();

    // Test without vectorization
    printf(" - Testing without vectorization:\n");
#endif
    selfTestKangarooTwelve(checksum);

#ifndef KeccakP1600_disableParallelism
    // Test with SSSE3 only if it's available
    if (cpu_has_SSSE3) {
        printf("\n - Testing with SSSE3 enabled:\n");
        KangarooTwelve_EnableAllCpuFeatures();
        KangarooTwelve_DisableAVX512();
        KangarooTwelve_DisableAVX2();
        selfTestKangarooTwelve(checksum);
    }
    // Test with SSSE3 and AVX2 if they're available
    if (cpu_has_AVX2) {
        printf("\n - Testing with AVX2 enabled:\n");
        KangarooTwelve_EnableAllCpuFeatures();
        KangarooTwelve_DisableAVX512();
        selfTestKangarooTwelve(checksum);
    }
    // Finally, test with everything enabled if we have AVX512
    if (cpu_has_AVX512) {
        printf("\n - Testing with AVX512 enabled:\n");
        KangarooTwelve_EnableAllCpuFeatures();
        selfTestKangarooTwelve(checksum);
    }

    // Test with changing features
    testKangarooTwelveWithChangingCpuFeatures();
#endif
}
