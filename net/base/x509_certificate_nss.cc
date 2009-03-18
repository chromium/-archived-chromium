// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <cert.h>
#include <prtime.h>
#include <secder.h>
#include <sechash.h>
#undef Lock

#include "base/logging.h"
#include "base/pickle.h"
#include "base/time.h"
#include "base/nss_init.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

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

bool X509Certificate::HasExpired() const {
  NOTIMPLEMENTED();
  return false;
}

int X509Certificate::Verify(const std::string& hostname,
                            bool rev_checking_enabled,
                            CertVerifyResult* verify_result) const {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
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
