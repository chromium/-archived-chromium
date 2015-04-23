// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/wininet_util.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

// static
int WinInetUtil::OSErrorToNetError(DWORD os_error) {
  // Optimize the common case.
  if (os_error == ERROR_IO_PENDING)
    return net::ERR_IO_PENDING;

  switch (os_error) {
    case ERROR_SUCCESS:
      return net::OK;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return net::ERR_FILE_NOT_FOUND;
    case ERROR_HANDLE_EOF:  // TODO(wtc): return net::OK?
      return net::ERR_CONNECTION_CLOSED;
    case ERROR_INVALID_HANDLE:
      return net::ERR_INVALID_HANDLE;
    case ERROR_INVALID_PARAMETER:
      return net::ERR_INVALID_ARGUMENT;

    case ERROR_INTERNET_CANNOT_CONNECT:
      return net::ERR_CONNECTION_FAILED;
    case ERROR_INTERNET_CONNECTION_RESET:
      return net::ERR_CONNECTION_RESET;
    case ERROR_INTERNET_DISCONNECTED:
      return net::ERR_INTERNET_DISCONNECTED;
    case ERROR_INTERNET_INVALID_URL:
      return net::ERR_INVALID_URL;
    case ERROR_INTERNET_NAME_NOT_RESOLVED:
      return net::ERR_NAME_NOT_RESOLVED;
    case ERROR_INTERNET_OPERATION_CANCELLED:
      return net::ERR_ABORTED;
    case ERROR_INTERNET_UNRECOGNIZED_SCHEME:
      return net::ERR_UNKNOWN_URL_SCHEME;

    // SSL certificate errors
    case ERROR_INTERNET_SEC_CERT_CN_INVALID:
      return net::ERR_CERT_COMMON_NAME_INVALID;
    case ERROR_INTERNET_SEC_CERT_DATE_INVALID:
      return net::ERR_CERT_DATE_INVALID;
    case ERROR_INTERNET_INVALID_CA:
      return net::ERR_CERT_AUTHORITY_INVALID;
    case ERROR_INTERNET_SEC_CERT_NO_REV:
      return net::ERR_CERT_NO_REVOCATION_MECHANISM;
    case ERROR_INTERNET_SEC_CERT_REV_FAILED:
      return net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case ERROR_INTERNET_SEC_CERT_REVOKED:
      return net::ERR_CERT_REVOKED;
    case ERROR_INTERNET_SEC_CERT_ERRORS:
      return net::ERR_CERT_CONTAINS_ERRORS;
    case ERROR_INTERNET_SEC_INVALID_CERT:
      return net::ERR_CERT_INVALID;

    case ERROR_INTERNET_EXTENDED_ERROR:
    default:
      return net::ERR_FAILED;
  }
}

}  // namespace net
