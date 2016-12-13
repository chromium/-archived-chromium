// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_VERIFY_RESULT_H_
#define NET_BASE_CERT_VERIFY_RESULT_H_

namespace net {

// The result of certificate verification.  Eventually this may contain the
// certificate chain that was constructed during certificate verification.
class CertVerifyResult {
 public:
  CertVerifyResult() { Reset(); }

  void Reset() {
    cert_status = 0;
    has_md5 = false;
    has_md2 = false;
    has_md4 = false;
    has_md5_ca = false;
    has_md2_ca = false;
  }

  int cert_status;

  // Properties of the certificate chain.
  bool has_md5;
  bool has_md2;
  bool has_md4;
  bool has_md5_ca;
  bool has_md2_ca;
};

}  // namespace net

#endif  // NET_BASE_CERT_VERIFY_RESULT_H_
