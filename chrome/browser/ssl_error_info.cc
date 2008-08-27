// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl_error_info.h"

#include "base/string_util.h"
#include "chrome/browser/cert_store.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/time_format.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"
#include "googleurl/src/gurl.h"

#include "chromium_strings.h"
#include "generated_resources.h"

SSLErrorInfo::SSLErrorInfo(const std::wstring& title,
                           const std::wstring& details,
                           const std::wstring& short_description,
                           const std::vector<std::wstring>& extra_info)
    : title_(title),
      details_(details),
      short_description_(short_description),
      extra_information_(extra_info) {
}

// static
SSLErrorInfo SSLErrorInfo::CreateError(ErrorType error_type,
                                       net::X509Certificate* cert,
                                       const GURL& request_url) {
  std::wstring title, details, short_description;
  std::vector<std::wstring> extra_info;
  switch (error_type) {
    case CERT_COMMON_NAME_INVALID: {
      title = l10n_util::GetString(IDS_CERT_ERROR_COMMON_NAME_INVALID_TITLE);
      // If the certificate contains multiple DNS names, we choose the most
      // representative one -- either the DNS name that's also in the subject
      // field, or the first one.  If this heuristic turns out to be
      // inadequate, we can consider choosing the DNS name that is the
      // "closest match" to the host name in the request URL, or listing all
      // the DNS names with an HTML <ul>.
      std::vector<std::string> dns_names;
      cert->GetDNSNames(&dns_names);
      DCHECK(!dns_names.empty());
      size_t i = 0;
      for (; i < dns_names.size(); ++i) {
        if (dns_names[i] == cert->subject().common_name)
          break;
      }
      if (i == dns_names.size())
        i = 0;
      details =
          l10n_util::GetStringF(IDS_CERT_ERROR_COMMON_NAME_INVALID_DETAILS,
                                UTF8ToWide(request_url.host()),
                                UTF8ToWide(dns_names[i]),
                                UTF8ToWide(request_url.host()));
      short_description =
          l10n_util::GetString(IDS_CERT_ERROR_COMMON_NAME_INVALID_DESCRIPTION);
      extra_info.push_back(
          l10n_util::GetString(IDS_CERT_ERROR_EXTRA_INFO_1));
      extra_info.push_back(
          l10n_util::GetStringF(
              IDS_CERT_ERROR_COMMON_NAME_INVALID_EXTRA_INFO_2,
              UTF8ToWide(cert->subject().common_name),
              UTF8ToWide(request_url.host())));
      break;
    }
    case CERT_DATE_INVALID:
      extra_info.push_back(
          l10n_util::GetString(IDS_CERT_ERROR_EXTRA_INFO_1));
      if (cert->HasExpired()) {
        title = l10n_util::GetString(IDS_CERT_ERROR_EXPIRED_TITLE);
        details = l10n_util::GetStringF(IDS_CERT_ERROR_EXPIRED_DETAILS,
                                        UTF8ToWide(request_url.host()),
                                        UTF8ToWide(request_url.host()));
        short_description =
            l10n_util::GetString(IDS_CERT_ERROR_EXPIRED_DESCRIPTION);
        extra_info.push_back(
            l10n_util::GetString(IDS_CERT_ERROR_EXPIRED_DETAILS_EXTRA_INFO_2));
      } else {
        // Then it must be not yet valid.  We don't check that it is not yet
        // valid as there is still a very unlikely chance that the cert might
        // have become valid since the error occurred.
        title = l10n_util::GetString(IDS_CERT_ERROR_NOT_YET_VALID_TITLE);
        details = l10n_util::GetStringF(IDS_CERT_ERROR_NOT_YET_VALID_DETAILS,
                                        UTF8ToWide(request_url.host()),
                                        UTF8ToWide(request_url.host()));
        short_description =
            l10n_util::GetString(IDS_CERT_ERROR_NOT_YET_VALID_DESCRIPTION);
        extra_info.push_back(
            l10n_util::GetString(
                IDS_CERT_ERROR_NOT_YET_VALID_DETAILS_EXTRA_INFO_2));
      }
      break;
    case CERT_AUTHORITY_INVALID:
      title = l10n_util::GetString(IDS_CERT_ERROR_AUTHORITY_INVALID_TITLE);
      details = l10n_util::GetStringF(IDS_CERT_ERROR_AUTHORITY_INVALID_DETAILS,
                                      UTF8ToWide(request_url.host()));
      short_description =
          l10n_util::GetString(IDS_CERT_ERROR_AUTHORITY_INVALID_DESCRIPTION);
      extra_info.push_back(
          l10n_util::GetString(IDS_CERT_ERROR_EXTRA_INFO_1));
      extra_info.push_back(
          l10n_util::GetStringF(IDS_CERT_ERROR_AUTHORITY_INVALID_EXTRA_INFO_2,
                                UTF8ToWide(request_url.host()),
                                UTF8ToWide(request_url.host())));
      extra_info.push_back(
          l10n_util::GetString(IDS_CERT_ERROR_AUTHORITY_INVALID_EXTRA_INFO_3));
      break;
    case CERT_CONTAINS_ERRORS:
      title = l10n_util::GetString(IDS_CERT_ERROR_CONTAINS_ERRORS_TITLE);
      details = l10n_util::GetStringF(IDS_CERT_ERROR_CONTAINS_ERRORS_DETAILS,
                                      UTF8ToWide(request_url.host()));
      short_description =
          l10n_util::GetString(IDS_CERT_ERROR_CONTAINS_ERRORS_DESCRIPTION);
      extra_info.push_back(
          l10n_util::GetStringF(IDS_CERT_ERROR_EXTRA_INFO_1,
                                UTF8ToWide(request_url.host())));
      extra_info.push_back(
          l10n_util::GetString(IDS_CERT_ERROR_CONTAINS_ERRORS_EXTRA_INFO_2));
      break;
    case CERT_NO_REVOCATION_MECHANISM:
      title =
          l10n_util::GetString(IDS_CERT_ERROR_NO_REVOCATION_MECHANISM_TITLE);
      details =
          l10n_util::GetString(IDS_CERT_ERROR_NO_REVOCATION_MECHANISM_DETAILS);
      short_description = l10n_util::GetString(
          IDS_CERT_ERROR_NO_REVOCATION_MECHANISM_DESCRIPTION);
      break;
    case CERT_UNABLE_TO_CHECK_REVOCATION:
      title =
          l10n_util::GetString(IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_TITLE);
      details = l10n_util::GetString(
          IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_DETAILS);
      short_description = l10n_util::GetString(
          IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_DESCRIPTION);
      break;
    case CERT_REVOKED:
      title = l10n_util::GetString(IDS_CERT_ERROR_REVOKED_CERT_TITLE);
      details = l10n_util::GetStringF(IDS_CERT_ERROR_REVOKED_CERT_DETAILS,
                                      UTF8ToWide(request_url.host()));
      short_description =
          l10n_util::GetString(IDS_CERT_ERROR_REVOKED_CERT_DESCRIPTION);
      extra_info.push_back(
          l10n_util::GetString(IDS_CERT_ERROR_EXTRA_INFO_1));
      extra_info.push_back(
          l10n_util::GetString(IDS_CERT_ERROR_REVOKED_CERT_EXTRA_INFO_2));
      break;
    case CERT_INVALID:
      title = l10n_util::GetString(IDS_CERT_ERROR_INVALID_CERT_TITLE);
      details = l10n_util::GetString(IDS_CERT_ERROR_INVALID_CERT_DETAILS);
      short_description =
          l10n_util::GetString(IDS_CERT_ERROR_INVALID_CERT_DESCRIPTION);
      break;
    case MIXED_CONTENTS:
      title = l10n_util::GetString(IDS_SSL_MIXED_CONTENT_TITLE);
      details = l10n_util::GetString(IDS_SSL_MIXED_CONTENT_DETAILS);
      short_description =
          l10n_util::GetString(IDS_SSL_MIXED_CONTENT_DESCRIPTION);
      break;
    case UNSAFE_CONTENTS:
      title = l10n_util::GetString(IDS_SSL_UNSAFE_CONTENT_TITLE);
      details = l10n_util::GetString(IDS_SSL_UNSAFE_CONTENT_DETAILS);
      short_description =
          l10n_util::GetString(IDS_SSL_UNSAFE_CONTENT_DESCRIPTION);
      break;
    case UNKNOWN:
      title = l10n_util::GetString(IDS_CERT_ERROR_UNKNOWN_ERROR_TITLE);
      details = l10n_util::GetString(IDS_CERT_ERROR_UNKNOWN_ERROR_DETAILS);
      short_description =
          l10n_util::GetString(IDS_CERT_ERROR_UNKNOWN_ERROR_DESCRIPTION);
      break;
    default:
      NOTREACHED();
  }
  return SSLErrorInfo(title, details, short_description, extra_info);
}

SSLErrorInfo::~SSLErrorInfo() {
}

// static
SSLErrorInfo::ErrorType SSLErrorInfo::NetErrorToErrorType(int net_error) {
  switch (net_error) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
      return CERT_COMMON_NAME_INVALID;
    case net::ERR_CERT_DATE_INVALID:
      return CERT_DATE_INVALID;
    case net::ERR_CERT_AUTHORITY_INVALID:
      return CERT_AUTHORITY_INVALID;
    case net::ERR_CERT_CONTAINS_ERRORS:
      return CERT_CONTAINS_ERRORS;
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
      return CERT_NO_REVOCATION_MECHANISM;
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      return CERT_UNABLE_TO_CHECK_REVOCATION;
    case net::ERR_CERT_REVOKED:
      return CERT_REVOKED;
    case net::ERR_CERT_INVALID:
      return CERT_INVALID;
    default:
      NOTREACHED();
      return UNKNOWN;
    }
}

// static
int SSLErrorInfo::GetErrorsForCertStatus(int cert_id,
                                         int cert_status,
                                         const GURL& url,
                                         std::vector<SSLErrorInfo>* errors) {
  const int kErrorFlags[] = {
    net::CERT_STATUS_COMMON_NAME_INVALID,
    net::CERT_STATUS_DATE_INVALID,
    net::CERT_STATUS_AUTHORITY_INVALID,
    net::CERT_STATUS_NO_REVOCATION_MECHANISM,
    net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
    net::CERT_STATUS_REVOKED,
    net::CERT_STATUS_INVALID
  };

  const ErrorType kErrorTypes[] = {
    CERT_COMMON_NAME_INVALID,
    CERT_DATE_INVALID,
    CERT_AUTHORITY_INVALID,
    CERT_NO_REVOCATION_MECHANISM,
    CERT_UNABLE_TO_CHECK_REVOCATION,
    CERT_REVOKED,
    CERT_INVALID
  };
  DCHECK(arraysize(kErrorFlags) == arraysize(kErrorTypes));

  scoped_refptr<net::X509Certificate> cert = NULL;
  int count = 0;
  for (int i = 0; i < arraysize(kErrorFlags); ++i) {
    if (cert_status & kErrorFlags[i]) {
      count++;
      if (!cert.get()) {
        bool r = CertStore::GetSharedInstance()->RetrieveCert(cert_id, &cert);
        DCHECK(r);
      }
      if (errors)
        errors->push_back(SSLErrorInfo::CreateError(kErrorTypes[i], cert, url));
    }
  }
  return count;
}

