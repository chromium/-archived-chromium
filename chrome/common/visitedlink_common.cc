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

#include "chrome/common/visitedlink_common.h"

#include "base/logging.h"
#include "base/md5.h"

const VisitedLinkCommon::Fingerprint VisitedLinkCommon::null_fingerprint_ = 0;
const VisitedLinkCommon::Hash VisitedLinkCommon::null_hash_ = -1;

VisitedLinkCommon::VisitedLinkCommon() :
    hash_table_(NULL),
    table_length_(0) {
}

VisitedLinkCommon::~VisitedLinkCommon() {
}

// FIXME: this uses linear probing, it should be replaced with quadratic
// probing or something better. See VisitedLinkMaster::AddFingerprint
bool VisitedLinkCommon::IsVisited(const char* canonical_url,
                                  size_t url_len) const {
  if (url_len == 0)
    return false;
  if (!hash_table_ || table_length_ == 0) {
    // Init() will always create a table, this means somebody forgot
    NOTREACHED();
    return false;
  }
  return IsVisited(ComputeURLFingerprint(canonical_url, url_len, salt_));
}

bool VisitedLinkCommon::IsVisited(Fingerprint fingerprint) const {
  // Go through the table until we find the item or an empty spot (meaning it
  // wasn't found). This loop will terminate as long as the table isn't full,
  // which should be enforced by AddFingerprint.
  Hash first_hash = HashFingerprint(fingerprint);
  Hash cur_hash = first_hash;
  while (true) {
    Fingerprint cur_fingerprint = FingerprintAt(cur_hash);
    if (cur_fingerprint == null_fingerprint_)
      return false;  // End of probe sequence found.
    if (cur_fingerprint == fingerprint)
      return true;  // Found a match.

    // This spot was taken, but not by the item we're looking for, search in
    // the next position.
    cur_hash++;
    if (cur_hash == table_length_)
      cur_hash = 0;
    if (cur_hash == first_hash) {
      // Wrapped around and didn't find an empty space, this means we're in an
      // infinite loop because AddFingerprint didn't do its job resizing.
      NOTREACHED();
      return false;
    }
  }
}

// Uses the top 64 bits of the MD5 sum of the canonical URL as the fingerprint,
// this is as random as any other subset of the MD5SUM.
//
// FIXME: this uses the MD5SUM of the 16-bit character version. For systems where
// wchar_t is not 16 bits (Linux uses 32 bits, I think), this will not be
// compatable. We should define explicitly what should happen here across
// platforms, and convert if necessary (probably to UTF-16).

// static
VisitedLinkCommon::Fingerprint VisitedLinkCommon::ComputeURLFingerprint(
    const char* canonical_url,
    size_t url_len,
    const uint8 salt[LINK_SALT_LENGTH]) {
  DCHECK(url_len > 0) << "Canonical URLs should not be empty";

  MD5Context ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, salt, sizeof(salt));
  MD5Update(&ctx, canonical_url, url_len * sizeof(char));

  MD5Digest digest;
  MD5Final(&digest, &ctx);

  // This is the same as "return *(Fingerprint*)&digest.a;" but if we do that
  // direct cast the alignment could be wrong, and we can't access a 64-bit int
  // on arbitrary alignment on some processors. This reinterpret_casts it
  // down to a char array of the same size as fingerprint, and then does the
  // bit cast, which amounts to a memcpy. This does not handle endian issues.
  return bit_cast<Fingerprint, uint8[8]>(
      *reinterpret_cast<uint8(*)[8]>(&digest.a));
}