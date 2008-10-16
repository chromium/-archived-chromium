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
  return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
}

///////////////////////////////////////////////////////////////////////////////
// ClientView, View overrides:

gfx::Size ClientView::GetPreferredSize() {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  if (contents_view_)
    return contents_view_->GetPreferredSize();
  return gfx::Size();
}

void ClientView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    DCHECK(GetContainer());
    DCHECK(contents_view_); // |contents_view_| must be valid now!
    AddChildView(contents_view_);
  }
}

void ClientView::Layout() {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  if (contents_view_)
    contents_view_->SetBounds(0, 0, width(), height());
}

};  // namespace ChromeViews
