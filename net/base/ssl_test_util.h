// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_TEST_UTIL_H_
#define NET_BASE_SSL_TEST_UTIL_H_

#include "build/build_config.h"

#include "base/file_path.h"

// TODO(dkegel): share this between net/base and 
// chrome/browser without putting it in net.lib

class SSLTestUtil {
 public:
  SSLTestUtil();

  ~SSLTestUtil();

  FilePath GetRootCertPath();

  FilePath GetOKCertPath();

  FilePath GetExpiredCertPath();

  // Hostname to use for test server
  static const char kHostName[];

  // Port to use for test server
  static const int kOKHTTPSPort;

  // Port to use for bad test server
  static const int kBadHTTPSPort;

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

  DISALLOW_COPY_AND_ASSIGN(SSLTestUtil);
};

#endif // NET_BASE_SSL_TEST_UTIL_H_
