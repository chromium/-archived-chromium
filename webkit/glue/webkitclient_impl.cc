// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webkitclient_impl.h"

namespace webkit_glue {

WebKit::WebClipboard* WebKitClientImpl::clipboard() {
  return &clipboard_;
}

}  // namespace webkit_glue
