// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FRAME_VIEW_H__
#define CHROME_BROWSER_FRAME_VIEW_H__

#include <set>

#include "chrome/views/view.h"

class BrowserWindow;
class OSExchangeData;

// FrameView is the View that contains all the views of the BrowserWindow
// (XPFrame or VistaFrame). FrameView forwards all drag and drop messages to
// the TabStrip.
class FrameView : public views::View {
 public:
  explicit FrameView(BrowserWindow* frame);
  virtual ~FrameView() {}

  // Adds view to the set of views that drops are allowed to occur on. You only
  // need invoke this for views whose y-coordinate extends above the tab strip
  // and you want to allow drops on.
  void AddViewToDropList(views::View* view);

 protected:
  // As long as ShouldForwardToTabStrip returns true, drag and drop methods
  // are forwarded to the tab strip.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // Returns true if the event should be forwarded to the TabStrip. This returns
  // true if y coordinate is less than the bottom of the tab strip, and is not
  // over another child view.
  virtual bool ShouldForwardToTabStrip(const views::DropTargetEvent& event);

  // Overriden to remove views from dropable_views_.
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

 private:
  // Creates and returns a new DropTargetEvent in the coordinates of the
  // TabStrip.
  views::DropTargetEvent* MapEventToTabStrip(
      const views::DropTargetEvent& event);

  // The BrowserWindow we're the child of.
  BrowserWindow* window_;

  // Initially set in CanDrop by invoking the same method on the TabStrip.
  bool can_drop_;

  // If true, drag and drop events are being forwarded to the tab strip.
  // This is used to determine when to send OnDragExited and OnDragExited
  // to the tab strip.
  bool forwarding_to_tab_strip_;

  // Set of additional views drops are allowed on. We do NOT own these.
  std::set<views::View*> dropable_views_;

  DISALLOW_EVIL_CONSTRUCTORS(FrameView);
};

#endif  // CHROME_BROWSER_FRAME_VIEW_H__

