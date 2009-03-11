// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ResourceError.h"

#undef LOG
#include "webkit/glue/weberror_impl.h"

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

WebErrorImpl::WebErrorImpl(const WebCore::ResourceError& e)
    : error_code_(e.errorCode()),
      failed_url_(webkit_glue::StringToStdString(e.failingURL())) {
}

WebErrorImpl::WebErrorImpl(const WebError& e)
    : error_code_(e.GetErrorCode()),
      failed_url_(e.GetFailedURL()) {
}

int WebErrorImpl::GetErrorCode() const {
  return error_code_;
}

const GURL& WebErrorImpl::GetFailedURL() const {
  return failed_url_;
}
