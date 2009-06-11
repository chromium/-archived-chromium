// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLLOADER_IMPL_H_
#define WEBKIT_GLUE_WEBURLLOADER_IMPL_H_

#include "base/ref_counted.h"
#include "webkit/api/public/WebURLLoader.h"

namespace webkit_glue {

class WebURLLoaderImpl : public WebKit::WebURLLoader {
 public:
  WebURLLoaderImpl();
  ~WebURLLoaderImpl();

  // WebURLLoader methods:
  virtual void loadSynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLResponse& response,
      WebKit::WebURLError& error,
      WebKit::WebData& data);
  virtual void loadAsynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool value);

 private:
  class Context;
  scoped_refptr<Context> context_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBURLLOADER_IMPL_H_
