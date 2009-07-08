// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/string_util.h"
#include "googleurl/src/gurl.h"
//#import "chrome/app/breakpad_mac.h"

namespace child_process_logging {

const int kMaxNumCrashURLChunks = 8;
const int kMaxNumURLChunkValueLength = 255;
const char *kUrlChunkFormatStr = "url-chunk-%d";

void SetActiveURLImpl(const GURL& url,
                      SetCrashKeyValueFuncPtr set_key_func,
                      ClearCrashKeyValueFuncPtr clear_key_func) {

  NSString *kUrlChunkFormatStr_utf8 = [NSString
      stringWithUTF8String:kUrlChunkFormatStr];

  // First remove any old url chunks we might have lying around.
  for (int i = 0; i < kMaxNumCrashURLChunks; i++) {
    // On Windows the url-chunk items are 1-based, so match that.
    NSString *key = [NSString stringWithFormat:kUrlChunkFormatStr_utf8, i+1];
    clear_key_func(key);
  }

  const std::string& raw_url_utf8 = url.possibly_invalid_spec();
  NSString *raw_url = [NSString stringWithUTF8String:raw_url_utf8.c_str()];
  size_t raw_url_length = [raw_url length];

  // Bail on zero-length URLs.
  if (raw_url_length == 0) {
    return;
  }

  // Parcel the URL up into up to 8, 255 byte segments.
  size_t start_ofs = 0;
  for (int i = 0;
       i < kMaxNumCrashURLChunks && start_ofs < raw_url_length;
       ++i) {

    // On Windows the url-chunk items are 1-based, so match that.
    NSString *key = [NSString stringWithFormat:kUrlChunkFormatStr_utf8, i+1];
    NSRange range;
    range.location = start_ofs;
    range.length = std::min((size_t)kMaxNumURLChunkValueLength,
                            raw_url_length - start_ofs);
    NSString *value = [raw_url substringWithRange:range];
    set_key_func(key, value);

    // Next chunk.
    start_ofs += kMaxNumURLChunkValueLength;
  }
}

void SetActiveURL(const GURL& url) {
/*
  // If Breakpad isn't initialized then bail.
  if (IsCrashReporterDisabled()) {
    return;
  }

  SetActiveURLImpl(url, SetCrashKeyValue, ClearCrashKeyValue);
*/
}

}  // namespace child_process_logging
