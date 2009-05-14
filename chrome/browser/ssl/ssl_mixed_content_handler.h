// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_MIXED_CONTENT_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_MIXED_CONTENT_HANDLER_H_

#include "chrome/browser/ssl/ssl_error_handler.h"
#include "chrome/browser/ssl/ssl_manager.h"

// The SSLMixedContentHandler class is used to query what to do with
// mixed content, from the IO thread to the UI thread.
class SSLMixedContentHandler : public SSLErrorHandler {
 public:
  // Created on the IO thread.
  SSLMixedContentHandler(ResourceDispatcherHost* rdh,
                         URLRequest* request,
                         ResourceType::Type resource_type,
                         const std::string& frame_origin,
                         const std::string& main_frame_origin,
                         int pid,
                         MessageLoop* ui_loop)
      : SSLErrorHandler(rdh, request, resource_type, frame_origin,
                        main_frame_origin, ui_loop),
        pid_(pid) {}

  int pid() const { return pid_; }

 protected:
  virtual void OnDispatchFailed() { TakeNoAction(); }
  virtual void OnDispatched() { manager()->OnMixedContent(this); }

 private:
  int pid_;

  DISALLOW_COPY_AND_ASSIGN(SSLMixedContentHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_MIXED_CONTENT_HANDLERR_H_
