// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_EV_ROOT_CA_METADATA_H_
#define NET_BASE_EV_ROOT_CA_METADATA_H_

#include <map>

#include "base/scoped_ptr.h"
#include "net/base/x509_certificate.h"

template <typename T>
struct DefaultSingletonTraits;

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

  friend struct DefaultSingletonTraits<EVRootCAMetadata>;

  typedef std::map<X509Certificate::Fingerprint, std::string,
                   X509Certificate::FingerprintLessThan> StringMap;

  // Maps an EV root CA cert's SHA-1 fingerprint to its EV policy OID.
  StringMap ev_policy_;

  // Contains dotted-decimal OID strings (in ASCII).  This is a C array of
  // C strings so that it can be passed directly to Windows CryptoAPI as
  // LPSTR*.
  scoped_array<const char*> policy_oids_;
  int num_policy_oids_;

  DISALLOW_COPY_AND_ASSIGN(EVRootCAMetadata);
};

}  // namespace net

#endif  // NET_BASE_EV_ROOT_CA_METADATA_H_
