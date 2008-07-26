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

#ifndef NET_BASE_EV_ROOT_CA_METADATA_H__
#define NET_BASE_EV_ROOT_CA_METADATA_H__

#include <map>

#include "net/base/x509_certificate.h"

namespace net {

// A singleton.  This class stores the meta data of the root CAs that issue
// extended-validation (EV) certificates.
class EVRootCAMetadata {
 public:
  static EVRootCAMetadata* GetInstance();

  // If the root CA cert has an EV policy OID, returns true and stores the
  // policy OID in *policy_oid.  Otherwise, returns false.
  bool GetPolicyOID(const X509Certificate::Fingerprint& fingerprint,
                    std::string* policy_oid) const;

  const char* const* GetPolicyOIDs() const { return policy_oids_.get(); }
  int NumPolicyOIDs() const { return num_policy_oids_; }

 private:
  EVRootCAMetadata();
  ~EVRootCAMetadata() { }

  static EVRootCAMetadata* instance_;

  typedef std::map<X509Certificate::Fingerprint, std::string,
                   X509Certificate::FingerprintLessThan> StringMap;

  // Maps an EV root CA cert's SHA-1 fingerprint to its EV policy OID.
  StringMap ev_policy_;

  // Contains dotted-decimal OID strings (in ASCII).  This is a C array of
  // C strings so that it can be passed directly to Windows CryptoAPI as
  // LPSTR*.
  scoped_array<const char*> policy_oids_;
  int num_policy_oids_;

  DISALLOW_EVIL_CONSTRUCTORS(EVRootCAMetadata);
};

}  // namespace net

#endif  // NET_BASE_EV_ROOT_CA_METADATA_H__