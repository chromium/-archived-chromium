// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/views/client_view.h"

namespace ChromeViews {

///////////////////////////////////////////////////////////////////////////////
// ClientView, public:

ClientView::ClientView(Window* window, View* contents_view)
    : window_(window),
      contents_view_(contents_view) {
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
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a ViewContainer.
  if (contents_view_)
    contents_view_->GetPreferredSize(out);
}

void ClientView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    DCHECK(GetViewContainer());
    DCHECK(contents_view_); // |contents_view_| must be valid now!
    AddChildView(contents_view_);
  }
}

void ClientView::DidChangeBounds(const CRect& previous, const CRect& current) {
  Layout();
}

void ClientView::Layout() {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a ViewContainer.
  if (contents_view_)
    contents_view_->SetBounds(0, 0, width(), height());
}

};  // namespace ChromeViews
