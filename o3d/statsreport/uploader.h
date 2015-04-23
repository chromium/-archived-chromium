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
#ifndef O3D_STATSREPORT_UPLOADER_H_
#define O3D_STATSREPORT_UPLOADER_H_

#ifdef OS_WINDOWS
#include <atlstr.h>
#endif

#include "statsreport/metrics.h"

namespace stats_report {

class StatsUploader {
 public:
  StatsUploader() {}
  virtual ~StatsUploader() {}
  virtual bool UploadMetrics(const char* extra_url_data,
                             const char* user_agent,
                             const char *content);
 private:
  DISALLOW_COPY_AND_ASSIGN(StatsUploader);
};

bool AggregateMetrics();
bool AggregateAndReportMetrics(const char* extra_url_arguments,
                               const char* user_agent,
                               bool force_report);
bool TestableAggregateAndReportMetrics(const char* extra_url_arguments,
                                       const char* user_agent,
                                       bool force_report,
                                       StatsUploader* stats_uploader);
bool UploadMetrics(const char* extra_url_data, const char* user_agent,
                   const char *content);

#ifdef OS_WINDOWS
void ResetPersistentMetrics(CRegKey *key);
#else
void ResetPersistentMetrics(void);
#endif

}  // namespace stats_report

#endif  // O3D_STATSREPORT_UPLOADER_H_
