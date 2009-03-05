// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DRAGCLIENT_IMPL_H__
#define WEBKIT_GLUE_DRAGCLIENT_IMPL_H__

#include "base/basictypes.h"
#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "DragClient.h"
#include "DragActions.h"
MSVC_POP_WARNING();

namespace WebCore {
class ClipBoard;
class DragData;
class IntPoint;
class KURL;
}

class WebViewImpl;

class DragClientImpl : public WebCore::DragClient {
public:
  DragClientImpl(WebViewImpl* webview) : webview_(webview) {}
  virtual ~DragClientImpl() {}

  virtual void willPerformDragDestinationAction(WebCore::DragDestinationAction,
                                                WebCore::DragData*);
  virtual void willPerformDragSourceAction(WebCore::DragSourceAction,
                                           const WebCore::IntPoint&,
                                           WebCore::Clipboard*);
  virtual WebCore::DragDestinationAction actionMaskForDrag(WebCore::DragData*);
  virtual WebCore::DragSourceAction dragSourceActionMaskForPoint(
      const WebCore::IntPoint& window_point);

  virtual void startDrag(WebCore::DragImageRef drag_image,
                         const WebCore::IntPoint& drag_image_origin,
                         const WebCore::IntPoint& event_pos,
                         WebCore::Clipboard* clipboard,
                         WebCore::Frame* frame,
                         bool is_link_drag = false);
  virtual WebCore::DragImageRef createDragImageForLink(
      WebCore::KURL&, const WebCore::String& label, WebCore::Frame*);

  virtual void dragControllerDestroyed();

private:
  DISALLOW_EVIL_CONSTRUCTORS(DragClientImpl);
  WebViewImpl* webview_;
};

#endif  // #ifndef WEBKIT_GLUE_DRAGCLIENT_IMPL_H__

