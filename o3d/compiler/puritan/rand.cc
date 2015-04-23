/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include <cstddef>
#include <iostream>
#include "rand.h"
#include "puritan_assert.h"

namespace Salem 
{
namespace Puritan 
{
static const unsigned a = 1103515245 ;
static const unsigned b = 12345;

// Convention:
// If seed passed in is zero, this class will use the strong random number
// generator from crypto API via CryptGenRandom()
// Otherwise fallback to the linear-congruential generator.
Rand::Rand (unsigned seed) : y (seed+1231)
{
  crypt_provider_ = NULL;
  available_ = 0;
  if (seed == 0)
    InitializeProvider();
}

unsigned Rand::rnd_uint(void) 
{
    unsigned result = 0;
    // If there is no provider, use the LCG.
    if (crypt_provider_ == NULL) {
        y = (a * y + b);
        result = y >> 4;
    }
#ifdef _WIN32
  // The following code is used for improved random-number generation.
  // TODO: Investigate use of openssl, which may do this on all 
  // platforms, but will still require platform-specific seeding.
    else {
        // Maintain a cache of random values by getting a large batch 
        // on each call, since calling CryptGenRandom() is expensive.
        // Note: This code is not thread-safe, neither is the original LCG.
        // Use InterlockedDecrement/CompareXchg etc. if that has to change.
        long chosen_index = --available_;

        if (chosen_index < 0) {
            // We are out of values, time to replenish the cache.
            ::CryptGenRandom(crypt_provider_, sizeof(cached_numbers_),
                             reinterpret_cast<BYTE*>(cached_numbers_));
            available_ = kCacheSize;
            chosen_index = --available_;
        }

        result = cached_numbers_[chosen_index];
    }
#endif

    return result;
}

double Rand::rnd_flt()
{
    unsigned r = rnd_uint();
    // We don't ask for the full range of random numbers because 
    // I see different rounding behavior on different machines at the limit.
    unsigned mask = 0xffffff;
    unsigned div = mask + 1;
    double dr = r & mask;
    double res =  dr / div;
    return res;
}

int  Rand::range (int from, int to)
{
    PURITAN_ASSERT (from <= to, "Range is malformed");
    if (from == to)
    {
        return from;
    }
    double x = (rnd_flt ()) * (to - from) + from;
    int r = static_cast <int>(x);
    return r;
}

unsigned Rand::urange (unsigned from, unsigned to)
{
    return range (from, to);
}

size_t Rand::srange (size_t from, size_t to)
{
    return range (static_cast <int>(from),
            static_cast <int>(to));
}

// Return random string from list of strings.
const char *Rand::from_list (const char **list)
{
    unsigned int i;
    for (i = 0; list[i]; i++)
        ;
    unsigned rn = range (0, i);
    return list[rn];
}

bool Rand::InitializeProvider() {
#ifdef _WIN32
    int result = ::CryptAcquireContext(& crypt_provider_, NULL, NULL,
                                       PROV_RSA_FULL,
                                       CRYPT_VERIFYCONTEXT);
    return (result != 0);
#else
    // If there is no cryptographic PRNG, this is a noop.
    return true;
#endif
}

}
}
