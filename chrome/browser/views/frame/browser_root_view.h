// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_ROOT_VIEW_H
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_ROOT_VIEW_H

#include "chrome/views/widget/root_view.h"

class OSExchangeData;
class TabStrip;

// RootView implementation used by BrowserFrame. This forwards drop events to
// the TabStrip. Visually the tabstrip extends to the top of the frame, but in
// actually it doesn't. The tabstrip is only as high as a tab. To enable
// dropping above the tabstrip BrowserRootView forwards drop events to the
// TabStrip.
class BrowserRootView : public views::RootView {
 public:
  explicit BrowserRootView(views::Widget* widget);

  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

 private:
  // Returns true if the event should be forwarded to the tabstrip.
  bool ShouldForwardToTabStrip(const views::DropTargetEvent& event);

  // Converts the event from the hosts coordinate system to the tabstrips
  // coordinate system.
  views::DropTargetEvent* MapEventToTabStrip(
      const views::DropTargetEvent& event);

  // The TabStrip.
  TabStrip* tabstrip_;

  // Is a drop allowed? This is set by CanDrop.
  bool can_drop_;

  // If true, drag and drop events are being forwarded to the tab strip.
  // This is used to determine when to send OnDragEntered and OnDragExited
  // to the tab strip.
  bool forwarding_to_tab_strip_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRootView);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_ROOT_VIEW_H
