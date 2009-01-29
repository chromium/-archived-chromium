// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include "base/histogram.h"
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

// static
X509Certificate* X509Certificate::CreateFromHandle(OSCertHandle cert_handle,
                                                   Source source) {
  DCHECK(cert_handle);
  DCHECK(source != SOURCE_UNUSED);

  // Check if we already have this certificate in memory.
  X509Certificate::Cache* cache = X509Certificate::Cache::GetInstance();
  X509Certificate* cached_cert =
      cache->Find(CalculateFingerprint(cert_handle));
  if (cached_cert) {
    DCHECK(cached_cert->source_ != SOURCE_UNUSED);
    if (cached_cert->source_ >= source) {
      // We've found a certificate with the same fingerprint in our cache.  We
      // own the |cert_handle|, which makes it our job to free it.
      FreeOSCertHandle(cert_handle);
      DHISTOGRAM_COUNTS(L"X509CertificateReuseCount", 1);
      return cached_cert;
    }
    // Kick out the old certificate from our cache.  The new one is better.
    cache->Remove(cached_cert);
  }
  // Otherwise, allocate a new object.
  return new X509Certificate(cert_handle, source);
}

// static
X509Certificate* X509Certificate::CreateFromBytes(const char* data,
                                                  int length) {
  OSCertHandle cert_handle = CreateOSCertHandleFromBytes(data, length);
  if (!cert_handle)
    return NULL;

  return CreateFromHandle(cert_handle, SOURCE_LONE_CERT_IMPORT);
}

X509Certificate::X509Certificate(OSCertHandle cert_handle, Source source)
    : cert_handle_(cert_handle), source_(source) {
  Initialize();
}

X509Certificate::X509Certificate(const std::string& subject,
                                 const std::string& issuer,
                                 base::Time start_date,
                                 base::Time expiration_date)
    : subject_(subject),
      issuer_(issuer),
      valid_start_(start_date),
      valid_expiry_(expiration_date),
      cert_handle_(NULL),
      source_(SOURCE_UNUSED) {
  memset(fingerprint_.data, 0, sizeof(fingerprint_.data));
}

X509Certificate::~X509Certificate() {
  // We might not be in the cache, but it is safe to remove ourselves anyway.
  X509Certificate::Cache::GetInstance()->Remove(this);
  if (cert_handle_)
    FreeOSCertHandle(cert_handle_);
}

}  // namespace net

