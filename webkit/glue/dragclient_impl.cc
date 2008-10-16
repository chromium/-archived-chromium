// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <objidl.h>
#endif

MSVC_PUSH_WARNING_LEVEL(0);
#if defined(OS_WIN)
#include "ClipboardWin.h"
#include "COMPtr.h"
#endif
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
#include "webkit/glue/context_node_types.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"

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
  return WebCore::DragDestinationActionAny; 
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

#if defined(OS_WIN)
  COMPtr<IDataObject> data_object(
      static_cast<WebCore::ClipboardWin*>(clipboard)->dataObject());
  DCHECK(data_object.get());
  WebDropData drop_data;
  WebDropData::PopulateWebDropData(data_object.get(), &drop_data);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  WebDropData drop_data;
#endif

  webview_->StartDragging(drop_data);
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

