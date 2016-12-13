// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_VERIFIER_H_
#define NET_BASE_CERT_VERIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"

namespace net {

class CertVerifyResult;
class X509Certificate;

// This class represents the task of verifying a certificate.  It can only
// verify a single certificate at a time, so if you need to verify multiple
// certificates at the same time, you will need to allocate a CertVerifier
// object for each certificate.
//
// TODO(wtc): This class is based on HostResolver.  We should create a base
// class for the common code between the two classes.
//
class CertVerifier {
 public:
  CertVerifier();

  // If a completion callback is pending when the verifier is destroyed, the
  // certificate verification is cancelled, and the completion callback will
  // not be called.
  ~CertVerifier();

  // Verifies the given certificate against the given hostname.  Returns OK if
  // successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value.  If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // |flags| is bitwise OR'd of X509Certificate::VerifyFlags.
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, certificate revocation
  // checking is performed.
  //
  // If VERIFY_EV_CERT is set in |flags| too, EV certificate verification is
  // performed.  If |flags| is VERIFY_EV_CERT (that is,
  // VERIFY_REV_CHECKING_ENABLED is not set), EV certificate verification will
  // not be performed.
  //
  // When callback is null, the operation completes synchronously.
  //
  // When callback is non-null, ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result code will
  // be passed to the callback when available.
  //
  int Verify(X509Certificate* cert, const std::string& hostname,
             int flags, CertVerifyResult* verify_result,
             CompletionCallback* callback);

 private:
  class Request;
  friend class Request;
  scoped_refptr<Request> request_;
  DISALLOW_COPY_AND_ASSIGN(CertVerifier);
};

}  // namespace net

#endif  // NET_BASE_CERT_VERIFIER_H_
