// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  // If present, try to download the resource to a standalone file.
  LOAD_ENABLE_DOWNLOAD_FILE = 1 << 7,

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

  // This load will not make any changes to cookies, including storing new
  // cookies or updating existing ones.
  LOAD_DO_NOT_SAVE_COOKIES = 1 << 13,

  // An SDCH dictionary was advertised, and an SDCH encoded response is
  // possible.
  LOAD_SDCH_DICTIONARY_ADVERTISED = 1 << 14,

  // Do not resolve proxies. This override is used when downloading PAC files
  // to avoid having a circular dependency.
  LOAD_BYPASS_PROXY = 1 << 15,
};

}  // namespace net

#endif  // NET_BASE_LOAD_FLAGS_H__

