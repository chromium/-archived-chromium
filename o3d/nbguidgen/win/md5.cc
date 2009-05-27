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

//
// This is adapted from libtomcrypt, which has been released into the
// public domain.

#include "nbguidgen/win/md5.h"
#include <memory.h>

#undef MAX
#undef MIN
#define MAX(x, y) ( ((x) > (y)) ? (x) : (y) )
#define MIN(x, y) ( ((x) < (y)) ? (x) : (y) )

#ifdef _MSC_VER

/* instrinsic rotate */
#include <stdlib.h>
#pragma intrinsic(_lrotr, _lrotl)
#define ROR(x, n) _lrotr(x, n)
#define ROL(x, n) _lrotl(x, n)

#else

/* rotates the hard way */
#define ROL(x, y) ( (((unsigned long)(x) << (unsigned long)((y) & 31)) | \
                     (((unsigned long)(x) & 0xFFFFFFFFUL) >> \
                      (unsigned long)(32-((y) & 31)))) & 0xFFFFFFFFUL)
#define ROR(x, y) ( ((((unsigned long)(x) & 0xFFFFFFFFUL) >> \
                      (unsigned long)((y) & 31)) | \
                     ((unsigned long)(x) << \
                      (unsigned long)(32 - ((y) & 31)))) & 0xFFFFFFFFUL)

#endif

#define STORE32L(x, y)                            \
     { (y)[3] = (unsigned char)(((x) >> 24)&255); \
       (y)[2] = (unsigned char)(((x) >> 16)&255); \
       (y)[1] = (unsigned char)(((x) >> 8)&255);  \
       (y)[0] = (unsigned char)((x) & 255); }

#define LOAD32L(x, y)                            \
     { x = ((unsigned long)((y)[3] & 255)<<24) | \
           ((unsigned long)((y)[2] & 255)<<16) | \
           ((unsigned long)((y)[1] & 255)<<8)  | \
           ((unsigned long)((y)[0] & 255)); }

#define STORE64L(x, y)                          \
     { (y)[7] = (unsigned char)(((x) >> 56) & 255); \
       (y)[6] = (unsigned char)(((x) >> 48) & 255); \
       (y)[5] = (unsigned char)(((x) >> 40) & 255); \
       (y)[4] = (unsigned char)(((x) >> 32) & 255); \
       (y)[3] = (unsigned char)(((x) >> 24) & 255); \
       (y)[2] = (unsigned char)(((x) >> 16) & 255); \
       (y)[1] = (unsigned char)(((x) >> 8) & 255);  \
       (y)[0] = (unsigned char)((x) & 255); }

#define F(x, y, z)  (z ^ (x & (y ^ z)))
#define G(x, y, z)  (y ^ (z & (y ^ x)))
#define H(x, y, z)  (x^y^z)
#define I(x, y, z)  (y^(x|(~z)))

#define FF(a, b, c, d, M, s, t) \
  a = (a + F(b, c, d) + M + t); \
  a = ROL(a, s) + b;

#define GG(a, b, c, d, M, s, t) \
  a = (a + G(b, c, d) + M + t); \
  a = ROL(a, s) + b;

#define HH(a, b, c, d, M, s, t) \
  a = (a + H(b, c, d) + M + t); \
  a = ROL(a, s) + b;

#define II(a, b, c, d, M, s, t) \
  a = (a + I(b, c, d) + M + t); \
  a = ROL(a, s) + b;

static void md5_compress(md5_t *md) {
  unsigned long i, W[16], a, b, c, d;

  /* copy the state into 512-bits into W[0..15] */
  for (i = 0; i < 16; i++) {
    LOAD32L(W[i], md->buf + (4*i));
  }

  /* copy state */
  a = md->state[0];
  b = md->state[1];
  c = md->state[2];
  d = md->state[3];

  FF(a, b, c, d, W[0], 7, 0xd76aa478UL)
  FF(d, a, b, c, W[1], 12, 0xe8c7b756UL)
  FF(c, d, a, b, W[2], 17, 0x242070dbUL)
  FF(b, c, d, a, W[3], 22, 0xc1bdceeeUL)
  FF(a, b, c, d, W[4], 7, 0xf57c0fafUL)
  FF(d, a, b, c, W[5], 12, 0x4787c62aUL)
  FF(c, d, a, b, W[6], 17, 0xa8304613UL)
  FF(b, c, d, a, W[7], 22, 0xfd469501UL)
  FF(a, b, c, d, W[8], 7, 0x698098d8UL)
  FF(d, a, b, c, W[9], 12, 0x8b44f7afUL)
  FF(c, d, a, b, W[10], 17, 0xffff5bb1UL)
  FF(b, c, d, a, W[11], 22, 0x895cd7beUL)
  FF(a, b, c, d, W[12], 7, 0x6b901122UL)
  FF(d, a, b, c, W[13], 12, 0xfd987193UL)
  FF(c, d, a, b, W[14], 17, 0xa679438eUL)
  FF(b, c, d, a, W[15], 22, 0x49b40821UL)
  GG(a, b, c, d, W[1], 5, 0xf61e2562UL)
  GG(d, a, b, c, W[6], 9, 0xc040b340UL)
  GG(c, d, a, b, W[11], 14, 0x265e5a51UL)
  GG(b, c, d, a, W[0], 20, 0xe9b6c7aaUL)
  GG(a, b, c, d, W[5], 5, 0xd62f105dUL)
  GG(d, a, b, c, W[10], 9, 0x02441453UL)
  GG(c, d, a, b, W[15], 14, 0xd8a1e681UL)
  GG(b, c, d, a, W[4], 20, 0xe7d3fbc8UL)
  GG(a, b, c, d, W[9], 5, 0x21e1cde6UL)
  GG(d, a, b, c, W[14], 9, 0xc33707d6UL)
  GG(c, d, a, b, W[3], 14, 0xf4d50d87UL)
  GG(b, c, d, a, W[8], 20, 0x455a14edUL)
  GG(a, b, c, d, W[13], 5, 0xa9e3e905UL)
  GG(d, a, b, c, W[2], 9, 0xfcefa3f8UL)
  GG(c, d, a, b, W[7], 14, 0x676f02d9UL)
  GG(b, c, d, a, W[12], 20, 0x8d2a4c8aUL)
  HH(a, b, c, d, W[5], 4, 0xfffa3942UL)
  HH(d, a, b, c, W[8], 11, 0x8771f681UL)
  HH(c, d, a, b, W[11], 16, 0x6d9d6122UL)
  HH(b, c, d, a, W[14], 23, 0xfde5380cUL)
  HH(a, b, c, d, W[1], 4, 0xa4beea44UL)
  HH(d, a, b, c, W[4], 11, 0x4bdecfa9UL)
  HH(c, d, a, b, W[7], 16, 0xf6bb4b60UL)
  HH(b, c, d, a, W[10], 23, 0xbebfbc70UL)
  HH(a, b, c, d, W[13], 4, 0x289b7ec6UL)
  HH(d, a, b, c, W[0], 11, 0xeaa127faUL)
  HH(c, d, a, b, W[3], 16, 0xd4ef3085UL)
  HH(b, c, d, a, W[6], 23, 0x04881d05UL)
  HH(a, b, c, d, W[9], 4, 0xd9d4d039UL)
  HH(d, a, b, c, W[12], 11, 0xe6db99e5UL)
  HH(c, d, a, b, W[15], 16, 0x1fa27cf8UL)
  HH(b, c, d, a, W[2], 23, 0xc4ac5665UL)
  II(a, b, c, d, W[0], 6, 0xf4292244UL)
  II(d, a, b, c, W[7], 10, 0x432aff97UL)
  II(c, d, a, b, W[14], 15, 0xab9423a7UL)
  II(b, c, d, a, W[5], 21, 0xfc93a039UL)
  II(a, b, c, d, W[12], 6, 0x655b59c3UL)
  II(d, a, b, c, W[3], 10, 0x8f0ccc92UL)
  II(c, d, a, b, W[10], 15, 0xffeff47dUL)
  II(b, c, d, a, W[1], 21, 0x85845dd1UL)
  II(a, b, c, d, W[8], 6, 0x6fa87e4fUL)
  II(d, a, b, c, W[15], 10, 0xfe2ce6e0UL)
  II(c, d, a, b, W[6], 15, 0xa3014314UL)
  II(b, c, d, a, W[13], 21, 0x4e0811a1UL)
  II(a, b, c, d, W[4], 6, 0xf7537e82UL)
  II(d, a, b, c, W[11], 10, 0xbd3af235UL)
  II(c, d, a, b, W[2], 15, 0x2ad7d2bbUL)
  II(b, c, d, a, W[9], 21, 0xeb86d391UL)

  md->state[0] = md->state[0] + a;
  md->state[1] = md->state[1] + b;
  md->state[2] = md->state[2] + c;
  md->state[3] = md->state[3] + d;
}

void MD5Init(md5_t *md) {
  md->state[0] = 0x67452301UL;
  md->state[1] = 0xefcdab89UL;
  md->state[2] = 0x98badcfeUL;
  md->state[3] = 0x10325476UL;
  md->curlen = 0;
  md->length = 0;
}

void MD5Process(md5_t *md, const unsigned char *buf, unsigned long len) {
  unsigned long n;
  while (len > 0) {
    n = MIN(len, (64 - md->curlen));
    memcpy(md->buf + md->curlen, buf, (size_t)n);
    md->curlen += n;
    buf += n;
    len -= n;

    /* is 64 bytes full? */
    if (md->curlen == 64) {
      md5_compress(md);
      md->length += 512;
      md->curlen = 0;
    }
  }
}

void MD5Finish(md5_t *md, unsigned char *hash) {
  int i;

  /* increase the length of the message */
  md->length += md->curlen * 8;

  /* append the '1' bit */
  md->buf[md->curlen++] = (unsigned char)0x80;

  /* if the length is currently above 56 bytes we append zeros
  * then compress.  Then we can fall back to padding zeros and length
  * encoding like normal.
  */
  if (md->curlen > 56) {
    while (md->curlen < 64) {
      md->buf[md->curlen++] = (unsigned char)0;
    }
    md5_compress(md);
    md->curlen = 0;
  }

  /* pad upto 56 bytes of zeroes */
  while (md->curlen < 56) {
    md->buf[md->curlen++] = (unsigned char)0;
  }

  /* store length */
  STORE64L(md->length, md->buf+56);
  md5_compress(md);

  /* copy output */
  for (i = 0; i < 4; i++) {
    STORE32L(md->state[i], hash+(4*i));
  }
}
