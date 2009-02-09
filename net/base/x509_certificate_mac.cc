// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <CommonCrypto/CommonDigest.h>
#include <time.h>

#include "base/logging.h"
#include "base/pickle.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ev_root_ca_metadata.h"
#include "net/base/net_errors.h"

using base::Time;

namespace net {

namespace {

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

OSStatus GetCertFieldsForOID(X509Certificate::OSCertHandle cert_handle,
                             CSSM_OID oid, uint32* num_of_fields,
                             CSSM_FIELD_PTR* fields) {
  *num_of_fields = 0;
  *fields = NULL;
  
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert_handle, &cert_data);
  if (status)
    return status;
  
  CSSM_CL_HANDLE cl_handle;
  status = SecCertificateGetCLHandle(cert_handle, &cl_handle);
  if (status)
    return status;
  
  status = CSSM_CL_CertGetAllFields(cl_handle, &cert_data, num_of_fields,
                                    fields);
  return status;
}    

void GetCertGeneralNamesForOID(X509Certificate::OSCertHandle cert_handle,
                               CSSM_OID oid, CE_GeneralNameType name_type,
                               std::vector<std::string>* result) {
  uint32 num_of_fields;
  CSSM_FIELD_PTR fields;
  OSStatus status = GetCertFieldsForOID(cert_handle, oid, &num_of_fields,
                                        &fields);
  if (status)
    return;
  
  for (size_t field = 0; field < num_of_fields; ++field) {
    if (CSSMOIDEqual(&fields[field].FieldOid, &oid)) {
      CSSM_X509_EXTENSION_PTR cssm_ext =
          (CSSM_X509_EXTENSION_PTR)fields[field].FieldValue.Data;
      CE_GeneralNames* alt_name =
          (CE_GeneralNames*) cssm_ext->value.parsedValue;
      
      for (size_t name = 0; name < alt_name->numNames; ++name) {
        const CE_GeneralName& name_struct = alt_name->generalName[name];
        // For future extension: We're assuming that these values are of types
        // GNT_RFC822Name, GNT_DNSName or GNT_URI, all of which are encoded as
        // IA5String. In general, we should be switching off
        // |name_struct.nameType| and doing type-appropriate conversions. See
        // certextensions.h and the comment immediately preceding
        // CE_GeneralNameType for more information.
        DCHECK(name_struct.nameType == GNT_RFC822Name ||
               name_struct.nameType == GNT_DNSName ||
               name_struct.nameType == GNT_URI);
        if (name_struct.nameType == name_type) {
          const CSSM_DATA& name_data = name_struct.name;
          std::string value =
          std::string(reinterpret_cast<std::string::value_type*>
                      (name_data.Data),
                      name_data.Length);
          result->push_back(value);
        }
      }
    }
  }
}

void GetCertDateForOID(X509Certificate::OSCertHandle cert_handle,
                       CSSM_OID oid, Time* result) {
  *result = Time::Time(); 
  
  uint32 num_of_fields;
  CSSM_FIELD_PTR fields;
  OSStatus status = GetCertFieldsForOID(cert_handle, oid, &num_of_fields,
                                        &fields);
  if (status)
    return;
  
  for (size_t field = 0; field < num_of_fields; ++field) {
    if (CSSMOIDEqual(&fields[field].FieldOid, &oid)) {
      CSSM_X509_TIME* x509_time =
          reinterpret_cast<CSSM_X509_TIME *>(fields[field].FieldValue.Data);
      std::string time_string =
          std::string(reinterpret_cast<std::string::value_type*>
                      (x509_time->time.Data),
                      x509_time->time.Length);
      
      DCHECK(x509_time->timeType == BER_TAG_UTC_TIME ||
             x509_time->timeType == BER_TAG_GENERALIZED_TIME);
      
      struct tm time;
      const char* parse_string;
      if (x509_time->timeType == BER_TAG_UTC_TIME)
        parse_string = "%y%m%d%H%M%SZ";
      else if (x509_time->timeType == BER_TAG_GENERALIZED_TIME)
        parse_string = "%y%m%d%H%M%SZ";
      else {
        // Those are the only two BER tags for time; if neither are used then
        // this is a rather broken cert.
        return;
      }
      
      strptime(time_string.c_str(), parse_string, &time);
      
      Time::Exploded exploded;
      exploded.year         = time.tm_year + 1900;
      exploded.month        = time.tm_mon + 1;
      exploded.day_of_week  = time.tm_wday;
      exploded.day_of_month = time.tm_mday;
      exploded.hour         = time.tm_hour;
      exploded.minute       = time.tm_min;
      exploded.second       = time.tm_sec;
      exploded.millisecond  = 0;
      
      *result = Time::FromUTCExploded(exploded);
      break;
    }
  }
}

}  // namespace

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
  
  GetCertDateForOID(cert_handle_, CSSMOID_X509V1ValidityNotBefore,
                    &valid_start_);
  GetCertDateForOID(cert_handle_, CSSMOID_X509V1ValidityNotAfter,
                    &valid_expiry_);
  
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
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert_handle_, &cert_data);
  if (status) {
    NOTREACHED();
    return;
  }

  pickle->WriteData(reinterpret_cast<char*>(cert_data.Data), cert_data.Length);
}

bool X509Certificate::HasExpired() const {
  NOTIMPLEMENTED();
  return false;
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();
  
  GetCertGeneralNamesForOID(cert_handle_, CSSMOID_SubjectAltName, GNT_DNSName,
                            dns_names);
  
  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}

int X509Certificate::Verify(const std::string& hostname,
                            bool rev_checking_enabled,
                            CertVerifyResult* verify_result) const {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
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

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
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

  return cert_handle;
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CFRelease(cert_handle);
}

// static
X509Certificate::Fingerprint X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  Fingerprint sha1;
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

}  // namespace net
