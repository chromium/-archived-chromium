// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ClipboardChromium.h"
#include "DragData.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "Image.h"
#include "KURL.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/dragclient_impl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"

using WebKit::WebDragData;

void DragClientImpl::willPerformDragDestinationAction(
    WebCore::DragDestinationAction,
    WebCore::DragData*) {
  // FIXME
}

void DragClientImpl::willPerformDragSourceAction(
    WebCore::DragSourceAction,
    const WebCore::IntPoint&,
    WebCore::Clipboard*) {
  // FIXME
}

WebCore::DragDestinationAction DragClientImpl::actionMaskForDrag(
    WebCore::DragData*) {
  if (webview_->delegate()->CanAcceptLoadDrops()) {
    return WebCore::DragDestinationActionAny;
  } else {
    return static_cast<WebCore::DragDestinationAction>
        (WebCore::DragDestinationActionDHTML |
         WebCore::DragDestinationActionEdit);
  }
}

WebCore::DragSourceAction DragClientImpl::dragSourceActionMaskForPoint(
    const WebCore::IntPoint& window_point) {
  // We want to handle drag operations for all source types.
  return WebCore::DragSourceActionAny;
}

void DragClientImpl::startDrag(WebCore::DragImageRef drag_image,
                               const WebCore::IntPoint& drag_image_origin,
                               const WebCore::IntPoint& event_pos,
                               WebCore::Clipboard* clipboard,
                               WebCore::Frame* frame,
                               bool is_link_drag) {
  // Add a ref to the frame just in case a load occurs mid-drag.
  RefPtr<WebCore::Frame> frame_protector = frame;

  WebDragData drag_data = webkit_glue::ChromiumDataObjectToWebDragData(
      static_cast<WebCore::ClipboardChromium*>(clipboard)->dataObject());

  webview_->StartDragging(drag_data);
}

WebCore::DragImageRef DragClientImpl::createDragImageForLink(
    WebCore::KURL&,
    const WebCore::String& label,
    WebCore::Frame*) {
  // FIXME
  return 0;
}

void DragClientImpl::dragControllerDestroyed() {
  delete this;
}
