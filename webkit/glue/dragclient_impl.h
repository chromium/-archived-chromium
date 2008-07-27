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

#ifndef WEBKIT_GLUE_DRAGCLIENT_IMPL_H__
#define WEBKIT_GLUE_DRAGCLIENT_IMPL_H__

#include "base/basictypes.h"

#pragma warning(push, 0)
#include "DragClient.h"
#include "DragActions.h"
#pragma warning(pop)

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
