// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_CERTIFICATE_H_
#define NET_BASE_X509_CERTIFICATE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(OS_MACOSX)
#include <Security/Security.h>
#elif defined(OS_LINUX)
// Forward declaration; real one in <cert.h>
struct CERTCertificateStr;
#endif

class Pickle;

namespace net {

class CertVerifyResult;

// X509Certificate represents an X.509 certificate used by SSL.
class X509Certificate : public base::RefCountedThreadSafe<X509Certificate> {
 public:
  // SHA-1 fingerprint (160 bits) of a certificate.
  struct Fingerprint {
    unsigned char data[20];
  };

  class FingerprintLessThan
      : public std::binary_function<Fingerprint, Fingerprint, bool> {
   public:
    bool operator() (const Fingerprint& lhs, const Fingerprint& rhs) const;
  };

  // Predicate functor used in maps when X509Certificate is used as the key.
  class LessThan
      : public std::binary_function<X509Certificate*, X509Certificate*, bool> {
   public:
    bool operator() (X509Certificate* lhs,  X509Certificate* rhs) const;
  };

  // A handle to the certificate object in the underlying crypto library.
  // We assume that OSCertHandle is a pointer type on all platforms and
  // NULL is an invalid OSCertHandle.
#if defined(OS_WIN)
  typedef PCCERT_CONTEXT OSCertHandle;
#elif defined(OS_MACOSX)
  typedef SecCertificateRef OSCertHandle;
#elif defined(OS_LINUX)
  typedef struct CERTCertificateStr* OSCertHandle;
#else
  // TODO(ericroman): not implemented
  typedef void* OSCertHandle;
#endif

  // Principal represent an X.509 principal.
  struct Principal {
    Principal() { }
    explicit Principal(std::string name) : common_name(name) { }

    // The different attributes for a principal.  They may be "".
    // Note that some of them can have several values.

    std::string common_name;
    std::string locality_name;
    std::string state_or_province_name;
    std::string country_name;

    std::vector<std::string> street_addresses;
    std::vector<std::string> organization_names;
    std::vector<std::string> organization_unit_names;
    std::vector<std::string> domain_components;
  };

  // This class is useful for maintaining policies about which certificates are
  // permitted or forbidden for a particular purpose.
  class Policy {
   public:
    // The judgments this policy can reach.
    enum Judgment {
      // We don't have policy information for this certificate.
      UNKNOWN,

      // This certificate is allowed.
      ALLOWED,

      // This certificate is denied.
      DENIED,
    };

    // Returns the judgment this policy makes about this certificate.
    Judgment Check(X509Certificate* cert) const;

    // Causes the policy to allow this certificate.
    void Allow(X509Certificate* cert);

    // Causes the policy to deny this certificate.
    void Deny(X509Certificate* cert);

   private:
    // The set of fingerprints of allowed certificates.
    std::set<Fingerprint, FingerprintLessThan> allowed_;

    // The set of fingerprints of denied certificates.
    std::set<Fingerprint, FingerprintLessThan> denied_;
  };

  // Where the certificate comes from.  The enumeration constants are
  // listed in increasing order of preference.
  enum Source {
    SOURCE_UNUSED = 0,            // The source_ member is not used.
    SOURCE_LONE_CERT_IMPORT = 1,  // From importing a certificate without
                                  // its intermediate CA certificates.
    SOURCE_FROM_NETWORK = 2,      // From the network.
  };

  // Create an X509Certificate from a handle to the certificate object
  // in the underlying crypto library. This is a transfer of ownership;
  // X509Certificate will properly dispose of |cert_handle| for you.
  // |source| specifies where |cert_handle| comes from.  Given two
  // certificate handles for the same certificate, our certificate cache
  // prefers the handle from the network because our HTTP cache isn't
  // caching the corresponding intermediate CA certificates yet
  // (http://crbug.com/7065).
  //
  // The returned pointer must be stored in a scoped_refptr<X509Certificate>.
  static X509Certificate* CreateFromHandle(OSCertHandle cert_handle,
                                           Source source);

  // Create an X509Certificate from the BER-encoded representation.
  // Returns NULL on failure.
  //
  // The returned pointer must be stored in a scoped_refptr<X509Certificate>.
  static X509Certificate* CreateFromBytes(const char* data, int length);

  // Create an X509Certificate from the representation stored in the given
  // pickle.  The data for this object is found relative to the given
  // pickle_iter, which should be passed to the pickle's various Read* methods.
  // Returns NULL on failure.
  //
  // The returned pointer must be stored in a scoped_refptr<X509Certificate>.
  static X509Certificate* CreateFromPickle(const Pickle& pickle,
                                           void** pickle_iter);

  // Creates a X509Certificate from the ground up.  Used by tests that simulate
  // SSL connections.
  X509Certificate(const std::string& subject, const std::string& issuer,
                  base::Time start_date, base::Time expiration_date);

  // Appends a representation of this object to the given pickle.
  void Persist(Pickle* pickle);

  // The subject of the certificate.  For HTTPS server certificates, this
  // represents the web server.  The common name of the subject should match
  // the host name of the web server.
  const Principal& subject() const { return subject_; }

  // The issuer of the certificate.
  const Principal& issuer() const { return issuer_; }

  // Time period during which the certificate is valid.  More precisely, this
  // certificate is invalid before the |valid_start| date and invalid after
  // the |valid_expiry| date.
  // If we were unable to parse either date from the certificate (or if the cert
  // lacks either date), the date will be null (i.e., is_null() will be true).
  const base::Time& valid_start() const { return valid_start_; }
  const base::Time& valid_expiry() const { return valid_expiry_; }

  // The fingerprint of this certificate.
  const Fingerprint& fingerprint() const { return fingerprint_; }

  // Gets the DNS names in the certificate.  Pursuant to RFC 2818, Section 3.1
  // Server Identity, if the certificate has a subjectAltName extension of
  // type dNSName, this method gets the DNS names in that extension.
  // Otherwise, it gets the common name in the subject field.
  void GetDNSNames(std::vector<std::string>* dns_names) const;

  // Convenience method that returns whether this certificate has expired as of
  // now.
  bool HasExpired() const;

  // Verifies the certificate against the given hostname.  Returns OK if
  // successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value.  If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // If |rev_checking_enabled| is true, certificate revocation checking is
  // performed.
  int Verify(const std::string& hostname,
             bool rev_checking_enabled,
             CertVerifyResult* verify_result) const;

  // Returns true if the certificate is an extended-validation (EV)
  // certificate.
  bool IsEV(int cert_status) const;

  OSCertHandle os_cert_handle() const { return cert_handle_; }

 private:
  friend class base::RefCountedThreadSafe<X509Certificate>;
  FRIEND_TEST(X509CertificateTest, Cache);

  // A cache of X509Certificate objects.
  class Cache {
   public:
    static Cache* GetInstance();
    void Insert(X509Certificate* cert);
    void Remove(X509Certificate* cert);
    X509Certificate* Find(const Fingerprint& fingerprint);

   private:
    typedef std::map<Fingerprint, X509Certificate*, FingerprintLessThan>
        CertMap;

    // Obtain an instance of X509Certificate::Cache via GetInstance().
    Cache() { }
    friend struct DefaultSingletonTraits<Cache>;

    // You must acquire this lock before using any private data of this object.
    // You must not block while holding this lock.
    Lock lock_;

    // The certificate cache.  You must acquire |lock_| before using |cache_|.
    CertMap cache_;

    DISALLOW_COPY_AND_ASSIGN(Cache);
  };

  // Construct an X509Certificate from a handle to the certificate object
  // in the underlying crypto library.
  X509Certificate(OSCertHandle cert_handle, Source source);

  ~X509Certificate();

  // Common object initialization code.  Called by the constructors only.
  void Initialize();

  // Creates an OS certificate handle from the BER-encoded representation.
  // Returns NULL on failure.
  static OSCertHandle CreateOSCertHandleFromBytes(const char* data,
                                                  int length);

  // Frees an OS certificate handle.
  static void FreeOSCertHandle(OSCertHandle cert_handle);

  // Calculates the SHA-1 fingerprint of the certificate.  Returns an empty
  // (all zero) fingerprint on failure.
  static Fingerprint CalculateFingerprint(OSCertHandle cert_handle);

  // The subject of the certificate.
  Principal subject_;

  // The issuer of the certificate.
  Principal issuer_;

  // This certificate is not valid before |valid_start_|
  base::Time valid_start_;

  // This certificate is not valid after |valid_expiry_|
  base::Time valid_expiry_;

  // The fingerprint of this certificate.
  Fingerprint fingerprint_;

  // A handle to the certificate object in the underlying crypto library.
  OSCertHandle cert_handle_;

  // Where the certificate comes from.
  Source source_;

  DISALLOW_COPY_AND_ASSIGN(X509Certificate);
};

}  // namespace net

#endif  // NET_BASE_X509_CERTIFICATE_H_
