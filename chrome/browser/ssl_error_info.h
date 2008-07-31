// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_SSL_ERROR_INFO_H__
#define CHROME_BROWSER_SSL_ERROR_INFO_H__

#include <string>
#include <vector>

#include "base/logging.h"
#include "net/base/x509_certificate.h"

class GURL;

// This class describes an error that happened while showing a page over SSL.
// An SSLErrorInfo object only exists on the UI thread and only contains
// information about an error (type of error and text details).
// Note no DISALLOW_EVIL_CONSTRUCTORS as we want the copy constructor.
class SSLErrorInfo {
 public:
  enum ErrorType {
    CERT_COMMON_NAME_INVALID = 0,
    CERT_DATE_INVALID,
    CERT_AUTHORITY_INVALID,
    CERT_CONTAINS_ERRORS,
    CERT_NO_REVOCATION_MECHANISM,
    CERT_UNABLE_TO_CHECK_REVOCATION,
    CERT_REVOKED,
    CERT_INVALID,
    MIXED_CONTENTS,
    UNSAFE_CONTENTS,
    UNKNOWN
  };

  virtual ~SSLErrorInfo();

  // Converts a network error code to an ErrorType.
  static ErrorType NetErrorToErrorType(int net_error);

  static SSLErrorInfo CreateError(ErrorType error_type,
                                  net::X509Certificate* cert,
                                  const GURL& request_url);

  // Populates the specified |errors| vector with the errors contained in
  // |cert_status|.  Returns the number of errors found.
  // Callers only interested in the error count can pass NULL for |errors|.
  static int GetErrorsForCertStatus(int cert_status,
                                    int cert_id,
                                    const GURL& request_url,
                                    std::vector<SSLErrorInfo>* errors);

  // A title describing the error, usually to be used with the details below.
  const std::wstring& title() const { return title_; }

  // A description of the error.
  const std::wstring& details() const { return details_; }

  // A short message describing the error (1 line).
  const std::wstring& short_description() const { return short_description_; }

  // A lengthy explanation of what the error is.  Each entry in the returned
  // vector is a paragraph.
  const std::vector<std::wstring>& extra_information() const {
    return extra_information_;
  }

private:
  SSLErrorInfo(const std::wstring& title,
               const std::wstring& details,
               const std::wstring& short_description,
               const std::vector<std::wstring>& extra_info);

  std::wstring title_;
  std::wstring details_;
  std::wstring short_description_;
  // Extra-informations contains paragraphs of text explaining in details what
  // the error is and what the risks are.
  std::vector<std::wstring> extra_information_;
};

#endif  // CHROME_BROWSER_SSL_ERROR_INFO_H__
