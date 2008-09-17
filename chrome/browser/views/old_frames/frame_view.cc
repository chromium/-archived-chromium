// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/old_frames/frame_view.h"

#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/os_exchange_data.h"

FrameView::FrameView(BrowserWindow* window)
    : window_(window),
      can_drop_(false),
      forwarding_to_tab_strip_(false) {
}

void FrameView::AddViewToDropList(ChromeViews::View* view) {
  dropable_views_.insert(view);
}

bool FrameView::CanDrop(const OSExchangeData& data) {
  can_drop_ = (window_->GetTabStrip()->IsVisible() &&
               !window_->GetTabStrip()->IsAnimating() &&
               data.HasURL());
  return can_drop_;
}

void FrameView::OnDragEntered(const ChromeViews::DropTargetEvent& event) {
  if (can_drop_ && ShouldForwardToTabStrip(event)) {
    forwarding_to_tab_strip_ = true;
    scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event));
    window_->GetTabStrip()->OnDragEntered(*mapped_event.get());
  }
}

int FrameView::OnDragUpdated(const ChromeViews::DropTargetEvent& event) {
  if (can_drop_) {
    if (ShouldForwardToTabStrip(event)) {
      scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
      if (!forwarding_to_tab_strip_) {
        window_->GetTabStrip()->OnDragEntered(*mapped_event.get());
        forwarding_to_tab_strip_ = true;
      }
      return window_->GetTabStrip()->OnDragUpdated(*mapped_event.get());
    } else if (forwarding_to_tab_strip_) {
      forwarding_to_tab_strip_ = false;
      window_->GetTabStrip()->OnDragExited();
    }
  }
  return DragDropTypes::DRAG_NONE;
}

void FrameView::OnDragExited() {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    window_->GetTabStrip()->OnDragExited();
  }
}

int FrameView::OnPerformDrop(const ChromeViews::DropTargetEvent& event) {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
    return window_->GetTabStrip()->OnPerformDrop(*mapped_event.get());
  }
  return DragDropTypes::DRAG_NONE;
}

bool FrameView::ShouldForwardToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  if (!window_->GetTabStrip()->IsVisible())
    return false;

  const int tab_y = window_->GetTabStrip()->y();
  const int tab_height = window_->GetTabStrip()->height();
  if (event.y() >= tab_y + tab_height)
    return false;

  if (event.y() >= tab_y)
    return true;

  // Mouse isn't over the tab strip. Only forward if the mouse isn't over
  // another view on the tab strip or is over a view we were told the user can
  // drop on.
  ChromeViews::View* view_over_mouse =
      GetViewForPoint(CPoint(event.x(), event.y()));
  return (view_over_mouse == this ||
          view_over_mouse == window_->GetTabStrip() ||
          dropable_views_.find(view_over_mouse) != dropable_views_.end());
}

void FrameView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (!is_add)
    dropable_views_.erase(child);
}

ChromeViews::DropTargetEvent* FrameView::MapEventToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  gfx::Point tab_strip_loc(event.location());
  ConvertPointToView(this, window_->GetTabStrip(), &tab_strip_loc);
  return new ChromeViews::DropTargetEvent(event.GetData(), tab_strip_loc.x(),
                                          tab_strip_loc.y(),
                                          event.GetSourceOperations());
}

