// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_LOGGING_H_
#define CHROME_RENDERER_RENDERER_LOGGING_H_

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace renderer_logging {

// Sets the URL that is logged if the renderer crashes. Use GURL() to clear
// the URL.
void SetActiveRendererURL(const GURL& url);

// Simple wrapper class that sets the active rendering URL in it's constructor
// and clears the active rendering URL in the destructor.
class ScopedActiveRenderingURLSetter {
 public:
  explicit ScopedActiveRenderingURLSetter(const GURL& url)  {
    SetActiveRendererURL(url);
  }

  ~ScopedActiveRenderingURLSetter()  {
    SetActiveRendererURL(GURL());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedActiveRenderingURLSetter);
};

}  // namespace renderer_logging

#if defined(OS_MACOSX) && __OBJC__
// Exported for testing purposes.

@class NSString;

typedef void (*SetCrashKeyValueFuncPtr)(NSString*, NSString*);
typedef void (*ClearCrashKeyValueFuncPtr)(NSString*);

namespace renderer_logging {
void SetActiveRendererURLImpl(const GURL& url,
                              SetCrashKeyValueFuncPtr set_key_func,
                              ClearCrashKeyValueFuncPtr clear_key_func);

extern const int kMaxNumCrashURLChunks;
extern const int kMaxNumURLChunkValueLength;
extern const char *kUrlChunkFormatStr;
}  // namespace renderer_logging

#endif  // defined(OS_MACOSX) && __OBJC__

#endif  // CHROME_RENDERER_RENDERER_LOGGING_H_
