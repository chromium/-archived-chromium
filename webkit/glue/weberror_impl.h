// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBERROR_IMPL_H__
#define WEBKIT_GLUE_WEBERROR_IMPL_H__

#include "base/basictypes.h"
#include "webkit/glue/weberror.h"
#include "googleurl/src/gurl.h"

namespace WebCore {
class ResourceError;
};

class WebErrorImpl : public WebError {
 public:
  explicit WebErrorImpl(const WebCore::ResourceError& error);
  explicit WebErrorImpl(const WebError& error);
  ~WebErrorImpl() {}

  // WebError implementation:
  virtual int GetErrorCode() const;
  virtual const GURL& GetFailedURL() const;

 private:
  int error_code_;
  GURL failed_url_;

  // Disallow assignment operator
  void operator=(const WebErrorImpl&);
};

#endif  // WEBKIT_GLUE_WEBERROR_IMPL_H__

