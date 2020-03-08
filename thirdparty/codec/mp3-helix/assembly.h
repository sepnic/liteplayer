/* ***** BEGIN LICENSE BLOCK *****
 * Version: RCSL 1.0/RPSL 1.0
 *
 * Portions Copyright (c) 1995-2002 RealNetworks, Inc. All Rights Reserved.
 *
 * The contents of this file, and the files included with this file, are
 * subject to the current version of the RealNetworks Public Source License
 * Version 1.0 (the "RPSL") available at
 * http://www.helixcommunity.org/content/rpsl unless you have licensed
 * the file under the RealNetworks Community Source License Version 1.0
 * (the "RCSL") available at http://www.helixcommunity.org/content/rcsl,
 * in which case the RCSL will apply. You may also obtain the license terms
 * directly from RealNetworks.  You may not use this file except in
 * compliance with the RPSL or, if you have a valid RCSL with RealNetworks
 * applicable to this file, the RCSL.  Please see the applicable RPSL or
 * RCSL for the rights, obligations and limitations governing use of the
 * contents of the file.
 *
 * This file is part of the Helix DNA Technology. RealNetworks is the
 * developer of the Original Code and owns the copyrights in the portions
 * it created.
 *
 * This file, and the files included with this file, is distributed and made
 * available on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND REALNETWORKS HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *
 * Technology Compatibility Kit Test Suite(s) Location:
 *    http://www.helixcommunity.org/content/tck
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

/**************************************************************************************
 * Fixed-point MP3 decoder
 * Jon Recker (jrecker@real.com), Ken Cooke (kenc@real.com)
 * June 2003
 *
 * assembly.h - assembly language functions and prototypes for supported platforms
 *
 * - inline rountines with access to 64-bit multiply results
 * - x86 (_WIN32) and ARM (ARM_ADS, _WIN32_WCE) versions included
 * - some inline functions are mix of asm and C for speed
 * - some functions are in native asm files, so only the prototype is given here
 *
 * MULSHIFT32(x, y)    signed multiply of two 32-bit integers (x and y), returns top 32 bits of 64-bit result
 * FASTABS(x)          branchless absolute value of signed integer x
 * CLZ(x)              count leading zeros in x
 * MADD64(sum, x, y)   (Windows only) sum [64-bit] += x [32-bit] * y [32-bit]
 * SHL64(sum, x, y)    (Windows only) 64-bit left shift using __int64
 * SAR64(sum, x, y)    (Windows only) 64-bit right shift using __int64
 */

#ifndef _HELIX_MP3_ASSEMBLY_H_
#define _HELIX_MP3_ASSEMBLY_H_

#include <stdlib.h>

#if defined(__GNUC__) && (defined(__thumb__) || defined(__arm__))

static __inline int MULSHIFT32(int x, int y)
{
    // important rules for smull RdLo, RdHi, Rm, Rs:
    //     RdHi and Rm can't be the same register
    //     RdLo and Rm can't be the same register
    //     RdHi and RdLo can't be the same register
    // Note: Rs determines early termination (leading sign bits) so if you want to specify
    //   which operand is Rs, put it in the SECOND argument (y)
     // For inline assembly, x and y are not assumed to be R0, R1 so it shouldn't matter
    //   which one is returned. (If this were a function call, returning y (R1) would
    //   require an extra "mov r0, r1")

    int zlow;
    __asm__ ("smull %0,%1,%2,%3" : "=&r" (zlow), "=r" (y) : "r" (x), "1" (y) : "cc");
    return y;
}

#define FASTABS(x) abs(x) //FB

#define CLZ(x) __builtin_clz(x) //FB

typedef union _U64 {
    int64_t w64;
    struct {
        /* little endian */
        unsigned int lo32;
        signed int   hi32;
    } r;
} U64;

static inline int64_t MADD64(int64_t sum64, int x, int y)
{
    U64 u;
    u.w64 = sum64;
    __asm__ ("smlal %0,%1,%2,%3" : "+&r" (u.r.lo32), "+&r" (u.r.hi32) : "r" (x), "r" (y) : "cc");
    return u.w64;
}

static __inline int64_t SAR64(int64_t x, int n)
{
    return x >> n;
}

#elif defined(__amd64__) || defined(__i386__)

static __inline__ int MULSHIFT32(int x, int y)
{
    int z;
    z = (int64_t)x * (int64_t)y >> 32;
    return z;
}

static __inline int FASTABS(int x)
{
    int sign;

    sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;

    return x;
}

static __inline int CLZ(int x)
{
    int numZeros;

    if (!x)
        return 32;

    /* count leading zeros with binary search */
    numZeros = 1;
    if (!((unsigned int)x >> 16)) { numZeros += 16; x <<= 16; }
    if (!((unsigned int)x >> 24)) { numZeros +=  8; x <<=  8; }
    if (!((unsigned int)x >> 28)) { numZeros +=  4; x <<=  4; }
    if (!((unsigned int)x >> 30)) { numZeros +=  2; x <<=  2; }

    numZeros -= ((unsigned int)x >> 31);

    return numZeros;
}

/* returns 64-bit value in [edx:eax] */
static __inline int64_t MADD64(int64_t sum64, int x, int y)
{
    sum64 += (int64_t)x * (int64_t)y;
    return sum64;
}

static __inline int64_t SAR64(int64_t x, int n)
{
    return x >> n;
}

#else

#error Unsupported platform in assembly.h

#endif  /* platforms */

#endif /* _ASSEMBLY_H */
