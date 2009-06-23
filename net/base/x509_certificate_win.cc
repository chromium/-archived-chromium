// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/ev_root_ca_metadata.h"
#include "net/base/net_errors.h"
#include "net/base/scoped_cert_chain_context.h"

#pragma comment(lib, "crypt32.lib")

using base::Time;

namespace net {

namespace {

//-----------------------------------------------------------------------------

// TODO(wtc): This is a copy of the MapSecurityError function in
// ssl_client_socket_win.cc.  Another function that maps Windows error codes
// to our network error codes is WinInetUtil::OSErrorToNetError.  We should
// eliminate the code duplication.
int MapSecurityError(SECURITY_STATUS err) {
  // There are numerous security error codes, but these are the ones we thus
  // far find interesting.
  switch (err) {
    case SEC_E_WRONG_PRINCIPAL:  // Schannel
    case CERT_E_CN_NO_MATCH:  // CryptoAPI
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_E_UNTRUSTED_ROOT:  // Schannel
    case CERT_E_UNTRUSTEDROOT:  // CryptoAPI
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_E_CERT_EXPIRED:  // Schannel
    case CERT_E_EXPIRED:  // CryptoAPI
      return ERR_CERT_DATE_INVALID;
    case CRYPT_E_NO_REVOCATION_CHECK:
      return ERR_CERT_NO_REVOCATION_MECHANISM;
    case CRYPT_E_REVOCATION_OFFLINE:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case CRYPT_E_REVOKED:  // Schannel and CryptoAPI
      return ERR_CERT_REVOKED;
    case SEC_E_CERT_UNKNOWN:
    case CERT_E_ROLE:
      return ERR_CERT_INVALID;
    // We received an unexpected_message or illegal_parameter alert message
    // from the server.
    case SEC_E_ILLEGAL_MESSAGE:
      return ERR_SSL_PROTOCOL_ERROR;
    case SEC_E_ALGORITHM_MISMATCH:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SEC_E_INVALID_HANDLE:
      return ERR_UNEXPECTED;
    case SEC_E_OK:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map the errors in the chain_context->TrustStatus.dwErrorStatus returned by
// CertGetCertificateChain to our certificate status flags.
int MapCertChainErrorStatusToCertStatus(DWORD error_status) {
  int cert_status = 0;

  // CERT_TRUST_IS_NOT_TIME_NESTED means a subject certificate's time validity
  // does not nest correctly within its issuer's time validity.
  const DWORD kDateInvalidErrors = CERT_TRUST_IS_NOT_TIME_VALID |
                                   CERT_TRUST_IS_NOT_TIME_NESTED |
                                   CERT_TRUST_CTL_IS_NOT_TIME_VALID;
  if (error_status & kDateInvalidErrors)
    cert_status |= CERT_STATUS_DATE_INVALID;

  const DWORD kAuthorityInvalidErrors = CERT_TRUST_IS_UNTRUSTED_ROOT |
                                        CERT_TRUST_IS_EXPLICIT_DISTRUST |
                                        CERT_TRUST_IS_PARTIAL_CHAIN;
  if (error_status & kAuthorityInvalidErrors)
    cert_status |= CERT_STATUS_AUTHORITY_INVALID;

  if ((error_status & CERT_TRUST_REVOCATION_STATUS_UNKNOWN) &&
      !(error_status & CERT_TRUST_IS_OFFLINE_REVOCATION))
    cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (error_status & CERT_TRUST_IS_OFFLINE_REVOCATION)
    cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  if (error_status & CERT_TRUST_IS_REVOKED)
    cert_status |= CERT_STATUS_REVOKED;

  const DWORD kWrongUsageErrors = CERT_TRUST_IS_NOT_VALID_FOR_USAGE |
                                  CERT_TRUST_CTL_IS_NOT_VALID_FOR_USAGE;
  if (error_status & kWrongUsageErrors) {
    // TODO(wtc): Handle these errors.
    // cert_status = |= CERT_STATUS_WRONG_USAGE;
  }

  // The rest of the errors.
  const DWORD kCertInvalidErrors =
      CERT_TRUST_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_IS_CYCLIC |
      CERT_TRUST_INVALID_EXTENSION |
      CERT_TRUST_INVALID_POLICY_CONSTRAINTS |
      CERT_TRUST_INVALID_BASIC_CONSTRAINTS |
      CERT_TRUST_INVALID_NAME_CONSTRAINTS |
      CERT_TRUST_CTL_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_HAS_NOT_SUPPORTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_DEFINED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_PERMITTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_EXCLUDED_NAME_CONSTRAINT |
      CERT_TRUST_NO_ISSUANCE_CHAIN_POLICY |
      CERT_TRUST_HAS_NOT_SUPPORTED_CRITICAL_EXT;
  if (error_status & kCertInvalidErrors)
    cert_status |= CERT_STATUS_INVALID;

  return cert_status;
}

//-----------------------------------------------------------------------------

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

// Saves some information about the certificate chain chain_context in
// *verify_result.  The caller MUST initialize *verify_result before calling
// this function.
void GetCertChainInfo(PCCERT_CHAIN_CONTEXT chain_context,
                      CertVerifyResult* verify_result) {
  PCERT_SIMPLE_CHAIN first_chain = chain_context->rgpChain[0];
  int num_elements = first_chain->cElement;
  PCERT_CHAIN_ELEMENT* element = first_chain->rgpElement;

  // Each chain starts with the end entity certificate (i = 0) and ends with
  // the root CA certificate (i = num_elements - 1).  Do not inspect the
  // signature algorithm of the root CA certificate because the signature on
  // the trust anchor is not important.
  for (int i = 0; i < num_elements - 1; ++i) {
    PCCERT_CONTEXT cert = element[i]->pCertContext;
    const char* algorithm = cert->pCertInfo->SignatureAlgorithm.pszObjId;
    if (strcmp(algorithm, szOID_RSA_MD5RSA) == 0) {
      // md5WithRSAEncryption: 1.2.840.113549.1.1.4
      verify_result->has_md5 = true;
      if (i != 0)
        verify_result->has_md5_ca = true;
    } else if (strcmp(algorithm, szOID_RSA_MD2RSA) == 0) {
      // md2WithRSAEncryption: 1.2.840.113549.1.1.2
      verify_result->has_md2 = true;
      if (i != 0)
        verify_result->has_md2_ca = true;
    } else if (strcmp(algorithm, szOID_RSA_MD4RSA) == 0) {
      // md4WithRSAEncryption: 1.2.840.113549.1.1.3
      verify_result->has_md4 = true;
    }
  }
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

  return CreateFromHandle(cert_handle, SOURCE_LONE_CERT_IMPORT);
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

int X509Certificate::Verify(const std::string& hostname,
                            int flags,
                            CertVerifyResult* verify_result) const {
  verify_result->Reset();

  // Build and validate certificate chain.

  CERT_CHAIN_PARA chain_para;
  memset(&chain_para, 0, sizeof(chain_para));
  chain_para.cbSize = sizeof(chain_para);
  // TODO(wtc): consider requesting the usage szOID_PKIX_KP_SERVER_AUTH
  // or szOID_SERVER_GATED_CRYPTO or szOID_SGC_NETSCAPE
  chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
  chain_para.RequestedUsage.Usage.cUsageIdentifier = 0;
  chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = NULL;  // LPSTR*
  // We can set CERT_CHAIN_RETURN_LOWER_QUALITY_CONTEXTS to get more chains.
  DWORD chain_flags = CERT_CHAIN_CACHE_END_CERT;
  if (flags & VERIFY_REV_CHECKING_ENABLED) {
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
    chain_flags |= CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  } else {
    chain_flags |= CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;
    // EV requires revocation checking.
    flags &= ~VERIFY_EV_CERT;
  }
  PCCERT_CHAIN_CONTEXT chain_context;
  if (!CertGetCertificateChain(
           NULL,  // default chain engine, HCCE_CURRENT_USER
           cert_handle_,
           NULL,  // current system time
           cert_handle_->hCertStore,  // search this store
           &chain_para,
           chain_flags,
           NULL,  // reserved
           &chain_context)) {
    return MapSecurityError(GetLastError());
  }
  ScopedCertChainContext scoped_chain_context(chain_context);

  GetCertChainInfo(chain_context, verify_result);

  verify_result->cert_status |= MapCertChainErrorStatusToCertStatus(
      chain_context->TrustStatus.dwErrorStatus);

  std::wstring wstr_hostname = ASCIIToWide(hostname);

  SSL_EXTRA_CERT_CHAIN_POLICY_PARA extra_policy_para;
  memset(&extra_policy_para, 0, sizeof(extra_policy_para));
  extra_policy_para.cbSize = sizeof(extra_policy_para);
  extra_policy_para.dwAuthType = AUTHTYPE_SERVER;
  extra_policy_para.fdwChecks = 0;
  extra_policy_para.pwszServerName =
      const_cast<wchar_t*>(wstr_hostname.c_str());

  CERT_CHAIN_POLICY_PARA policy_para;
  memset(&policy_para, 0, sizeof(policy_para));
  policy_para.cbSize = sizeof(policy_para);
  policy_para.dwFlags = 0;
  policy_para.pvExtraPolicyPara = &extra_policy_para;

  CERT_CHAIN_POLICY_STATUS policy_status;
  memset(&policy_status, 0, sizeof(policy_status));
  policy_status.cbSize = sizeof(policy_status);

  if (!CertVerifyCertificateChainPolicy(
           CERT_CHAIN_POLICY_SSL,
           chain_context,
           &policy_para,
           &policy_status)) {
    return MapSecurityError(GetLastError());
  }

  if (policy_status.dwError) {
    verify_result->cert_status |= MapNetErrorToCertStatus(
        MapSecurityError(policy_status.dwError));

    // CertVerifyCertificateChainPolicy reports only one error (in
    // policy_status.dwError) if the certificate has multiple errors.
    // CertGetCertificateChain doesn't report certificate name mismatch, so
    // CertVerifyCertificateChainPolicy is the only function that can report
    // certificate name mismatch.
    //
    // To prevent a potential certificate name mismatch from being hidden by
    // some other certificate error, if we get any other certificate error,
    // we call CertVerifyCertificateChainPolicy again, ignoring all other
    // certificate errors.  Both extra_policy_para.fdwChecks and
    // policy_para.dwFlags allow us to ignore certificate errors, so we set
    // them both.
    if (policy_status.dwError != CERT_E_CN_NO_MATCH) {
      const DWORD extra_ignore_flags =
          0x00000080 |  // SECURITY_FLAG_IGNORE_REVOCATION
          0x00000100 |  // SECURITY_FLAG_IGNORE_UNKNOWN_CA
          0x00002000 |  // SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
          0x00000200;   // SECURITY_FLAG_IGNORE_WRONG_USAGE
      extra_policy_para.fdwChecks = extra_ignore_flags;
      const DWORD ignore_flags =
          CERT_CHAIN_POLICY_IGNORE_ALL_NOT_TIME_VALID_FLAGS |
          CERT_CHAIN_POLICY_IGNORE_INVALID_BASIC_CONSTRAINTS_FLAG |
          CERT_CHAIN_POLICY_ALLOW_UNKNOWN_CA_FLAG |
          CERT_CHAIN_POLICY_IGNORE_WRONG_USAGE_FLAG |
          CERT_CHAIN_POLICY_IGNORE_INVALID_NAME_FLAG |
          CERT_CHAIN_POLICY_IGNORE_INVALID_POLICY_FLAG |
          CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS |
          CERT_CHAIN_POLICY_ALLOW_TESTROOT_FLAG |
          CERT_CHAIN_POLICY_TRUST_TESTROOT_FLAG |
          CERT_CHAIN_POLICY_IGNORE_NOT_SUPPORTED_CRITICAL_EXT_FLAG |
          CERT_CHAIN_POLICY_IGNORE_PEER_TRUST_FLAG;
      policy_para.dwFlags = ignore_flags;
      if (!CertVerifyCertificateChainPolicy(
               CERT_CHAIN_POLICY_SSL,
               chain_context,
               &policy_para,
               &policy_status)) {
        return MapSecurityError(GetLastError());
      }
      if (policy_status.dwError) {
        verify_result->cert_status |= MapNetErrorToCertStatus(
            MapSecurityError(policy_status.dwError));
      }
    }
  }

  // TODO(wtc): Suppress CERT_STATUS_NO_REVOCATION_MECHANISM for now to be
  // compatible with WinHTTP, which doesn't report this error (bug 3004).
  verify_result->cert_status &= ~CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  // TODO(ukai): combine regular cert verification and EV cert verification.
  if ((flags & VERIFY_EV_CERT) && VerifyEV())
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  return OK;
}

// Returns true if the certificate is an extended-validation certificate.
//
// This function checks the certificatePolicies extensions of the
// certificates in the certificate chain according to Section 7 (pp. 11-12)
// of the EV Certificate Guidelines Version 1.0 at
// http://cabforum.org/EV_Certificate_Guidelines.pdf.
bool X509Certificate::VerifyEV() const {
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
  Fingerprint fingerprint = CalculateFingerprint(root_cert);
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

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  OSCertHandle cert_handle = NULL;
  if (!CertAddEncodedCertificateToStore(
      NULL,  // the cert won't be persisted in any cert store
      X509_ASN_ENCODING,
      reinterpret_cast<const BYTE*>(data), length,
      CERT_STORE_ADD_USE_EXISTING,
      &cert_handle))
    return NULL;

  return cert_handle;
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CertFreeCertificateContext(cert_handle);
}

// static
X509Certificate::Fingerprint X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  DCHECK(NULL != cert->pbCertEncoded);
  DCHECK(0 != cert->cbCertEncoded);

  BOOL rv;
  Fingerprint sha1;
  DWORD sha1_size = sizeof(sha1.data);
  rv = CryptHashCertificate(NULL, CALG_SHA1, 0, cert->pbCertEncoded,
                            cert->cbCertEncoded, sha1.data, &sha1_size);
  DCHECK(rv && sha1_size == sizeof(sha1.data));
  if (!rv)
    memset(sha1.data, 0, sizeof(sha1.data));
  return sha1;
}

}  // namespace net
