// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_mixed_content_handler.h"

#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/ssl/ssl_policy.h"

SSLMixedContentHandler::SSLMixedContentHandler(
    ResourceDispatcherHost* rdh,
    URLRequest* request,
    ResourceType::Type resource_type,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    int pid,
    MessageLoop* ui_loop)
    : SSLErrorHandler(rdh, request, resource_type, frame_origin,
                      main_frame_origin, ui_loop),
      pid_(pid) {
}

void SSLMixedContentHandler::OnDispatchFailed() {
  TakeNoAction();
}

void SSLMixedContentHandler::OnDispatched() {
  manager_->policy()->OnMixedContent(this);
}
