// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/api/src/TemporaryGlue.h"

#include <wtf/Assertions.h>
#undef LOG

#include "webkit/glue/webview_impl.h"

using WebCore::Frame;

namespace WebKit {

// static
WebMediaPlayer* TemporaryGlue::createWebMediaPlayer(
    WebMediaPlayerClient* client, Frame* frame) {
  WebViewImpl* webview = WebFrameImpl::FromFrame(frame)->GetWebViewImpl();
  if (!webview || !webview->delegate())
    return NULL;

  return webview->delegate()->CreateWebMediaPlayer(client);
}

}  // namespace WebKit
