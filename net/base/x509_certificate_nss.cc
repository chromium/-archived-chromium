// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <cert.h>
#include <pk11pub.h>
#include <prtime.h>
#include <secder.h>
#include <secerr.h>
#include <sechash.h>
#undef Lock

#include "base/logging.h"
#include "base/pickle.h"
#include "base/time.h"
#include "base/nss_init.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

class ScopedCERTCertificate {
 public:
  explicit ScopedCERTCertificate(CERTCertificate* cert)
      : cert_(cert) {}

  ~ScopedCERTCertificate() {
    if (cert_)
      CERT_DestroyCertificate(cert_);
  }

 private:
  CERTCertificate* cert_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTCertificate);
};

class ScopedCERTCertList {
 public:
  explicit ScopedCERTCertList(CERTCertList* cert_list)
      : cert_list_(cert_list) {}

  ~ScopedCERTCertList() {
    if (cert_list_)
      CERT_DestroyCertList(cert_list_);
  }

 private:
  CERTCertList* cert_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTCertList);
};

// ScopedCERTValOutParam manages destruction of values in the CERTValOutParam
// array that cvout points to.  cvout must be initialized as passed to
// CERT_PKIXVerifyCert, so that the array must be terminated with
// cert_po_end type.
// When it goes out of scope, it destroys values of cert_po_trustAnchor
// and cert_po_certList types, but doesn't release the array itself.
class ScopedCERTValOutParam {
 public:
  explicit ScopedCERTValOutParam(CERTValOutParam* cvout)
      : cvout_(cvout) {}

  ~ScopedCERTValOutParam() {
    if (cvout_ == NULL)
      return;
    for (CERTValOutParam *p = cvout_; p->type != cert_po_end; p++) {
      switch (p->type) {
        case cert_po_trustAnchor:
          if (p->value.pointer.cert) {
            CERT_DestroyCertificate(p->value.pointer.cert);
            p->value.pointer.cert = NULL;
          }
          break;
        case cert_po_certList:
          if (p->value.pointer.chain) {
            CERT_DestroyCertList(p->value.pointer.chain);
            p->value.pointer.chain = NULL;
          }
          break;
        default:
          break;
      }
    }
  }

 private:
  CERTValOutParam* cvout_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTValOutParam);
};

// Map PORT_GetError() return values to our network error codes.
int MapSecurityError(int err) {
  switch (err) {
    case SEC_ERROR_INVALID_TIME:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return ERR_CERT_DATE_INVALID;
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_UNTRUSTED_CERT:
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_ERROR_REVOKED_CERTIFICATE:
      return ERR_CERT_REVOKED;
    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_BAD_SIGNATURE:
    case SEC_ERROR_CERT_NOT_VALID:
    // TODO(port): add an ERR_CERT_WRONG_USAGE error code.
    case SEC_ERROR_CERT_USAGES_INVALID:
      return ERR_CERT_INVALID;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map PORT_GetError() return values to our cert status flags.
int MapCertErrorToCertStatus(int err) {
  switch (err) {
    case SEC_ERROR_INVALID_TIME:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return CERT_STATUS_DATE_INVALID;
    case SEC_ERROR_UNTRUSTED_CERT:
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
      return CERT_STATUS_AUTHORITY_INVALID;
    case SEC_ERROR_REVOKED_CERTIFICATE:
      return CERT_STATUS_REVOKED;
    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_BAD_SIGNATURE:
    case SEC_ERROR_CERT_NOT_VALID:
    // TODO(port): add an CERT_STATUS_WRONG_USAGE error code.
    case SEC_ERROR_CERT_USAGES_INVALID:
      return CERT_STATUS_INVALID;
    default:
      return 0;
  }
}

// Saves some information about the certificate chain cert_list in
// *verify_result.  The caller MUST initialize *verify_result before calling
// this function.
// Note that cert_list[0] is the end entity certificate and cert_list doesn't
// contain the root CA certificate.
void GetCertChainInfo(CERTCertList* cert_list,
                      CertVerifyResult* verify_result) {
  int i = 0;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node), i++) {
    SECAlgorithmID& signature = node->cert->signature;
    SECOidTag oid_tag = SECOID_FindOIDTag(&signature.algorithm);
    switch (oid_tag) {
      case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
        verify_result->has_md5 = true;
        if (i != 0)
          verify_result->has_md5_ca = true;
        break;
      case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
        verify_result->has_md2 = true;
        if (i != 0)
          verify_result->has_md2_ca = true;
        break;
      case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
        verify_result->has_md4 = true;
        break;
      default:
        break;
    }
  }
}

// TODO(port): Implement this more simply, and put it in the right place
base::Time PRTimeToBaseTime(PRTime prtime) {
  PRExplodedTime prxtime;
  PR_ExplodeTime(prtime, PR_GMTParameters, &prxtime);

  base::Time::Exploded exploded;
  exploded.year         = prxtime.tm_year;
  exploded.month        = prxtime.tm_month + 1;
  exploded.day_of_week  = prxtime.tm_wday;
  exploded.day_of_month = prxtime.tm_mday;
  exploded.hour         = prxtime.tm_hour;
  exploded.minute       = prxtime.tm_min;
  exploded.second       = prxtime.tm_sec;
  exploded.millisecond  = prxtime.tm_usec / 1000;

  return base::Time::FromUTCExploded(exploded);
}

void ParsePrincipal(SECItem* der_name,
                    X509Certificate::Principal* principal) {

  CERTName name;
  PRArenaPool* arena = NULL;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  DCHECK(arena != NULL);
  if (arena == NULL)
    return;

  // TODO(dkegel): is CERT_NameTemplate what we always want here?
  SECStatus rv;
  rv = SEC_QuickDERDecodeItem(arena, &name, CERT_NameTemplate, der_name);
  DCHECK(rv == SECSuccess);
  if ( rv != SECSuccess ) {
    PORT_FreeArena(arena, PR_FALSE);
    return;
  }

  std::vector<std::string> common_names, locality_names, state_names,
      country_names;

  // TODO(jcampan): add business_category and serial_number.
  static const SECOidTag kOIDs[] = {
      SEC_OID_AVA_COMMON_NAME,
      SEC_OID_AVA_LOCALITY,
      SEC_OID_AVA_STATE_OR_PROVINCE,
      SEC_OID_AVA_COUNTRY_NAME,
      SEC_OID_AVA_STREET_ADDRESS,
      SEC_OID_AVA_ORGANIZATION_NAME,
      SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME,
      SEC_OID_AVA_DC };

  std::vector<std::string>* values[] = {
      &common_names, &locality_names,
      &state_names, &country_names,
      &principal->street_addresses,
      &principal->organization_names,
      &principal->organization_unit_names,
      &principal->domain_components };
  DCHECK(arraysize(kOIDs) == arraysize(values));

  CERTRDN** rdns = name.rdns;
  for (size_t rdn = 0; rdns[rdn]; ++rdn) {
    CERTAVA** avas = rdns[rdn]->avas;
    for (size_t pair = 0; avas[pair] != 0; ++pair) {
      SECOidTag tag = CERT_GetAVATag(avas[pair]);
      for (size_t oid = 0; oid < arraysize(kOIDs); ++oid) {
        if (kOIDs[oid] == tag) {
          SECItem* decode_item = CERT_DecodeAVAValue(&avas[pair]->value);
          if (!decode_item)
            break;
          std::string value(reinterpret_cast<char*>(decode_item->data),
                            decode_item->len);
          values[oid]->push_back(value);
          SECITEM_FreeItem(decode_item, PR_TRUE);
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
  PORT_FreeArena(arena, PR_FALSE);
}

void ParseDate(SECItem* der_date, base::Time* result) {
  PRTime prtime;
  SECStatus rv = DER_DecodeTimeChoice(&prtime, der_date);
  DCHECK(rv == SECSuccess);
  *result = PRTimeToBaseTime(prtime);
}

void GetCertSubjectAltNamesOfType(X509Certificate::OSCertHandle cert_handle,
                                  CERTGeneralNameType name_type,
                                  std::vector<std::string>* result) {

  SECItem alt_name;
  SECStatus rv = CERT_FindCertExtension(cert_handle,
      SEC_OID_X509_SUBJECT_ALT_NAME, &alt_name);
  if (rv != SECSuccess)
    return;

  PRArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  DCHECK(arena != NULL);

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena, &alt_name);

  CERTGeneralName* name = alt_name_list;
  while (name) {
    // For future extension: We're assuming that these values are of types
    // RFC822Name, DNSName or URI.  See the mac code for notes.
    DCHECK(name->type == certRFC822Name ||
           name->type == certDNSName ||
           name->type == certURI);
    if (name->type == name_type) {
      unsigned char* p = name->name.other.data;
      int len = name->name.other.len;
      std::string value = std::string(reinterpret_cast<char*>(p), len);
      result->push_back(value);
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
  PORT_FreeArena(arena, PR_FALSE);
  PORT_Free(alt_name.data);
}

} // namespace

void X509Certificate::Initialize() {
  ParsePrincipal(&cert_handle_->derSubject, &subject_);
  ParsePrincipal(&cert_handle_->derIssuer, &issuer_);

  ParseDate(&cert_handle_->validity.notBefore, &valid_start_);
  ParseDate(&cert_handle_->validity.notAfter, &valid_expiry_);

  fingerprint_ = CalculateFingerprint(cert_handle_);

  // Store the certificate in the cache in case we need it later.
  X509Certificate::Cache::GetInstance()->Insert(this);
}

// static
X509Certificate* X509Certificate::CreateFromPickle(const Pickle& pickle,
                                                   void** pickle_iter) {
  const char* data;
  int length;
  if (!pickle.ReadData(pickle_iter, &data, &length))
    return NULL;

  return CreateFromBytes(data, length);
}

void X509Certificate::Persist(Pickle* pickle) {
  pickle->WriteData(reinterpret_cast<const char*>(cert_handle_->derCert.data),
                    cert_handle_->derCert.len);
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();

  // Compare with CERT_VerifyCertName().
  GetCertSubjectAltNamesOfType(cert_handle_, certDNSName, dns_names);

  // TODO(port): suppress nss's support of the obsolete extension
  //  SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME
  // by providing our own authCertificate callback.

  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}

// TODO(ukai): fix to use this method to verify certificate on SSL channel.
// Note that it's not being used yet.  We need to fix SSLClientSocketNSS to
// use this method to verify ssl certificate.
// The problem is that we get segfault when unit tests is going to terminate
// if PR_Cleanup is called in NSSInitSingleton destructor.
int X509Certificate::Verify(const std::string& hostname,
                            int flags,
                            CertVerifyResult* verify_result) const {
  verify_result->Reset();

  // Make sure that the hostname matches with the common name of the cert.
  SECStatus status = CERT_VerifyCertName(cert_handle_, hostname.c_str());
  if (status != SECSuccess)
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  // Make sure that the cert is valid now.
  SECCertTimeValidity validity = CERT_CheckCertValidTimes(
      cert_handle_, PR_Now(), PR_TRUE);
  if (validity != secCertTimeValid)
    verify_result->cert_status |= CERT_STATUS_DATE_INVALID;

  CERTRevocationFlags revocation_flags;
  // TODO(ukai): Fix to use OCSP.
  // OCSP mode would fail with SEC_ERROR_UNKNOWN_ISSUER.
  // We need to set up OCSP and install an HTTP client for NSS.
  bool use_ocsp = false;
  // EV requires revocation checking.
  if (!(flags & VERIFY_REV_CHECKING_ENABLED))
    flags &= ~VERIFY_EV_CERT;

  // TODO(wtc): Use CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE and
  // CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE for EV certificate
  // verification.
  PRUint64 revocation_method_flags =
      CERT_REV_M_TEST_USING_THIS_METHOD |
      CERT_REV_M_ALLOW_NETWORK_FETCHING |
      CERT_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE |
      CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE |
      CERT_REV_M_STOP_TESTING_ON_FRESH_INFO;
  PRUint64 revocation_method_independent_flags =
      CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST |
      CERT_REV_MI_NO_OVERALL_INFO_REQUIREMENT;
  PRUint64 method_flags[2];
  method_flags[cert_revocation_method_crl] = revocation_method_flags;
  method_flags[cert_revocation_method_ocsp] = revocation_method_flags;

  int number_of_defined_methods;
  CERTRevocationMethodIndex preferred_revocation_methods[1];
  if (use_ocsp) {
    number_of_defined_methods = 2;
    preferred_revocation_methods[0] = cert_revocation_method_ocsp;
  } else {
    number_of_defined_methods = 1;
    preferred_revocation_methods[0] = cert_revocation_method_crl;
  }

  revocation_flags.leafTests.number_of_defined_methods =
      number_of_defined_methods;
  revocation_flags.leafTests.cert_rev_flags_per_method = method_flags;
  revocation_flags.leafTests.number_of_preferred_methods =
      arraysize(preferred_revocation_methods);
  revocation_flags.leafTests.preferred_methods = preferred_revocation_methods;
  revocation_flags.leafTests.cert_rev_method_independent_flags =
      revocation_method_independent_flags;
  revocation_flags.chainTests.number_of_defined_methods =
      number_of_defined_methods;
  revocation_flags.chainTests.cert_rev_flags_per_method = method_flags;
  revocation_flags.chainTests.number_of_preferred_methods =
      arraysize(preferred_revocation_methods);
  revocation_flags.chainTests.preferred_methods = preferred_revocation_methods;
  revocation_flags.chainTests.cert_rev_method_independent_flags =
      revocation_method_independent_flags;

  CERTValInParam cvin[2];
  int cvin_index = 0;
  // We can't use PK11_ListCerts(PK11CertListCA, NULL) for cert_pi_trustAnchors.
  // We get SEC_ERROR_UNTRUSTED_ISSUER (-8172) for our test root CA cert with
  // it by NSS 3.12.0.3.
  // No need to set cert_pi_trustAnchors here.
  // TODO(ukai): use cert_pi_useAIACertFetch (new feature in NSS 3.12.1).
  cvin[cvin_index].type = cert_pi_revocationFlags;
  cvin[cvin_index].value.pointer.revocation = &revocation_flags;
  cvin_index++;
  cvin[cvin_index].type = cert_pi_end;

  CERTValOutParam cvout[3];
  int cvout_index = 0;
  cvout[cvout_index].type = cert_po_trustAnchor;
  cvout[cvout_index].value.pointer.cert = NULL;
  cvout_index++;
  cvout[cvout_index].type = cert_po_certList;
  cvout[cvout_index].value.pointer.chain = NULL;
  int cvout_cert_list_index = cvout_index;
  cvout_index++;
  cvout[cvout_index].type = cert_po_end;
  ScopedCERTValOutParam scoped_cvout(cvout);

  status = CERT_PKIXVerifyCert(cert_handle_, certificateUsageSSLServer,
                               cvin, cvout, NULL);
  if (status != SECSuccess) {
    int err = PORT_GetError();
    LOG(ERROR) << "CERT_PKIXVerifyCert failed err=" << err;
    // CERT_PKIXVerifyCert rerports the wrong error code for
    // expired certificates (NSS bug 491174)
    if (err == SEC_ERROR_CERT_NOT_VALID &&
        (verify_result->cert_status & CERT_STATUS_DATE_INVALID) != 0)
      err = SEC_ERROR_EXPIRED_CERTIFICATE;
    verify_result->cert_status |= MapCertErrorToCertStatus(err);
    return MapCertStatusToNetError(verify_result->cert_status);
  }

  GetCertChainInfo(cvout[cvout_cert_list_index].value.pointer.chain,
                   verify_result);
  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);
  if ((flags & VERIFY_EV_CERT) && VerifyEV())
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  return OK;
}

// TODO(port): Implement properly on Linux.
bool X509Certificate::VerifyEV() const {
  NOTIMPLEMENTED();
  return false;
}

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  base::EnsureNSSInit();

  SECItem der_cert;
  der_cert.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data));
  der_cert.len = length;
  return CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der_cert,
                                 NULL, PR_FALSE, PR_TRUE);
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CERT_DestroyCertificate(cert_handle);
}

// static
X509Certificate::Fingerprint X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  Fingerprint sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

  DCHECK(NULL != cert->derCert.data);
  DCHECK(0 != cert->derCert.len);

  SECStatus rv = HASH_HashBuf(HASH_AlgSHA1, sha1.data,
                              cert->derCert.data, cert->derCert.len);
  DCHECK(rv == SECSuccess);

  return sha1;
}

}  // namespace net
