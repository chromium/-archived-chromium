// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_logging.h"

#include <string>

#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace renderer_logging {

// We use a static string to hold the most recent active url. If we crash, the
// crash handler code will send the contents of this string to the browser.
std::string active_url;

// Sets the URL that is logged if the renderer crashes. Use GURL() to clear
// the URL.
void SetActiveRendererURL(const GURL& url) {
  active_url = url.possibly_invalid_spec();
}

}  // namespace renderer_logging
