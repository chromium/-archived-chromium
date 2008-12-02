// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_TEST_UTIL_H_
#define NET_BASE_SSL_TEST_UTIL_H_

#include "build/build_config.h"

#include "base/path_service.h"

class SSLTestUtil {
 public:
  SSLTestUtil();

  ~SSLTestUtil();

  FilePath GetRootCertPath();

  FilePath GetOKCertPath();

  // Where test data is kept in source tree
  static const wchar_t kDocRoot[];

  // Hostname to use for test server
  static const char kHostName[];

  // Port to use for test server
  static const int kOKHTTPSPort;

  // Issuer name of the cert that should be trusted for the test to work.
  static const wchar_t kCertIssuerName[];

  // Returns false if our test root certificate is not trusted.
  bool CheckCATrusted();

 private:
  FilePath cert_dir_;

#if defined(OS_LINUX)
  struct PrivateCERTCertificate;
  PrivateCERTCertificate *cert_;
#endif
};

#endif
