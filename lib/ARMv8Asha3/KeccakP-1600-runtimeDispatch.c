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

ARM CPU feature detection adapted from libaegis by Frank Denis.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "KT-atomic.h"
#include "KeccakP-1600-SnP.h"

#ifdef KeccakP1600_disableParallelism
#undef KeccakP1600_enable_simd_options
#else

// Forward declarations
void KangarooTwelve_SetArmProcessorCapabilities();

#ifdef KeccakP1600_enable_simd_options
K12_ATOMIC(int) K12_NEON_requested_disabled = 0;
K12_ATOMIC(int) K12_ARM_SHA3_requested_disabled = 0;
#endif  // KeccakP1600_enable_simd_options

K12_ATOMIC(int) K12_enableNEON = 0;
K12_ATOMIC(int) K12_enableARM_SHA3 = 0;

/* ---------------------------------------------------------------- */
/* Platform-specific includes for CPU feature detection */
/* ---------------------------------------------------------------- */

#if defined(__linux__) && (defined(__aarch64__) || defined(__arm__))
#define K12_HAVE_LINUX_ARM
#if defined(__GLIBC__) || defined(__BIONIC__)
#include <sys/auxv.h>
#define K12_HAVE_GETAUXVAL
#endif
#endif

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm__))
#define K12_HAVE_APPLE_ARM
#include <sys/sysctl.h>
#endif

#if defined(_WIN32) && (defined(_M_ARM64) || defined(_M_ARM))
#define K12_HAVE_WINDOWS_ARM
#include <windows.h>
#endif

#if defined(__ANDROID__) && (defined(__aarch64__) || defined(__arm__))
#define K12_HAVE_ANDROID_ARM
#include <cpu-features.h>
#endif

/* ---------------------------------------------------------------- */
/* Hardware capability constants */
/* ---------------------------------------------------------------- */

// 32-bit ARM hwcaps (AT_HWCAP)
#ifndef K12_ARM_HWCAP_NEON
#define K12_ARM_HWCAP_NEON (1L << 12)
#endif

// AArch64 hwcaps (AT_HWCAP)
#ifndef K12_AARCH64_HWCAP_ASIMD
#define K12_AARCH64_HWCAP_ASIMD (1L << 1)
#endif

#ifndef K12_AARCH64_HWCAP_SHA3
#define K12_AARCH64_HWCAP_SHA3 (1L << 17)
#endif

/* ---------------------------------------------------------------- */
/* CPU feature detection */
/* ---------------------------------------------------------------- */

enum arm_cpu_feature {
    ARM_NEON = 1 << 0,
    ARM_SHA3 = 1 << 1,
    ARM_UNDEFINED = 1 << 30
};

static K12_ATOMIC(int) g_arm_cpu_features = ARM_UNDEFINED;

#if defined(K12_HAVE_LINUX_ARM) && defined(K12_HAVE_GETAUXVAL)
static int _have_hwcap(unsigned long hwcap_bit) {
    unsigned long hwcap = getauxval(AT_HWCAP);
    return (hwcap & hwcap_bit) != 0;
}
#endif

#if defined(K12_HAVE_APPLE_ARM)
static int _have_arm_feature(const char *feature_name) {
    int64_t feature_present = 0;
    size_t size = sizeof(feature_present);
    if (sysctlbyname(feature_name, &feature_present, &size, NULL, 0) != 0) {
        return 0;
    }
    return feature_present != 0;
}
#endif

static enum arm_cpu_feature get_arm_cpu_features(void) {
    if (g_arm_cpu_features != ARM_UNDEFINED) {
        return g_arm_cpu_features;
    }

    enum arm_cpu_feature features = 0;

    /* ---------------------------------------------------------------- */
    /* NEON Detection */
    /* ---------------------------------------------------------------- */

    // Compile-time check - if built with NEON, assume available
#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
    features |= ARM_NEON;
#elif defined(K12_HAVE_LINUX_ARM) && defined(K12_HAVE_GETAUXVAL)
    // Runtime detection on Linux ARM
#if defined(__aarch64__)
    if (_have_hwcap(K12_AARCH64_HWCAP_ASIMD)) {
        features |= ARM_NEON;
    }
#elif defined(__arm__)
    if (_have_hwcap(K12_ARM_HWCAP_NEON)) {
        features |= ARM_NEON;
    }
#endif
#elif defined(K12_HAVE_ANDROID_ARM)
    // Android detection
    uint64_t android_features = android_getCpuFeatures();
    if (android_features & ANDROID_CPU_ARM_FEATURE_NEON) {
        features |= ARM_NEON;
    }
#elif defined(K12_HAVE_WINDOWS_ARM)
    // Windows ARM64 - assume all have NEON
    features |= ARM_NEON;
#endif

    /* ---------------------------------------------------------------- */
    /* SHA3 Detection (requires NEON) */
    /* ---------------------------------------------------------------- */

    if (features & ARM_NEON) {
        // Compile-time check
#if defined(__ARM_FEATURE_SHA3)
        features |= ARM_SHA3;
#elif defined(K12_HAVE_LINUX_ARM) && defined(K12_HAVE_GETAUXVAL) && defined(__aarch64__)
        // Runtime detection on Linux AArch64
        if (_have_hwcap(K12_AARCH64_HWCAP_SHA3)) {
            features |= ARM_SHA3;
        }
#elif defined(K12_HAVE_APPLE_ARM)
        // macOS/Apple Silicon detection
        if (_have_arm_feature("hw.optional.arm.FEAT_SHA3")) {
            features |= ARM_SHA3;
        }
#endif
    }

    g_arm_cpu_features = features;
    return features;
}

void KangarooTwelve_SetArmProcessorCapabilities() {
    enum arm_cpu_feature features = get_arm_cpu_features();
    K12_enableNEON = (features & ARM_NEON) != 0;
    K12_enableARM_SHA3 = (features & ARM_SHA3) != 0;

#ifdef KeccakP1600_enable_simd_options
    K12_enableNEON = K12_enableNEON && !K12_NEON_requested_disabled;
    K12_enableARM_SHA3 = K12_enableARM_SHA3 && !K12_ARM_SHA3_requested_disabled;
#endif  // KeccakP1600_enable_simd_options
}

static void ensure_arm_capabilities(void) {
    if (g_arm_cpu_features == ARM_UNDEFINED) {
        KangarooTwelve_SetArmProcessorCapabilities();
    }
}

/* ---------------------------------------------------------------- */
/* External function declarations */
/* ---------------------------------------------------------------- */

// Generic ARM64 implementations (from KeccakP-1600-opt64.c)
extern void KeccakP1600_opt64_Initialize(void *state);
extern void KeccakP1600_opt64_AddByte(void *state, unsigned char data, unsigned int offset);
extern void KeccakP1600_opt64_AddBytes(void *state, const unsigned char *data, unsigned int offset, unsigned int length);
extern void KeccakP1600_opt64_Permute_12rounds(void *state);
extern void KeccakP1600_opt64_ExtractBytes(const void *state, unsigned char *data, unsigned int offset, unsigned int length);
extern size_t KeccakP1600_opt64_12rounds_FastLoop_Absorb(void *state, unsigned int laneCount, const unsigned char *data, size_t dataByteLen);

// ARMv8-A SHA3 optimized implementations (from assembly)
extern void KeccakP1600_ARMv8Asha3_Permute_12rounds(void *state);
extern size_t KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb(void *state, unsigned int laneCount, const unsigned char *data, size_t dataByteLen);
extern void KeccakP1600times2_ARMv8Asha3_Permute_12rounds(void *state);
extern void KT128_ARMv8Asha3_Process2Leaves(const unsigned char *input, unsigned char *output);
extern void KT256_ARMv8Asha3_Process2Leaves(const unsigned char *input, unsigned char *output);

/* ---------------------------------------------------------------- */
/* Dispatch functions for Keccak-p[1600] */
/* ---------------------------------------------------------------- */

const char * KeccakP1600_GetImplementation() {
    if (K12_enableARM_SHA3) {
        return "ARMv8-A+SHA3 optimized implementation";
    } else {
        return "Generic ARM64 implementation";
    }
}

void KeccakP1600_Initialize(void *state) {
    ensure_arm_capabilities();
    KeccakP1600_opt64_Initialize(state);  // Both use same initialization
}

void KeccakP1600_AddByte(void *state, unsigned char data, unsigned int offset) {
    KeccakP1600_opt64_AddByte(state, data, offset);  // Both use same AddByte
}

void KeccakP1600_AddBytes(void *state, const unsigned char *data, unsigned int offset, unsigned int length) {
    KeccakP1600_opt64_AddBytes(state, data, offset, length);  // Both use same AddBytes
}

void KeccakP1600_Permute_12rounds(void *state) {
    if (K12_enableARM_SHA3) {
        KeccakP1600_ARMv8Asha3_Permute_12rounds(state);
    } else {
        KeccakP1600_opt64_Permute_12rounds(state);
    }
}

void KeccakP1600_ExtractBytes(const void *state, unsigned char *data, unsigned int offset, unsigned int length) {
    KeccakP1600_opt64_ExtractBytes(state, data, offset, length);  // Both use same ExtractBytes
}

size_t KeccakP1600_12rounds_FastLoop_Absorb(void *state, unsigned int laneCount, const unsigned char *data, size_t dataByteLen) {
    if (K12_enableARM_SHA3) {
        return KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb(state, laneCount, data, dataByteLen);
    } else {
        return KeccakP1600_opt64_12rounds_FastLoop_Absorb(state, laneCount, data, dataByteLen);
    }
}

/* ---------------------------------------------------------------- */
/* Dispatch functions for Keccak-p[1600]×2 */
/* ---------------------------------------------------------------- */

int KeccakP1600times2_IsAvailable() {
    return K12_enableARM_SHA3;
}

const char * KeccakP1600times2_GetImplementation() {
    if (K12_enableARM_SHA3) {
        return "ARMv8-A+SHA3 optimized implementation";
    } else {
        return "";
    }
}

void KeccakP1600times2_Permute_12rounds(void *state) {
    if (K12_enableARM_SHA3) {
        KeccakP1600times2_ARMv8Asha3_Permute_12rounds(state);
    }
}

void KT128_Process2Leaves(const unsigned char *input, unsigned char *output) {
    if (K12_enableARM_SHA3) {
        KT128_ARMv8Asha3_Process2Leaves(input, output);
    }
}

void KT256_Process2Leaves(const unsigned char *input, unsigned char *output) {
    if (K12_enableARM_SHA3) {
        KT256_ARMv8Asha3_Process2Leaves(input, output);
    }
}

/* ---------------------------------------------------------------- */
/* Keccak-p[1600]×4 (not available on ARM) */
/* ---------------------------------------------------------------- */

int KeccakP1600times4_IsAvailable() {
    return 0;
}

const char * KeccakP1600times4_GetImplementation() {
    return "";
}

void KT128_Process4Leaves(const unsigned char *input, unsigned char *output) {
    (void)input;
    (void)output;
}

void KT256_Process4Leaves(const unsigned char *input, unsigned char *output) {
    (void)input;
    (void)output;
}

/* ---------------------------------------------------------------- */
/* Keccak-p[1600]×8 (not available on ARM) */
/* ---------------------------------------------------------------- */

int KeccakP1600times8_IsAvailable() {
    return 0;
}

const char * KeccakP1600times8_GetImplementation() {
    return "";
}

void KT128_Process8Leaves(const unsigned char *input, unsigned char *output) {
    (void)input;
    (void)output;
}

void KT256_Process8Leaves(const unsigned char *input, unsigned char *output) {
    (void)input;
    (void)output;
}

/* ---------------------------------------------------------------- */
/* Optional API for disabling CPU features */
/* ---------------------------------------------------------------- */

#ifdef KeccakP1600_enable_simd_options

int KangarooTwelve_DisableNeon(void) {
    KangarooTwelve_SetArmProcessorCapabilities();
    K12_NEON_requested_disabled = 1;
    if (K12_enableNEON) {
        KangarooTwelve_SetArmProcessorCapabilities();
        return 1;  // NEON was disabled on this call.
    } else {
        return 0;  // Nothing changed.
    }
}

int KangarooTwelve_DisableArmSha3(void) {
    KangarooTwelve_SetArmProcessorCapabilities();
    K12_ARM_SHA3_requested_disabled = 1;
    if (K12_enableARM_SHA3) {
        KangarooTwelve_SetArmProcessorCapabilities();
        return 1;  // ARM SHA3 was disabled on this call.
    } else {
        return 0;  // Nothing changed.
    }
}

void KangarooTwelve_EnableAllArmCpuFeatures(void) {
    K12_NEON_requested_disabled = 0;
    K12_ARM_SHA3_requested_disabled = 0;
    KangarooTwelve_SetArmProcessorCapabilities();
}

#endif  // KeccakP1600_enable_simd_options

#endif  // !KeccakP1600_disableParallelism
