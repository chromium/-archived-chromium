// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#include <objidl.h>

#pragma warning(push, 0)
#include "ClipboardWin.h"
#include "COMPtr.h"
#include "DragData.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "Image.h"
#include "KURL.h"
#pragma warning(pop)
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

  COMPtr<IDataObject> data_object(
      static_cast<WebCore::ClipboardWin*>(clipboard)->dataObject());
  DCHECK(data_object.get());
  WebDropData drop_data;
  WebDropData::PopulateWebDropData(data_object.get(), &drop_data);

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
