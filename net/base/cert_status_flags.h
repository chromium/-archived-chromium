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

#ifndef NET_BASE_CERT_STATUS_FLAGS_H_
#define NET_BASE_CERT_STATUS_FLAGS_H_

namespace net {

// Status flags, such as errors and extended validation.
enum {
  // Bits 0 to 15 are for errors.
  CERT_STATUS_ALL_ERRORS                 = 0xFFFF,
  CERT_STATUS_COMMON_NAME_INVALID        = 1 << 0,
  CERT_STATUS_DATE_INVALID               = 1 << 1,
  CERT_STATUS_AUTHORITY_INVALID          = 1 << 2,
  // 1 << 3 is reserved for ERR_CERT_CONTAINS_ERRORS (not useful with WinHTTP).
  CERT_STATUS_NO_REVOCATION_MECHANISM    = 1 << 4,
  CERT_STATUS_UNABLE_TO_CHECK_REVOCATION = 1 << 5,
  CERT_STATUS_REVOKED                    = 1 << 6,
  CERT_STATUS_INVALID                    = 1 << 7,

  // Bits 16 to 30 are for non-error statuses.
  CERT_STATUS_IS_EV                      = 1 << 16,
  CERT_STATUS_REV_CHECKING_ENABLED       = 1 << 17,

  // 1 << 31 (the sign bit) is reserved so that the cert status will never be
  // negative.
};

// Returns true if the specified cert status has an error set.
static inline bool IsCertStatusError(int status) {
  return (CERT_STATUS_ALL_ERRORS & status) != 0;
}

}  // namespace net

#endif  // NET_BASE_CERT_STATUS_FLAGS_H_
