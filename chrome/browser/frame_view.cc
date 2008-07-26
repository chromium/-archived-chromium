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

#include "chrome/browser/frame_view.h"

#include "chrome/browser/chrome_frame.h"
#include "chrome/browser/tabs/tab_strip.h"
#include "chrome/common/os_exchange_data.h"

FrameView::FrameView(ChromeFrame* frame)
    : frame_(frame),
      can_drop_(false),
      forwarding_to_tab_strip_(false) {
}

void FrameView::AddViewToDropList(ChromeViews::View* view) {
  dropable_views_.insert(view);
}

bool FrameView::CanDrop(const OSExchangeData& data) {
  can_drop_ = (frame_->GetTabStrip()->IsVisible() &&
               !frame_->GetTabStrip()->IsAnimating() &&
               data.HasURL());
  return can_drop_;
}

void FrameView::OnDragEntered(const ChromeViews::DropTargetEvent& event) {
  if (can_drop_ && ShouldForwardToTabStrip(event)) {
    forwarding_to_tab_strip_ = true;
    scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event));
    frame_->GetTabStrip()->OnDragEntered(*mapped_event.get());
  }
}

int FrameView::OnDragUpdated(const ChromeViews::DropTargetEvent& event) {
  if (can_drop_) {
    if (ShouldForwardToTabStrip(event)) {
      scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
      if (!forwarding_to_tab_strip_) {
        frame_->GetTabStrip()->OnDragEntered(*mapped_event.get());
        forwarding_to_tab_strip_ = true;
      }
      return frame_->GetTabStrip()->OnDragUpdated(*mapped_event.get());
    } else if (forwarding_to_tab_strip_) {
      forwarding_to_tab_strip_ = false;
      frame_->GetTabStrip()->OnDragExited();
    }
  }
  return DragDropTypes::DRAG_NONE;
}

void FrameView::OnDragExited() {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    frame_->GetTabStrip()->OnDragExited();
  }
}

int FrameView::OnPerformDrop(const ChromeViews::DropTargetEvent& event) {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
    return frame_->GetTabStrip()->OnPerformDrop(*mapped_event.get());
  }
  return DragDropTypes::DRAG_NONE;
}

bool FrameView::ShouldForwardToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  if (!frame_->GetTabStrip()->IsVisible())
    return false;

  const int tab_y = frame_->GetTabStrip()->GetY();
  const int tab_height = frame_->GetTabStrip()->GetHeight();
  if (event.GetY() >= tab_y + tab_height)
    return false;

  if (event.GetY() >= tab_y)
    return true;

  // Mouse isn't over the tab strip. Only forward if the mouse isn't over
  // another view on the tab strip or is over a view we were told the user can
  // drop on.
  ChromeViews::View* view_over_mouse =
      GetViewForPoint(CPoint(event.GetX(), event.GetY()));
  return (view_over_mouse == this ||
          view_over_mouse == frame_->GetTabStrip() ||
          dropable_views_.find(view_over_mouse) != dropable_views_.end());
}

void FrameView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (!is_add)
    dropable_views_.erase(child);
}

ChromeViews::DropTargetEvent* FrameView::MapEventToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  gfx::Point tab_strip_loc(event.location());
  ConvertPointToView(this, frame_->GetTabStrip(), &tab_strip_loc);
  return new ChromeViews::DropTargetEvent(event.GetData(), tab_strip_loc.x(),
                                          tab_strip_loc.y(),
                                          event.GetSourceOperations());
}
