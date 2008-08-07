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

#include "chrome/browser/views/frame/opaque_frame.h"

#include "chrome/browser/tabs/tab_strip.h"
#include "chrome/browser/views/frame/browser_view2.h"
#include "chrome/browser/views/frame/opaque_non_client_view.h"
#include "chrome/views/window_delegate.h"

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, public:

OpaqueFrame::OpaqueFrame(BrowserView2* browser_view)
    : CustomFrameWindow(browser_view, new OpaqueNonClientView(this, false)),
      browser_view_(browser_view) {
  browser_view_->set_frame(this);
}

OpaqueFrame::~OpaqueFrame() {
}

gfx::Rect OpaqueFrame::GetToolbarBounds() const {
  return browser_view_->GetToolbarBounds();
}

gfx::Rect OpaqueFrame::GetContentsBounds() const {
  return browser_view_->GetClientAreaBounds();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, BrowserFrame implementation:

gfx::Rect OpaqueFrame::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) {
  return GetOpaqueNonClientView()->GetWindowBoundsForClientBounds(
      client_bounds);
}

void OpaqueFrame::SizeToContents(const gfx::Rect& contents_bounds) {
  gfx::Rect window_bounds = GetOpaqueNonClientView()->
      GetWindowBoundsForClientBounds(contents_bounds);
  SetBounds(window_bounds);
}

gfx::Rect OpaqueFrame::GetBoundsForTabStrip(TabStrip* tabstrip) const {
  return GetOpaqueNonClientView()->GetBoundsForTabStrip(tabstrip);
}

ChromeViews::Window* OpaqueFrame::GetWindow() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, ChromeViews::HWNDViewContainer overrides:

bool OpaqueFrame::AcceleratorPressed(ChromeViews::Accelerator* accelerator) {
  return browser_view_->AcceleratorPressed(*accelerator);
}

bool OpaqueFrame::GetAccelerator(int cmd_id,
                                 ChromeViews::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, private:

OpaqueNonClientView* OpaqueFrame::GetOpaqueNonClientView() const {
  // We can safely assume that this conversion is true.
  return static_cast<OpaqueNonClientView*>(non_client_view_);
}

