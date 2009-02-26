// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_CLIENT_IMPL_H_
#define WEBKIT_CLIENT_IMPL_H_

#include "WebKitClient.h"

#include "base/scoped_ptr.h"
#include "webkit/glue/webclipboard_impl.h"

namespace webkit_glue {

class WebKitClientImpl : public WebKit::WebKitClient {
 public:
  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard();

 private:
  scoped_ptr<WebClipboardImpl> clipboard_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_CLIENT_IMPL_H_
