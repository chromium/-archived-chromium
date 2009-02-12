// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    cert_status |= MapNetErrorToCertStatus(error);
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

