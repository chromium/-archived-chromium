// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/bindings_utils.h"

#include "chrome/renderer/render_view.h"
#include "webkit/glue/webframe.h"

RenderView* GetRenderViewForCurrentContext() {
  WebFrame* webframe = WebFrame::RetrieveFrameForCurrentContext();
  DCHECK(webframe) << "RetrieveCurrentFrame called when not in a V8 context.";
  if (!webframe)
    return NULL;

  WebView* webview = webframe->GetView();
  if (!webview)
    return NULL;  // can happen during closing

  RenderView* renderview = static_cast<RenderView*>(webview->GetDelegate());
  DCHECK(renderview) << "Encountered a WebView without a WebViewDelegate";
  return renderview;
}
