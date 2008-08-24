// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_status_flags.h"
#include "net/http/cert_status_cache.h"

namespace net {

CertStatusCache::CertStatusCache() {
}

CertStatusCache::~CertStatusCache() {
  for (HostMap::iterator iter = fingerprint_to_bad_hosts_.begin();
       iter != fingerprint_to_bad_hosts_.end(); ++iter) {
    delete iter->second;
  }
}

int CertStatusCache::GetCertStatus(const X509Certificate& cert,
                                   const std::string& host) const {
  StatusMap::const_iterator iter =
      fingerprint_to_cert_status_.find(cert.fingerprint());
  if (iter != fingerprint_to_cert_status_.end()) {
    int cert_status = iter->second;

    // We get the CERT_STATUS_COMMON_NAME_INVALID error based on the host.
    HostMap::const_iterator fp_iter =
        fingerprint_to_bad_hosts_.find(cert.fingerprint());
    if (fp_iter != fingerprint_to_bad_hosts_.end()) {
      StringSet* bad_hosts = fp_iter->second;
      StringSet::const_iterator host_iter = bad_hosts->find(host);
      if (host_iter != bad_hosts->end())
        cert_status |= net::CERT_STATUS_COMMON_NAME_INVALID;
    }

    return cert_status;
  }
  return 0;  // The cert has never had errors.
}

void CertStatusCache::SetCertStatus(const X509Certificate& cert,
                                    const std::string& host,
                                    int status) {
  // We store the CERT_STATUS_COMMON_NAME_INVALID status separately as it is
  // host related.
  fingerprint_to_cert_status_[cert.fingerprint()] =
      status & ~net::CERT_STATUS_COMMON_NAME_INVALID;

  if ((status & net::CERT_STATUS_COMMON_NAME_INVALID) != 0) {
    StringSet* bad_hosts;
    HostMap::const_iterator iter =
        fingerprint_to_bad_hosts_.find(cert.fingerprint());
    if (iter == fingerprint_to_bad_hosts_.end()) {
      bad_hosts = new StringSet;
      fingerprint_to_bad_hosts_[cert.fingerprint()] = bad_hosts;
    } else {
      bad_hosts = iter->second;
    }
    bad_hosts->insert(host);
  }
}

}

