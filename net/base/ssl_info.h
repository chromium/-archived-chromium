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

#ifndef NET_BASE_SSL_INFO_H_
#define NET_BASE_SSL_INFO_H_

#include "net/base/cert_status_flags.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

// SSL connection info.
// This is really a struct.  All members are public.
class SSLInfo {
 public:
  SSLInfo() : cert_status(0), security_bits(-1) { }

  void Reset() {
    cert = NULL;
    security_bits = -1;
    cert_status = 0;
  }

  bool is_valid() const { return cert != NULL; }

  // Adds the specified |error| to the cert status.
  void SetCertError(int error) {
    int error_flag = 0;
    switch (error) {
    case ERR_CERT_COMMON_NAME_INVALID:
      error_flag = CERT_STATUS_COMMON_NAME_INVALID;
      break;
    case ERR_CERT_DATE_INVALID:
      error_flag = CERT_STATUS_DATE_INVALID;
      break;
    case ERR_CERT_AUTHORITY_INVALID:
      error_flag = CERT_STATUS_AUTHORITY_INVALID;
      break;
    case ERR_CERT_NO_REVOCATION_MECHANISM:
      error_flag = CERT_STATUS_NO_REVOCATION_MECHANISM;
      break;
    case ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      error_flag = CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
      break;
    case ERR_CERT_REVOKED:
      error_flag = CERT_STATUS_REVOKED;
      break;
    case ERR_CERT_CONTAINS_ERRORS:
    case ERR_CERT_INVALID:
      error_flag = CERT_STATUS_INVALID;
      break;
    default:
      NOTREACHED();
      return;
    }
    cert_status |= error_flag;
  }

  // The SSL certificate.
  scoped_refptr<X509Certificate> cert;

  // Bitmask of status info of |cert|, representing, for example, known errors
  // and extended validation (EV) status.
  // See cert_status_flags.h for values.
  int cert_status;

  // The security strength, in bits, of the SSL cipher suite.
  // 0 means the connection is not encrypted.
  // -1 means the security strength is unknown.
  int security_bits;
};

}  // namespace net

#endif  // NET_BASE_SSL_INFO_H_
