// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_root_view.h"

#include "app/drag_drop_types.h"
#include "app/os_exchange_data.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/tabs/tab_strip_wrapper.h"

BrowserRootView::BrowserRootView(views::Widget* widget)
    : views::RootView(widget),
      tabstrip_(NULL),
      can_drop_(false),
      forwarding_to_tab_strip_(false) {
}

bool BrowserRootView::CanDrop(const OSExchangeData& data) {
  can_drop_ = (tabstrip_ && tabstrip_->GetView()->IsVisible() &&
               !tabstrip_->IsAnimating() && data.HasURL());
  return can_drop_;
}

void BrowserRootView::OnDragEntered(const views::DropTargetEvent& event) {
  if (can_drop_ && ShouldForwardToTabStrip(event)) {
    forwarding_to_tab_strip_ = true;
    scoped_ptr<views::DropTargetEvent> mapped_event(MapEventToTabStrip(event));
    tabstrip_->GetView()->OnDragEntered(*mapped_event.get());
  }
}

int BrowserRootView::OnDragUpdated(const views::DropTargetEvent& event) {
  if (can_drop_) {
    if (ShouldForwardToTabStrip(event)) {
      scoped_ptr<views::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
      if (!forwarding_to_tab_strip_) {
        tabstrip_->GetView()->OnDragEntered(*mapped_event.get());
        forwarding_to_tab_strip_ = true;
      }
      return tabstrip_->GetView()->OnDragUpdated(*mapped_event.get());
    } else if (forwarding_to_tab_strip_) {
      forwarding_to_tab_strip_ = false;
      tabstrip_->GetView()->OnDragExited();
    }
  }
  return DragDropTypes::DRAG_NONE;
}

void BrowserRootView::OnDragExited() {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    tabstrip_->GetView()->OnDragExited();
  }
}

int BrowserRootView::OnPerformDrop(const views::DropTargetEvent& event) {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    scoped_ptr<views::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
    return tabstrip_->GetView()->OnPerformDrop(*mapped_event.get());
  }
  return DragDropTypes::DRAG_NONE;
}

bool BrowserRootView::ShouldForwardToTabStrip(
    const views::DropTargetEvent& event) {
  if (!tabstrip_->GetView()->IsVisible())
    return false;

  // Allow the drop as long as the mouse is over the tabstrip or vertically
  // before it.
  gfx::Point tab_loc_in_host;
  ConvertPointToView(tabstrip_->GetView(), this, &tab_loc_in_host);
  return event.y() < tab_loc_in_host.y() + tabstrip_->GetView()->height();
}

views::DropTargetEvent* BrowserRootView::MapEventToTabStrip(
    const views::DropTargetEvent& event) {
  gfx::Point tab_strip_loc(event.location());
  ConvertPointToView(this, tabstrip_->GetView(), &tab_strip_loc);
  return new views::DropTargetEvent(event.GetData(), tab_strip_loc.x(),
                                    tab_strip_loc.y(),
                                    event.GetSourceOperations());
}
