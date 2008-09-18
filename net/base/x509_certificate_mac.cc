// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <CommonCrypto/CommonDigest.h>
#include <map>

#include "base/histogram.h"
#include "base/lock.h"
#include "base/pickle.h"
#include "base/singleton.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ev_root_ca_metadata.h"

// NOTE: This Mac implementation is almost entirely untested. TODO(avi): test
// it to make sure it does what the docs imply it does.
// NOTE: This implementation doesn't keep track of dates. Calling code is
// expected to use SecTrustEvaluate(x509cert.os_cert_handle()) and look at the
// result there.

namespace net {

namespace {

// Returns true if this cert fingerprint is the null (all zero) fingerprint.
// We use this as a bogus fingerprint value.
bool IsNullFingerprint(const X509Certificate::Fingerprint& fingerprint) {
  for (size_t i = 0; i < arraysize(fingerprint.data); ++i) {
    if (fingerprint.data[i] != 0)
      return false;
  }
  return true;
}

// Calculates the SHA-1 fingerprint of the certificate.  Returns an empty
// (all zero) fingerprint on failure.
X509Certificate::Fingerprint CalculateFingerprint(
    X509Certificate::OSCertHandle cert) {
  X509Certificate::Fingerprint sha1;
  memset(sha1.data, 0, sizeof(sha1.data));
  
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert, &cert_data);
  if (status)
    return sha1;
  
  DCHECK(NULL != cert_data.Data);
  DCHECK(0 != cert_data.Length);
  
  CC_SHA1(cert_data.Data, cert_data.Length, sha1.data);

  return sha1;
}

inline bool CSSMOIDEqual(const CSSM_OID* oid1, const CSSM_OID* oid2) {
  return oid1->Length == oid2->Length &&
      (memcmp(oid1->Data, oid2->Data, oid1->Length) == 0);
}

void ParsePrincipal(const CSSM_X509_NAME* name,
                    X509Certificate::Principal* principal) {
  std::vector<std::string> common_names, locality_names, state_names,
      country_names;

  // TODO(jcampan): add business_category and serial_number.
  const CSSM_OID* kOIDs[] = { &CSSMOID_CommonName,
                              &CSSMOID_LocalityName,
                              &CSSMOID_StateProvinceName,
                              &CSSMOID_CountryName,
                              &CSSMOID_StreetAddress,
                              &CSSMOID_OrganizationName,
                              &CSSMOID_OrganizationalUnitName,
                              &CSSMOID_DNQualifier };  // This should be "DC"
                                                       // but is undoubtedly
                                                       // wrong. TODO(avi):
                                                       // Find the right OID.

  std::vector<std::string>* values[] = {
      &common_names, &locality_names,
      &state_names, &country_names,
      &(principal->street_addresses),
      &(principal->organization_names),
      &(principal->organization_unit_names),
      &(principal->domain_components) };
  DCHECK(arraysize(kOIDs) == arraysize(values));

  for (size_t rdn = 0; rdn < name->numberOfRDNs; ++rdn) {
    CSSM_X509_RDN rdn_struct = name->RelativeDistinguishedName[rdn];
    for (size_t pair = 0; pair < rdn_struct.numberOfPairs; ++pair) {
      CSSM_X509_TYPE_VALUE_PAIR pair_struct =
          rdn_struct.AttributeTypeAndValue[pair];
      for (size_t oid = 0; oid < arraysize(kOIDs); ++oid) {
        if (CSSMOIDEqual(&pair_struct.type, kOIDs[oid])) {
          std::string value =
              std::string(reinterpret_cast<std::string::value_type*>
                              (pair_struct.value.Data),
                          pair_struct.value.Length);
          values[oid]->push_back(value);
          break;
        }
      }
    }
  }

  // We don't expect to have more than one CN, L, S, and C.
  std::vector<std::string>* single_value_lists[4] = {
      &common_names, &locality_names, &state_names, &country_names };
  std::string* single_values[4] = {
      &principal->common_name, &principal->locality_name,
      &principal->state_or_province_name, &principal->country_name };
  for (size_t i = 0; i < arraysize(single_value_lists); ++i) {
    DCHECK(single_value_lists[i]->size() <= 1);
    if (single_value_lists[i]->size() > 0)
      *(single_values[i]) = (*(single_value_lists[i]))[0];
  }
}

void GetCertFieldsForOID(X509Certificate::OSCertHandle cert_handle,
                         CSSM_OID oid,
                         std::vector<std::string>* result) {
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert_handle, &cert_data);
  if (status)
    return;
  
  CSSM_CL_HANDLE cl_handle;
  status = SecCertificateGetCLHandle(cert_handle, &cl_handle);
  if (status)
    return;
  
  uint32 num_of_fields;
  CSSM_FIELD_PTR fields;
  status = CSSM_CL_CertGetAllFields(cl_handle, &cert_data, &num_of_fields,
                                    &fields);
  if (status)
    return;
  
  for (size_t field = 0; field < num_of_fields; ++field) {
    if (CSSMOIDEqual(&fields[field].FieldOid, &oid)) {
      std::string value =
          std::string(reinterpret_cast<std::string::value_type*>
                      (fields[field].FieldValue.Data),
                      fields[field].FieldValue.Length);
      result->push_back(value);
    }
  }
}

}  // namespace

bool X509Certificate::FingerprintLessThan::operator()(
    const Fingerprint& lhs,
    const Fingerprint& rhs) const {
  for (size_t i = 0; i < sizeof(lhs.data); ++i) {
    if (lhs.data[i] < rhs.data[i])
      return true;
    if (lhs.data[i] > rhs.data[i])
      return false;
  }
  return false;
}

bool X509Certificate::LessThan::operator()(X509Certificate* lhs,
                                           X509Certificate* rhs) const {
  if (lhs == rhs)
    return false;

  X509Certificate::FingerprintLessThan fingerprint_functor;
  return fingerprint_functor(lhs->fingerprint_, rhs->fingerprint_);
}

// A thread-safe cache for X509Certificate objects.
//
// The cache does not hold a reference to the certificate objects.  The objects
// must |Remove| themselves from the cache upon destruction (or else the cache
// will be holding dead pointers to the objects).
class X509Certificate::Cache {
 public:
  // Get the singleton object for the cache.
  static X509Certificate::Cache* GetInstance() {
    return Singleton<X509Certificate::Cache>::get();
  }

  // Insert |cert| into the cache.  The cache does NOT AddRef |cert|.  The cache
  // must not already contain a certificate with the same fingerprint.
  void Insert(X509Certificate* cert) {
    AutoLock lock(lock_);

    DCHECK(!IsNullFingerprint(cert->fingerprint())) <<
        "Only insert certs with real fingerprints.";
    DCHECK(cache_.find(cert->fingerprint()) == cache_.end());
    cache_[cert->fingerprint()] = cert;
  };

  // Remove |cert| from the cache.  The cache does not assume that |cert| is
  // already in the cache.
  void Remove(X509Certificate* cert) {
    AutoLock lock(lock_);

    CertMap::iterator pos(cache_.find(cert->fingerprint()));
    if (pos == cache_.end())
      return;  // It is not an error to remove a cert that is not in the cache.
    cache_.erase(pos);
  };

  // Find a certificate in the cache with the given fingerprint.  If one does
  // not exist, this method returns NULL.
  X509Certificate* Find(const Fingerprint& fingerprint) {
    AutoLock lock(lock_);

    CertMap::iterator pos(cache_.find(fingerprint));
    if (pos == cache_.end())
      return NULL;

    return pos->second;
  };

 private:
  typedef std::map<Fingerprint, X509Certificate*, FingerprintLessThan> CertMap;

  // Obtain an instance of X509Certificate::Cache via GetInstance().
  Cache() { }
  friend class DefaultSingletonTraits<X509Certificate::Cache>;

  // You must acquire this lock before using any private data of this object.
  // You must not block while holding this lock.
  Lock lock_;

  // The certificate cache.  You must acquire |lock_| before using |cache_|.
  CertMap cache_;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};

void X509Certificate::Initialize() {
  const CSSM_X509_NAME* name;
  OSStatus status = SecCertificateGetSubject(cert_handle_, &name);
  if (!status) {
    ParsePrincipal(name, &subject_);
  }
  status = SecCertificateGetIssuer(cert_handle_, &name);
  if (!status) {
    ParsePrincipal(name, &issuer_);
  }
  
  fingerprint_ = CalculateFingerprint(cert_handle_);

  // Store the certificate in the cache in case we need it later.
  X509Certificate::Cache::GetInstance()->Insert(this);
}

// static
X509Certificate* X509Certificate::CreateFromHandle(OSCertHandle cert_handle) {
  DCHECK(cert_handle);

  // Check if we already have this certificate in memory.
  X509Certificate::Cache* cache = X509Certificate::Cache::GetInstance();
  X509Certificate* cert = cache->Find(CalculateFingerprint(cert_handle));
  if (cert) {
    // We've found a certificate with the same fingerprint in our cache.  We own
    // the |cert_handle|, which makes it our job to free it.
    CFRelease(cert_handle);
    DHISTOGRAM_COUNTS(L"X509CertificateReuseCount", 1);
    return cert;
  }
  // Otherwise, allocate a new object.
  return new X509Certificate(cert_handle);
}

// static
X509Certificate* X509Certificate::CreateFromPickle(const Pickle& pickle,
                                                   void** pickle_iter) {
  const char* data;
  int length;
  if (!pickle.ReadData(pickle_iter, &data, &length))
    return NULL;
  CSSM_DATA cert_data;
  cert_data.Data = const_cast<uint8*>(reinterpret_cast<const uint8*>(data));
  cert_data.Length = length;

  OSCertHandle cert_handle = NULL;
  OSStatus status = SecCertificateCreateFromData(&cert_data,
                                                 CSSM_CERT_X_509v3,
                                                 CSSM_CERT_ENCODING_BER,
                                                 &cert_handle);
  if (status)
    return NULL;

  return CreateFromHandle(cert_handle);
}

X509Certificate::X509Certificate(OSCertHandle cert_handle)
    : cert_handle_(cert_handle) {
  Initialize();
}

X509Certificate::X509Certificate(std::string subject, std::string issuer,
                                 Time, Time)
    : subject_(subject),
      issuer_(issuer),
      cert_handle_(NULL) {
  memset(fingerprint_.data, 0, sizeof(fingerprint_.data));
}

void X509Certificate::Persist(Pickle* pickle) {
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert_handle_, &cert_data);
  if (status) {
    NOTREACHED();
    return;
  }

  pickle->WriteData(reinterpret_cast<char*>(cert_data.Data), cert_data.Length);
}

X509Certificate::~X509Certificate() {
  // We might not be in the cache, but it is safe to remove ourselves anyway.
  X509Certificate::Cache::GetInstance()->Remove(this);
  if (cert_handle_)
    CFRelease(cert_handle_);
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();
  
  GetCertFieldsForOID(cert_handle_, CSSMOID_SubjectAltName, dns_names);
  
  // TODO(avi): wtc says we need more parsing here. Return and fix when the
  // unit tests are complete and we can verify we're doing this right.
  
  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}
  
// Returns true if the certificate is an extended-validation certificate.
//
// The certificate has already been verified by the HTTP library.  cert_status
// represents the result of that verification.  This function performs
// additional checks of the certificatePolicies extensions of the certificates
// in the certificate chain according to Section 7 (pp. 11-12) of the EV
// Certificate Guidelines Version 1.0 at
// http://cabforum.org/EV_Certificate_Guidelines.pdf.
bool X509Certificate::IsEV(int cert_status) const {
  // TODO(avi): implement this
  NOTIMPLEMENTED();
  return false;
}

X509Certificate::Policy::Judgment X509Certificate::Policy::Check(
    X509Certificate* cert) const {
  // It shouldn't matter which set we check first, but we check denied first
  // in case something strange has happened.

  if (denied_.find(cert->fingerprint()) != denied_.end()) {
    // DCHECK that the order didn't matter.
    DCHECK(allowed_.find(cert->fingerprint()) == allowed_.end());
    return DENIED;
  }

  if (allowed_.find(cert->fingerprint()) != allowed_.end()) {
    // DCHECK that the order didn't matter.
    DCHECK(denied_.find(cert->fingerprint()) == denied_.end());
    return ALLOWED;
  }

  // We don't have a policy for this cert.
  return UNKNOWN;
}

void X509Certificate::Policy::Allow(X509Certificate* cert) {
  // Put the cert in the allowed set and (maybe) remove it from the denied set.
  denied_.erase(cert->fingerprint());
  allowed_.insert(cert->fingerprint());
}

void X509Certificate::Policy::Deny(X509Certificate* cert) {
  // Put the cert in the denied set and (maybe) remove it from the allowed set.
  allowed_.erase(cert->fingerprint());
  denied_.insert(cert->fingerprint());
}

}  // namespace net

