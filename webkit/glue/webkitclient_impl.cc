// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// TODO(darin): This include should not be necessary, but for some reason the
// Mac build requires it.
#include "config.h"

#include "webkit/glue/webkitclient_impl.h"

namespace webkit_glue {

WebKit::WebClipboard* WebKitClientImpl::clipboard() {
  return &clipboard_;
}

}  // namespace webkit_glue
