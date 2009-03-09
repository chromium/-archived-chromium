// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_LOGGING_H_
#define CHROME_RENDERER_RENDERER_LOGGING_H_

#include "base/basictypes.h"

class GURL;

namespace renderer_logging {

// Sets the URL that is logged if the renderer crashes. Use GURL() to clear
// the URL.
void SetActiveRendererURL(const GURL& url);

// Simple wrapper class that sets the active rendering URL in it's constructor
// and clears the active rendering URL in the destructor.
class ScopedActiveRenderingURLSetter {
 public:
  explicit ScopedActiveRenderingURLSetter(const GURL& url);
  ~ScopedActiveRenderingURLSetter();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedActiveRenderingURLSetter);
};

}  // namespace renderer_logging

#endif  // CHROME_RENDERER_RENDERER_LOGGING_H_
