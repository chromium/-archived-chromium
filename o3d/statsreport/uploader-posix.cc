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
#include "backend/serverconnectionmanager.h"
#include "statsreport/common/const_product.h"
#include "statsreport/const_server.h"
#include "iobuffer/iobuffer-inl.h"
#include "statsreport/aggregator-posix-inl.h"
#include "statsreport/const-posix.h"
#include "statsreport/formatter.h"
#include "statsreport/uploader.h"


DECLARE_int32(metrics_aggregation_interval);
DECLARE_int32(metrics_upload_interval);
DECLARE_string(metrics_server_name);
DECLARE_int32(metrics_server_port);

using stats_report::Formatter;
using stats_report::MetricsAggregatorPosix;

namespace {

bool UploadMetrics(const char* extra_url_data, const char* user_agent,
                   const char *content) {
  o3d_transfer_service::ServerConnectionManager scm(
      METRICS_SERVER_NAME,
      METRICS_SERVER_PORT,
      false,
      PRODUCT_VERSION_STRING,
      PRODUCT_NAME_STRING);

  PathString path = PathString("/" METRICS_SERVER_PATH "?") +
                    kStatsServerParamSourceId + "=" +
                    PRODUCT_NAME_STRING + "&" +
                    kStatsServerParamVersion + "=" +
                    PRODUCT_VERSION_STRING + "&" +
                    extra_url_data;

  IOBuffer buffer_out;
  o3d_transfer_service::HttpResponse response;
  return scm.SimplePost(path, const_cast<char *>(content),
                        strlen(content), &buffer_out, &response);
}

}  // namespace stats_report
