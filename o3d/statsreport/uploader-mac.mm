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

#import <Cocoa/Cocoa.h>
#include "base/logging.h"

#include "statsreport/uploader.h"
#include "statsreport/aggregator-mac.h"
#include "statsreport/const-mac.h"
#include "statsreport/formatter.h"
#include "statsreport/common/const_product.h"
#include "statsreport/const_server.h"

const float kTimeOutInterval = 10.0;

namespace stats_report {
  // TODO: Refactor to avoid cross platform code duplication.

bool UploadMetrics(const char* extra_url_data, const char* user_agent,
                   const char *content) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];


  // TODO: refactor to where there's a platform-independent HTTP
  //      interface to the ConnectionManager and pass it through into here.
  DLOG(INFO) << "Uploading metrics . . . ";

  NSString *url_string =
      [NSString stringWithFormat:@"http://%s:%d/%s?%s=%s&%s=%s&%s",
       METRICS_SERVER_NAME,
       METRICS_SERVER_PORT,
       METRICS_SERVER_PATH,
       kStatsServerParamSourceId,
       PRODUCT_NAME_STRING,
       kStatsServerParamVersion,
       PRODUCT_VERSION_STRING,
       extra_url_data];
  DLOG(INFO) << "Url: " << [url_string UTF8String];

  NSURL *url = [NSURL URLWithString:url_string];

  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];

  // don't send cookies or user-identifiable info with our request.
  [request setHTTPShouldHandleCookies:NO];
  [request setTimeoutInterval:kTimeOutInterval];
  int content_len = (content == NULL) ? 0 : strlen(content);
  if (content_len) {
    NSData *content_data = [NSData dataWithBytesNoCopy:(void*)content
                                                length:content_len];
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:content_data];
  }
  NSURLResponse *response = nil;
  NSError *error = nil;

  [NSURLConnection sendSynchronousRequest:request
                        returningResponse:&response
                                    error:&error];

  [pool release];

  return (error == nil);
}

}  // namespace stats_report
