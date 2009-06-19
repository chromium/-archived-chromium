// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CERT_REQUEST_INFO_H_
#define NET_BASE_SSL_CERT_REQUEST_INFO_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"

namespace net {

class X509Certificate;

// The SSLCertRequestInfo class contains the info that allows a user to
// select a certificate to send to the SSL server for client authentication.
class SSLCertRequestInfo
    : public base::RefCountedThreadSafe<SSLCertRequestInfo> {
 public:
  // The host and port of the SSL server that requested client authentication.
  std::string host_and_port;

  // A list of client certificates that match the server's criteria in the
  // SSL CertificateRequest message.  In TLS 1.0, the CertificateRequest
  // message is defined as:
  //   enum {
  //     rsa_sign(1), dss_sign(2), rsa_fixed_dh(3), dss_fixed_dh(4),
  //     (255)
  //   } ClientCertificateType;
  //
  //   opaque DistinguishedName<1..2^16-1>;
  //
  //   struct {
  //       ClientCertificateType certificate_types<1..2^8-1>;
  //       DistinguishedName certificate_authorities<3..2^16-1>;
  //   } CertificateRequest;
  std::vector<scoped_refptr<X509Certificate> > client_certs;
};

}  // namespace net

#endif  // NET_BASE_SSL_CERT_REQUEST_INFO_H_
