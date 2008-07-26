// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_MD5_H__
#define BASE_MD5_H__

#include <string>

// These functions perform MD5 operations. The simplest call is MD5Sum to
// generate the MD5 sum of the given data.
//
// You can also compute the MD5 sum of data incrementally by making multiple
// calls to MD5Update:
//    MD5Context ctx; // intermediate MD5 data: do not use
//    MD5Init(&ctx);
//    MD5Update(&ctx, data1, length1);
//    MD5Update(&ctx, data2, length2);
//    ...
//
//    MD5Digest digest; // the result of the computation
//    MD5Final(&digest, &ctx);
//
// You can call MD5DigestToBase16 to generate a string of the digest.

// The output of an MD5 operation
typedef struct MD5Digest_struct {
  unsigned char a[16];
} MD5Digest;

// Used for storing intermediate data during an MD5 computation. Callers
// should not access the data.
typedef char MD5Context[88];

// Computes the MD5 sum of the given data buffer with the given length.
// The given 'digest' structure will be filled with the result data.
void MD5Sum(const void* data, size_t length, MD5Digest* digest);

// Initializes the given MD5 context structure for subsequent calls to
// MD5Update.
void MD5Init(MD5Context* context);

// For the given buffer of data, updates the given MD5 context with the sum of
// the data. You can call this any number of times during the computation,
// exept that MD5Init must have been called first.
void MD5Update(MD5Context* context, const void* buf, size_t len);

// Finalizes the MD5 operation and fills the buffer with the digest.
void MD5Final(MD5Digest* digest, MD5Context* pCtx);

// Converts a digest into human-readable hexadecimal.
std::string MD5DigestToBase16(const MD5Digest& digest);

// Returns the MD5 (in hexadecimal) of a string.
std::string MD5String(const std::string& str);

#endif // BASE_MD5_H__
