// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#undef LOG

#include "webkit/glue/webview_delegate.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"

GURL WebViewDelegate::GetAlternateErrorPageURL(const GURL& failedURL,
                                               ErrorPageType error_type) {
  return GURL();
}
