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

#include "base/logging.h"
#include "chrome/views/client_view.h"

namespace ChromeViews {

///////////////////////////////////////////////////////////////////////////////
// ClientView, public:

ClientView::ClientView(Window* window, View* contents_view)
    : window_(window),
      contents_view_(contents_view) {
  DCHECK(window && contents_view);
}

int ClientView::NonClientHitTest(const gfx::Point& point) {
  CRect bounds;
  GetBounds(&bounds, APPLY_MIRRORING_TRANSFORMATION);
  if (gfx::Rect(bounds).Contains(point.x(), point.y()))
    return HTCLIENT;
  return HTNOWHERE;
}

///////////////////////////////////////////////////////////////////////////////
// ClientView, View overrides:

void ClientView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  contents_view_->GetPreferredSize(out);
}

void ClientView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    DCHECK(GetViewContainer());
    AddChildView(contents_view_);
  }
}

void ClientView::DidChangeBounds(const CRect& previous, const CRect& current) {
  Layout();
}

void ClientView::Layout() {
  contents_view_->SetBounds(0, 0, GetWidth(), GetHeight());
}

};  // namespace ChromeViews