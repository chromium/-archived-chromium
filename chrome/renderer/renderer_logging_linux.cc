// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_logging.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace renderer_logging {

// Sets the URL that is logged if the renderer crashes. Use GURL() to clear
// the URL.
void SetActiveRendererURL(const GURL& url) {
  // crbug.com/9646
}

}  // namespace renderer_logging
