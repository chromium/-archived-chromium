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

#ifndef NET_BASE_LOAD_FLAGS_H__
#define NET_BASE_LOAD_FLAGS_H__

namespace net {

// These flags provide metadata about the type of the load request.  They are
// intended to be OR'd together.
enum {
  LOAD_NORMAL = 0,

  // This is "normal reload", meaning an if-none-match/if-modified-since query
  LOAD_VALIDATE_CACHE = 1 << 0,

  // This is "shift-reload", meaning a "pragma: no-cache" end-to-end fetch
  LOAD_BYPASS_CACHE = 1 << 1,

  // This is a back/forward style navigation where the cached content should
  // be preferred over any protocol specific cache validation.
  LOAD_PREFERRING_CACHE = 1 << 2,

  // This is a navigation that will fail if it cannot serve the requested
  // resource from the cache (or some equivalent local store).
  LOAD_ONLY_FROM_CACHE = 1 << 3,

  // This is a navigation that will not use the cache at all.  It does not
  // impact the HTTP request headers.
  LOAD_DISABLE_CACHE = 1 << 4,

  // This is a navigation that will not be intercepted by any registered
  // URLRequest::Interceptors.
  LOAD_DISABLE_INTERCEPT = 1 << 5,

  // If present, upload progress messages should be provided to initiator.
  LOAD_ENABLE_UPLOAD_PROGRESS = 1 << 6,

  // If present, ignores certificate mismatches with the domain name.
  // (The default behavior is to trigger an OnSSLCertificateError callback.)
  LOAD_IGNORE_CERT_COMMON_NAME_INVALID = 1 << 8,

  // If present, ignores certificate expiration dates
  // (The default behavior is to trigger an OnSSLCertificateError callback).
  LOAD_IGNORE_CERT_DATE_INVALID = 1 << 9,

  // If present, trusts all certificate authorities
  // (The default behavior is to trigger an OnSSLCertificateError callback).
  LOAD_IGNORE_CERT_AUTHORITY_INVALID = 1 << 10,

  // If present, ignores certificate revocation
  // (The default behavior is to trigger an OnSSLCertificateError callback).
  LOAD_IGNORE_CERT_REVOCATION = 1 << 11,

  // If present, ignores wrong key usage of the certificate
  // (The default behavior is to trigger an OnSSLCertificateError callback).
  LOAD_IGNORE_CERT_WRONG_USAGE = 1 << 12,
};

}  // namespace net

#endif  // NET_BASE_LOAD_FLAGS_H__
