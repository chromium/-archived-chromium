/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Helper class to manage the process of uploading metrics.

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>
#include <time.h>
#include "base/logging.h"

#include "statsreport/uploader.h"
#include "statsreport/aggregator-win32.h"
#include "statsreport/const-win32.h"
#include "statsreport/persistent_iterator-win32.h"
#include "statsreport/formatter.h"
#include "statsreport/common/const_product.h"
#include "statsreport/const_server.h"

namespace stats_report {
// TODO: Refactor to avoid cross platform code duplication.

bool UploadMetrics(const char* extra_url_data, const char* user_agent,
                   const char *content) {
  // TODO: refactor to where there's a platform-independent HTTP
  //      interface to the ConnectionManager and pass it through into here.
  DLOG(INFO) << "Uploading metrics . . . ";
  CString url;
  url.Format(L"http://%hs:%d/%hs?%hs=%hs&%hs=%hs&%hs",
             METRICS_SERVER_NAME,
             METRICS_SERVER_PORT,
             METRICS_SERVER_PATH,
             kStatsServerParamSourceId,
             PRODUCT_NAME_STRING,
             kStatsServerParamVersion,
             PRODUCT_VERSION_STRING,
             extra_url_data);
  DLOG(INFO) << "Url: " << url;

  CComPtr<IXMLHttpRequest> request;
  // create the http request object
  HRESULT hr = request.CoCreateInstance(CLSID_XMLHTTPRequest);
  if (FAILED(hr))
    return false;

  DLOG(INFO) << "Created request.";
  // open the request
  // TODO: async?
  CComVariant empty;
  CComVariant var_false(false);
  hr = request->open(CComBSTR("POST"), CComBSTR(url),
                     var_false, empty, empty);
  if (FAILED(hr))
    return false;
  DLOG(INFO) << "Opened request.";

  hr = request->setRequestHeader(CComBSTR("User-Agent"), CComBSTR(user_agent));
  if (FAILED(hr))
    DLOG(WARNING) << "Failed to set user-agent";

  // and send the file
  // TODO: progress?
  CComSafeArray<BYTE> content_array(strlen(content));
  hr = request->send(CComVariant(content));
  if (FAILED(hr))
    return false;
  DLOG(INFO) << "Sent content_array.";

#ifndef NDEBUG
  CComBSTR response;
  hr = request->get_responseText(&response);
#endif

  return true;
}

}  // namespace stats_report
