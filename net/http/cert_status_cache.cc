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
