// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_CLIENT_IMPL_H_
#define WEBKIT_CLIENT_IMPL_H_

#include "WebKitClient.h"

#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "webkit/glue/webclipboard_impl.h"

class MessageLoop;

namespace webkit_glue {

class WebKitClientImpl : public WebKit::WebKitClient {
 public:
  WebKitClientImpl();

  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry() = 0;
  virtual void setCookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url,
      const WebKit::WebString&) = 0;
  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url) = 0;
  virtual void prefetchHostName(const WebKit::WebString&) = 0;
  virtual WebKit::WebString defaultLocale() = 0;
  virtual double currentTime();
  virtual void setSharedTimerFiredFunction(void (*func)());
  virtual void setSharedTimerFireTime(double fireTime);
  virtual void stopSharedTimer();
  virtual void callOnMainThread(void (*func)());

 private:
  void DoTimeout() {
    if (shared_timer_func_)
      shared_timer_func_();
  }

  WebClipboardImpl clipboard_;
  MessageLoop* main_loop_;
  base::OneShotTimer<WebKitClientImpl> shared_timer_;
  void (*shared_timer_func_)();
};

}  // namespace webkit_glue

#endif  // WEBKIT_CLIENT_IMPL_H_
