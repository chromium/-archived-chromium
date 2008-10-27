// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include "base/histogram.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ev_root_ca_metadata.h"
#include "net/base/scoped_cert_chain_context.h"

#pragma comment(lib, "crypt32.lib")

using base::Time;

namespace net {

namespace {

// Calculates the SHA-1 fingerprint of the certificate.  Returns an empty
// (all zero) fingerprint on failure.
X509Certificate::Fingerprint CalculateFingerprint(PCCERT_CONTEXT cert) {
  DCHECK(NULL != cert->pbCertEncoded);
  DCHECK(0 != cert->cbCertEncoded);

  BOOL rv;
  X509Certificate::Fingerprint sha1;
  DWORD sha1_size = sizeof(sha1.data);
  rv = CryptHashCertificate(NULL, CALG_SHA1, 0, cert->pbCertEncoded,
                            cert->cbCertEncoded, sha1.data, &sha1_size);
  DCHECK(rv && sha1_size == sizeof(sha1.data));
  if (!rv)
    memset(sha1.data, 0, sizeof(sha1.data));
  return sha1;
}

// Wrappers of malloc and free for CRYPT_DECODE_PARA, which requires the
// WINAPI calling convention.
void* WINAPI MyCryptAlloc(size_t size) {
  return malloc(size);
}

void WINAPI MyCryptFree(void* p) {
  free(p);
}

// Decodes the cert's subjectAltName extension into a CERT_ALT_NAME_INFO
// structure and stores it in *output.
void GetCertSubjectAltName(PCCERT_CONTEXT cert,
                           scoped_ptr_malloc<CERT_ALT_NAME_INFO>* output) {
  PCERT_EXTENSION extension = CertFindExtension(szOID_SUBJECT_ALT_NAME2,
                                                cert->pCertInfo->cExtension,
                                                cert->pCertInfo->rgExtension);
  if (!extension)
    return;

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = MyCryptAlloc;
  decode_para.pfnFree = MyCryptFree;
  CERT_ALT_NAME_INFO* alt_name_info = NULL;
  DWORD alt_name_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           szOID_SUBJECT_ALT_NAME2,
                           extension->Value.pbData,
                           extension->Value.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &alt_name_info,
                           &alt_name_info_size);
  if (rv)
    output->reset(alt_name_info);
}

///////////////////////////////////////////////////////////////////////////
//
// Functions used by X509Certificate::IsEV
//
///////////////////////////////////////////////////////////////////////////

// Constructs a certificate chain starting from the end certificate
// 'cert_context', matching any of the certificate policies.
//
// Returns the certificate chain context on success, or NULL on failure.
// The caller is responsible for freeing the certificate chain context with
// CertFreeCertificateChain.
PCCERT_CHAIN_CONTEXT ConstructCertChain(
    PCCERT_CONTEXT cert_context,
    const char* const* policies,
    int num_policies) {
  CERT_CHAIN_PARA chain_para;
  memset(&chain_para, 0, sizeof(chain_para));
  chain_para.cbSize = sizeof(chain_para);
  chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
  chain_para.RequestedUsage.Usage.cUsageIdentifier = 0;
  chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = NULL;  // LPSTR*
  chain_para.RequestedIssuancePolicy.dwType = USAGE_MATCH_TYPE_OR;
  chain_para.RequestedIssuancePolicy.Usage.cUsageIdentifier = num_policies;
  chain_para.RequestedIssuancePolicy.Usage.rgpszUsageIdentifier =
      const_cast<char**>(policies);
  PCCERT_CHAIN_CONTEXT chain_context;
  if (!CertGetCertificateChain(
      NULL,  // default chain engine, HCCE_CURRENT_USER
      cert_context,
      NULL,  // current system time
      cert_context->hCertStore,  // search this store
      &chain_para,
      CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT |
      CERT_CHAIN_CACHE_END_CERT,
      NULL,  // reserved
      &chain_context)) {
    return NULL;
  }
  return chain_context;
}

// Decodes the cert's certificatePolicies extension into a CERT_POLICIES_INFO
// structure and stores it in *output.
void GetCertPoliciesInfo(PCCERT_CONTEXT cert,
                         scoped_ptr_malloc<CERT_POLICIES_INFO>* output) {
  PCERT_EXTENSION extension = CertFindExtension(szOID_CERT_POLICIES,
                                                cert->pCertInfo->cExtension,
                                                cert->pCertInfo->rgExtension);
  if (!extension)
    return;

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = MyCryptAlloc;
  decode_para.pfnFree = MyCryptFree;
  CERT_POLICIES_INFO* policies_info = NULL;
  DWORD policies_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           szOID_CERT_POLICIES,
                           extension->Value.pbData,
                           extension->Value.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &policies_info,
                           &policies_info_size);
  if (rv)
    output->reset(policies_info);
}

// Returns true if the policy is in the array of CERT_POLICY_INFO in
// the CERT_POLICIES_INFO structure.
bool ContainsPolicy(const CERT_POLICIES_INFO* policies_info,
                    const char* policy) {
  int num_policies = policies_info->cPolicyInfo;
  for (int i = 0; i < num_policies; i++) {
    if (!strcmp(policies_info->rgPolicyInfo[i].pszPolicyIdentifier, policy))
      return true;
  }
  return false;
}

// Helper function to parse a principal from a WinInet description of that
// principal.
void ParsePrincipal(const std::string& description,
                    X509Certificate::Principal* principal) {
  // The description of the principal is a string with each LDAP value on
  // a separate line.
  const std::string kDelimiters("\r\n");

  std::vector<std::string> common_names, locality_names, state_names,
      country_names;

  // TODO(jcampan): add business_category and serial_number.
  const std::string kPrefixes[] = { std::string("CN="),
                                    std::string("L="),
                                    std::string("S="),
                                    std::string("C="),
                                    std::string("STREET="),
                                    std::string("O="),
                                    std::string("OU="),
                                    std::string("DC=") };

  std::vector<std::string>* values[] = {
      &common_names, &locality_names,
      &state_names, &country_names,
      &(principal->street_addresses),
      &(principal->organization_names),
      &(principal->organization_unit_names),
      &(principal->domain_components) };
  DCHECK(arraysize(kPrefixes) == arraysize(values));

  StringTokenizer str_tok(description, kDelimiters);
  while (str_tok.GetNext()) {
    std::string entry = str_tok.token();
    for (int i = 0; i < arraysize(kPrefixes); i++) {
      if (!entry.compare(0, kPrefixes[i].length(), kPrefixes[i])) {
        std::string value = entry.substr(kPrefixes[i].length());
        // Remove enclosing double-quotes if any.
        if (value.size() >= 2 &&
            value[0] == '"' && value[value.size() - 1] == '"')
          value = value.substr(1, value.size() - 2);
        values[i]->push_back(value);
        break;
      }
    }
  }

  // We don't expect to have more than one CN, L, S, and C.
  std::vector<std::string>* single_value_lists[4] = {
      &common_names, &locality_names, &state_names, &country_names };
  std::string* single_values[4] = {
      &principal->common_name, &principal->locality_name,
      &principal->state_or_province_name, &principal->country_name };
  for (int i = 0; i < arraysize(single_value_lists); ++i) {
    int length = static_cast<int>(single_value_lists[i]->size());
    DCHECK(single_value_lists[i]->size() <= 1);
    if (single_value_lists[i]->size() > 0)
      *(single_values[i]) = (*(single_value_lists[i]))[0];
  }
}

}  // namespace

void X509Certificate::Initialize() {
  std::wstring subject_info;
  std::wstring issuer_info;
  DWORD name_size;
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Subject,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            NULL, 0);
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Subject,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            WriteInto(&subject_info, name_size), name_size);
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Issuer,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            NULL, 0);
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Issuer,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            WriteInto(&issuer_info, name_size), name_size);
  ParsePrincipal(WideToUTF8(subject_info), &subject_);
  ParsePrincipal(WideToUTF8(issuer_info), &issuer_);

  valid_start_ = Time::FromFileTime(cert_handle_->pCertInfo->NotBefore);
  valid_expiry_ = Time::FromFileTime(cert_handle_->pCertInfo->NotAfter);

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
    CertFreeCertificateContext(cert_handle);
    DHISTOGRAM_COUNTS(L"X509CertificateReuseCount", 1);
    return cert;
  }
  // Otherwise, allocate a new object.
  return new X509Certificate(cert_handle);
}

// static
X509Certificate* X509Certificate::CreateFromBytes(const char* data,
                                                  int length) {
  OSCertHandle cert_handle = NULL;
  if (!CertAddEncodedCertificateToStore(
      NULL,  // the cert won't be persisted in any cert store
      X509_ASN_ENCODING,
      reinterpret_cast<const BYTE*>(data), length,
      CERT_STORE_ADD_USE_EXISTING,
      &cert_handle))
    return NULL;

  return CreateFromHandle(cert_handle);
}

// static
X509Certificate* X509Certificate::CreateFromPickle(const Pickle& pickle,
                                                   void** pickle_iter) {
  const char* data;
  int length;
  if (!pickle.ReadData(pickle_iter, &data, &length))
    return NULL;

  OSCertHandle cert_handle = NULL;
  if (!CertAddSerializedElementToStore(
      NULL,  // the cert won't be persisted in any cert store
      reinterpret_cast<const BYTE*>(data), length,
      CERT_STORE_ADD_USE_EXISTING, 0, CERT_STORE_CERTIFICATE_CONTEXT_FLAG,
      NULL, reinterpret_cast<const void **>(&cert_handle)))
    return NULL;

  return CreateFromHandle(cert_handle);
}

X509Certificate::X509Certificate(OSCertHandle cert_handle)
    : cert_handle_(cert_handle) {
  Initialize();
}

X509Certificate::X509Certificate(std::string subject, std::string issuer,
                                 Time start_date, Time expiration_date)
    : subject_(subject),
      issuer_(issuer),
      valid_start_(start_date),
      valid_expiry_(expiration_date),
      cert_handle_(NULL) {
  memset(fingerprint_.data, 0, sizeof(fingerprint_.data));
}

void X509Certificate::Persist(Pickle* pickle) {
  DWORD length;
  if (!CertSerializeCertificateStoreElement(cert_handle_, 0,
      NULL, &length)) {
    NOTREACHED();
    return;
  }
  BYTE* data = reinterpret_cast<BYTE*>(pickle->BeginWriteData(length));
  if (!CertSerializeCertificateStoreElement(cert_handle_, 0,
      data, &length)) {
    NOTREACHED();
    length = 0;
  }
  pickle->TrimWriteData(length);
}

X509Certificate::~X509Certificate() {
  // We might not be in the cache, but it is safe to remove ourselves anyway.
  X509Certificate::Cache::GetInstance()->Remove(this);
  if (cert_handle_)
    CertFreeCertificateContext(cert_handle_);
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();
  scoped_ptr_malloc<CERT_ALT_NAME_INFO> alt_name_info;
  GetCertSubjectAltName(cert_handle_, &alt_name_info);
  CERT_ALT_NAME_INFO* alt_name = alt_name_info.get();
  if (alt_name) {
    int num_entries = alt_name->cAltEntry;
    for (int i = 0; i < num_entries; i++) {
      // dNSName is an ASN.1 IA5String representing a string of ASCII
      // characters, so we can use WideToASCII here.
      if (alt_name->rgAltEntry[i].dwAltNameChoice == CERT_ALT_NAME_DNS_NAME)
        dns_names->push_back(WideToASCII(alt_name->rgAltEntry[i].pwszDNSName));
    }
  }
  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}

bool X509Certificate::HasExpired() const {
  return Time::Now() > valid_expiry();
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
  if (net::IsCertStatusError(cert_status) ||
      (cert_status & net::CERT_STATUS_REV_CHECKING_ENABLED) == 0)
    return false;

  net::EVRootCAMetadata* metadata = net::EVRootCAMetadata::GetInstance();

  PCCERT_CHAIN_CONTEXT chain_context = ConstructCertChain(cert_handle_,
      metadata->GetPolicyOIDs(), metadata->NumPolicyOIDs());
  if (!chain_context)
    return false;
  ScopedCertChainContext scoped_chain_context(chain_context);

  DCHECK(chain_context->cChain != 0);
  // If the cert doesn't match any of the policies, the
  // CERT_TRUST_IS_NOT_VALID_FOR_USAGE bit (0x10) in
  // chain_context->TrustStatus.dwErrorStatus is set.
  DWORD error_status = chain_context->TrustStatus.dwErrorStatus;
  DWORD info_status = chain_context->TrustStatus.dwInfoStatus;
  if (!chain_context->cChain || error_status != CERT_TRUST_NO_ERROR)
    return false;

  // Check the end certificate simple chain (chain_context->rgpChain[0]).
  // If the end certificate's certificatePolicies extension contains the
  // EV policy OID of the root CA, return true.
  PCERT_CHAIN_ELEMENT* element = chain_context->rgpChain[0]->rgpElement;
  int num_elements = chain_context->rgpChain[0]->cElement;
  if (num_elements < 2)
    return false;

  // Look up the EV policy OID of the root CA.
  PCCERT_CONTEXT root_cert = element[num_elements - 1]->pCertContext;
  X509Certificate::Fingerprint fingerprint = CalculateFingerprint(root_cert);
  std::string ev_policy_oid;
  if (!metadata->GetPolicyOID(fingerprint, &ev_policy_oid))
    return false;
  DCHECK(!ev_policy_oid.empty());

  // Get the certificatePolicies extension of the end certificate.
  PCCERT_CONTEXT end_cert = element[0]->pCertContext;
  scoped_ptr_malloc<CERT_POLICIES_INFO> policies_info;
  GetCertPoliciesInfo(end_cert, &policies_info);
  if (!policies_info.get())
    return false;

  return ContainsPolicy(policies_info.get(), ev_policy_oid.c_str());
}

}  // namespace net

