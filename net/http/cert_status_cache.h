// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_CERT_STATUS_CACHE_H
#define NET_HTTP_CERT_STATUS_CACHE_H

#include <vector>
#include <map>

#include "net/base/x509_certificate.h"

// This class is used to remember the status of certificates, as WinHTTP
// does not report errors once it has been told to ignore them.
// It only exists because of the WinHTTP bug.
// IMPORTANT: this class is not thread-safe.

namespace net {

class CertStatusCache {
 public:
  CertStatusCache();
  ~CertStatusCache();

  int GetCertStatus(const X509Certificate& cert,
                    const std::string& host_name) const;
  void SetCertStatus(const X509Certificate& cert,
                     const std::string& host_name,
                     int status);

 private:
  typedef std::map<X509Certificate::Fingerprint, int,
                   X509Certificate::FingerprintLessThan> StatusMap;
  typedef std::set<std::string> StringSet;
  typedef std::map<X509Certificate::Fingerprint, StringSet*,
                   X509Certificate::FingerprintLessThan> HostMap;

  StatusMap fingerprint_to_cert_status_;

  // We keep a map for each cert to the list of host names that have been marked
  // with the CN invalid error, as that error is host name specific.
  HostMap fingerprint_to_bad_hosts_;

  DISALLOW_EVIL_CONSTRUCTORS(CertStatusCache);
};

}  // namespace net

#endif  // NET_HTTP_CERT_STATUS_CACHE_H

