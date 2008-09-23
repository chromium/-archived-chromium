// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include "base/logging.h"

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

// Get the singleton object for the cache.
// static
X509Certificate::Cache* X509Certificate::Cache::GetInstance() {
  return Singleton<X509Certificate::Cache>::get();
}

// Insert |cert| into the cache.  The cache does NOT AddRef |cert|.  The cache
// must not already contain a certificate with the same fingerprint.
void X509Certificate::Cache::Insert(X509Certificate* cert) {
  AutoLock lock(lock_);

  DCHECK(!IsNullFingerprint(cert->fingerprint())) <<
      "Only insert certs with real fingerprints.";
  DCHECK(cache_.find(cert->fingerprint()) == cache_.end());
  cache_[cert->fingerprint()] = cert;
};

// Remove |cert| from the cache.  The cache does not assume that |cert| is
// already in the cache.
void X509Certificate::Cache::Remove(X509Certificate* cert) {
  AutoLock lock(lock_);

  CertMap::iterator pos(cache_.find(cert->fingerprint()));
  if (pos == cache_.end())
    return;  // It is not an error to remove a cert that is not in the cache.
  cache_.erase(pos);
};

// Find a certificate in the cache with the given fingerprint.  If one does
// not exist, this method returns NULL.
X509Certificate* X509Certificate::Cache::Find(const Fingerprint& fingerprint) {
  AutoLock lock(lock_);

  CertMap::iterator pos(cache_.find(fingerprint));
  if (pos == cache_.end())
    return NULL;

  return pos->second;
};

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

